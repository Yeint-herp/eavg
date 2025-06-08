#include "tests.h"
#include <unistd.h>
#include <libgen.h>
#include <string.h>

extern void test_create_and_destroy(void);
extern void test_entity_add_lookup(void);

extern void test_add_and_find_attribute(void);
extern void test_add_and_find_relation_type(void);
extern void test_add_int_double_string_binary_entityref(void);
extern void test_edges_and_traversal(void);

extern void test_save_load_empty_db(void);
extern void test_save_load_simple_graph(void);

int main(int argc, char **argv) {
    (void)argc;
    if (argv[0]) {
        char pathbuf[1024];
        strncpy(pathbuf, argv[0], sizeof(pathbuf) - 1);
        pathbuf[sizeof(pathbuf) - 1] = 0;

        char *dir = dirname(pathbuf);
        if (chdir(dir) != 0) {
            perror("chdir");
            return 1;
        }
    }

    RUN(test_create_and_destroy);
    RUN(test_entity_add_lookup);

    RUN(test_add_and_find_attribute);
    RUN(test_add_and_find_relation_type);

    RUN(test_add_int_double_string_binary_entityref);

    RUN(test_edges_and_traversal);
    RUN(test_save_load_empty_db);
    RUN(test_save_load_simple_graph);

    return 0;
}

