#include <stdio.h>

int main(int argc, char* argv[])
{
    if (argc != 3) {
        printf("Usage: asm.exe program.asm memin.txt\n");
        return 1;
    }

    printf("Assembler skeleton\n");
    printf("Input asm file: %s\n", argv[1]);
    printf("Output memin file: %s\n", argv[2]);

    return 0;
}