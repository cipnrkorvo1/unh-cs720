#
# this is the code for the second file
# get42 just returns 43
# note that the routine uses r7
#
	export	get43
	
get43:
	ldimm	r7,43
	stind	r7,-1(fp)
	ret
	