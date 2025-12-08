#
# this is the main program to be linked with another file
# this file will call get42 once, get43 once and add the two return values
# together. other files will supply get42 and get43 functions. there is also
# a file that contains only data
#
	import	get43		# need to import external functions
	import	get42
	import x1
	
mainx20:addi	fp,sp		#establish a fp (ie by copying the sp)
	
	ldimm	r0,4		#allocate a local for the return value
	subi	sp,r0	
	
	call	get42		#first call: store return value in r0
	ldind	r0,-1(fp)
	
	call	get43		#second call: add return value to r0
	ldind	r1,-1(fp)
	addi	r0,r1
	
	store	r0,x1		#store sum into x1
	store	r0,result		#store sum into result
	
	halt			#halt
	export	result		#variable to store result	
result:
	word	0
export mainx20

	
