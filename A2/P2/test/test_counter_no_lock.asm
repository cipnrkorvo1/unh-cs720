count:
    word 0
mainx20:
    ldimm r4, 10000
    ldimm r1, 1
    jmp loop
loop:
    load r0, count
    addi r0, r1
    store r0, count
    subi r4, r1
    bgt r4, r5, loop
end:
    halt
export count
export mainx20
