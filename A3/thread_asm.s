.global asm_yield
.align 8

# INCOMING STRUCT
#    // callee saved registers
#   int64_t rbx;
#   int64_t r12;
#   int64_t r13;
#   int64_t r14;
#   int64_t r15;

#   int64_t rbp;    // frame pointer
#   int64_t rsp;    // stack pointer
#   int64_t rip;    // program counter

#   int64_t rdi;    // first argument in function call
#   int64_t rsi;    // second argument in function call

# %rdi = FROM*, %rsi = TO*, qwords
asm_yield:
    # prologue
    push    %rbp
    mov     %rsp, %rbp
    # save current registers into FROM
    mov     %rbx, (%rdi)
    mov     %r12, 8(%rdi)
    mov     %r13, 16(%rdi)
    mov     %r14, 24(%rdi)
    mov     %r15, 32(%rdi)

    mov     %rbp, 40(%rdi)
    mov     %rsp, 48(%rdi)
    
    mov     %rdi, 64(%rdi)
    mov     %rsi, 72(%rdi)

    # load TO into current registers
    mov     (%rsi), %rbx
    mov     8(%rsi), %r12
    mov     16(%rsi), %r13
    mov     24(%rsi), %r14
    mov     32(%rsi), %r15

    mov     40(%rsi), %rbp
    mov     48(%rsi), %rsp

    mov     64(%rsi), %rdi
    mov     72(%rsi), %rsi
    # epilogue
    mov     %rbp, %rsp
    pop     %rbp
    ret
