#include "tests.h"
#include "../eavg.h"

TEST(test_create_and_destroy) {
    eavgDB *db = eavgDB_create(64);
    ASSERT(db != NULL);
    eavgDB_destroy(db);
}

TEST(test_entity_add_lookup) {
    eavgDB *db = eavgDB_create(64);
    eavgEntity *e = eavgDB_addEntity(db, 42, "Test");
    ASSERT(e != NULL);
    ASSERT(e->typeId == 42);
    ASSERT(eavgDB_findEntityById(db, e->id) == e);
    ASSERT(eavgDB_findEntityByName(db, "Test") == e);
    eavgDB_destroy(db);
}

