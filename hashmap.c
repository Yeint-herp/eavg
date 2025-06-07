#include "hashmap.h"
#include <stdlib.h>
#include <string.h>

#ifndef NDEBUG
#include <assert.h>
void u64_map_check(u64_map *m) {
    assert(m);
    assert(m->count <= m->capacity);
    {
        size_t actual = 0;
        size_t i;
        for (i = 0; i < m->capacity; i++) {
            if (m->keys[i]) actual++;
        }
        assert(actual == m->count);
    }
}
#endif

static size_t next_power_of_two(size_t v) {
    size_t p = 1;
    while (p < v) p <<= 1;
    return p;
}

u64_map *u64_map_create(size_t initial_capacity) {
    size_t cap = next_power_of_two(initial_capacity);
    u64_map *m = malloc(sizeof *m);
    if (!m) return NULL;
    m->capacity = cap;
    m->count    = 0;
    m->keys     = calloc(cap, sizeof *m->keys);
    m->values   = calloc(cap, sizeof *m->values);
    if (!m->keys || !m->values) {
        free(m->keys);
        free(m->values);
        free(m);
        return NULL;
    }
    return m;
}

void u64_map_destroy(u64_map *m) {
    if (!m) return;
    free(m->keys);
    free(m->values);
    free(m);
}

static int u64_map_grow(u64_map *m) {
    size_t i;
    size_t newcap     = m->capacity * 2;
    uint64_t *newkeys = calloc(newcap, sizeof *newkeys);
    void     *newvals = calloc(newcap, sizeof *newvals);
    if (!newkeys || !newvals) {
        free(newkeys);
        free(newvals);
        return -1;
    }
    for (i = 0; i < m->capacity; i++) {
        uint64_t k = m->keys[i];
        if (k) {
            size_t idx = (k * 11400714819323198485ULL) & (newcap - 1);
            while (newkeys[idx]) idx = (idx + 1) & (newcap - 1);
            newkeys[idx]       = k;
            ((void**)newvals)[idx] = m->values[i];
        }
    }
    free(m->keys);
    free(m->values);
    m->keys     = newkeys;
    m->values   = newvals;
    m->capacity = newcap;
    return 0;
}

int u64_map_put(u64_map *m, uint64_t key, void *value) {
    if (m->count * 100 >= m->capacity * 70) {
        if (u64_map_grow(m) < 0) {
#ifndef NDEBUG
            assert(!"u64_map_grow failed");
#endif
            return -1;
        }
    }
    {
        size_t idx = (key * 11400714819323198485ULL) & (m->capacity - 1);
        while (m->keys[idx] && m->keys[idx] != key) {
            idx = (idx + 1) & (m->capacity - 1);
        }
        if (!m->keys[idx]) {
            m->keys[idx] = key;
            m->count++;
        }
        m->values[idx] = value;
    }
#ifndef NDEBUG
    u64_map_check(m);
#endif
    return 0;
}

void *u64_map_get(u64_map *m, uint64_t key) {
    {
        size_t idx = (key * 11400714819323198485ULL) & (m->capacity - 1);
        while (m->keys[idx]) {
            if (m->keys[idx] == key) {
#ifndef NDEBUG
                u64_map_check(m);
#endif
                return m->values[idx];
            }
            idx = (idx + 1) & (m->capacity - 1);
        }
    }
#ifndef NDEBUG
    u64_map_check(m);
#endif
    return NULL;
}

int u64_map_remove(u64_map *m, uint64_t key) {
    size_t cap = m->capacity;
    size_t idx = (key * 11400714819323198485ULL) & (cap - 1);

    while (m->keys[idx]) {
        if (m->keys[idx] == key) break;
        idx = (idx + 1) & (cap - 1);
    }
    if (!m->keys[idx]) return -1;

    m->keys[idx]   = 0;
    m->values[idx] = NULL;
    m->count--;

    size_t next = (idx + 1) & (cap - 1);
    while (m->keys[next]) {
        uint64_t rekey   = m->keys[next];
        void    *revalue = m->values[next];
        m->keys[next]    = 0;
        m->values[next]  = NULL;
        m->count--;
        u64_map_put(m, rekey, revalue);
        next = (next + 1) & (cap - 1);
    }
#ifndef NDEBUG
    u64_map_check(m);
#endif
    return 0;
}

#ifndef NDEBUG
void str_map_check(str_map *m) {
    assert(m);
    assert(m->count <= m->capacity);
    {
        size_t actual = 0, i;
        for (i = 0; i < m->capacity; i++) {
            if (m->keys[i]) actual++;
        }
        assert(actual == m->count);
    }
}
#endif

str_map *str_map_create(size_t initial_capacity) {
    size_t cap = next_power_of_two(initial_capacity);
    str_map *m = malloc(sizeof *m);
    if (!m) return NULL;
    m->capacity = cap;
    m->count    = 0;
    m->keys     = calloc(cap, sizeof *m->keys);
    m->values   = calloc(cap, sizeof *m->values);
    if (!m->keys || !m->values) {
        free(m->keys);
        free(m->values);
        free(m);
        return NULL;
    }
    return m;
}

void str_map_destroy(str_map *m) {
    if (!m) return;
    free(m->keys);
    free(m->values);
    free(m);
}

static int str_map_grow(str_map *m) {
    size_t i;
    size_t newcap    = m->capacity * 2;
    char   **nkeys   = calloc(newcap, sizeof *nkeys);
    void    **nvals  = calloc(newcap, sizeof *nvals);
    if (!nkeys || !nvals) {
        free(nkeys);
        free(nvals);
        return -1;
    }
    for (i = 0; i < m->capacity; i++) {
        char *k = m->keys[i];
        if (k) {
            size_t idx = 0;
            {
                uint64_t h = 1469598103934665603ULL;
                const unsigned char *p = (unsigned char*)k;
                while (*p) { h ^= *p++; h *= 1099511628211ULL; }
                idx = h & (newcap - 1);
            }
            while (nkeys[idx]) idx = (idx + 1) & (newcap - 1);
            nkeys[idx] = k;
            nvals[idx] = m->values[i];
        }
    }
    free(m->keys);
    free(m->values);
    m->keys     = nkeys;
    m->values   = nvals;
    m->capacity = newcap;
    return 0;
}

int str_map_put(str_map *m, const char *key, void *value) {
    if (m->count * 100 >= m->capacity * 70) {
        if (str_map_grow(m) < 0) {
#ifndef NDEBUG
            assert(!"str_map_grow failed");
#endif
            return -1;
        }
    }
    {
        size_t idx = 0;
        {
            uint64_t h = 1469598103934665603ULL;
            const unsigned char *p = (unsigned char*)key;
            while (*p) { h ^= *p++; h *= 1099511628211ULL; }
            idx = h & (m->capacity - 1);
        }
        while (m->keys[idx] && strcmp(m->keys[idx], key)) {
            idx = (idx + 1) & (m->capacity - 1);
        }
        if (!m->keys[idx]) {
            m->keys[idx] = (char*)key;
            m->count++;
        }
        m->values[idx] = value;
    }
#ifndef NDEBUG
    str_map_check(m);
#endif
    return 0;
}

void *str_map_get(str_map *m, const char *key) {
    {
        size_t idx = 0;
        {
            uint64_t h = 1469598103934665603ULL;
            const unsigned char *p = (unsigned char*)key;
            while (*p) { h ^= *p++; h *= 1099511628211ULL; }
            idx = h & (m->capacity - 1);
        }
        while (m->keys[idx]) {
            if (strcmp(m->keys[idx], key) == 0) {
#ifndef NDEBUG
                str_map_check(m);
#endif
                return m->values[idx];
            }
            idx = (idx + 1) & (m->capacity - 1);
        }
    }
#ifndef NDEBUG
    str_map_check(m);
#endif
    return NULL;
}

int str_map_remove(str_map *m, const char *key) {
    size_t   cap = m->capacity;
    uint64_t h   = 1469598103934665603ULL;
    for (const unsigned char *p = (const unsigned char*)key; *p; p++) {
        h ^= *p;
        h *= 1099511628211ULL;
    }
    size_t idx = h & (cap - 1);

    while (m->keys[idx]) {
        if (strcmp(m->keys[idx], key) == 0) break;
        idx = (idx + 1) & (cap - 1);
    }
    if (!m->keys[idx]) return -1;

    m->keys[idx]   = NULL;
    m->values[idx] = NULL;
    m->count--;

    size_t next = (idx + 1) & (cap - 1);
    while (m->keys[next]) {
        char   *rekey   = m->keys[next];
        void   *revalue = m->values[next];
        m->keys[next]   = NULL;
        m->values[next] = NULL;
        m->count--;
        str_map_put(m, rekey, revalue);
        next = (next + 1) & (cap - 1);
    }
#ifndef NDEBUG
    str_map_check(m);
#endif
    return 0;
}
