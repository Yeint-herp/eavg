#include "tests.h"
#include "../eavg.h"

TEST(test_add_and_find_relation_type) {
    eavgDB *db = eavgDB_create(16);
    eavgRelationType *r = eavgDB_addRelationType(db, "rel");
    ASSERT(r != NULL);
    ASSERT(r->id != 0);

    eavgRelationType *byId   = eavgDB_findRelationTypeById(db, r->id);
    eavgRelationType *byName = eavgDB_findRelationTypeByName(db, "rel");
    ASSERT(byId   == r);
    ASSERT(byName == r);

    eavgDB_destroy(db);
}

