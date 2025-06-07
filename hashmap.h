#ifndef HASHMAP_H
#define HASHMAP_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t    capacity;
    size_t    count;
    uint64_t *keys;
    void    **values;
} u64_map;

typedef struct {
    size_t   capacity;
    size_t   count;
    char   **keys;
    void    **values;
} str_map;

#ifndef NDEBUG
  #include <assert.h>
  void u64_map_check(u64_map *m);
  void str_map_check(str_map *m);
#else
  #define u64_map_check(m)  ((void)0)
  #define str_map_check(m)  ((void)0)
#endif

u64_map *u64_map_create(size_t initial_capacity);
void     u64_map_destroy(u64_map *m);
int      u64_map_put(u64_map *m, uint64_t key, void *value);
void    *u64_map_get(u64_map *m, uint64_t key);
int u64_map_remove(u64_map *m, uint64_t key);

str_map *str_map_create(size_t initial_capacity);
void     str_map_destroy(str_map *m);
int      str_map_put(str_map *m, const char *key, void *value);
void    *str_map_get(str_map *m, const char *key);
int str_map_remove(str_map *m, const char *key);

#endif /* HASHMAP_H */

