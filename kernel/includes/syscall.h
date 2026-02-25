#ifndef SYSCALL_H
#define SYSCALL_H

#define SYSCALL_NULL        0
#define SYSCALL_WRITE       1
#define SYSCALL_WRITE_FILE  2
#define SYSCALL_READ_FILE   3
#define SYSCALL_OPEN        4
#define SYSCALL_CLOSE       5
#define SYSCALL_READ        6
#define SYSCALL_SEEK        7
#define SYSCALL_TELL        8

#define SYSCALL_OK            0
#define SYSCALL_ERR_INVALID  -1
#define SYSCALL_ERR_NOT_FOUND -2
#define SYSCALL_ERR_READ_FAIL -3
#define SYSCALL_ERR_UNKNOWN  -4

#endif