#ifndef LAYER1_TESTS_H
#define LAYER1_TESTS_H

#include "../config.h"

/*
 * YAML-style print hierarchy: level 0 = suite, 1 = test, 2 = detail.
 * Indent is computed from level (TEST_LEVEL_*, TEST_INDENT_SPACES_PER_LEVEL in config.h).
 */

/* Tag strings (with ANSI codes) for test output */
#define TEST_TAG_INFO   "\033[1;34m[ INFO ]\033[0m"
#define TEST_TAG_OK     "\033[1;32m[  OK  ]\033[0m"
#define TEST_TAG_FAIL   "\033[1;31m[ FAIL ]\033[0m"
#define TEST_TAG_RUN    "\033[1;33m[ RUN ]\033[0m"

/** Message with specified hierarchy level and optional tag (NULL = no tag). */
struct test_print_msg {
	int level;
	const char *tag;
};

void test_print_prefix(const struct test_print_msg *m);

void run_layer1_tests(void);

#endif