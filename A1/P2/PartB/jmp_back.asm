export f
export b
jmp f
x:  alloc 3
b:  addi r1, r2
    addi r3, r4
    call aaa
    ret
y:  word 0
    word 1
    word 2
f:  subi r5, r6
    subi r7, r8
    call b
    halt
z:  alloc 2
    word 7
aaa:halt
