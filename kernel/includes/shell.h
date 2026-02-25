#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

void shell_init();
void shell_print(uint32_t v);
void handle_backspace();

#endif