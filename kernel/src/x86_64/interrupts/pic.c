#include <x86_64/commands.h>
#include <stdint.h>

// I/O base addresses for Master and Slave PICs
#define PIC1		    0x20
#define PIC2		    0xA0
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	    (PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	    (PIC2+1)

// Initialization Control Word 1 bits
#define ICW1_ICW4	    0x01 // Expect ICW4 during init
#define ICW1_SINGLE	    0x02 // Single mode (0 = Cascade)
#define ICW1_INTERVAL4	0x04 // Call address interval (8080/8085)
#define ICW1_LEVEL	    0x08 // Level triggered mode
#define ICW1_INIT	    0x10 // Initialization bit

// Initialization Control Word 4 bits
#define ICW4_8086	    0x01 // 8086/88 (MCS-80/85) mode
#define PIC_EOI		    0x20 // End of Interrupt command
#define CASCADE_IRQ     2    // Slave PIC is connected to IRQ2 on Master

// Remap the PIC interrupt offsets to avoid conflicts with CPU exceptions
void PIC_remap(int offset1, int offset2) {
    // Start initialization sequence
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

	// ICW2: Set vector offsets (usually 0x20 for Master, 0x28 for Slave)
	outb(PIC1_DATA, offset1);
	outb(PIC2_DATA, offset2);

	// ICW3: Define Master/Slave relationship
	outb(PIC1_DATA, 1 << CASCADE_IRQ); // Master: Slave is on IRQ2
	outb(PIC2_DATA, 2);                // Slave: Tell it its identity is 2

	// ICW4: Set environment mode
	outb(PIC1_DATA, ICW4_8086);
	outb(PIC2_DATA, ICW4_8086);

	// Mask all interrupts initially
	outb(PIC1_DATA, 0);
	outb(PIC2_DATA, 0);
}

// Signal the PIC that an interrupt has been handled
void pic_send_eoi(uint8_t irq) {
    // If IRQ came from Slave PIC, notify Slave
    if (irq >= 8) outb(PIC2_COMMAND, PIC_EOI);

    // Always notify Master PIC
    outb(PIC1_COMMAND, PIC_EOI);
}

// Disable a specific IRQ line
void IRQ_set_mask(uint8_t irq) {
    uint16_t port;
    if(irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    uint8_t value = inb(port) | (1 << irq);
    outb(port, value);        
}

// Enable a specific IRQ line
void IRQ_clear_mask(uint8_t irq) {
    uint16_t port;
    if(irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    uint8_t value = inb(port) & ~(1 << irq);
    outb(port, value);        
}