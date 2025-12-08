export x1
export y1
export mainx20
jmp mainx20
x1: word 63
y1: word 127
    word 255
mainx20:
    addi r0, r1
    load r2, y1
    addi r2, r3
    load r4, x1
    addi r4, r5
    call two
    halt
z1: word 511
two:
    subi r6, r7
    load r8, z1
    subi r8, r9
    ret
