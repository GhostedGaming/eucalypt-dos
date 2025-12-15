#include <stdint.h>
#include <flanterm/flanterm.h>
#include <x86_64/serial.h>
#include <x86_64/allocator/heap.h>

extern struct flanterm_context *ft_ctx;

#define SYSCALL_NULL 0
#define SYSCALL_WRITE 1
#define SYSCALL_KMALLOC 2
#define SYSCALL_KFREE 3

void *global_ptr = 0;

int64_t syscall_handler(uint8_t syscall, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    switch (syscall) {
        case SYSCALL_NULL:
            return 0;

        case SYSCALL_WRITE:
            serial_print("Sys Write\n");
            flanterm_write(ft_ctx, (const char *)arg1);
            return 0;

        case SYSCALL_KMALLOC: {
            void *ptr = kmalloc(arg1);
            serial_print("Sys Kmalloc\n");
            return (int64_t)ptr;
        }

        case SYSCALL_KFREE:
            serial_print("Sys Kfree\n");
            kfree((void *)arg1);
            return 0;

        default:
            return -1;
    }
}
