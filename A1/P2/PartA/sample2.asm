export x1
export y1
export x2
export y2
	jmp start
x1:	word 99
y1:	word 77
x2:	word 0
y2:	word 0
start:	load r0, x1
	store r0, y2
	load r1, y1
	addi r0, r1
	store r0, x2
y3:	call x3
	subi r0, r1
	store r0, y2
end:	halt
import x3


