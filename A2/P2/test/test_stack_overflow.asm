export mainx20
export initialSP
export lastSP
initialSP:
    word 0
lastSP:
    word 0
mainx20:
    store r14, initialSP
    call function
    halt
function:
    store r14, lastSP
    call function
