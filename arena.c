#include "arena.h"
#include <stdlib.h>
#include <stdint.h>

#ifndef NDEBUG
#include <assert.h>
void arena_check(Arena *a) {
    ArenaBlock *b = a->blocks;
    while (b) {
        assert(b->used <= b->capacity);
        assert(b->capacity > 0);
        b = b->next;
    }
}
#endif

#define ALIGNMENT sizeof(void*)

void arena_init(Arena *a, size_t block_size) {
    a->blocks     = NULL;
    a->block_size = block_size;
}

void arena_destroy(Arena *a) {
    ArenaBlock *b = a->blocks;
    while (b) {
        ArenaBlock *n = b->next;
        free(b);
        b = n;
    }
    a->blocks = NULL;
}

void *arena_alloc(Arena *a, size_t size) {
    size_t aligned = (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
#ifndef NDEBUG
    assert(size > 0);
#endif
    ArenaBlock *b = a->blocks;
    if (!b || b->used + aligned > b->capacity) {
        size_t cap = a->block_size > aligned ? a->block_size : aligned;
        b = malloc(sizeof(ArenaBlock) + cap);
        if (!b) return NULL;
        b->next     = a->blocks;
        b->capacity = cap;
        b->used     = 0;
        a->blocks   = b;
    }
    void *p = b->data + b->used;
    b->used += aligned;
#ifndef NDEBUG
    arena_check(a);
#endif
    return p;
}

