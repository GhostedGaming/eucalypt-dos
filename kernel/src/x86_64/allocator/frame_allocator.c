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
            uint64_t top = memmap_response->entries[i]->base + memmap_response->entries[i]->length;
            if (top > highest_addr) highest_addr = top;
        }
    }

    // Initialize bit_map to first usable region
    for (uint64_t i = 0; i < count; i++) {
        if (memmap_response->entries[i]->type == LIMINE_MEMMAP_USABLE) {
            bit_map = (uint8_t *)(memmap_response->entries[i]->base + hhdm_offset);
            serial_print("bitmap_phys: ");
            serial_print_hex(memmap_response->entries[i]->base);
            serial_print("\nbitmap virt: ");
            serial_print_hex((uint64_t)bit_map);
            serial_print("\nhighest_addr: ");
            serial_print_hex(highest_addr);
            serial_print("\nbitmap size: ");
            serial_print_num(highest_addr / 4096);
            serial_print(" bytes\n");
            break;
        }
    }

    // Mark everything as used, then free only usable regions
    serial_print("marking everything used...\n");
    for (uint64_t i = 0; i < highest_addr / 4096; i++) bit_map[i] = USED;

    serial_print("marking usable as unused...\n");
    for (uint64_t i = 0; i < count; i++) {
        if (memmap_response->entries[i]->type == LIMINE_MEMMAP_USABLE) {
            for (uint64_t j = 0; j < memmap_response->entries[i]->length; j += 4096)
                bit_map[(memmap_response->entries[i]->base + j) / 4096] = UNUSED;
        }
    }

    // Reserve the first 16MiB for the kernel and bitmap
    serial_print("reserving first 16MiB...\n");
    for (uint64_t i = 0; i < (16 * 1024 * 1024) / 4096; i++) bit_map[i] = USED;

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