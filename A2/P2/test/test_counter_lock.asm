count:
    word 0
lock:
    word -1
mainx20:
    ldimm r4, 10000
    ldimm r1, 1
    getpid r10
    jmp loop
loop:
    # lock
    ldimm r8, -1
    ldimm r9, -1
    cmpxchg r9, r10, lock
    beq r8, r9, add
    jmp loop
add:
    # add
    load r0, count
    addi r0, r1
    store r0, count
    subi r4, r1
    # unlock
    ldimm r9, -1
    store r9, lock
    # loop
    bgt r4, r5, loop
end:
    halt
export count
export mainx20
