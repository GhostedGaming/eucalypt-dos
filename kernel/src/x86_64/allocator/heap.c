#include <x86_64/allocator/heap.h>
#include <x86_64/allocator/frame_allocator.h>
#include <x86_64/allocator/addr.h>
#include <x86_64/serial.h>
#include <stdbool.h>

struct linked_list_node {
    struct linked_list_node *next;
    struct linked_list_node *prev;
    size_t size;
    bool used;
};

struct linked_list {
    struct linked_list_node *first;
    struct linked_list_node *last;
};

static struct linked_list heap_list;

#define NODE_DATA(node) ((void *)((uint8_t *)(node) + sizeof(struct linked_list_node)))
#define NODE_FROM_DATA(ptr) ((struct linked_list_node *)((uint8_t *)(ptr) - sizeof(struct linked_list_node)))

static void linked_list_init(struct linked_list *list) {
    list->first = NULL;
    list->last = NULL;
}

static struct linked_list_node *expand_heap(size_t size) {
    size_t total = size + sizeof(struct linked_list_node);
    size_t frames_needed = (total + 4095) / 4096;

    serial_print("expand_heap: size=");
    serial_print_num(size);
    serial_print(" frames_needed=");
    serial_print_num(frames_needed);
    serial_print("\n");

    uint64_t phys = frame_alloc();
    if (!phys) {
        serial_print("expand_heap: frame_alloc failed\n");
        return NULL;
    }

    struct linked_list_node *node = (struct linked_list_node *)phys_to_virt(phys);
    node->prev = heap_list.last;
    node->next = NULL;
    node->size = 4096 - sizeof(struct linked_list_node);
    node->used = false;

    if (heap_list.last) {
        heap_list.last->next = node;
    } else {
        heap_list.first = node;
    }
    heap_list.last = node;

    serial_print("expand_heap: first frame at virt=");
    serial_print_hex((uint64_t)node);
    serial_print("\n");

    for (size_t i = 1; i < frames_needed; i++) {
        uint64_t phys2 = frame_alloc();
        if (!phys2) {
            serial_print("expand_heap: frame_alloc failed on frame ");
            serial_print_num(i);
            serial_print("\n");
            break;
        }
        struct linked_list_node *node2 = (struct linked_list_node *)phys_to_virt(phys2);
        node2->prev = heap_list.last;
        node2->next = NULL;
        node2->size = 4096 - sizeof(struct linked_list_node);
        node2->used = false;
        heap_list.last->next = node2;
        heap_list.last = node2;

        serial_print("expand_heap: extra frame ");
        serial_print_num(i);
        serial_print(" at virt=");
        serial_print_hex((uint64_t)node2);
        serial_print("\n");
    }

    return node;
}

void heap_init() {
    linked_list_init(&heap_list);
    expand_heap(0);
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    const size_t alignment = 8;
    size = (size + (alignment - 1)) & ~(alignment - 1);

    for (struct linked_list_node *node = heap_list.first; node; node = node->next) {
        if (!node->used && node->size >= size) {
            size_t remaining = node->size - size;
            if (remaining >= sizeof(struct linked_list_node) + 8) {
                struct linked_list_node *new_node =
                    (struct linked_list_node *)((uint8_t *)NODE_DATA(node) + size);
                new_node->prev = node;
                new_node->next = node->next;
                if (new_node->next) new_node->next->prev = new_node;
                else heap_list.last = new_node;
                new_node->size = remaining - sizeof(struct linked_list_node);
                new_node->used = false;
                node->next = new_node;
                node->size = size;
            }
            node->used = true;
            serial_print("kmalloc: size=");
            serial_print_num(size);
            serial_print(" ptr=");
            serial_print_hex((uint64_t)NODE_DATA(node));
            serial_print("\n");
            return NODE_DATA(node);
        }
    }

    serial_print("kmalloc: no free block, expanding heap\n");
    struct linked_list_node *node = expand_heap(size);
    if (!node) {
        serial_print("kmalloc: out of memory\n");
        return NULL;
    }
    return kmalloc(size);
}

void kfree(void *ptr) {
    if (!ptr) return;

    struct linked_list_node *node = NODE_FROM_DATA(ptr);
    serial_print("kfree: ptr=");
    serial_print_hex((uint64_t)ptr);
    serial_print(" size=");
    serial_print_num(node->size);
    serial_print("\n");

    node->used = false;

    // coalesce with previous
    if (node->prev && !node->prev->used) {
        struct linked_list_node *prev = node->prev;
        prev->size += sizeof(struct linked_list_node) + node->size;
        prev->next = node->next;
        if (node->next) node->next->prev = prev;
        else heap_list.last = prev;
        node = prev;
        serial_print("kfree: coalesced with prev\n");
    }

    // coalesce with next
    if (node->next && !node->next->used) {
        struct linked_list_node *next = node->next;
        node->size += sizeof(struct linked_list_node) + next->size;
        node->next = next->next;
        if (next->next) next->next->prev = node;
        else heap_list.last = node;
        serial_print("kfree: coalesced with next\n");
    }
}