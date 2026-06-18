/*
 * SIMP processor simulator
 * ------------------------------------------------------------------
 * Simulates the SIMP RISC processor together with its I/O devices:
 *   - 16 general purpose 32-bit registers
 *   - 4096 x 32-bit main memory
 *   - 23 hardware (I/O) registers
 *   - 3 interrupts (timer / disk / external irq2)
 *   - a 32-bit timer
 *   - 32 LEDs and an 8-digit 7-segment display
 *   - a 256x256 monochrome monitor (frame buffer)
 *   - a hard disk of 128 sectors x 128 words, accessed through DMA
 *
 * The processor is modelled cycle by cycle. To faithfully imitate real
 * hardware (where a flip-flop has a current value Q and a next value D),
 * the values that a device produces during a clock cycle (the interrupt
 * status bits, the timer, the disk) only become visible to the processor
 * on the FOLLOWING clock cycle. Concretely: the interrupt decision at the
 * top of the main loop is taken using the status registers as they were
 * at the end of the previous cycle, while the status bits that are raised
 * during the current cycle (irq2 line sampling, timer match, disk done)
 * are updated only after the instruction has executed. This one-cycle
 * "register" delay is exactly what real hardware does, and it is what the
 * reference outputs expect.
 *
 * Command line (13 parameters):
 *   sim.exe memin.txt diskin.txt irq2in.txt memout.txt regout.txt
 *           trace.txt hwregtrace.txt cycles.txt leds.txt display7seg.txt
 *           diskout.txt monitor.txt monitor.yuv
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

 /* ============================================================
  * Sizes and constants
  * ============================================================ */
#define NREG          16          /* number of general purpose registers      */
#define MEM_SIZE      4096         /* main memory depth (12-bit address)       */
#define PC_MASK       0xFFF        /* PC is 12 bits                            */
#define NIO           23           /* number of hardware (I/O) registers       */
#define DISK_SECTORS  128          /* number of disk sectors                   */
#define SECTOR_WORDS  128          /* words per disk sector                    */
#define DISK_WORDS    (DISK_SECTORS * SECTOR_WORDS)   /* 16384                 */
#define MON_SIZE      (256 * 256)  /* monitor frame buffer (65536 pixels)      */
#define DISK_DELAY    1024         /* clock cycles to service a disk command   */
#define MAX_LINE      600          /* input line buffer (max line length 500)  */

  /* I/O register indices (by name, for readability) */
enum {
    IO_IRQ0ENABLE = 0, IO_IRQ1ENABLE, IO_IRQ2ENABLE,
    IO_IRQ0STATUS, IO_IRQ1STATUS, IO_IRQ2STATUS,
    IO_IRQHANDLER, IO_IRQRETURN, IO_CLKS,
    IO_LEDS, IO_DISPLAY7SEG,
    IO_TIMERENABLE, IO_TIMERCURRENT, IO_TIMERMAX,
    IO_DISKCMD, IO_DISKSECTOR, IO_DISKBUFFER, IO_DISKSTATUS,
    IO_RESERVED18, IO_RESERVED19,
    IO_MONITORADDR, IO_MONITORDATA, IO_MONITORCMD
};

/* Names of the hardware registers, used in hwregtrace.txt */
static const char* io_names[NIO] = {
    "irq0enable", "irq1enable", "irq2enable",
    "irq0status", "irq1status", "irq2status",
    "irqhandler", "irqreturn", "clks",
    "leds", "display7seg",
    "timerenable", "timercurrent", "timermax",
    "diskcmd", "disksector", "diskbuffer", "diskstatus",
    "reserved", "reserved",
    "monitoraddr", "monitordata", "monitorcmd"
};

/* ============================================================
 * Machine state
 * ============================================================ */
static int           reg[NREG];           /* general purpose registers (R0..R15)   */
static unsigned int  mem[MEM_SIZE];        /* main memory                           */
static unsigned int  io[NIO];              /* hardware registers                    */
static unsigned int  disk[DISK_WORDS];     /* hard disk content                     */
static unsigned char monitor[MON_SIZE];    /* monitor frame buffer                  */

static unsigned int  pc = 0;               /* program counter (12-bit)              */
static unsigned int  cycle = 0;            /* global clock-cycle counter            */
static int           halted = 0;           /* set when HALT executes                */
static int           in_isr = 0;           /* 1 while inside an interrupt handler   */

/* irq2 external line: list of cycle numbers (ascending) where it rises */
static unsigned int* irq2_list = NULL;
static int           irq2_count = 0;
static int           irq2_idx = 0;       /* next entry to consume                 */

/* disk DMA state */
static int           disk_busy = 0;        /* 1 while a command is being serviced   */
static int           disk_remaining = 0;   /* cycles left until the command is done  */

/* output files */
static FILE* f_trace, * f_hwtrace, * f_leds, * f_7seg;

/* ============================================================
 * Small helpers
 * ============================================================ */

 /* Sign-extend a 12-bit immediate to 32 bits (replicate bit 11). */
static int sign_extend12(unsigned int imm12)
{
    if (imm12 & 0x800)
        return (int)(imm12 | 0xFFFFF000u);
    return (int)imm12;
}

/* These three are valid only during the execution of one instruction. */
static int  cur_imm1;   /* sign-extended imm12 of the current instruction      */
static int  cur_imm2;   /* second word (imm32) of the current instruction, or 0 */

/* Read the value of an operand register.
 *  $zero -> 0, $imm1 -> sign-extended imm12, $imm2 -> imm32, else the file. */
static int read_reg(int idx)
{
    if (idx == 0) return 0;
    if (idx == 1) return cur_imm1;
    if (idx == 2) return cur_imm2;
    return reg[idx];
}

/* Write a register. Writes to $zero/$imm1/$imm2 are ignored (read-only). */
static void write_reg(int idx, int value)
{
    if (idx <= 2) return;           /* R0, R1, R2 cannot be written           */
    reg[idx] = value;
}

/* ============================================================
 * I/O register access (in / out instructions)
 * ============================================================ */

 /* Log a single hardware-register access to hwregtrace.txt. */
static void hwtrace(const char* rw, int idx, unsigned int data)
{
    if (idx < 0 || idx >= NIO) return;
    fprintf(f_hwtrace, "%08X %s %s %08X\n", cycle, rw, io_names[idx], data);
}

/* Execute "in":  R[rd] = IORegister[addr]. */
static unsigned int io_read(int addr)
{
    unsigned int val;
    if (addr < 0 || addr >= NIO) return 0;
    /* Reading monitorcmd always returns 0 (per spec). */
    if (addr == IO_MONITORCMD) val = 0;
    else                       val = io[addr];
    hwtrace("READ", addr, val);
    return val;
}

/* Execute "out": IORegister[addr] = value, then apply side effects. */
static void io_write(int addr, unsigned int value)
{
    if (addr < 0 || addr >= NIO) return;

    hwtrace("WRITE", addr, value);

    /* LEDs: log a line only when the value actually changes. */
    if (addr == IO_LEDS && value != io[IO_LEDS]) {
        io[IO_LEDS] = value;
        fprintf(f_leds, "%08X %08X\n", cycle, value);
        return;
    }
    /* 7-segment display: same change-only logging. */
    if (addr == IO_DISPLAY7SEG && value != io[IO_DISPLAY7SEG]) {
        io[IO_DISPLAY7SEG] = value;
        fprintf(f_7seg, "%08X %08X\n", cycle, value);
        return;
    }

    /* Monitor: a write of 1 to monitorcmd draws the pixel. */
    if (addr == IO_MONITORCMD) {
        io[IO_MONITORCMD] = value;
        if (value == 1) {
            unsigned int a = io[IO_MONITORADDR] & 0xFFFF;   /* 16-bit address  */
            if (a < MON_SIZE)
                monitor[a] = (unsigned char)(io[IO_MONITORDATA] & 0xFF);
        }
        return;
    }

    /* Disk command: start a read/write only if the disk is free. */
    if (addr == IO_DISKCMD) {
        io[IO_DISKCMD] = value;
        if ((value == 1 || value == 2) && io[IO_DISKSTATUS] == 0) {
            io[IO_DISKSTATUS] = 1;          /* busy                            */
            disk_busy = 1;
            disk_remaining = DISK_DELAY;
        }
        return;
    }

    /* Default: just store the value. */
    io[addr] = value;
}

/* Perform the DMA transfer for the active disk command and finish it. */
static void disk_complete(void)
{
    int i;
    unsigned int sector = io[IO_DISKSECTOR] & 0x7F;        /* 7-bit sector     */
    unsigned int buf = io[IO_DISKBUFFER] & PC_MASK;     /* 12-bit address   */
    unsigned int cmd = io[IO_DISKCMD];

    if (cmd == 1) {                       /* read sector -> memory buffer       */
        for (i = 0; i < SECTOR_WORDS; i++)
            mem[(buf + i) & (MEM_SIZE - 1)] = disk[sector * SECTOR_WORDS + i];
    }
    else if (cmd == 2) {                /* write memory buffer -> sector      */
        for (i = 0; i < SECTOR_WORDS; i++)
            disk[sector * SECTOR_WORDS + i] = mem[(buf + i) & (MEM_SIZE - 1)];
    }

    io[IO_DISKCMD] = 0;                 /* command consumed                  */
    io[IO_DISKSTATUS] = 0;                 /* free again                        */
    io[IO_IRQ1STATUS] = 1;                 /* signal the disk interrupt         */
    disk_busy = 0;
}

/* ============================================================
 * Per-cycle device updates (timer, disk, external irq2 line)
 * These run AFTER the instruction executes, so anything they raise is
 * only seen by the interrupt logic on the next cycle (registered state).
 * Called once for every clock cycle the instruction occupied.
 * ============================================================ */
static void device_tick(unsigned int dev_cycle)
{
    /* Sample the external irq2 line for this exact cycle. */
    while (irq2_idx < irq2_count && irq2_list[irq2_idx] == dev_cycle) {
        io[IO_IRQ2STATUS] = 1;
        irq2_idx++;
    }

    /* Timer. */
    if (io[IO_TIMERENABLE] == 1) {
        if (io[IO_TIMERCURRENT] == io[IO_TIMERMAX]) {
            io[IO_IRQ0STATUS] = 1;
            io[IO_TIMERCURRENT] = 0;
        }
        else {
            io[IO_TIMERCURRENT]++;
        }
    }

    /* Disk: count down and finish after DISK_DELAY cycles. */
    if (disk_busy) {
        disk_remaining--;
        if (disk_remaining <= 0)
            disk_complete();
    }
}

/* ============================================================
 * Trace
 * ============================================================ */
static void write_trace(unsigned int exec_cycle, unsigned int trace_pc,
    unsigned int inst)
{
    int i;
    fprintf(f_trace, "%08X %03X %08X", exec_cycle, trace_pc, inst);
    /* R0 is always 0; R1 is imm1; R2 is imm2 (or 0); R3..R15 are the file. */
    fprintf(f_trace, " %08X", 0);
    fprintf(f_trace, " %08X", (unsigned int)cur_imm1);
    fprintf(f_trace, " %08X", (unsigned int)cur_imm2);
    for (i = 3; i < NREG; i++)
        fprintf(f_trace, " %08X", (unsigned int)reg[i]);
    fprintf(f_trace, "\n");
}

/* ============================================================
 * Instruction execution
 * Returns the next PC. 'n' is the number of words/cycles (1 or 2).
 * ============================================================ */
static unsigned int execute(unsigned int inst, unsigned int n)
{
    int opcode = (inst >> 24) & 0xFF;
    int rd = (inst >> 20) & 0xF;
    int rs = (inst >> 16) & 0xF;
    int rt = (inst >> 12) & 0xF;

    int vs = read_reg(rs);
    int vt = read_reg(rt);
    unsigned int next_pc = (pc + n) & PC_MASK;   /* default: fall through      */

    switch (opcode) {
    case 0:  write_reg(rd, vs + vt);                       break; /* add  */
    case 1:  write_reg(rd, vs - vt);                       break; /* sub  */
    case 2:  write_reg(rd, vs * vt);                       break; /* mul  */
    case 3:  write_reg(rd, vs * vt + reg[rd]);             break; /* mac  */
    case 4:  write_reg(rd, vs & vt);                       break; /* and  */
    case 5:  write_reg(rd, vs | vt);                       break; /* or   */
    case 6:  write_reg(rd, vs ^ vt);                       break; /* xor  */
    case 7:  write_reg(rd, (int)((unsigned int)vs << (vt & 31)));  break; /* sll */
    case 8:  write_reg(rd, vs >> (vt & 31));               break; /* sra (arithmetic) */
    case 9:  write_reg(rd, (int)((unsigned int)vs >> (vt & 31)));  break; /* srl */
    case 10: if (vs == vt) next_pc = read_reg(rd) & PC_MASK; break; /* beq */
    case 11: if (vs != vt) next_pc = read_reg(rd) & PC_MASK; break; /* bne */
    case 12: if (vs < vt) next_pc = read_reg(rd) & PC_MASK; break; /* blt */
    case 13: if (vs > vt) next_pc = read_reg(rd) & PC_MASK; break; /* bgt */
    case 14: if (vs <= vt) next_pc = read_reg(rd) & PC_MASK; break; /* ble */
    case 15: if (vs >= vt) next_pc = read_reg(rd) & PC_MASK; break; /* bge */
    case 16: /* jal */
        write_reg(rd, (int)((pc + n) & PC_MASK));   /* save return address     */
        next_pc = read_reg(rs) & PC_MASK;
        break;
    case 17: /* lw  */
        write_reg(rd, (int)mem[(vs + vt) & (MEM_SIZE - 1)]);
        break;
    case 18: /* sw  */
        mem[(vs + vt) & (MEM_SIZE - 1)] = (unsigned int)read_reg(rd);
        break;
    case 19: /* reti */
        next_pc = io[IO_IRQRETURN] & PC_MASK;
        in_isr = 0;
        break;
    case 20: /* in  */
        write_reg(rd, (int)io_read(vs + vt));
        break;
    case 21: /* out */
        io_write(vs + vt, (unsigned int)read_reg(rd));
        break;
    case 22: /* halt */
        halted = 1;
        break;
    default:
        /* Unknown opcode: ignore and fall through. */
        break;
    }

    return next_pc;
}

/* ============================================================
 * Input loading
 * ============================================================ */

 /* Load a hex-per-line file into a word array (up to 'max' words). */
static void load_words(const char* path, unsigned int* arr, int max)
{
    FILE* f = fopen(path, "r");
    char line[MAX_LINE];
    int i = 0;
    if (f == NULL) return;                  /* missing file -> all zeros        */
    while (i < max && fgets(line, sizeof(line), f) != NULL) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        arr[i++] = (unsigned int)strtoul(line, NULL, 16);
    }
    fclose(f);
}

/* Load irq2in.txt: one decimal cycle number per line, ascending. */
static void load_irq2(const char* path)
{
    FILE* f = fopen(path, "r");
    char line[MAX_LINE];
    int cap = 16;
    if (f == NULL) return;
    irq2_list = (unsigned int*)malloc(cap * sizeof(unsigned int));
    while (fgets(line, sizeof(line), f) != NULL) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n' || *p == '\r') continue;
        if (irq2_count == cap) {
            cap *= 2;
            irq2_list = (unsigned int*)realloc(irq2_list, cap * sizeof(unsigned int));
        }
        irq2_list[irq2_count++] = (unsigned int)strtoul(p, NULL, 10);
    }
    fclose(f);
}

/* ============================================================
 * Output writing (final files)
 * ============================================================ */

 /* Write a word array, trimming trailing zero words. */
static void write_mem_trimmed(const char* path, unsigned int* arr, int count)
{
    FILE* f = fopen(path, "w");
    int last = -1, i;
    if (f == NULL) return;
    for (i = 0; i < count; i++)
        if (arr[i] != 0) last = i;
    for (i = 0; i <= last; i++)
        fprintf(f, "%08X\n", arr[i]);
    fclose(f);
}

/* Write the whole disk (always the full 128*128 words). */
static void write_disk(const char* path)
{
    FILE* f = fopen(path, "w");
    int i;
    if (f == NULL) return;
    for (i = 0; i < DISK_WORDS; i++)
        fprintf(f, "%08X\n", disk[i]);
    fclose(f);
}

/* Write registers R3..R15 (one per line, 8 hex digits). */
static void write_regout(const char* path)
{
    FILE* f = fopen(path, "w");
    int i;
    if (f == NULL) return;
    for (i = 3; i < NREG; i++)
        fprintf(f, "%08X\n", (unsigned int)reg[i]);
    fclose(f);
}

/* monitor.txt: one pixel per line (2 hex digits), trailing zeros trimmed. */
static void write_monitor_txt(const char* path)
{
    FILE* f = fopen(path, "w");
    int last = -1, i;
    if (f == NULL) return;
    for (i = 0; i < MON_SIZE; i++)
        if (monitor[i] != 0) last = i;
    for (i = 0; i <= last; i++)
        fprintf(f, "%02X\n", monitor[i]);
    fclose(f);
}

/* monitor.yuv: the full 65536-byte frame buffer, raw binary. */
static void write_monitor_yuv(const char* path)
{
    FILE* f = fopen(path, "wb");
    if (f == NULL) return;
    fwrite(monitor, 1, MON_SIZE, f);
    fclose(f);
}

/* ============================================================
 * Main
 * ============================================================ */
int main(int argc, char* argv[])
{
    if (argc != 14) {
        printf("Usage: sim.exe memin.txt diskin.txt irq2in.txt memout.txt "
            "regout.txt trace.txt hwregtrace.txt cycles.txt leds.txt "
            "display7seg.txt diskout.txt monitor.txt monitor.yuv\n");
        return 1;
    }

    /* --- load inputs --- */
    load_words(argv[1], mem, MEM_SIZE);     /* memin.txt   */
    load_words(argv[2], disk, DISK_WORDS);   /* diskin.txt  */
    load_irq2(argv[3]);                      /* irq2in.txt  */

    /* --- open the streaming output files --- */
    f_trace = fopen(argv[6], "w");        /* trace.txt        */
    f_hwtrace = fopen(argv[7], "w");        /* hwregtrace.txt   */
    f_leds = fopen(argv[9], "w");        /* leds.txt         */
    f_7seg = fopen(argv[10], "w");        /* display7seg.txt  */
    if (!f_trace || !f_hwtrace || !f_leds || !f_7seg) {
        printf("Error: cannot open output files\n");
        return 1;
    }

    /* --- fetch / decode / execute loop --- */
    while (!halted) {
        unsigned int inst, second, exec_cycle, irq;
        int rd, rs, rt, n;
        unsigned int k;

        /* 1. Interrupt check (uses the registered status from prior cycles). */
        irq = (io[IO_IRQ0ENABLE] & io[IO_IRQ0STATUS]) |
            (io[IO_IRQ1ENABLE] & io[IO_IRQ1STATUS]) |
            (io[IO_IRQ2ENABLE] & io[IO_IRQ2STATUS]);
        if (irq && !in_isr) {
            io[IO_IRQRETURN] = pc;               /* remember where we were      */
            pc = io[IO_IRQHANDLER] & PC_MASK;    /* jump to the handler         */
            in_isr = 1;
        }

        /* 2. Fetch and decode the first word. */
        inst = mem[pc & (MEM_SIZE - 1)];
        rd = (inst >> 20) & 0xF;
        rs = (inst >> 16) & 0xF;
        rt = (inst >> 12) & 0xF;

        /* An instruction uses a second word (imm2) iff one of its operand
           fields selects register 2 ($imm2). Such instructions take 2 cycles. */
        n = (rd == 2 || rs == 2 || rt == 2) ? 2 : 1;
        second = (n == 2) ? mem[(pc + 1) & (MEM_SIZE - 1)] : 0;

        cur_imm1 = sign_extend12(inst & 0xFFF);
        cur_imm2 = (n == 2) ? (int)second : 0;

        /* The reported cycle is the (second) cycle of the instruction. */
        exec_cycle = cycle + n - 1;
        io[IO_CLKS] = exec_cycle;

        /* 3. Trace this instruction (registers shown are the pre-execute values). */
        write_trace(exec_cycle, pc, inst);

        /* The hwregtrace / leds / 7seg events use the same cycle number. */
        cycle = exec_cycle;            /* set 'cycle' so logging uses it       */

        /* 4. Execute. */
        {
            unsigned int next_pc = execute(inst, n);
            pc = next_pc;
        }

        /* 5. Advance devices for every cycle this instruction occupied. */
        cycle = exec_cycle - (n - 1);  /* restore to the first occupied cycle  */
        for (k = 0; k < (unsigned int)n; k++)
            device_tick(cycle + k);
        cycle = exec_cycle + 1;        /* next free cycle                      */
    }

    /* --- close streaming files --- */
    fclose(f_trace);
    fclose(f_hwtrace);
    fclose(f_leds);
    fclose(f_7seg);

    /* --- write the final-state output files --- */
    write_mem_trimmed(argv[4], mem, MEM_SIZE);   /* memout.txt   */
    write_regout(argv[5]);                       /* regout.txt   */
    {
        FILE* f = fopen(argv[8], "w");           /* cycles.txt   */
        if (f) { fprintf(f, "%08X\n", cycle); fclose(f); }
    }
    write_disk(argv[11]);                         /* diskout.txt  */
    write_monitor_txt(argv[12]);                  /* monitor.txt  */
    write_monitor_yuv(argv[13]);                  /* monitor.yuv  */

    free(irq2_list);
    return 0;
}
