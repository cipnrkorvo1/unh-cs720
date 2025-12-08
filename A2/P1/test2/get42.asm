#
# this is the code for the second file
# get42 just returns 42
# note that the routine uses r7
#
	export	get42
	
get42:
	ldimm	r7,42
	stind	r7,-1(fp)
	ret
	