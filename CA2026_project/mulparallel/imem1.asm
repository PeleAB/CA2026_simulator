add $r2, $zero, $imm, 4			# i = 4
add $r9, $zero, $imm, 7			# r9 = 7 (i bound)
add $r14, $zero, $imm, 15		# r14 = 15 (j bound)

Loop_i:
mul $r6, $r2, $imm, 16			# r6 = i * 16
add $r3, $zero, $zero, 0		# j = 0

Loop_j:
add $r5, $zero, $zero, 0		# acc = 0
add $r4, $zero, $zero, 0		# k_base = 0 (processes k, k+1, k+2, k+3)

Loop_k4:
# Software pipelined: issue all loads first, then compute
# This hides memory latency on no-forwarding architectures

# === Issue loads for k, k+1, k+2, k+3 ===
lw $r11, $r6, $r4, 0			# A[i][k+0]
add $r10, $r4, $imm, 1			# compute k+1
lw $r15, $r6, $r10, 0			# A[i][k+1]
mul $r7, $r4, $imm, 16			# B_offset = k*16
add $r7, $r7, $r3, 0			# B_addr = k*16 + j
lw $r12, $r7, $imm, 256			# B[k+0][j]

add $r8, $r10, $imm, 1			# compute k+2
mul $r7, $r10, $imm, 16			# B_offset = (k+1)*16
add $r7, $r7, $r3, 0
lw $r13, $r7, $imm, 256			# B[k+1][j]

# === Compute k+0 while loading continues ===
mul $r12, $r11, $r12, 0			# A[i][k+0] * B[k+0][j]
lw $r11, $r6, $r8, 0			# A[i][k+2] (reuse r11)
add $r5, $r5, $r12, 0			# acc += product[k+0]

# === Compute k+1 ===
mul $r13, $r15, $r13, 0			# A[i][k+1] * B[k+1][j]
mul $r7, $r8, $imm, 16			# B_offset = (k+2)*16
add $r5, $r5, $r13, 0			# acc += product[k+1]

# === Load B[k+2] and B[k+3], compute k+2 ===
add $r7, $r7, $r3, 0
add $r10, $r8, $imm, 1			# k+3
lw $r12, $r7, $imm, 256			# B[k+2][j]
lw $r15, $r6, $r10, 0			# A[i][k+3]
mul $r7, $r10, $imm, 16			# B_offset = (k+3)*16
mul $r12, $r11, $r12, 0			# A[i][k+2] * B[k+2][j]
add $r7, $r7, $r3, 0
add $r5, $r5, $r12, 0			# acc += product[k+2]

# === Load B[k+3], compute k+3 ===
lw $r13, $r7, $imm, 256			# B[k+3][j]
add $r4, $r4, $imm, 4			# k_base += 4 (for next iteration)
mul $r13, $r15, $r13, 0			# A[i][k+3] * B[k+3][j]
add $r7, $zero, $imm, 16		# r7 = 16 (loop bound)
add $r5, $r5, $r13, 0			# acc += product[k+3]

# Loop: k_base goes 0, 4, 8, 12 (4 iterations total)
blt $imm, $r4, $r7, Loop_k4		# if k_base < 16 continue
add $zero, $zero, $zero, 0		# nop (delay slot)

# Store result
add $r8, $r6, $r3, 0			# addr = i*16 + j
sw $r5, $r8, $imm, 512			# C[i][j] = acc
# J-loop
blt $imm, $r3, $r14, Loop_j		# if j < 15 goto Loop_j
add $r3, $r3, $imm, 1			# j++ (delay slot)
# I-loop
blt $imm, $r2, $r9, Loop_i		# if i < 7 goto Loop_i
add $r2, $r2, $imm, 1			# i++ (delay slot)
halt $zero, $zero, $zero, 0