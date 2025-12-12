#include <x86_64/memory/heap.h>

#include <stddef.h>
#include <stdint.h>

struct linked_list_node {
    struct linked_list_node *next;
    struct linked_list_node *prev;
    void *data;
    size_t size;
};

struct linked_list {
    struct linked_list_node *first;
    struct linked_list_node *last;
};

static struct linked_list heap_list;
static uint8_t *heap_start = NULL;
static size_t heap_size = 0;
static size_t heap_offset = 0;

void linked_list_init(struct linked_list *list) {
    list->first = NULL;
    list->last = NULL;
}

void heap_init(void *start, size_t size) {
    heap_start = (uint8_t *)start;
    heap_size = size;
    heap_offset = 0;
    linked_list_init(&heap_list);
}

void append(struct linked_list *list, struct linked_list_node *node) {
    node->next = NULL;
    node->prev = list->last;

    if (list->last) {
        list->last->next = node;
    } else {
        list->first = node;
    }
    list->last = node;
}

void *kmalloc(size_t size) {
    if (!heap_start || heap_offset + size > heap_size) {
        return NULL;
    }

    struct linked_list_node *node = (struct linked_list_node *)(heap_start + heap_offset);
    node->data = (void *)((uintptr_t)node + sizeof(struct linked_list_node));
    node->size = size;
    
    heap_offset += sizeof(struct linked_list_node) + size;
    append(&heap_list, node);
    
    return node->data;
}

void kfree(void *ptr) {
    if (!ptr) return;
    
    for (struct linked_list_node *node = heap_list.first; node; node = node->next) {
        if (node->data == ptr) {
            if (node->prev) {
                node->prev->next = node->next;
            } else {
                heap_list.first = node->next;
            }
            if (node->next) {
                node->next->prev = node->prev;
            } else {
                heap_list.last = node->prev;
            }
            return;
        }
    }
}