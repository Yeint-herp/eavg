#include "eavg.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#define EAVG_PERS_MAGIC  "EAVGPERS"
#define EAVG_PERS_VERSION 1

static char *strdup_arena(Arena *a, const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char  *d = arena_alloc(a, n);
    if (d) memcpy(d, s, n);
    return d;
}

static char *strdup_arena_intern(Arena *arena, str_map *internTable, const char *s) {
    if (!s) return NULL;
    char *interned = str_map_get(internTable, s);
    if (interned) return interned;

    size_t len = strlen(s);
    char *copy = arena_alloc(arena, len + 1);
    memcpy(copy, s, len + 1);

    str_map_put(internTable, copy, copy);
    return copy;
}

#define LOCK_RD(db)  pthread_rwlock_rdlock(&((db)->lock))
#define UNLOCK_RD(db) pthread_rwlock_unlock(&((db)->lock))
#define LOCK_WR(db)  pthread_rwlock_wrlock(&((db)->lock))
#define UNLOCK_WR(db) pthread_rwlock_unlock(&((db)->lock))

eavgDB *eavgDB_create(size_t initial_capacity) {
    eavgDB *db = malloc(sizeof *db);
    if (!db) return NULL;

    db->entitiesById     = u64_map_create(initial_capacity);
    db->entitiesByName   = str_map_create(initial_capacity);
    db->attributesById   = u64_map_create(initial_capacity);
    db->attributesByName = str_map_create(initial_capacity);
    db->relationTypesById   = u64_map_create(initial_capacity);
    db->relationTypesByName = str_map_create(initial_capacity);
    db->valuesByEntity         = u64_map_create(initial_capacity);
    db->adjIndexBySource       = u64_map_create(initial_capacity);
    db->reverseAdjIndexByTarget= u64_map_create(initial_capacity);

    arena_init(&db->entityArena,    1<<16);
    arena_init(&db->attributeArena, 1<<16);
    arena_init(&db->valueArena,     1<<20);
    arena_init(&db->edgeArena,      1<<20);

    db->nextEntityId        = 1;
    db->nextAttributeId     = 1;
    db->nextValueId         = 1;
    db->nextRelationTypeId  = 1;
    db->nextEdgeId          = 1;

    pthread_rwlock_init(&db->lock, NULL);
    return db;
}

void eavgDB_destroy(eavgDB *db) {
    if (!db) return;

    LOCK_WR(db);
    u64_map_destroy(db->entitiesById);
    str_map_destroy(db->entitiesByName);
    u64_map_destroy(db->attributesById);
    str_map_destroy(db->attributesByName);
    u64_map_destroy(db->relationTypesById);
    str_map_destroy(db->relationTypesByName);
    u64_map_destroy(db->valuesByEntity);
    u64_map_destroy(db->adjIndexBySource);
    u64_map_destroy(db->reverseAdjIndexByTarget);

    arena_destroy(&db->entityArena);
    arena_destroy(&db->attributeArena);
    arena_destroy(&db->valueArena);
    arena_destroy(&db->edgeArena);
    UNLOCK_WR(db);

    pthread_rwlock_destroy(&db->lock);
    free(db);
}

eavgEntity *eavgDB_addEntity(eavgDB *db,
                             eavg_u32 typeId,
                             const char *name)
{
    LOCK_WR(db);
    eavgEntity *e = EAVG_ENTITY_ALLOC(db);
    EAVG_ASSERT(e);
    e->id     = db->nextEntityId++;
    e->typeId = typeId;
    e->name   = strdup_arena(&db->entityArena, name);
    u64_map_put(db->entitiesById, e->id, e);
    if (name) str_map_put(db->entitiesByName, e->name, e);
    UNLOCK_WR(db);
    return e;
}

eavgEntity *eavgDB_findEntityById(eavgDB *db, eavg_u64 id) {
    LOCK_RD(db);
    eavgEntity *e = u64_map_get(db->entitiesById, id);
    UNLOCK_RD(db);
    return e;
}

eavgEntity *eavgDB_findEntityByName(eavgDB *db, const char *name) {
    LOCK_RD(db);
    eavgEntity *e = str_map_get(db->entitiesByName, name);
    UNLOCK_RD(db);
    return e;
}

void eavgDB_forEachEntity(eavgDB *db,
                          eavgEntityCallback cb,
                          void *userData)
{
    LOCK_RD(db);
    size_t cap = db->entitiesById->capacity;
    for (size_t i = 0; i < cap; i++) {
        eavgEntity *e = (eavgEntity*)db->entitiesById->values[i];
        if (e && cb(db, e, userData) != 0) break;
    }
    UNLOCK_RD(db);
}

eavgAttribute *eavgDB_addAttribute(eavgDB *db,
                                   const char *name,
                                   eavg_u32 dataType)
{
    LOCK_WR(db);
    eavgAttribute *a = EAVG_ATTR_ALLOC(db);
    EAVG_ASSERT(a);
    a->id       = db->nextAttributeId++;
    a->dataType = dataType;
    a->name     = strdup_arena(&db->attributeArena, name);
    a->onValueAdded = NULL;
    a->userData     = NULL;
    u64_map_put(db->attributesById, a->id, a);
    if (name) str_map_put(db->attributesByName, a->name, a);
    UNLOCK_WR(db);
    return a;
}

eavgAttribute *eavgDB_findAttributeById(eavgDB *db, eavg_u64 id) {
    LOCK_RD(db);
    eavgAttribute *a = u64_map_get(db->attributesById, id);
    UNLOCK_RD(db);
    return a;
}

eavgAttribute *eavgDB_findAttributeByName(eavgDB *db, const char *name) {
    LOCK_RD(db);
    eavgAttribute *a = str_map_get(db->attributesByName, name);
    UNLOCK_RD(db);
    return a;
}

static eavgValRec *add_value_rec(eavgDB *db,
                                 eavg_u64 entityId,
                                 eavg_u64 attributeId)
{
    eavgValueList *vl = u64_map_get(db->valuesByEntity, entityId);
    if (!vl) {
        vl = arena_alloc(&db->valueArena, sizeof *vl);
        vl->entityId = entityId;
        vl->values   = NULL;
        vl->count    = vl->cap = 0;
        u64_map_put(db->valuesByEntity, entityId, vl);
    }
    if (vl->count == vl->cap) {
        size_t newCap = vl->cap ? vl->cap * 2 : 4;
        vl->values    = arena_alloc(&db->valueArena,
                                    newCap * sizeof *vl->values);
        vl->cap       = newCap;
    }
    eavgValRec *rec = &vl->values[vl->count++];
    rec->id          = db->nextValueId++;
    rec->attributeId = attributeId;
    return rec;
}

#define DEFINE_ADD_VALUE_FN(NAME, TYPECHECK, FIELD, ASSIGN) \
eavgValRec *eavgDB_add##NAME##Value(eavgDB *db,                  \
    eavg_u64 entityId, eavg_u64 attributeId, TYPECHECK v)      \
{                                                               \
    LOCK_WR(db);                                                \
    eavgAttribute *at = eavgDB_findAttributeByIdNoLock(db, attributeId); \
    if (!at || at->dataType != EAVG_DATA_TYPE_##ASSIGN) {       \
        UNLOCK_WR(db); return NULL;                            \
    }                                                           \
    eavgValRec *r = add_value_rec(db, entityId, attributeId);  \
    r->data.FIELD = v;                                          \
    if (at->onValueAdded)                                       \
        at->onValueAdded(at, r, at->userData);                  \
    UNLOCK_WR(db);                                              \
    return r;                                                   \
}

DEFINE_ADD_VALUE_FN(Int, long, intValue, INT)
DEFINE_ADD_VALUE_FN(Double, double, doubleValue, DOUBLE)

eavgValRec *eavgDB_addStringValue(eavgDB *db,
                                  eavg_u64 entityId,
                                  eavg_u64 attributeId,
                                  const char *s)
{
    LOCK_WR(db);
    eavgAttribute *at = eavgDB_findAttributeByIdNoLock(db, attributeId);
    if (!at || at->dataType != EAVG_DATA_TYPE_STRING) {
        UNLOCK_WR(db); return NULL;
    }
    eavgValRec *r = add_value_rec(db, entityId, attributeId);
    r->data.stringValue = strdup_arena(&db->valueArena, s);
    if (at->onValueAdded) at->onValueAdded(at, r, at->userData);
    UNLOCK_WR(db);
    return r;
}

eavgValRec *eavgDB_addBinaryValue(eavgDB *db,
                                  eavg_u64 entityId,
                                  eavg_u64 attributeId,
                                  const unsigned char *buf,
                                  size_t len)
{
    LOCK_WR(db);
    eavgAttribute *at = eavgDB_findAttributeByIdNoLock(db, attributeId);
    if (!at || at->dataType != EAVG_DATA_TYPE_BINARY) {
        UNLOCK_WR(db); return NULL;
    }
    eavgValRec *r = add_value_rec(db, entityId, attributeId);
    unsigned char *b = arena_alloc(&db->valueArena, len);
    if (b && buf) memcpy(b, buf, len);
    r->data.binaryValue = b;
    if (at->onValueAdded) at->onValueAdded(at, r, at->userData);
    UNLOCK_WR(db);
    return r;
}

eavgValRec *eavgDB_addEntityRefValue(eavgDB *db,
                                     eavg_u64 entityId,
                                     eavg_u64 attributeId,
                                     eavg_u64 refId)
{
    LOCK_WR(db);
    eavgAttribute *at = eavgDB_findAttributeByIdNoLock(db, attributeId);
    if (!at || at->dataType != EAVG_DATA_TYPE_ENTITY) {
        UNLOCK_WR(db); return NULL;
    }
    eavgValRec *r = add_value_rec(db, entityId, attributeId);
    r->data.entityRef = refId;
    if (at->onValueAdded) at->onValueAdded(at, r, at->userData);
    UNLOCK_WR(db);
    return r;
}

eavgRelationType *
eavgDB_addRelationType(eavgDB *db, const char *name)
{
    LOCK_WR(db);
    eavgRelationType *rt = EAVG_RELTYPE_ALLOC(db);
    EAVG_ASSERT(rt);
    rt->id   = db->nextRelationTypeId++;
    rt->name = strdup_arena(&db->attributeArena, name);
    u64_map_put(db->relationTypesById, rt->id, rt);
    if (name) str_map_put(db->relationTypesByName, rt->name, rt);
    UNLOCK_WR(db);
    return rt;
}

eavgRelationType *
eavgDB_findRelationTypeById(eavgDB *db, eavg_u64 id)
{
    LOCK_RD(db);
    eavgRelationType *rt = u64_map_get(db->relationTypesById, id);
    UNLOCK_RD(db);
    return rt;
}

eavgRelationType *
eavgDB_findRelationTypeByName(eavgDB *db, const char *name)
{
    LOCK_RD(db);
    eavgRelationType *rt = str_map_get(db->relationTypesByName, name);
    UNLOCK_RD(db);
    return rt;
}

eavgEdgeRec *
eavgDB_addEdge(eavgDB *db,
               eavg_u64 src,
               eavg_u64 tgt,
               eavg_u64 relTypeId,
               double weight)
{
    const eavgRelationType *rel = eavgDB_findRelationTypeById(db, relTypeId);
    const char *label = rel ? strdup_arena_intern(&db->edgeArena, db->relationTypesByName, rel->name) : NULL;


    return eavgDB_addEdgeEx(db, src, tgt, relTypeId, weight,
                            EAVG_EDGE_DIR_OUT, label,
                            (uint64_t)(time(NULL)) * 1000);
}

eavgEdgeRec *
eavgDB_addEdgeEx(eavgDB *db,
                 eavg_u64    src,
                 eavg_u64    tgt,
                 eavg_u64    relTypeId,
                 double      weight,
                 eavgEdgeDir direction,
                 const char *label,
                 uint64_t    timestamp)
{
    LOCK_WR(db);
    eavgAdjList *fwd = u64_map_get(db->adjIndexBySource, src);
    if (!fwd) {
        fwd        = EAVG_ADJLIST_ALLOC(db);
        fwd->srcId = src;
        fwd->edges = NULL;
        fwd->count = fwd->cap = 0;
        u64_map_put(db->adjIndexBySource, src, fwd);
    }
    if (fwd->count == fwd->cap) {
        size_t newCap = fwd->cap ? fwd->cap * 2 : 4;
        fwd->edges     = arena_alloc(&db->edgeArena, newCap * sizeof *fwd->edges);
        fwd->cap       = newCap;
    }

    eavgAdjList *rev = u64_map_get(db->reverseAdjIndexByTarget, tgt);
    if (!rev) {
        rev        = EAVG_ADJLIST_ALLOC(db);
        rev->srcId = tgt;
        rev->edges = NULL;
        rev->count = rev->cap = 0;
        u64_map_put(db->reverseAdjIndexByTarget, tgt, rev);
    }
    if (rev->count == rev->cap) {
        size_t newCap = rev->cap ? rev->cap * 2 : 4;
        rev->edges     = arena_alloc(&db->edgeArena, newCap * sizeof *rev->edges);
        rev->cap       = newCap;
    }

    eavgEdgeRec *e = &fwd->edges[fwd->count++];
    e->id             = db->nextEdgeId++;
    e->relationTypeId = relTypeId;
    e->targetEntity   = tgt;
    e->weight         = weight;
    e->direction      = direction;
    e->label = label ? strdup_arena(&db->edgeArena, label) : NULL;
    e->timestamp      = timestamp;

    rev->edges[rev->count++] = *e;
    UNLOCK_WR(db);
    return e;
}

eavgAdjList *eavgDB_getAdjList(eavgDB *db, eavg_u64 src) {
    LOCK_RD(db);
    eavgAdjList *al = u64_map_get(db->adjIndexBySource, src);
    UNLOCK_RD(db);
    return al;
}
eavgAdjList *eavgDB_getReverseAdjList(eavgDB *db, eavg_u64 tgt) {
    LOCK_RD(db);
    eavgAdjList *al = u64_map_get(db->reverseAdjIndexByTarget, tgt);
    UNLOCK_RD(db);
    return al;
}

void eavgDB_forEachEdge(eavgDB *db,
                        eavgEdgeCallback cb,
                        void *userData)
{
    LOCK_RD(db);
    size_t cap = db->adjIndexBySource->capacity;
    for (size_t i = 0; i < cap; i++) {
        eavgAdjList *al = (eavgAdjList*)db->adjIndexBySource->values[i];
        if (!al) continue;
        for (size_t j = 0; j < al->count; j++) {
            if (cb(db, &al->edges[j], userData) != 0) {
                UNLOCK_RD(db);
                return;
            }
        }
    }
    UNLOCK_RD(db);
}

int eavgDB_removeRelationType(eavgDB *db, eavg_u64 id) {
    LOCK_WR(db);
    eavgRelationType *rt = u64_map_get(db->relationTypesById, id);
    if (!rt) { UNLOCK_WR(db); return -1; }

    u64_map_remove(db->relationTypesById, id);
    str_map_remove(db->relationTypesByName, rt->name);
    UNLOCK_WR(db);
    return 0;
}

int eavgDB_removeValue(eavgDB *db, eavg_u64 id) {
    LOCK_WR(db);
    for (size_t i = 0; i < db->valuesByEntity->capacity; i++) {
        eavgValueList *vl = (eavgValueList*)db->valuesByEntity->values[i];
        if (!vl) continue;
        for (size_t j = 0; j < vl->count; j++) {
            if (vl->values[j].id == id) {
                for (size_t k = j + 1; k < vl->count; k++) {
                    vl->values[k-1] = vl->values[k];
                }
                vl->count--;
                UNLOCK_WR(db);
                return 0;
            }
        }
    }
    UNLOCK_WR(db);
    return -1;
}

int
eavgDB_removeEntity(eavgDB *db, eavg_u64 entityId)
{
    LOCK_WR(db);
    eavgEntity *e = u64_map_get(db->entitiesById, entityId);
    if (!e) {
        UNLOCK_WR(db);
        return -1;
    }

    u64_map_remove(db->valuesByEntity, entityId);

    u64_map_remove(db->adjIndexBySource, entityId);
    u64_map_remove(db->reverseAdjIndexByTarget, entityId);

    for (size_t i = 0; i < db->adjIndexBySource->capacity; i++) {
        eavgAdjList *al = (eavgAdjList*)db->adjIndexBySource->values[i];
        if (!al) continue;
        size_t w = 0;
        for (size_t j = 0; j < al->count; j++) {
            if (al->edges[j].targetEntity != entityId) {
                al->edges[w++] = al->edges[j];
            }
        }
        al->count = w;
    }

    u64_map_remove(db->entitiesById, entityId);
    if (e->name) {
        str_map_remove(db->entitiesByName, e->name);
    }

    UNLOCK_WR(db);
    return 0;
}

int eavgDB_removeEdge(eavgDB *db, eavg_u64 id) {
    int removed = 0;
    LOCK_WR(db);
    for (size_t i = 0; i < db->adjIndexBySource->capacity; i++) {
        eavgAdjList *al = (eavgAdjList*)db->adjIndexBySource->values[i];
        if (!al) continue;
        for (size_t j = 0; j < al->count; j++) {
            if (al->edges[j].id == id) {
                for (size_t k = j+1; k < al->count; k++)
                    al->edges[k-1] = al->edges[k];
                al->count--;
                removed++;
                break;
            }
        }
    }
    for (size_t i = 0; i < db->reverseAdjIndexByTarget->capacity; i++) {
        eavgAdjList *al = (eavgAdjList*)db->reverseAdjIndexByTarget->values[i];
        if (!al) continue;
        for (size_t j = 0; j < al->count; j++) {
            if (al->edges[j].id == id) {
                for (size_t k = j+1; k < al->count; k++)
                    al->edges[k-1] = al->edges[k];
                al->count--;
                removed++;
                break;
            }
        }
    }
    UNLOCK_WR(db);
    return removed ? 0 : -1;
}

const char *eavgValRec_getString(const eavgValRec *rec) {
    return rec->data.stringValue;
}

long eavgValRec_getInt(const eavgValRec *rec) {
    return rec->data.intValue;
}

eavg_u64 eavgValRec_getEntityRef(const eavgValRec *rec) {
    return rec->data.entityRef;
}

int eavgDB_updateEdgeLabel(eavgDB *db, eavg_u64 edgeId, const char *newLabel) {
    int updated = 0;
    LOCK_WR(db);
    char *copy = newLabel
        ? strdup_arena(&db->edgeArena, newLabel)
        : NULL;

    for (size_t i = 0; i < db->adjIndexBySource->capacity; i++) {
        eavgAdjList *al = (eavgAdjList*)db->adjIndexBySource->values[i];
        if (!al) continue;
        for (size_t j = 0; j < al->count; j++) {
            if (al->edges[j].id == edgeId) {
                al->edges[j].label = copy;
                updated++;
                break;
            }
        }
    }
    for (size_t i = 0; i < db->reverseAdjIndexByTarget->capacity; i++) {
        eavgAdjList *al = (eavgAdjList*)db->reverseAdjIndexByTarget->values[i];
        if (!al) continue;
        for (size_t j = 0; j < al->count; j++) {
            if (al->edges[j].id == edgeId) {
                al->edges[j].label = copy;
                updated++;
                break;
            }
        }
    }
    UNLOCK_WR(db);
    return updated ? 0 : -1;
}

int eavgDB_updateEdgeWeight(eavgDB *db, eavg_u64 edgeId, double newWeight) {
    int updated = 0;
    LOCK_WR(db);
    for (size_t i = 0; i < db->adjIndexBySource->capacity; i++) {
        eavgAdjList *al = (eavgAdjList*)db->adjIndexBySource->values[i];
        if (!al) continue;
        for (size_t j = 0; j < al->count; j++) {
            if (al->edges[j].id == edgeId) {
                al->edges[j].weight = newWeight;
                updated++;
                break;
            }
        }
    }
    for (size_t i = 0; i < db->reverseAdjIndexByTarget->capacity; i++) {
        eavgAdjList *al = (eavgAdjList*)db->reverseAdjIndexByTarget->values[i];
        if (!al) continue;
        for (size_t j = 0; j < al->count; j++) {
            if (al->edges[j].id == edgeId) {
                al->edges[j].weight = newWeight;
                updated++;
                break;
            }
        }
    }
    UNLOCK_WR(db);
    return updated ? 0 : -1;
}

eavgEntity **eavgDB_findEntitiesByType(eavgDB *db,
                                       eavg_u32 typeId,
                                       size_t *outCount)
{
    LOCK_RD(db);
    size_t cap = db->entitiesById->capacity;
    size_t cnt = 0;

    for (size_t i = 0; i < cap; i++) {
        eavgEntity *e = (eavgEntity*)db->entitiesById->values[i];
        if (e && e->typeId == typeId) cnt++;
    }

    eavgEntity **results = NULL;
    if (cnt) {
        results = malloc(cnt * sizeof *results);
        size_t idx = 0;
        for (size_t i = 0; i < cap; i++) {
            eavgEntity *e = (eavgEntity*)db->entitiesById->values[i];
            if (e && e->typeId == typeId) {
                results[idx++] = e;
            }
        }
    }
    UNLOCK_RD(db);

    *outCount = cnt;
    return results;
}

eavgEdgeRec *eavgDB_getFilteredEdges(
    eavgDB *db,
    eavg_u64 entityId,
    eavgEdgeDir dir,
    eavgEdgeFilter filter,
    void *userData,
    size_t *outCount)
{
    LOCK_RD(db);

    eavgAdjList *fwd = NULL, *rev = NULL;
    if (dir & EAVG_EDGE_DIR_OUT) {
        fwd = u64_map_get(db->adjIndexBySource, entityId);
    }
    if (dir & EAVG_EDGE_DIR_IN) {
        rev = u64_map_get(db->reverseAdjIndexByTarget, entityId);
    }

    eavgEdgeRec *results = NULL;
    size_t       count   = 0;

    #define TRY_APPEND(edge_ptr)                                       \
        do {                                                           \
            if (!filter || filter(edge_ptr, userData)) {              \
                eavgEdgeRec *tmp =                                     \
                  realloc(results, (count + 1) * sizeof *results);    \
                if (!tmp) {                                            \
                    free(results);                                     \
                    UNLOCK_RD(db);                                     \
                    *outCount = 0;                                     \
                    return NULL;                                       \
                }                                                      \
                results = tmp;                                         \
                results[count++] = *(edge_ptr);                       \
            }                                                          \
        } while (0)

    if (fwd) {
        for (size_t i = 0; i < fwd->count; i++) {
            TRY_APPEND(&fwd->edges[i]);
        }
    }
    if (rev) {
        for (size_t i = 0; i < rev->count; i++) {
            TRY_APPEND(&rev->edges[i]);
        }
    }

    #undef TRY_APPEND

    UNLOCK_RD(db);

    *outCount = count;
    return results;
}

static int write_all(FILE *f, const void *buf, size_t sz) {
    return fwrite(buf, 1, sz, f) == sz ? 0 : -1;
}
static int read_all(FILE *f, void *buf, size_t sz) {
    return fread(buf, 1, sz, f) == sz ? 0 : -1;
}

static int write_u32(FILE *f, uint32_t v) {
    uint32_t le = v;
    return write_all(f, &le, sizeof le);
}
static int write_u64(FILE *f, uint64_t v) {
    uint64_t le = v;
    return write_all(f, &le, sizeof le);
}
static int read_u32(FILE *f, uint32_t *out) {
    return read_all(f, out, sizeof *out);
}
static int read_u64(FILE *f, uint64_t *out) {
    return read_all(f, out, sizeof *out);
}

static int write_cstr(FILE *f, const char *s) {
    uint32_t len = s ? (uint32_t)strlen(s) : 0;
    if (write_u32(f, len)) return -1;
    return len ? write_all(f, s, len) : 0;
}
static char *read_cstr(FILE *f, Arena *arena) {
    uint32_t len;
    if (read_u32(f, &len)) return NULL;
    if (len == 0) return NULL;
    char *buf = arena_alloc(arena, len+1);
    if (!buf) return NULL;
    if (read_all(f, buf, len)) { return NULL; }
    buf[len] = '\0';
    return buf;
}

int eavgDB_save(eavgDB *db, const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) return -1;
    
    if (write_all(f, EAVG_PERS_MAGIC, 8) ||
        write_u32(f, EAVG_PERS_VERSION)) {
        fclose(f);
        return -1;
    }

    LOCK_RD(db);
    
    size_t ecount = 0;
    for (size_t i = 0; i < db->entitiesById->capacity; i++) {
        if (db->entitiesById->values[i]) ++ecount;
    }
    if (write_u64(f, ecount)) goto fail;
    for (size_t i = 0; i < db->entitiesById->capacity; i++) {
        eavgEntity *e = (eavgEntity*)db->entitiesById->values[i];
        if (!e) continue;
        write_u64(f, e->id);
        write_u32(f, e->typeId);
        write_cstr(f, e->name);
    }

    size_t acount = 0;
    for (size_t i = 0; i < db->attributesById->capacity; i++) {
        if (db->attributesById->values[i]) ++acount;
    }
    write_u64(f, acount);
    for (size_t i = 0; i < db->attributesById->capacity; i++) {
        eavgAttribute *a = (eavgAttribute*)db->attributesById->values[i];
        if (!a) continue;
        write_u64(f, a->id);
        write_u32(f, a->dataType);
        write_cstr(f, a->name);
    }

    size_t rcount = 0;
    for (size_t i = 0; i < db->relationTypesById->capacity; i++) {
        if (db->relationTypesById->values[i]) ++rcount;
    }
    write_u64(f, rcount);
    for (size_t i = 0; i < db->relationTypesById->capacity; i++) {
        eavgRelationType *r = (eavgRelationType*)db->relationTypesById->values[i];
        if (!r) continue;
        write_u64(f, r->id);
        write_cstr(f, r->name);
    }

    size_t vcount = 0;
    for (size_t i = 0; i < db->valuesByEntity->capacity; i++) {
        eavgValueList *vl = (eavgValueList*)db->valuesByEntity->values[i];
        if (!vl) continue;
        vcount += vl->count;
    }
    write_u64(f, vcount);
    for (size_t i = 0; i < db->valuesByEntity->capacity; i++) {
        eavgValueList *vl = (eavgValueList*)db->valuesByEntity->values[i];
        if (!vl) continue;
        for (size_t j = 0; j < vl->count; j++) {
            eavgValRec *r = &vl->values[j];
            write_u64(f, r->id);
            write_u64(f, vl->entityId);
            write_u64(f, r->attributeId);
            
            eavgAttribute *at = u64_map_get(db->attributesById, r->attributeId);
            write_u32(f, at->dataType);
            switch (at->dataType) {
            case EAVG_DATA_TYPE_INT:
                write_all(f, &r->data.intValue, sizeof r->data.intValue);
                break;
            case EAVG_DATA_TYPE_DOUBLE:
                write_all(f, &r->data.doubleValue, sizeof r->data.doubleValue);
                break;
            case EAVG_DATA_TYPE_STRING:
                write_cstr(f, r->data.stringValue);
                break;
            case EAVG_DATA_TYPE_ENTITY:
                write_u64(f, r->data.entityRef);
                break;
            default:
                /* skip unsupported types */
                break;
            }
        }
    }

    size_t edgeCount = 0;
    for (size_t i = 0; i < db->adjIndexBySource->capacity; i++) {
        eavgAdjList *al = (eavgAdjList*)db->adjIndexBySource->values[i];
        if (al) edgeCount += al->count;
    }
    write_u64(f, edgeCount);
    for (size_t i = 0; i < db->adjIndexBySource->capacity; i++) {
        eavgAdjList *al = (eavgAdjList*)db->adjIndexBySource->values[i];
        if (!al) continue;
        for (size_t j = 0; j < al->count; j++) {
            eavgEdgeRec *e = &al->edges[j];
            write_u64(f, e->id);
            write_u64(f, al->srcId);
            write_u64(f, e->targetEntity);
            write_u64(f, e->relationTypeId);
            write_all(f, &e->weight, sizeof e->weight);
            write_u32(f, (uint32_t)e->direction);
            write_u64(f, e->timestamp);
            write_cstr(f, e->label);
        }
    }

    UNLOCK_RD(db);
    fclose(f);
    return 0;

 fail:
    UNLOCK_RD(db);
    fclose(f);
    return -1;
}

eavgDB *eavgDB_load(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    char magic[8];
    if (read_all(f, magic, 8) != 0 ||
        memcmp(magic, EAVG_PERS_MAGIC, 8) != 0) {
        fclose(f);
        return NULL;
    }
    uint32_t version;
    if (read_u32(f, &version) != 0 || version != EAVG_PERS_VERSION) {
        fclose(f);
        return NULL;
    }

    eavgDB *db = eavgDB_create(128);
    if (!db) {
        fclose(f);
        return NULL;
    }

    LOCK_WR(db);

    uint64_t ecount;
    if (read_u64(f, &ecount) != 0) goto fail;
    for (uint64_t i = 0; i < ecount; i++) {
        uint64_t id;
        uint32_t typeId;
        char *name;
        if (read_u64(f, &id) != 0 ||
            read_u32(f, &typeId) != 0) goto fail;
        name = read_cstr(f, &db->entityArena);
        eavgEntity *e = EAVG_ENTITY_ALLOC(db);
        e->id     = id;
        e->typeId = typeId;
        e->name   = name;
        u64_map_put(db->entitiesById, id, e);
        if (name) str_map_put(db->entitiesByName, name, e);
        if (id >= db->nextEntityId) db->nextEntityId = id + 1;
    }

    uint64_t acount;
    if (read_u64(f, &acount) != 0) goto fail;
    for (uint64_t i = 0; i < acount; i++) {
        uint64_t id;
        uint32_t dataType;
        char *name;
        if (read_u64(f, &id) != 0 ||
            read_u32(f, &dataType) != 0) goto fail;
        name = read_cstr(f, &db->attributeArena);
        eavgAttribute *a = EAVG_ATTR_ALLOC(db);
        a->id            = id;
        a->dataType      = dataType;
        a->name          = name;
        a->onValueAdded  = NULL;
        a->userData      = NULL;
        u64_map_put(db->attributesById, id, a);
        if (name) str_map_put(db->attributesByName, name, a);
        if (id >= db->nextAttributeId) db->nextAttributeId = id + 1;
    }

    uint64_t rcount;
    if (read_u64(f, &rcount) != 0) goto fail;
    for (uint64_t i = 0; i < rcount; i++) {
        uint64_t id;
        char *name;
        if (read_u64(f, &id) != 0) goto fail;
        name = read_cstr(f, &db->attributeArena);
        eavgRelationType *rt = EAVG_RELTYPE_ALLOC(db);
        rt->id   = id;
        rt->name = name;
        u64_map_put(db->relationTypesById, id, rt);
        if (name) str_map_put(db->relationTypesByName, name, rt);
        if (id >= db->nextRelationTypeId) db->nextRelationTypeId = id + 1;
    }

    uint64_t vcount;
    if (read_u64(f, &vcount) != 0) goto fail;
    for (uint64_t i = 0; i < vcount; i++) {
        uint64_t recId, entityId, attributeId;
        uint32_t dtype;
        if (read_u64(f, &recId)     != 0 ||
            read_u64(f, &entityId)  != 0 ||
            read_u64(f, &attributeId) != 0 ||
            read_u32(f, &dtype)     != 0) goto fail;
            
        eavgValueList *vl = u64_map_get(db->valuesByEntity, entityId);
        if (!vl) {
            vl = arena_alloc(&db->valueArena, sizeof *vl);
            vl->entityId = entityId;
            vl->values   = NULL;
            vl->count    = vl->cap = 0;
            u64_map_put(db->valuesByEntity, entityId, vl);
        }
        if (vl->count == vl->cap) {
            size_t newCap = vl->cap ? vl->cap * 2 : 4;
            vl->values    = arena_alloc(&db->valueArena,
                                        newCap * sizeof *vl->values);
            vl->cap       = newCap;
        }
        eavgValRec *r = &vl->values[vl->count++];
        r->id          = recId;
        r->attributeId = attributeId;
        
        switch (dtype) {
          case EAVG_DATA_TYPE_INT:
            read_all(f, &r->data.intValue, sizeof r->data.intValue);
            break;
          case EAVG_DATA_TYPE_DOUBLE:
            read_all(f, &r->data.doubleValue, sizeof r->data.doubleValue);
            break;
          case EAVG_DATA_TYPE_STRING:
            r->data.stringValue = read_cstr(f, &db->valueArena);
            break;
          case EAVG_DATA_TYPE_ENTITY:
            read_u64(f, &r->data.entityRef);
            break;
          default:
            /* skip unknown */
            break;
        }
        if (recId >= db->nextValueId) db->nextValueId = recId + 1;
    }

    uint64_t edgeCount;
    if (read_u64(f, &edgeCount) != 0) goto fail;
    for (uint64_t i = 0; i < edgeCount; i++) {
        uint64_t edgeId, srcId, tgtId, relTypeId, ts;
        double   weight;
        uint32_t dir;
        char    *label;
        if (read_u64(f, &edgeId)     != 0 ||
            read_u64(f, &srcId)      != 0 ||
            read_u64(f, &tgtId)      != 0 ||
            read_u64(f, &relTypeId)  != 0 ||
            read_all(f, &weight, sizeof weight) != 0 ||
            read_u32(f, &dir)        != 0 ||
            read_u64(f, &ts)         != 0) goto fail;
        label = read_cstr(f, &db->edgeArena);

        eavgAdjList *fwd = u64_map_get(db->adjIndexBySource, srcId);
        if (!fwd) {
            fwd       = EAVG_ADJLIST_ALLOC(db);
            fwd->srcId = srcId;
            fwd->edges = NULL;
            fwd->count = fwd->cap = 0;
            u64_map_put(db->adjIndexBySource, srcId, fwd);
        }
        if (fwd->count == fwd->cap) {
            size_t nc = fwd->cap ? fwd->cap*2 : 4;
            fwd->edges = arena_alloc(&db->edgeArena, nc * sizeof *fwd->edges);
            fwd->cap   = nc;
        }
        eavgEdgeRec *e = &fwd->edges[fwd->count++];
        e->id             = edgeId;
        e->relationTypeId = relTypeId;
        e->targetEntity   = tgtId;
        e->weight         = weight;
        e->direction      = (eavgEdgeDir)dir;
        e->timestamp      = ts;
        e->label          = label;

        eavgAdjList *rev = u64_map_get(db->reverseAdjIndexByTarget, tgtId);
        if (!rev) {
            rev       = EAVG_ADJLIST_ALLOC(db);
            rev->srcId = tgtId;
            rev->edges = NULL;
            rev->count = rev->cap = 0;
            u64_map_put(db->reverseAdjIndexByTarget, tgtId, rev);
        }
        if (rev->count == rev->cap) {
            size_t nc = rev->cap ? rev->cap*2 : 4;
            rev->edges = arena_alloc(&db->edgeArena, nc * sizeof *rev->edges);
            rev->cap   = nc;
        }
        rev->edges[rev->count++] = *e;

        if (edgeId >= db->nextEdgeId) db->nextEdgeId = edgeId + 1;
    }

    UNLOCK_WR(db);
    fclose(f);
    return db;

fail:
    UNLOCK_WR(db);
    fclose(f);
    eavgDB_destroy(db);
    return NULL;
}
