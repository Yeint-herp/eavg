#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>

typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t             capacity;
    size_t             used;
    char               data[];
} ArenaBlock;

typedef struct {
    ArenaBlock *blocks;
    size_t      block_size;
} Arena;

#ifndef NDEBUG
  #include <assert.h>
  void arena_check(Arena *a);
#else
  #define arena_check(a) ((void)0)
#endif

void arena_init(Arena *a, size_t block_size);
void arena_destroy(Arena *a);
void *arena_alloc(Arena *a, size_t size);

#endif /* ARENA_H */

