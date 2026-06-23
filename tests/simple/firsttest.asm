# simple test file for assembler

start:
   add $t0, $zero, $imm1, 5, 0
add $t1, $zero, $imm1, 7, 0

middle: add $t2, $t0, $t1, 0, 0
beq $zero, $zero, $zero, middle, 0
halt $zero, $zero, $zero, 0, 0
beq $zero, $zero, $zero, start, 0
bigconst: add $t0, $zero, $imm2, 0, 0x12345678
.word 8 0x11111111
