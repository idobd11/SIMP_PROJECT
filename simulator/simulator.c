#include <stdio.h>

int main(int argc, char* argv[])
{
    if (argc != 14) {
        printf("Usage: sim.exe memin.txt diskin.txt irq2in.txt memout.txt regout.txt trace.txt hwregtrace.txt cycles.txt leds.txt display7seg.txt diskout.txt monitor.txt monitor.yuv\n");
        return 1;
    }

    printf("Simulator skeleton\n");

    return 0;
}