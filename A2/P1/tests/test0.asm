export x1
export y1
export mainx20
jmp mainx20
x1:     alloc 3
y1:     word 5
        word 4
        word 3
        word 2
        word 1
        word 305419896
mainx20:load r1, y1
        ldaddr r10, y1
        ldind r2, 1(r10)
        addi r1, r2
        stind r1, 5(r10)
        call outside
        halt
import outside
