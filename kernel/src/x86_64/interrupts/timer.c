#include <x86_64/interrupts/timer.h>
#include <x86_64/commands.h>
#include <x86_64/interrupts/pic.h>

// Global tick counter updated by the IRQ0 handler
static volatile uint64_t timer_ticks = 0;

// Function called by the PIT interrupt (IRQ0)
void on_irq0() {
    timer_ticks++;
}

// Blocks execution for a specified number of PIT ticks
void timer_wait(uint32_t ticks) {
    uint64_t start_ticks = timer_ticks;
    while (timer_ticks < start_ticks + ticks) {
        // Use HLT instruction to save power while waiting
        __asm__ volatile ("hlt");
    }
}

// Initialize the Programmable Interval Timer (PIT)
void init_timer() {
    // 1193182 Hz / 11932 = ~100 Hz (10ms per tick)
    uint16_t divisor = 11932;

    // Send Mode/Command register: Select Channel 0, Square Wave Mode
    outb(0x43, 0x36);
    
    // Set reload value (Low byte then High byte)
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

// Retrieve the current uptime in ticks
uint64_t get_timer_ticks() {
    return timer_ticks;
}