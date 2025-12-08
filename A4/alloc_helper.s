.section .data
.global GC_registers



.section .text
.global GC_getRegisters

GC_getRegisters:

    // move addr of array to rax
    mov     %rdi, %rax

    // store each register in this array
    // get previous stack pointers
    pop     %rcx
    mov     %rsp, 0(%rax)
    mov     %rbp, 8(%rax)
    mov     %rbx, 16(%rax)
    mov     %rsi, 24(%rax)
    mov     %rdi, 32(%rax)
    // restore stack pointers
    push    %rcx
    ret
