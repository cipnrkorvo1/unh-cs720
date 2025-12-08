export x1
export x2
export cool
x1:	word 0
x2:	word 0
	jmp start
start:	jmp start
	jmp start
	jmp out
	call cool
cool:	call cool
	call cool
	call out
	halt
import out
