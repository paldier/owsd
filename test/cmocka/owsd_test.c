#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <json-c/json.h>
#include <libwebsockets.h>

#include "common.h"
#include "config.h"

/* A test case that does nothing and succeeds. */
static void null_test_success(void **state) {
	(void) state; /* unused */
}

static void test_cfg_parse_success(void **state)
{
	(void) state; /* unused */

	char *str = file_to_str("files/tmp/owsd/cfg.json");
	assert_non_null(str);
	if (str)
		free(str);
}

static void test_cfg_parse_fail(void **state)
{
	(void) state; /* unused */

	char *str = file_to_str("NON_EXISTENT_FILE");
	assert_null(str);
	if (str)
		free(str);
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(null_test_success),
		cmocka_unit_test(test_cfg_parse_success),
		cmocka_unit_test(test_cfg_parse_fail),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
