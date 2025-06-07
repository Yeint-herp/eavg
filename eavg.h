#ifndef EAVG_H
#define EAVG_H

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <pthread.h>

#include "hashmap.h"
#include "arena.h"

#define Borrows        	/* pointer is borrowed, not owned */
#define Owns           	/* pointer is owned by caller */
#define LT_db    	/* lifetime tied to the eavgDB instance */
#define LT_arena 	/* lifetime tied to the underlying Arena */

#ifndef NDEBUG
  #define EAVG_ASSERT(expr) assert(expr)
#else
  #define EAVG_ASSERT(expr) ((void)0)
#endif

typedef unsigned long  eavg_u64;
typedef unsigned int   eavg_u32;

#define STATIC_ASSERT(cond,msg) typedef char static_assertion_##msg[(cond)?1:-1]
STATIC_ASSERT(sizeof(eavg_u64)==8, must_be_8_bytes);
STATIC_ASSERT(sizeof(eavg_u32)==4, must_be_4_bytes);

#define EAVG_DATA_TYPE_INT      1
#define EAVG_DATA_TYPE_DOUBLE   2
#define EAVG_DATA_TYPE_STRING   3
#define EAVG_DATA_TYPE_BINARY   4
#define EAVG_DATA_TYPE_ENTITY   5


#define EAVG_ENTITY_ALLOC(db)    ((eavgEntity*)arena_alloc(&((db)->entityArena), sizeof(eavgEntity)))
#define EAVG_ATTR_ALLOC(db)      ((eavgAttribute*)arena_alloc(&((db)->attributeArena), sizeof(eavgAttribute)))
#define EAVG_RELTYPE_ALLOC(db)   ((eavgRelationType*)arena_alloc(&((db)->attributeArena), sizeof(eavgRelationType)))
#define EAVG_ADJLIST_ALLOC(db)   ((eavgAdjList*)arena_alloc(&((db)->edgeArena), sizeof(eavgAdjList)))
#define EAVG_FREE_ENTITY(ptr)    arena_free(ptr)

struct eavgDB;
struct eavgValRec;
struct eavgAttribute;

/** A node in the graph.  
 *  typeId is a user-defined tag for classification.  
 *  name is owned by the db’s arena (borrowed to user). */
typedef struct {
    eavg_u64   id;
    eavg_u32   typeId;    /**< user-defined tag */
    char      *name;      /**< Borrows from entityArena */
} eavgEntity;

typedef struct eavgAttribute {
    eavg_u64    id;
    char       *name;       /**< Borrows from attributeArena */
    eavg_u32    dataType;
    void    (*onValueAdded)(struct eavgAttribute*, struct eavgValRec*, void*);
    void       *userData;
} eavgAttribute;

typedef union {
    long           intValue;
    double         doubleValue;
    char          *stringValue;
    unsigned char *binaryValue;
    eavg_u64       entityRef;
} eavgValueData;

typedef struct eavgValRec {
    eavg_u64        id;
    eavg_u64        attributeId;
    eavgValueData   data;
} eavgValRec;

typedef struct {
    eavg_u64   id;
    char      *name;      /**< Borrows from attributeArena */
} eavgRelationType;

/** Edge direction mask for filtering:  
 *   OUT  = outgoing from src  
 *   IN   = incoming to tgt  
 *   BOTH = both */
typedef enum {
    EAVG_EDGE_DIR_OUT  = 1,
    EAVG_EDGE_DIR_IN   = 2,
    EAVG_EDGE_DIR_BOTH = 3
} eavgEdgeDir;

typedef struct eavgEdgeRec {
    eavg_u64      id;
    eavg_u64      relationTypeId;
    eavg_u64      targetEntity;
    double        weight;
    eavgEdgeDir   direction;
    char         *label;
    uint64_t      timestamp;
} eavgEdgeRec;

typedef struct {
    eavg_u64      srcId;
    eavgEdgeRec  *edges;
    size_t        count, cap;
} eavgAdjList;

typedef struct {
    eavg_u64     entityId;
    eavgValRec  *values;
    size_t       count, cap;
} eavgValueList;

typedef int (*eavgEntityCallback)(struct eavgDB*, eavgEntity*, void*);
typedef int (*eavgEdgeCallback)(struct eavgDB*, eavgEdgeRec*, void*);

typedef struct eavgDB {
    u64_map   *entitiesById;
    str_map   *entitiesByName;
    u64_map   *attributesById;
    str_map   *attributesByName;
    u64_map   *relationTypesById;
    str_map   *relationTypesByName;
    u64_map   *valuesByEntity;
    u64_map   *adjIndexBySource;
    u64_map   *reverseAdjIndexByTarget;

    Arena      entityArena;
    Arena      attributeArena;
    Arena      valueArena;
    Arena      edgeArena;

    eavg_u64   nextEntityId;
    eavg_u64   nextAttributeId;
    eavg_u64   nextValueId;
    eavg_u64   nextRelationTypeId;
    eavg_u64   nextEdgeId;

    pthread_rwlock_t lock;
} eavgDB;

Owns eavgDB *eavgDB_create(size_t initial_capacity);
void    eavgDB_destroy(Owns eavgDB *db);

Owns eavgEntity *eavgDB_addEntity(        eavgDB*, eavg_u32 typeId, const char* name);
Borrows LT_db eavgEntity *eavgDB_findEntityById(    eavgDB*, eavg_u64 id);
Borrows LT_db eavgEntity *eavgDB_findEntityByName(  eavgDB*, const char* name);
int     eavgDB_removeEntity(eavgDB*, eavg_u64 entityId);
int 	eavgDB_removeRelationType(eavgDB*, eavg_u64 id);
int 	eavgDB_removeValue(eavgDB*, eavg_u64 id);
int 	eavgDB_removeEdge(eavgDB*, eavg_u64 id);

void    eavgDB_forEachEntity(               eavgDB*, eavgEntityCallback, void*);

Owns eavgAttribute *eavgDB_addAttribute(       eavgDB*, const char* name, eavg_u32 dataType);
Borrows LT_db eavgAttribute *eavgDB_findAttributeById(   eavgDB*, eavg_u64 id);
Borrows LT_db eavgAttribute *eavgDB_findAttributeByName( eavgDB*, const char* name);

eavgValRec *eavgDB_addIntValue(    eavgDB*, eavg_u64, eavg_u64, long);
eavgValRec *eavgDB_addDoubleValue( eavgDB*, eavg_u64, eavg_u64, double);
eavgValRec *eavgDB_addStringValue( eavgDB*, eavg_u64, eavg_u64, const char*);
eavgValRec *eavgDB_addBinaryValue( eavgDB*, eavg_u64, eavg_u64, const unsigned char*, size_t);
eavgValRec *eavgDB_addEntityRefValue(eavgDB*, eavg_u64, eavg_u64, eavg_u64);

const char *eavgValRec_getString(const eavgValRec *rec);
long        eavgValRec_getInt(const eavgValRec *rec);
eavg_u64    eavgValRec_getEntityRef(const eavgValRec *rec);

int eavgDB_updateEdgeLabel(eavgDB*, eavg_u64 edgeId, const char *newLabel);
int eavgDB_updateEdgeWeight(eavgDB*, eavg_u64 edgeId, double newWeight);

eavgEntity **eavgDB_findEntitiesByType(eavgDB*, eavg_u32 typeId, size_t *outCount);

Owns eavgRelationType *eavgDB_addRelationType(       eavgDB*, const char* name);
Borrows LT_db eavgRelationType *eavgDB_findRelationTypeById(   eavgDB*, eavg_u64 id);
Borrows LT_db eavgRelationType *eavgDB_findRelationTypeByName( eavgDB*, const char* name);

eavgEdgeRec *eavgDB_addEdge(   eavgDB*, eavg_u64 src, eavg_u64 tgt, eavg_u64 relTypeId, double weight);
eavgEdgeRec *eavgDB_addEdgeEx( eavgDB*, eavg_u64 src, eavg_u64 tgt, eavg_u64 relTypeId,
                               double weight, eavgEdgeDir direction, const char* label, uint64_t timestamp);

Borrows LT_db eavgAdjList *eavgDB_getAdjList(        eavgDB*, eavg_u64 src);
Borrows LT_db eavgAdjList *eavgDB_getReverseAdjList( eavgDB*, eavg_u64 tgt);
void eavgDB_forEachEdge(           eavgDB*, eavgEdgeCallback, void*);

/* unsafe */

static inline eavgEntity  *eavgDB_findEntityByIdNoLock(const eavgDB *db, eavg_u64 id) {
    return (eavgEntity*)u64_map_get(db->entitiesById, id);
}
static inline eavgEntity  *eavgDB_findEntityByNameNoLock(const eavgDB *db, const char *name) {
    return (eavgEntity*)str_map_get(db->entitiesByName, name);
}
static inline eavgAttribute*eavgDB_findAttributeByIdNoLock(const eavgDB *db, eavg_u64 id) {
    return (eavgAttribute*)u64_map_get(db->attributesById, id);
}
static inline eavgAdjList *eavgDB_getAdjListNoLock(const eavgDB *db, eavg_u64 src) {
    return (eavgAdjList*)u64_map_get(db->adjIndexBySource, src);
}
static inline eavgAdjList *eavgDB_getReverseAdjListNoLock(const eavgDB *db, eavg_u64 tgt) {
    return (eavgAdjList*)u64_map_get(db->reverseAdjIndexByTarget, tgt);
}

#endif /* EAVG_H */

