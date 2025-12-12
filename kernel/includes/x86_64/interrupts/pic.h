#ifndef PIC_H
#define PIC_H

#include <stdint.h>

void PIC_remap(int offset1, int offset2);
void pic_send_eoi(uint8_t irq);
void IRQ_set_mask(uint8_t irq);
void IRQ_clear_mask(uint8_t irq);

#endif