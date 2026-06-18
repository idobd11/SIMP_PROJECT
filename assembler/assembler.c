#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LINE 500
#define MAX_FIELD_LEN 50
#define MEM_SIZE 4096

/* ============================================
Structs and globals
============================================== */

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

    while (str[start] != '\0' && str[start] == ' ' || str[start] == '\t' || str[start] == '\n' || str[start] == '\r') {
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

 /* ============================================
 .word/.array 
 ============================================== */

 /* ============================================
 encoding functions 
 ============================================== */

 /* ============================================
 Main
 ============================================== */


int main(int argc, char* argv[])
{
    FILE* input;
    FILE* output;
    char line[MAX_LINE + 1];

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

    while (fgets(line, MAX_LINE + 1, input) != NULL){
        remove_comment(line);
        trim(line);

        if (line[0] == '\0') {
            continue;
        }
        if (parse_instruction_line(line, opcode, rd, rs, rt, imm1, imm2)) {
            opcode_num = get_opcode_number(opcode);
            rd_num = get_register_number(rd);
            rs_num = get_register_number(rs);
            rt_num = get_register_number(rt);

            printf("opcode = %s -> %d\n", opcode, opcode_num);
            printf("rd     = %s -> %d\n", rd, rd_num);
            printf("rs     = %s -> %d\n", rs, rs_num);
            printf("rt     = %s -> %d\n", rt, rt_num);
            printf("imm1   = %s\n", imm1);
            printf("imm2   = %s\n", imm2);
            printf("\n");
        }
        else {
            printf("Could not parse line: %s\n", line);
        }

    }
   
    fclose(input);
    fclose(output);

    return 0;
}