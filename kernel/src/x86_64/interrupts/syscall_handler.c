#include <stdint.h>
#include <flanterm/flanterm.h>
#include <x86_64/allocator/heap.h>
#include <ramdisk/fat12.h>

extern struct flanterm_context *ft_ctx;

#define SYSCALL_NULL            0
#define SYSCALL_WRITE           1
#define SYSCALL_KMALLOC         2
#define SYSCALL_KFREE           3
#define SYSCALL_WRITE_FILE      4
#define SYSCALL_READ_FILE       5

int64_t syscall_handler(uint64_t syscall, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    switch (syscall) {
        case SYSCALL_WRITE:
            if (!arg1)
                return -1;
            flanterm_write(ft_ctx, (const char *)arg1);
            return 0;

        case SYSCALL_KMALLOC:
            if (arg1 == 0)
                return 0;
            return (int64_t)kmalloc(arg1);

        case SYSCALL_KFREE:
            if (!arg1)
                return -1;
            kfree((void *)arg1);
            return 0;

        case SYSCALL_WRITE_FILE:
            if (!arg1 || !arg2 || arg3 == 0)
                return -1;
            write_file((const char *)arg1, (uint8_t *)arg2);
            return 0;

        case SYSCALL_READ_FILE: {
            if (!arg1 || !arg2)
                return -1;

            dir_entry_t *file = find_file((const char *)arg1);
            if (!file)
                return -2;

            uint32_t size = 0;
            uint8_t *data = read_file(file, &size);
            if (!data || size == 0) {
                kfree(file);
                return -3;
            }

            *(uint32_t *)arg2 = size;
            kfree(file);
            return (int64_t)data;
        }

        default:
            return -1;
    }
}
