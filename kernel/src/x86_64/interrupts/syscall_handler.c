#include <stdint.h>
#include <flanterm/flanterm.h>
#include <ramdisk/fat12.h>
#include <x86_64/allocator/heap.h>

extern struct flanterm_context *ft_ctx;

// Definitions for available system calls
typedef enum {
    SYSCALL_NULL        = 0,
    SYSCALL_WRITE       = 1,
    SYSCALL_WRITE_FILE  = 2,
    SYSCALL_READ_FILE   = 3,
    SYSCALL_OPEN        = 4,
    SYSCALL_CLOSE       = 5,
    SYSCALL_READ        = 6,
    SYSCALL_SEEK        = 7,
    SYSCALL_TELL        = 8,
} syscall_t;

// Standardized return codes
typedef enum {
    SYSCALL_OK            =  0,
    SYSCALL_ERR_INVALID   = -1,
    SYSCALL_ERR_NOT_FOUND = -2,
    SYSCALL_ERR_READ_FAIL = -3,
    SYSCALL_ERR_UNKNOWN   = -4,
} syscall_err_t;

// Writes a string to the terminal
static int64_t syscall_write(uint64_t buf) {
    if (!buf) return SYSCALL_ERR_INVALID;
    flanterm_write(ft_ctx, (const char *)buf);
    return SYSCALL_OK;
}

// Writes data to a file on the ramdisk
static int64_t syscall_write_file(uint64_t path, uint64_t data, uint64_t size) {
    if (!path || !data || size == 0) return SYSCALL_ERR_INVALID;
    write_file((const char *)path, (uint8_t *)data, (uint32_t)size);
    return SYSCALL_OK;
}

// Reads an entire file into memory and returns the pointer
static int64_t syscall_read_file(uint64_t path, uint64_t size_out) {
    if (!path || !size_out) return SYSCALL_ERR_INVALID;

    dir_entry_t *file = find_file((const char *)path);
    if (!file) return SYSCALL_ERR_NOT_FOUND;

    uint32_t size = 0;
    uint8_t *data = read_file(file, &size);
    kfree(file); // Cleanup temporary entry structure

    if (!data || size == 0) return SYSCALL_ERR_READ_FAIL;

    *(uint32_t *)size_out = size; // Return the read size via pointer
    return (int64_t)data;         // Return data pointer to user
}

// Open a file and return a File Descriptor
static int64_t syscall_open(uint64_t path) {
    if (!path) return SYSCALL_ERR_INVALID;
    return open_file((const char *)path);
}

// Close an open File Descriptor
static int64_t syscall_close(uint64_t fd) {
    close_file((int32_t)fd);
    return SYSCALL_OK;
}

// Read data from an open File Descriptor
static int64_t syscall_read(uint64_t fd, uint64_t buf, uint64_t len) {
    if (!buf || len == 0) return SYSCALL_ERR_INVALID;
    return read_file_fd((int32_t)fd, (uint8_t *)buf, (uint32_t)len);
}

// Reposition the read/write offset of a file
static int64_t syscall_seek(uint64_t fd, uint64_t position) {
    return seek_file((int32_t)fd, (uint32_t)position);
}

// Get current position in a file
static int64_t syscall_tell(uint64_t fd) {
    return (int64_t)tell_file((int32_t)fd);
}

// Generic entry point for all system calls dispatched from Assembly
int64_t syscall_handler(uint64_t syscall, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    switch ((syscall_t)syscall) {
        case SYSCALL_WRITE:      return syscall_write(arg1);
        case SYSCALL_WRITE_FILE: return syscall_write_file(arg1, arg2, arg3);
        case SYSCALL_READ_FILE:  return syscall_read_file(arg1, arg2);
        case SYSCALL_OPEN:       return syscall_open(arg1);
        case SYSCALL_CLOSE:      return syscall_close(arg1);
        case SYSCALL_READ:       return syscall_read(arg1, arg2, arg3);
        case SYSCALL_SEEK:       return syscall_seek(arg1, arg2);
        case SYSCALL_TELL:       return syscall_tell(arg1);
        default:                 return SYSCALL_ERR_UNKNOWN;
    }
}