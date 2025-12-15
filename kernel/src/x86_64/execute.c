#include <x86_64/execute.h>
#include <x86_64/allocator/heap.h>
#include <stdint.h>
#include <stddef.h>

void load_and_execute_app(void *app_binary, size_t app_size) {
    uint8_t *app_stack = kmalloc(4096);
    uint64_t stack_top = (uint64_t)(app_stack + 4096);
    
    __asm__ volatile (
        "mov %%rsp, %%rbx\n\t"
        "mov %1, %%rsp\n\t"
        "call *%0\n\t"
        "mov %%rbx, %%rsp"
        :
        : "r" ((uint64_t)app_binary), "r" (stack_top)
        : "rbx", "memory"
    );
    
    kfree(app_stack);
}