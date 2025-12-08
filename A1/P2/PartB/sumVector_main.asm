export sum
export top
export done
export mainx20
mainx20:jmp skipData
sum:
	word 0
len:
	word 5
vector:
	word 1
	word 2
	word 3
	word 4
	word 5
skipData:
	ldimm	r0, 0		# r0 is the loop index
	load	r1, len		# r1 is the upperbound for the loop
	ldaddr	r2, vector	# r2 is a pointer to an vector element
        ldimm   r3, 0		# r3 is the running sum
        ldimm	r4, 1		# r4 always contains 1, the loop increment
top:
	beq	r0, r1, done	# loop exit condition
	ldind	r5, 0(r2)	# fetch vector[i]
        addi	r3, r5		# add it to the sum
        addi	r2, r4		# increment pointer
        addi	r0, r4		# increment loop index
	jmp	top
done:
	store	r3, sum
	halt

