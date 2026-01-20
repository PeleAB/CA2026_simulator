	add $r14, $zero, $imm, 15		# r14 = 15 (Loop condition)
	add $r2, $zero, $zero, 0		# Loop_i: i = 0
	Loop_i:
	mul $r6, $r2, $imm, 16			# R6 = i * 16
	add $r3, $zero, $zero, 0		# Loop_j: j = 0
	Loop_j:
	add $r5, $zero, $zero, 0		# Loop_k: accumulator = 0
	add $r4, $zero, $zero, 0		# k = 0
	Loop_k:
	add $r8, $r6, $r4, 0			# R8 = i*16 + k (address A[i][k])
	lw $r11, $r8, $zero, 0			# R11 = A[i][k]
	mul $r7, $r4, $imm, 16			# R7 = k * 16
	add $r7, $r7, $r3, 0			# R7 = k*16 + j
	lw $r12, $r7, $imm, 256			# R12 = B[k][j] (at address i*16 + j + 0x100)
	mul $r12, $r11, $r12, 0			# R12 = A[i][k] * B[k][j]
	add $r5, $r5, $r12, 0			# accumulator += product
	blt $imm, $r4, $r14, Loop_k		# if k < 15 goto Loop_k
	add $r4, $r4, $imm, 1			# k++ delay slot
	add $r8, $r6, $r3, 0			# R8 = i*16 + j
	sw  $r5, $r8, $imm, 512			# C[i][j] = accumulator (address C[i][j] i*16 + j + 0x200)
	blt $imm, $r3, $r14, Loop_j		# if j < 15 goto Loop_j
	add $r3, $r3, $imm, 1			# j++ delay slot
	blt $imm, $r2, $r14, Loop_i		# if i < 15 goto Loop_i
	add $r2, $r2, $imm, 1			# i++ delay slot
	halt $zero, $zero, $zero, 0		# halt