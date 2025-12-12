#include <x86_64/serial.h>

void serial_init() {
    outb(COM1_PORT + UART_INTR_EN, 0x00);
    
    outb(COM1_PORT + UART_LINE_CTRL, 0x80);
    
    outb(COM1_PORT + UART_DATA, 0x01);
    outb(COM1_PORT + UART_DATA + 1, 0x00);
    
    outb(COM1_PORT + UART_LINE_CTRL, 0x03);
    
    outb(COM1_PORT + UART_MODEM_CTRL, 0x03);
}

void serial_putchar(char c) {
    while (!(inb(COM1_PORT + UART_LINE_STATUS) & 0x20)) {
        asm volatile("pause");
    }
    outb(COM1_PORT + UART_DATA, (uint8_t)c);
}

void serial_print(const char *str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(*str++);
    }
}
