#include <x86_64/interrupts/keyboard.h>
#include <x86_64/commands.h>
#include <flanterm/flanterm.h>
#include <stdbool.h>
#include <stdint.h>

extern struct flanterm_context *ft_ctx;

// PS/2 Controller IO Ports and status flags
#define KB_DATA_PORT          0x60
#define KB_STATUS_PORT        0x64
#define KB_CMD_PORT           0x64
#define KB_STATUS_OUTPUT_FULL 0x01 // Bit 0: Output buffer has data for CPU
#define KB_STATUS_INPUT_FULL  0x02 // Bit 1: Input buffer is full (CPU shouldn't write)

// Special key definitions
#define UNKNOWN 0xFFFFFFFF
#define ESC     (0xFFFFFFFF - 1)
#define CTRL    (0xFFFFFFFF - 2)
#define LSHFT   (0xFFFFFFFF - 3)
#define RSHFT   (0xFFFFFFFF - 4)
#define ALT     (0xFFFFFFFF - 5)
#define CAPS    (0xFFFFFFFF - 29)
#define NONE    (0xFFFFFFFF - 30)

// Keyboard state tracking
static bool shift_held = false;
static bool caps_lock  = false;

// Scan code set 1: Lowercase/Standard mapping
static const uint32_t lowercase[128] = { /* ... mapping ... */ };

// Scan code set 1: Uppercase/Shift mapping
static const uint32_t uppercase[128] = { /* ... mapping ... */ };

// Wait until the PS/2 controller is ready to receive a command
static void kb_wait_write(void) {
    int timeout = 100000;
    while ((inb(KB_STATUS_PORT) & KB_STATUS_INPUT_FULL) && --timeout > 0);
}

// Wait until the PS/2 controller has data ready to be read
static void kb_wait_read(void) {
    int timeout = 100000;
    while (!(inb(KB_STATUS_PORT) & KB_STATUS_OUTPUT_FULL) && --timeout > 0);
}

// Clear any pending data in the output buffer
static void kb_flush(void) {
    while (inb(KB_STATUS_PORT) & KB_STATUS_OUTPUT_FULL)
        inb(KB_DATA_PORT);
}

// Send a command byte to the keyboard and wait for Acknowledgement (0xFA)
static bool kb_send(uint8_t cmd) {
    for (int attempt = 0; attempt < 10; attempt++) {
        kb_wait_write();
        outb(KB_DATA_PORT, cmd);

        kb_wait_read();
        uint8_t resp = inb(KB_DATA_PORT);

        if (resp == 0xFA) return true;
        if (resp == 0xFE) continue; // Resend on failure
    }
    return false;
}

// Perform hardware initialization of the PS/2 keyboard
void init_keyboard(void) {
    kb_flush();

    // Disable keyboard interface during config
    kb_wait_write();
    outb(KB_CMD_PORT, 0xAD);

    kb_flush();

    // Re-enable keyboard interface
    kb_wait_write();
    outb(KB_CMD_PORT, 0xAE);

    kb_flush();

    // Reset keyboard and run Basic Assurance Test (BAT)
    kb_send(0xFF);
    kb_wait_read();
    inb(KB_DATA_PORT); // Expect 0xAA (Passed)

    kb_flush();

    // Tell keyboard to start sending scan codes
    kb_send(0xF4);

    kb_flush();
}

// Main interrupt handler called when a key is pressed or released
void keyboard_handler(void) {
    // Ensure data is actually available
    if (!(inb(KB_STATUS_PORT) & KB_STATUS_OUTPUT_FULL))
        return;

    uint8_t raw      = inb(KB_DATA_PORT);
    bool    pressed  = (raw & 0x80) == 0; // Bit 7 set means key released
    uint8_t scancode = raw & 0x7F;

    if (scancode >= 128) return;

    switch (scancode) {
        case 42: // Left Shift
        case 54: // Right Shift
            shift_held = pressed;
            return;

        case 58: // Caps Lock
            if (pressed) caps_lock = !caps_lock;
            return;

        default:
            if (!pressed) return; // Ignore key releases for standard characters

            // Determine if character should be uppercase
            bool upper = shift_held ^ caps_lock;
            uint32_t val = upper ? uppercase[scancode] : lowercase[scancode];

            // Filter out non-printable/control keys
            if (val == UNKNOWN || val == CAPS   || val == LSHFT ||
                val == RSHFT   || val == CTRL   || val == ALT   ||
                val == ESC     || val == NONE)
                return;

            if (val != 0) {
                // Write key to terminal context
                flanterm_write(ft_ctx, (const char *)&val);
            }
            return;
    }
}