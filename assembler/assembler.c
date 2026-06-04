#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LINE 500
#define MAX_FIELD_LEN 50
#define MENM_SIZE 4096

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

    while (str[start] == ' ' || str[start] == '\t' || str[start] == '\n' || str[start] == '\r') {
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
        printf("%s\n", line);

    }

  
   
    fclose(input);
    fclose(output);

    return 0;
}