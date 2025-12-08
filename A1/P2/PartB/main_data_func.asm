#
# 
#
mainx20:
	
	addi	fp,sp		#establish a fp (ie by copying the sp)
	
	ldimm	r0,4		#allocate a local for the return value
	subi	sp,r0	
	
	call	get42		#first call: store return value in r0
	ldind	r0,-1(fp)
	
	call	get42		#second call: add return value to r0
	ldind	r1,-1(fp)
	addi	r0,r1
	
	store	r0,result	#store sum into result
	
	halt				#halt
	export	result		#variable to store result	
	export mainx20
result:
	word	0
	word	1
	word	2
	word	3
	word	4
	word	5

export	get42
	
get42:
	ldimm	r7,42
	stind	r7,-1(fp)
	ret
	