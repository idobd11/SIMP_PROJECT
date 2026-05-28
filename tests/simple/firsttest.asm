# simple test file for assembler

add $t0, $zero, $imm1, 5, 0 # put 5 in t0
add $t1, $zero, $imm1, 7, 0 # put 7 in t1
add $t2, $t0, $t1, 0, 0 # t2 = t0 + t1
halt $zero, $zero, $zero, 0, 0