export mainx20
t0: word -1
t1: word -1
t2: word -1
t3: word -1
t4: word -1
t5: word -1
t6: word -1
t7: word -1
mainx20:
    getpid r5
    ldaddr r0, t0
    addi r0, r5
    stind r5, 0(r0)
    halt
export t0
export t1
export t2
export t3
export t4
export t5
export t6
export t7
