#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <flanterm/flanterm.h>
#include <flanterm/fb.h>

#include <x86_64/serial.h>
#include <x86_64/gdt.h>
#include <x86_64/interrupts/pic.h>
#include <x86_64/idt/idt.h>
#include <x86_64/interrupts/timer.h>
#include <x86_64/memory/heap.h>

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(4);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void hcf(void) {
    for (;;) {
#if defined (__x86_64__)
        asm ("hlt");
#elif defined (__aarch64__) || defined (__riscv)
        asm ("wfi");
#elif defined (__loongarch64)
        asm ("idle 0");
#endif
    }
}

struct flanterm_context *ft_ctx = NULL;

void kmain(void) {
    __asm__ volatile ("cli");
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    uint32_t *fb_ptr = framebuffer->address;
    uint64_t fb_width = framebuffer->width;
    uint64_t fb_height = framebuffer->height;
    uint64_t fb_pitch = framebuffer->pitch;

    ft_ctx = flanterm_fb_init(
        NULL,
        NULL,
        fb_ptr, fb_width, fb_height, fb_pitch,
        framebuffer->red_mask_size, framebuffer->red_mask_shift,
        framebuffer->green_mask_size, framebuffer->green_mask_shift,
        framebuffer->blue_mask_size, framebuffer->blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        0, 0,
        0, 0
    );
    serial_init();
    flanterm_write(ft_ctx, "Initializing Heap...\n");
    serial_print("Initializing Heap...\n");
    heap_init((void *)0xFFFF800000100000, 0x100000);
    flanterm_write(ft_ctx, "Heap Initialized\n");
    serial_print("Heap Initialized\n");
    serial_print("Initializing GDT...\n");
    flanterm_write(ft_ctx, "Initializing GDT...\n");
    init_gdt();
    load_gdt();
    flanterm_write(ft_ctx, "GDT initialized\nRemapping PIC\n");
    serial_print("GDT initialized\nRemapping PIC\n");
    PIC_remap(32, 47);
    flanterm_write(ft_ctx, "PIC Remapped Successfully\nInitializing IDT\n");
    serial_print("PIC Remapped Successfully\nInitializing IDT\n");
    idt_init();
    flanterm_write(ft_ctx, "IDT Intiialized and Loaded\nInitializing Timer\n");
    serial_print("IDT Intiialized and Loaded\nInitializing Timer\n");
    init_timer();
    flanterm_write(ft_ctx, "Timer Initialized\nEnabling Interrupts\n");
    serial_print("Timer Initialized\nEnabling Interrupts\n");
    __asm__ volatile ("sti");
    flanterm_write(ft_ctx, "Interrupts Enabled\nTesting Kmalloc");
    serial_print("Interrupts Enabled\nTesting Kmalloc\n");
    kmalloc(1024);
    flanterm_write(ft_ctx, "Kmalloc Test Successful\nSystem Halted\n");
    serial_print("Kmalloc Test Successful\nSystem Halted\n");

    hcf();
}
