begin:ldimm r5, -5
	ldimm r4, 1
	ldimm r7, 0
	ldaddr r6, x
	ldimm r3, -1
	jmp continue
x:	alloc 5
	word 10
	word 11
	word 12
	word 13
	word 14
y:	alloc 1
continue:
	beq r5, r3, done
	ldind	r7, 5(r6)	
	addi r8, r7
	subi r5, r3
	jmp continue
done:	store r8, y
	halt
export y
export begin