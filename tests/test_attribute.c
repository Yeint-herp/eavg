#include "tests.h"
#include "../eavg.h"

TEST(test_add_and_find_attribute) {
    eavgDB *db = eavgDB_create(16);
    eavgAttribute *a = eavgDB_addAttribute(db, "attr", EAVG_DATA_TYPE_INT);
    ASSERT(a != NULL);
    ASSERT(a->dataType == EAVG_DATA_TYPE_INT);
    ASSERT(a->id != 0);

    eavgAttribute *byId   = eavgDB_findAttributeById(db, a->id);
    eavgAttribute *byName = eavgDB_findAttributeByName(db, "attr");
    ASSERT(byId   == a);
    ASSERT(byName == a);

    eavgDB_destroy(db);
}

