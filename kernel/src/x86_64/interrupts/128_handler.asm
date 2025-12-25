global isr128_handler
extern syscall_handler

isr128_handler:
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rdx
    push rcx
    push rax

    mov rdi, rax
    mov rsi, rbx
    mov rdx, rcx
    mov rcx, rdx
    mov r8, rsi
    mov r9, rdi

    call syscall_handler

    pop rax
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11

    iretq