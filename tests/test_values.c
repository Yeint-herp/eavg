#include "tests.h"
#include "../eavg.h"
#include <string.h>

TEST(test_add_int_double_string_binary_entityref) {
    eavgDB *db = eavgDB_create(16);

    eavgEntity *ent = eavgDB_addEntity(db, 1, "E");
    ASSERT(ent);

    eavgAttribute *iA = eavgDB_addAttribute(db, "aint", EAVG_DATA_TYPE_INT);
    eavgAttribute *dA = eavgDB_addAttribute(db, "adbl", EAVG_DATA_TYPE_DOUBLE);
    eavgAttribute *sA = eavgDB_addAttribute(db, "astr", EAVG_DATA_TYPE_STRING);
    eavgAttribute *bA = eavgDB_addAttribute(db, "abin", EAVG_DATA_TYPE_BINARY);
    eavgAttribute *eA = eavgDB_addAttribute(db, "aent", EAVG_DATA_TYPE_ENTITY);

    eavgValRec *v1 = eavgDB_addIntValue(db, ent->id, iA->id, 123);
    ASSERT(v1); ASSERT(v1->data.intValue == 123);

    eavgValRec *v2 = eavgDB_addDoubleValue(db, ent->id, dA->id, 3.1415);
    ASSERT(v2); ASSERT(v2->data.doubleValue == 3.1415);

    eavgValRec *v3 = eavgDB_addStringValue(db, ent->id, sA->id, "hello");
    ASSERT(v3); ASSERT(strcmp(v3->data.stringValue, "hello") == 0);

    unsigned char blob[] = { 9, 8, 7 };
    eavgValRec *v4 = eavgDB_addBinaryValue(db, ent->id, bA->id, blob, sizeof(blob));
    ASSERT(v4);
    for (size_t i = 0; i < sizeof(blob); i++) {
        ASSERT(v4->data.binaryValue[i] == blob[i]);
    }

    eavgValRec *v5 = eavgDB_addEntityRefValue(db, ent->id, eA->id, ent->id);
    ASSERT(v5); ASSERT(v5->data.entityRef == ent->id);

    eavgDB_destroy(db);
}

