#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>



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
 SOpcode/register conversion
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
    char line[501];

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

    while (fgets(line, 501, input) != NULL){
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