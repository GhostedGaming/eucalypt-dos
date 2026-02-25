global isr128_handler
extern syscall_handler

isr128_handler:
    push rcx
    push r11

    mov rcx, rdx    ; arg3 -> rcx (4th arg in sysv)
    mov rdx, rsi    ; arg2 -> rdx (3rd arg)
    mov rsi, rdi    ; arg1 -> rsi (2nd arg)
    mov rdi, rax    ; syscall number -> rdi (1st arg)

    call syscall_handler

    pop r11
    pop rcx
    iretq