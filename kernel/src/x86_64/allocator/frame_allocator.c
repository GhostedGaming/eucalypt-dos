#include <x86_64/allocator/frame_allocator.h>
#include <x86_64/allocator/addr.h>
#include <x86_64/serial.h>

#define UNUSED 0x00
#define USED   0x01

uint8_t *bit_map;
uint64_t highest_addr = 0;

void frame_allocator_init(struct limine_memmap_response *memmap_response, uint64_t hhdm_offset) {
    uint64_t count = memmap_response->entry_count;
    
    // Find the highest addr
    for (uint64_t i = 0; i < count; i++) {
        if (memmap_response->entries[i]->type == LIMINE_MEMMAP_USABLE) {
            for (uint64_t j = 0; j < memmap_response->entries[i]->length; j += 4096) {
                if (memmap_response->entries[i]->base + j > highest_addr) {
                    highest_addr = memmap_response->entries[i]->base + j;
                }
            }
        }
    }

    // Initialize bit_map to first usable region
    uint64_t bitmap_phys = 0;
    for (uint64_t i = 0; i < count; i++) {
        if (memmap_response->entries[i]->type == LIMINE_MEMMAP_USABLE) {
            bitmap_phys = memmap_response->entries[i]->base;
            break;
        }
    }
    bit_map = (uint8_t *)(bitmap_phys + hhdm_offset);

    serial_print("bitmap_phys: ");
    serial_print_hex(bitmap_phys);
    serial_print("\nbitmap virt: ");
    serial_print_hex((uint64_t)bit_map);
    serial_print("\nhighest_addr: ");
    serial_print_hex(highest_addr);
    serial_print("\nbitmap size: ");
    serial_print_num(highest_addr / 4096);
    serial_print(" bytes\n");

    serial_print("marking everything used...\n");
    // Mark everything as used so we can later mark it as unused
    for (uint64_t i = 0; i < highest_addr / 4096; i++) {
        bit_map[i] = USED;
    }

    serial_print("marking usable as unused...\n");
    // Here we mark the usable memory as unused
    for (uint64_t i = 0; i < count; i++) {
        if (memmap_response->entries[i]->type == LIMINE_MEMMAP_USABLE) {
            for (uint64_t j = 0; j < memmap_response->entries[i]->length; j += 4096) {
                uint64_t frame = (memmap_response->entries[i]->base + j) / 4096;
                bit_map[frame] = UNUSED;
            }
        }
    }

    // Mark the bitmap frames themselves as used
    uint64_t bitmap_frames = (highest_addr / 4096 + 4095) / 4096;
    for (uint64_t i = 0; i < bitmap_frames; i++) {
        bit_map[(bitmap_phys / 4096) + i] = USED;
    }

    serial_print("frame allocator initialized\n");
}

uint64_t frame_alloc() {
    // Find the first unused frame
    for (uint64_t i = 0; i < highest_addr / 4096; i++) {
        if (bit_map[i] == UNUSED) {
            bit_map[i] = USED;
            // Then return it
            serial_print("frame_alloc: ");
            serial_print_hex(i * 4096);
            serial_print("\n");
            return i * 4096;
        }
    }
    serial_print("frame_alloc: out of memory\n");
    return 0;
}

void frame_free(uint64_t frame) {
    // Check if the frame is valid
    if (frame >= highest_addr) {
        // Return if not
        serial_print("frame_free: invalid frame ");
        serial_print_hex(frame);
        serial_print("\n");
        return;
    }
    // Free the frame
    bit_map[frame / 4096] = UNUSED;
    serial_print("frame_free: ");
    serial_print_hex(frame);
    serial_print("\n");
}