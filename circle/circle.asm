lw $t0, $imm1, $zero, $zero, 0x100, 0   # Load center offset from address 0x100 into $t0
lw $t1, $imm1, $zero, $zero, 0x101, 0   # Load radius from address 0x101 into $t1
srl $s0, $t0, $imm1, $zero, 8, 0         # Compute Yc = offset / 256, store in $s0
and $s1, $t0, $imm1, $zero, 255, 0       # Compute Xc = offset % 256, store in $s1
mul $s2, $t1, $t1, $zero, 0, 0           # Compute R^2 = radius * radius, store in $s2
sub $s3, $s0, $t1, $zero, 0, 0           # Initialize loop variable Y = Yc - R into $s3
add $s4, $s0, $t1, $zero, 0, 0           # Compute upper loop bound Y_end = Yc + R into $s4

L_Y_LOOP:
blt $zero, $s4, $s3, L_END, 0            # If Y_end < Y (out of bounds) -> exit program
sub $t2, $s3, $s0, $zero, 0, 0           # Compute vertical distance dy = Y - Yc into $t2
mul $s5, $t2, $t2, $zero, 0, 0           # Compute dy^2 = dy * dy, store in $s5
sub $s6, $s1, $t1, $zero, 0, 0           # Initialize inner loop variable X = Xc - R into $s6
add $s7, $s1, $t1, $zero, 0, 0           # Compute right loop bound X_end = Xc + R into $s7

L_X_LOOP:
blt $zero, $s7, $s6, L_NEXT_Y, 0         # If X_end < X (end of row) -> go to next Y line
sub $t2, $s6, $s1, $zero, 0, 0           # Compute horizontal distance dx = X - Xc into $t2
mul $t2, $t2, $t2, $zero, 0, 0           # Compute dx^2 = dx * dx, store in $t2
add $t3, $t2, $s5, $zero, 0, 0           # Compute total squared distance: t3 = dx^2 + dy^2
blt $zero, $s2, $t3, L_NEXT_X, 0         # If R^2 < (dx^2 + dy^2) -> skip drawing this pixel

sll $t2, $s3, $imm1, $zero, 8, 0         # Calculate Y * 256, store in $t2
add $t2, $t2, $s6, $zero, 0, 0           # Compute final flat address: addr = (Y * 256) + X
out $t2, $imm1, $zero, $zero, 11, 0      # Write pixel address to IO register monitoraddr (11)
out $imm1, $zero, $zero, $zero, 255, 12  # Write color value white (255) to monitordata (12)
out $imm1, $zero, $zero, $zero, 1, 13    # Send command 1 to monitorcmd (13) to draw pixel

L_NEXT_X:
add $s6, $s6, $imm1, $zero, 1, 0         # Increment horizontal loop variable: X = X + 1
beq $zero, $zero, $zero, L_X_LOOP, 0     # Jump back to start of X loop

L_NEXT_Y:
add $s3, $s3, $imm1, $zero, 1, 0         # Increment vertical loop variable: Y = Y + 1
beq $zero, $zero, $zero, L_Y_LOOP, 0     # Jump back to start of Y loop

L_END:
halt $zero, $zero, $zero, 0, 0           # Halt execution and stop simulator