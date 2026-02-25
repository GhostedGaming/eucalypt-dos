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

extern void isr128_handler();
extern void* isr_stub_table[];
static bool vectors[MAX_IDT_DESCS];

typedef struct {
	uint16_t    isr_low;      // The lower 16 bits of the ISR's address
	uint16_t    kernel_cs;    // The GDT segment selector that the CPU will load into CS before calling the ISR
	uint8_t	    ist;          // The IST in the TSS that the CPU will load into RSP; set to zero for now
	uint8_t     attributes;   // Type and attributes; see the IDT page
	uint16_t    isr_mid;      // The higher 16 bits of the lower 32 bits of the ISR's address
	uint32_t    isr_high;     // The higher 32 bits of the ISR's address
	uint32_t    reserved;     // Set to zero
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t exception_number, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) cpu_state_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;

static idtr_t idtr;

__attribute__((aligned(0x10))) 
static idt_entry_t idt[256];

static const char *exception_names[] = {
    "Division Error",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "Floating Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating Point",
    "Virtualization",
    "Control Protection",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection",
    "VMM Communication",
    "Security Exception",
    "Reserved"
};

static void print_separator() {
    serial_print("========================================\n");
}

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

    if (exception_number == 14) {
        uint64_t cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        serial_print("  FAULT ADDR : ");
        serial_print_hex(cr2);
        serial_print("\n");
    }

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

void irq_handler(uint8_t irq) {
    switch (irq) {
    case 0:
        on_irq0();
        pic_send_eoi(0);
        break;
    case 1:
        keyboard_handler();
        pic_send_eoi(1);
        break;
    default:
        break;
    }
}

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    idt_entry_t* descriptor = &idt[vector];

    descriptor->isr_low        = (uint64_t)isr & 0xFFFF;
    descriptor->kernel_cs      = 8;
    descriptor->ist            = 0;
    descriptor->attributes     = flags;
    descriptor->isr_mid        = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isr_high       = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved       = 0;
}

void idt_init() {
    idtr.base = (uintptr_t)&idt[0];
    idtr.limit = (uint16_t)sizeof(idt_entry_t) * MAX_IDT_DESCS - 1;
    for (uint8_t vector = 0; vector < 32; vector++) {
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
        vectors[vector] = true;
    }

    for (uint8_t vector = 0; vector < 16; vector++) {
        idt_set_descriptor(32 + vector, isr_stub_table[32 + vector], 0x8E);
        vectors[32 + vector] = true;
    }

    idt_set_descriptor(128, isr128_handler, 0x8E);
    IRQ_clear_mask(0);
    IRQ_clear_mask(1);

    __asm__ volatile ("lidt %0" : : "m"(idtr));
    __asm__ volatile ("sti");
}