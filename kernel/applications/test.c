#include <stdint.h>

void other_function(void) {
    char msg[] = "Hello world!\n";

    __asm__ volatile (
        "mov $1, %%rax\n\t"
        "mov %0, %%rbx\n\t"
        "int $0x80"
        :
        : "r"(msg)
        : "rax", "rbx", "rcx", "rdx",
          "rsi", "rdi",
          "r8", "r9", "r10", "r11",
          "cc", "memory"
    );
}

void _start(void) {
    other_function();
}
