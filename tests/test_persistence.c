#include "tests.h"
#include "../eavg.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static int countEntitiesCB(eavgDB *db, eavgEntity *e, void *ud) {
    (void)db; (void)e;
    size_t *cnt = ud;
    (*cnt)++;
    return 0;
}
static int countEdgesCB(eavgDB *db, eavgEdgeRec *e, void *ud) {
    (void)db; (void)e;
    size_t *cnt = ud;
    (*cnt)++;
    return 0;
}

TEST(test_save_load_empty_db) {
    const char *fname = "test_empty.db";
    
    eavgDB *db = eavgDB_create(4);
    ASSERT(db);
    ASSERT(eavgDB_save(db, fname) == 0);
    eavgDB_destroy(db);

    eavgDB *db2 = eavgDB_load(fname);
    ASSERT(db2);

    size_t ec = 0;
    eavgDB_forEachEntity(db2, countEntitiesCB, &ec);
    ASSERT(ec == 0);

    size_t edc = 0;
    eavgDB_forEachEdge(db2, countEdgesCB, &edc);
    ASSERT(edc == 0);

    eavgDB_destroy(db2);
}

TEST(test_save_load_simple_graph) {
    const char *fname = "test_simple.db";
    eavgDB *db = eavgDB_create(8);
    ASSERT(db);

    eavgEntity *n1 = eavgDB_addEntity(db, 7, "NodeA");
    ASSERT(n1 && n1->id == 1);
    eavgEntity *n2 = eavgDB_addEntity(db, 7, "NodeB");
    ASSERT(n2 && n2->id == 2);

    eavgAttribute *attr = eavgDB_addAttribute(db, "label", EAVG_DATA_TYPE_STRING);
    ASSERT(attr && attr->id == 1);
    eavgValRec *vr = eavgDB_addStringValue(db, n1->id, attr->id, "hello");
    ASSERT(vr && strcmp(eavgValRec_getString(vr), "hello") == 0);

    eavgRelationType *rt = eavgDB_addRelationType(db, "connects");
    ASSERT(rt && rt->id == 1);
    eavgEdgeRec *e = eavgDB_addEdge(db, n1->id, n2->id, rt->id, 3.14);
    ASSERT(e && e->id == 1);

    ASSERT(eavgDB_save(db, fname) == 0);
    eavgDB_destroy(db);

    eavgDB *db2 = eavgDB_load(fname);
    ASSERT(db2);

    eavgEntity *n1b = eavgDB_findEntityByName(db2, "NodeA");
    ASSERT(n1b && n1b->typeId == 7 && strcmp(n1b->name, "NodeA") == 0);
    eavgEntity *n2b = eavgDB_findEntityByName(db2, "NodeB");
    ASSERT(n2b && n2b->id == 2);

    eavgAttribute *attrb = eavgDB_findAttributeByName(db2, "label");
    ASSERT(attrb && attrb->dataType == EAVG_DATA_TYPE_STRING);

    eavgRelationType *rtb = eavgDB_findRelationTypeByName(db2, "connects");
    ASSERT(rtb && rtb->id == rt->id);

    size_t edcount = 0;
    eavgDB_forEachEdge(db2, countEdgesCB, &edcount);
    ASSERT(edcount == 1);

    eavgAdjList *alist = eavgDB_getAdjList(db2, n1b->id);
    ASSERT(alist && alist->count == 1);
    eavgEdgeRec *e2 = &alist->edges[0];
    ASSERT(e2->targetEntity == n2b->id);
    ASSERT(e2->relationTypeId == rtb->id);
    ASSERT(fabs(e2->weight - 3.14) < 1e-9);

    eavgDB_destroy(db2);
}
