#include "memory.h"

typedef struct block {
    uint32_t size;
    uint8_t free;
    struct block* next;
} block_t;

static block_t* heap_head = NULL;
static uint32_t heap_end;

#define HEAP_MAX_SIZE 0x100000

void memory_init(uint32_t heap_start) {
    heap_head = (block_t*)heap_start;
    heap_head->size = HEAP_MAX_SIZE - sizeof(block_t);
    heap_head->free = 1;
    heap_head->next = NULL;
    heap_end = heap_start + HEAP_MAX_SIZE;
}

static void split_block(block_t* block, uint32_t size) {
    if (block->size > size + sizeof(block_t)) {
        block_t* new_block = (block_t*)((uint32_t)block + sizeof(block_t) + size);
        new_block->size = block->size - size - sizeof(block_t);
        new_block->free = 1;
        new_block->next = block->next;
        block->size = size;
        block->next = new_block;
    }
}

void* kmalloc(uint32_t size) {
    block_t* curr = heap_head;
    while (curr) {
        if (curr->free && curr->size >= size) {
            split_block(curr, size);
            curr->free = 0;
            return (void*)((uint32_t)curr + sizeof(block_t));
        }
        curr = curr->next;
    }
    return NULL;
}

static void merge_blocks() {
    block_t* curr = heap_head;
    while (curr && curr->next) {
        if (curr->free && curr->next->free) {
            curr->size += sizeof(block_t) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

void kfree(void* ptr) {
    if (!ptr) return;
    block_t* block = (block_t*)((uint32_t)ptr - sizeof(block_t));
    block->free = 1;
    merge_blocks();
}

#define STACK_MAX_SIZE 0x4000
static uint32_t stack_top = 0x200000;

void* kalloc_stack(uint32_t size) {
    if (size > STACK_MAX_SIZE) return NULL;
    stack_top -= size;
    return (void*)stack_top;
}

void kfree_stack(void* ptr) {
    stack_top = (uint32_t)ptr;
}
