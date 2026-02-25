#include <x86_64/idt/idt.h>
#include <x86_64/interrupts/pic.h>
#include <x86_64/interrupts/timer.h>
#include <x86_64/interrupts/keyboard.h>
#include <x86_64/serial.h>
#include <stdint.h>
#include <stdbool.h>
#include <flanterm/fb.h>

#define MAX_IDT_DESCS 255

extern struct flanterm_context *ft_ctx;

// External ISR stubs defined in assembly
extern void isr128_handler();
extern void* isr_stub_table[];

// Track which vectors are currently in use
static bool vectors[MAX_IDT_DESCS];

// Structure of a single IDT entry in 64-bit mode (x86_64)
typedef struct {
	uint16_t    isr_low;      // Lower 16 bits of the ISR address
	uint16_t    kernel_cs;    // Code segment selector (GDT)
	uint8_t	    ist;          // Interrupt Stack Table index
	uint8_t     attributes;   // Type and attributes (P, DPL, Gate Type)
	uint16_t    isr_mid;      // Middle 16 bits of the ISR address
	uint32_t    isr_high;     // Upper 32 bits of the ISR address
	uint32_t    reserved;     // Must be set to zero
} __attribute__((packed)) idt_entry_t;

// Structure representing the CPU state saved on the stack during an interrupt
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t exception_number, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) cpu_state_t;

// Structure for the IDTR register (passed to 'lidt')
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;

static idtr_t idtr;

// The actual IDT table, aligned for performance
__attribute__((aligned(0x10))) 
static idt_entry_t idt[256];

// Human-readable names for the first 32 CPU exceptions
static const char *exception_names[] = {
    "Division Error", "Debug", "Non-Maskable Interrupt", "Breakpoint",
    "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
    "Stack Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
    "Floating Point Exception", "Alignment Check", "Machine Check", "SIMD Floating Point",
    "Virtualization", "Control Protection", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Hypervisor Injection", "VMM Communication",
    "Security Exception", "Reserved"
};

// Helper for debug logging
static void print_separator() {
    serial_print("========================================\n");
}

// Main handler for CPU exceptions (traps and faults)
void exception_handler(uint64_t exception_number, cpu_state_t *state) {
    print_separator();
    serial_print("  KERNEL PANIC - EXCEPTION ");
    serial_print_num(exception_number);
    serial_print(": ");
    
    if (exception_number < 32) {
        serial_print(exception_names[exception_number]);
    } else if (exception_number == 128) {
        serial_print("Application Error");
    } else {
        serial_print("Unknown");
    }
    serial_print("\n");
    print_separator();

    serial_print("  ERROR CODE : ");
    serial_print_hex(state->error_code);
    serial_print("\n");

    // For Page Faults, read the faulting address from the CR2 register
    if (exception_number == 14) {
        uint64_t cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        serial_print("  FAULT ADDR : ");
        serial_print_hex(cr2);
        serial_print("\n");
    }

    // Dump general-purpose registers for debugging
    print_separator();
    serial_print("  REGISTERS\n");
    print_separator();

    serial_print("  RIP: "); serial_print_hex(state->rip);
    serial_print("  RSP: "); serial_print_hex(state->rsp);   serial_print("\n");
    serial_print("  RAX: "); serial_print_hex(state->rax);
    serial_print("  RBX: "); serial_print_hex(state->rbx);   serial_print("\n");
    serial_print("  RCX: "); serial_print_hex(state->rcx);
    serial_print("  RDX: "); serial_print_hex(state->rdx);   serial_print("\n");
    serial_print("  RSI: "); serial_print_hex(state->rsi);
    serial_print("  RDI: "); serial_print_hex(state->rdi);   serial_print("\n");
    serial_print("  RBP: "); serial_print_hex(state->rbp);
    serial_print("  R8 : "); serial_print_hex(state->r8);    serial_print("\n");
    serial_print("  R9 : "); serial_print_hex(state->r9);
    serial_print("  R10: "); serial_print_hex(state->r10);   serial_print("\n");
    serial_print("  R11: "); serial_print_hex(state->r11);
    serial_print("  R12: "); serial_print_hex(state->r12);   serial_print("\n");
    serial_print("  R13: "); serial_print_hex(state->r13);
    serial_print("  R14: "); serial_print_hex(state->r14);   serial_print("\n");
    serial_print("  R15: "); serial_print_hex(state->r15);
    serial_print("  CS : "); serial_print_hex(state->cs);    serial_print("\n");
    serial_print("  SS : "); serial_print_hex(state->ss);
    serial_print("  RFLAGS: "); serial_print_hex(state->rflags); serial_print("\n");

    print_separator();

    // Allow application-level errors to return, otherwise halt the system
    if (exception_number == 128) {
        serial_print("  Application error - system continuing\n");
        print_separator();
        return;
    }

    serial_print("  System Halted\n");
    print_separator();

    while (1) {
        __asm__ volatile ("hlt");
    }
}

// Dispatcher for hardware interrupts (IRQs)
void irq_handler(uint8_t irq) {
    switch (irq) {
    case 0: // System Timer
        on_irq0();
        pic_send_eoi(0);
        break;
    case 1: // PS/2 Keyboard
        keyboard_handler();
        pic_send_eoi(1);
        break;
    default:
        break;
    }
}

// Configures a specific gate in the IDT
void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    idt_entry_t* descriptor = &idt[vector];

    descriptor->isr_low        = (uint64_t)isr & 0xFFFF;
    descriptor->kernel_cs      = 8; // Assumes GDT kernel code segment is at index 1 (0x08)
    descriptor->ist            = 0;
    descriptor->attributes     = flags;
    descriptor->isr_mid        = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isr_high       = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved       = 0;
}

// Initialize the IDT and enable hardware interrupts
void idt_init() {
    // Set up the IDTR register
    idtr.base = (uintptr_t)&idt[0];
    idtr.limit = (uint16_t)sizeof(idt_entry_t) * MAX_IDT_DESCS - 1;

    // Load CPU exception handlers (0-31)
    for (uint8_t vector = 0; vector < 32; vector++) {
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8E); // 0x8E: Present, Ring 0, Interrupt Gate
        vectors[vector] = true;
    }

    // Load Hardware IRQ handlers (mapped to 32-47 by PIC)
    for (uint8_t vector = 0; vector < 16; vector++) {
        idt_set_descriptor(32 + vector, isr_stub_table[32 + vector], 0x8E);
        vectors[32 + vector] = true;
    }

    // Set up custom application error/syscall handler
    idt_set_descriptor(128, isr128_handler, 0x8E);

    // Unmask essential IRQs in the PIC
    IRQ_clear_mask(0); // Timer
    IRQ_clear_mask(1); // Keyboard

    // Load the IDT into the CPU and enable interrupts
    __asm__ volatile ("lidt %0" : : "m"(idtr));
    __asm__ volatile ("sti");
}