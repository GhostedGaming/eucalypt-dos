#include <stdint.h>

void other_function() {
    char msg[] = "Hello world!\n";
    
    register uint64_t syscall_num __asm__("rax") = 1;
    register uint64_t arg1 __asm__("rbx") = (uint64_t)msg;
    
    __asm__ volatile (
        "int $0x80"
        : "+r" (syscall_num)
        : "r" (arg1)
        : "memory"
    );
}

void _start() {
    other_function();
}