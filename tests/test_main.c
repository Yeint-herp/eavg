#include "tests.h"

extern void test_create_and_destroy(void);
extern void test_entity_add_lookup(void);

extern void test_add_and_find_attribute(void);
extern void test_add_and_find_relation_type(void);
extern void test_add_int_double_string_binary_entityref(void);
extern void test_edges_and_traversal(void);

int main(void) {
    RUN(test_create_and_destroy);
    RUN(test_entity_add_lookup);

    RUN(test_add_and_find_attribute);
    RUN(test_add_and_find_relation_type);

    RUN(test_add_int_double_string_binary_entityref);

    RUN(test_edges_and_traversal);

    return 0;
}

