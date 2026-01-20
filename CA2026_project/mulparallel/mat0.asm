	add $r14, $zero, $imm, 15		# r14 = 15 (loop condition for j and k)
	add $r2, $zero, $zero, 0		# i = 0
	add $r9, $zero, $imm, 3			# r9 = 3 (loop condition for i, rows 0-3)
Loop_i:
	mul $r6, $r2, $imm, 16			# r6 = i * 16
	add $r3, $zero, $zero, 0		# j = 0
Loop_j:
	add $r4, $zero, $zero, 0		# k = 0
	add $r5, $zero, $zero, 0		# accumulator = 0
Loop_k:
	lw $r11, $r6, $r4, 0			# r11 = A[i][k] (address i*16 + k)
	mul $r7, $r4, $imm, 16			# r7 = k * 16
	add $r7, $r7, $r3, 0			# r7 = k*16 + j
	lw $r12, $r7, $imm, 256			# r12 = B[k][j] (address k*16 + j + 0x100)
	mul $r12, $r11, $r12, 0			# r12 = A[i][k] * B[k][j]
	add $r5, $r5, $r12, 0			# accumulator += product
	blt $imm, $r4, $r14, Loop_k		# if k < 15 goto Loop_k
	add $r4, $r4, $imm, 1			# k++ delay slot
	add $r8, $r6, $r3, 0			# r8 = i*16 + j
	sw $r5, $r8, $imm, 512			# C[i][j] = accumulator (address i*16 + j + 0x200)
	blt $imm, $r3, $r14, Loop_j		# if j < 15 goto Loop_j
	add $r3, $r3, $imm, 1			# j++ delay slot
	blt $imm, $r2, $r9, Loop_i		# if i < 3 goto Loop_i
	add $r2, $r2, $imm, 1			# i++ delay slot
	halt $zero, $zero, $zero, 0		# halt
