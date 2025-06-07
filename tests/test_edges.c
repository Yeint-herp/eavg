#include "tests.h"
#include "../eavg.h"

static int   entity_cb_count;
static int   edge_cb_count;

static int entity_cb(eavgDB *db, eavgEntity *e, void *ud) {
    (void)db; (void)ud; (void)e;
    entity_cb_count++;
    return 0;
}

static int edge_cb(eavgDB *db, eavgEdgeRec *er, void *ud) {
    (void)db; (void)ud; (void)er;
    edge_cb_count++;
    return 0;
}

TEST(test_edges_and_traversal) {
    eavgDB *db = eavgDB_create(16);

    eavgEntity *src = eavgDB_addEntity(db, 0, "S");
    eavgEntity *tgt = eavgDB_addEntity(db, 0, "T");

    eavgRelationType *rt = eavgDB_addRelationType(db, "rel");

    eavgEdgeRec *er = eavgDB_addEdge(db, src->id, tgt->id, rt->id, 2.5);
    ASSERT(er != NULL);

    eavgAdjList *out = eavgDB_getAdjList(db, src->id);
    ASSERT(out != NULL && out->count == 1);
    ASSERT(out->edges[0].id == er->id);

    eavgAdjList *in = eavgDB_getReverseAdjList(db, tgt->id);
    ASSERT(in != NULL && in->count == 1);
    ASSERT(in->edges[0].id == er->id);

    entity_cb_count = 0;
    eavgDB_forEachEntity(db, entity_cb, NULL);
    ASSERT(entity_cb_count == 2);

    edge_cb_count = 0;
    eavgDB_forEachEdge(db, edge_cb, NULL);
    ASSERT(edge_cb_count == 1);

    eavgDB_destroy(db);
}

