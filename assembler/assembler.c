#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LINE 500
#define MAX_FIELD_LEN 50
#define MEM_SIZE 4096
#define MAX_LABELS 500


/* ============================================
Structs and globals
============================================== */

typedef struct {
    char name[MAX_FIELD_LEN];
    int address;
} Label;

/* ============================================
 Utility functions
============================================== */

void remove_comment(char line[])
{
    char* comment_start;

    comment_start = strchr(line, '#');

    if (comment_start != NULL) {
        *comment_start = '\0';
    }
}

void trim(char str[])
{
    int start = 0;
    int end;
    int i = 0;

    while (str[start] != '\0' &&
        (str[start] == ' ' || str[start] == '\t' || str[start] == '\n' || str[start] == '\r')) {
        start++;
    }

    end = (int)strlen(str) - 1;

    while (end >= start &&
        (str[end] == ' ' || str[end] == '\t' || str[end] == '\n' || str[end] == '\r')) {
        end--;
    }

    while (start <= end) {
        str[i] = str[start];
        i++;
        start++;
    }

    str[i] = '\0';
}

int parse_number(const char* str, long* value)
{
    char* endptr;
    long result;

    result = strtol(str, &endptr, 0);

    if (endptr == str) {
        return 0; /* no number was read */
    }

    if (*endptr != '\0') {
        return 0; /* extra invalid characters */
    }

    *value = result;
    return 1;
}
/* ============================================
 Parser function
 ============================================== */

int parse_instruction_line(char line[],
    char opcode[],
    char rd[],
    char rs[],
    char rt[],
    char imm1[],
    char imm2[])
{
    char* parts[5];
    char* token;
    int count = 0;

    token = strtok(line, ",");

    while (token != NULL && count < 5) {
        trim(token);
        parts[count] = token;
        count++;

        token = strtok(NULL, ",");
    }

    if (count != 5) {
        return 0;
    }

    if (sscanf(parts[0], "%s %s", opcode, rd) != 2) {
        return 0;
    }

    strcpy(rs, parts[1]);
    strcpy(rt, parts[2]);
    strcpy(imm1, parts[3]);
    strcpy(imm2, parts[4]);

    return 1;
}


 /* ============================================
 Opcode/register conversion
 ============================================== */
#define NUM_REGISTERS 16
#define NUM_OPCODES 23

const char* registers[NUM_REGISTERS] = {
    "$zero", "$imm1", "$imm2", "$v0", "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$s0", "$s1", "$gp", "$sp", "$ra"
};

const char* opcodes[NUM_OPCODES] = {
    "add", "sub", "mul", "mac", "and", "or", "xor", "sll",
    "sra", "srl", "beq", "bne", "blt", "bgt", "ble", "bge",
    "jal", "lw", "sw", "reti", "in", "out", "halt"
};

int get_register_number(const char* reg_name)
{
    int i;

    for (i = 0; i < NUM_REGISTERS; i++) {
        if (strcmp(reg_name, registers[i]) == 0) {
            return i;
        }
    }

    return -1;
}

int get_opcode_number(const char* op_name)
{
    int i;

    for (i = 0; i < NUM_OPCODES; i++) {
        if (strcmp(op_name, opcodes[i]) == 0) {
            return i;
        }
    }

    return -1;
}
 /* ============================================
 Labels / Symbols
 ============================================== */

int is_label_definition(char line[])
{
    char* colon;

    colon = strchr(line, ':');

    if (colon != NULL) {
        return 1;
    }

    return 0;
}

void extract_label_name(char line[], char label_name[])
{
    char* colon;
    int len;

    colon = strchr(line, ':');

    if (colon == NULL) {
        label_name[0] = '\0';
        return;
    }

    len = (int)(colon - line);

    strncpy(label_name, line, len);
    label_name[len] = '\0';

    trim(label_name);
}

void remove_label_from_line(char line[])
{
    char* colon;
    char temp[MAX_LINE + 1];

    colon = strchr(line, ':');

    if (colon == NULL) {
        return;
    }

    strcpy(temp, colon + 1);
    trim(temp);
    strcpy(line, temp);
}

int find_label(Label labels[], int label_count, const char* name)
{
    int i;

    for (i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

int add_label(Label labels[], int* label_count, const char* name, int address)
{
    if (*label_count >= MAX_LABELS) {
        return 0;
    }

    if (find_label(labels, *label_count, name) != -1) {
        return 0;
    }

    strcpy(labels[*label_count].name, name);
    labels[*label_count].address = address;
    (*label_count)++;

    return 1;
}

int resolve_immediate(const char* imm, Label labels[], int label_count, long* value)
{
    int label_index;

    if (parse_number(imm, value)) {
        return 1;
    }

    label_index = find_label(labels, label_count, imm);

    if (label_index != -1) {
        *value = labels[label_index].address;
        return 1;
    }

    return 0;
}

int first_pass(FILE* input, Label labels[], int* label_count)
{
    char line[MAX_LINE + 1];
    char label_name[MAX_FIELD_LEN];

    char opcode[MAX_FIELD_LEN];
    char rd[MAX_FIELD_LEN];
    char rs[MAX_FIELD_LEN];
    char rt[MAX_FIELD_LEN];
    char imm1[MAX_FIELD_LEN];
    char imm2[MAX_FIELD_LEN];

    int pc;
    int rd_num;
    int rs_num;
    int rt_num;

    pc = 0;

    while (fgets(line, MAX_LINE + 1, input) != NULL) {
        remove_comment(line);
        trim(line);

        if (line[0] == '\0') {
            continue;
        }

        if (is_label_definition(line)) {
            extract_label_name(line, label_name);

            if (label_name[0] != '\0') {
                if (!add_label(labels, label_count, label_name, pc)) {
                    printf("Error: duplicate label or too many labels: %s\n", label_name);
                    return 0;
                }
            }

            remove_label_from_line(line);

            if (line[0] == '\0') {
                continue;
            }
        }

        /* Directives will be handled later */
        if (line[0] == '.') {
            continue;
        }

        if (!parse_instruction_line(line, opcode, rd, rs, rt, imm1, imm2)) {
            printf("Pass 1 parse error: %s\n", line);
            return 0;
        }

        rd_num = get_register_number(rd);
        rs_num = get_register_number(rs);
        rt_num = get_register_number(rt);

        if (rd_num == -1 || rs_num == -1 || rt_num == -1) {
            printf("Pass 1 register error\n");
            return 0;
        }

        pc++;

        if (rd_num == 2 || rs_num == 2 || rt_num == 2) {
            pc++;
        }
    }

    return 1;
}

 /* ============================================
 .word/.array 
 ============================================== */

 /* ============================================
 encoding functions 
 ============================================== */

unsigned int encode_instruction(int opcode, int rd, int rs, int rt, long imm1)
{
    unsigned int inst = 0;

    inst |= ((unsigned int)opcode & 0xFF) << 24;
    inst |= ((unsigned int)rd & 0xF) << 20;
    inst |= ((unsigned int)rs & 0xF) << 16;
    inst |= ((unsigned int)rt & 0xF) << 12;
    inst |= ((unsigned int)imm1 & 0xFFF);

    return inst;
}

 /* ============================================
 Main
 ============================================== */


int main(int argc, char* argv[])
{
    FILE* input;
    FILE* output;
    char line[MAX_LINE + 1];

    unsigned int memory[MEM_SIZE] = { 0 };
    int pc = 0;
    int i;

    Label labels[MAX_LABELS];
    int label_count = 0;

    char opcode[MAX_FIELD_LEN] = "";
    char rd[MAX_FIELD_LEN] = "";
    char rs[MAX_FIELD_LEN] = "";
    char rt[MAX_FIELD_LEN] = "";
    char imm1[MAX_FIELD_LEN] = "";
    char imm2[MAX_FIELD_LEN] = "";

    int opcode_num;
    int rd_num;
    int rs_num;
    int rt_num;

    long imm1_num;
    long imm2_num;

    unsigned int encoded_instruction;

    if (argc != 3) {
        printf("Usage: asm.exe program.asm memin.txt\n");
        return 1;
    }

    input = fopen(argv[1], "r");
    if (input == NULL) {
        printf("Error: cannot open input file\n");
        return 1;
    }

    output = fopen(argv[2], "w");
    if (output == NULL) {
        printf("Error: cannot open output file\n");
        fclose(input);
        return 1;
    }

    printf("Successfully opened all files! Ready to parse...\n");

    if (!first_pass(input, labels, &label_count)) {
        fclose(input);
        fclose(output);
        return 1;
    }

    printf("Labels found:\n");
    for (i = 0; i < label_count; i++) {
        printf("%s -> %d\n", labels[i].name, labels[i].address);
    }

    rewind(input);
    pc = 0;

    while (fgets(line, MAX_LINE + 1, input) != NULL){
        remove_comment(line);
        trim(line);

        if (line[0] == '\0') {
            continue;
        }
        if (is_label_definition(line)) {
            remove_label_from_line(line);

            if (line[0] == '\0') {
                continue;
            }
        }

        if (parse_instruction_line(line, opcode, rd, rs, rt, imm1, imm2)) {
            opcode_num = get_opcode_number(opcode);
            rd_num = get_register_number(rd);
            rs_num = get_register_number(rs);
            rt_num = get_register_number(rt);

            if (!resolve_immediate(imm1, labels, label_count, &imm1_num)) {
                printf("Invalid imm1 or unknown label: %s\n", imm1);
                continue;
            }

            if (!resolve_immediate(imm2, labels, label_count, &imm2_num)) {
                printf("Invalid imm2 or unknown label: %s\n", imm2);
                continue;
            }

            encoded_instruction = encode_instruction(opcode_num, rd_num, rs_num, rt_num, imm1_num);

            memory[pc] = encoded_instruction;
            pc++;

            if (rd_num == 2 || rs_num == 2 || rt_num == 2) {
                memory[pc] = (unsigned int)imm2_num;
                pc++;
            }

            printf("opcode = %s -> %d\n", opcode, opcode_num);
            printf("rd     = %s -> %d\n", rd, rd_num);
            printf("rs     = %s -> %d\n", rs, rs_num);
            printf("rt     = %s -> %d\n", rt, rt_num);
            printf("imm1   = %s -> %ld\n", imm1, imm1_num);
            printf("imm2   = %s -> %ld\n", imm2, imm2_num);
            printf("encoded = %08X\n", encoded_instruction);
            printf("\n");
        }
        else {
            printf("Could not parse line: %s\n", line);
        }

    }
   
    for (i = 0; i < pc; i++) {
        fprintf(output, "%08X\n", memory[i]);
    }
    fclose(input);
    fclose(output);

    return 0;
}