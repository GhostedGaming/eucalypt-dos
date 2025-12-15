#include <x86_64/execute.h>
#include <x86_64/allocator/heap.h>
#include <stdint.h>
#include <stddef.h>

void load_and_execute_app(void *app_entry, size_t app_size) {
    uint8_t *stack = kmalloc(4096);
    uint64_t stack_top = ((uint64_t)stack + 4096) & ~0xFULL;

    __asm__ volatile (
        "mov %%rsp, %%r12\n\t"
        "mov %0, %%rsp\n\t"
        "jmp *%1\n\t"
        "mov %%r12, %%rsp\n\t"
        :
        : "r"(stack_top), "r"(app_entry)
        : "r12",
          "rax", "rcx", "rdx", "rsi", "rdi",
          "r8", "r9", "r10", "r11",
          "memory"
    );

    kfree(stack);
}
