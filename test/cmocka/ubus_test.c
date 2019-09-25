#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <json-c/json.h>
#include <libwebsockets.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <libubox/uloop.h>
#include <libubus.h>

#include <json-editor.h>

#include "ubusx_acl.h"

#define IP "111.111.111.111"

struct test_env {
	struct ubus_context *ctx;
	bool match;
	int reconnect_count;
};

static void invoke_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	struct test_env *e = (struct test_env *) req->priv;
	struct json_object *obj, *clients;
	char *str;
    int i;

	e->match = false;

	str = blobmsg_format_json(msg, true);
	assert_non_null(str);

	obj = json_tokener_parse(str);
	assert_non_null(obj);

    json_object_object_get_ex(obj, "clients", &clients);

    assert_true(json_object_is_type(clients, json_type_array));

    for(i = 0; i < json_object_array_length(clients); i++) {
        struct json_object *elem = NULL, *tmp;

		elem = json_object_array_get_idx(clients, i);
		if (!elem)
			continue;

        tmp = json_object_get_by_string(elem, "ip");
		assert_non_null(tmp);
        if (strncmp(json_object_get_string(tmp), IP, 16))
            continue;

		tmp = json_object_get_by_string(elem, "state");
		assert_non_null(tmp);
		if (!strncmp(json_object_get_string(tmp), "Teardown", 11))
			continue;
		e->match = true;

		tmp = json_object_get_by_string(elem, "reconnect_count");
		assert_non_null(tmp);

		assert_true(json_object_is_type(tmp, json_type_int));

		e->reconnect_count = json_object_get_int64(tmp);
    }

    json_object_put(obj);
    free(str);
}

static int invoke_uproxy(struct test_env *e, char *method, char *ip, int index, void *cb)
{
	struct blob_buf bb = {0};
	uint32_t obj_id;
	int rv;

	rv = ubus_lookup_id(e->ctx, "owsd.ubusproxy", &obj_id);
	if (rv)
		return rv;

	rv = blob_buf_init(&bb, 0);
	if (rv)
		return rv;

	if (ip)
	    blobmsg_add_string(&bb, "ip", ip);

	if (index >= 0)
	    blobmsg_add_u32(&bb, "index", index);

	rv = ubus_invoke(e->ctx, obj_id, method, bb.head, cb, e, 1500);

	blob_buf_free(&bb);

	return rv;
}

static void test_ubus_add(void **state) {
	struct test_env *e = (struct test_env *) *state;
	int rv;

	rv = invoke_uproxy(e, "add", IP, -1, NULL);
	assert_int_equal(rv, 0);

	rv = invoke_uproxy(e, "list", NULL, -1, invoke_cb);
	assert_int_equal(rv, 0);

	assert_true(e->match);
}

static void test_ubus_remove_by_ip(void **state) {
	struct test_env *e = (struct test_env *) *state;
	int rv;

	test_ubus_add(state);

	rv = invoke_uproxy(e, "remove", IP, -1, NULL);
	assert_int_equal(rv, 0);

	rv = invoke_uproxy(e, "list", NULL, -1, invoke_cb);
	assert_int_equal(rv, 0);
	assert_false(e->match);
}

static void test_ubus_remove_by_index(void **state) {
	struct test_env *e = (struct test_env *) *state;
	int rv;

	test_ubus_add(state);

	rv = invoke_uproxy(e, "remove", NULL, 1, NULL);
	assert_int_equal(rv, 0);

	rv = invoke_uproxy(e, "list", NULL, -1, invoke_cb);
	assert_int_equal(rv, 0);
	assert_false(e->match);
}

static void test_ubus_retry(void **state) {
	struct test_env *e = (struct test_env *) *state;
	int rv;

	test_ubus_add(state);
	assert_int_equal(e->reconnect_count, 0);

	/* need to give lws time to process connect attempts and retry this one,
	 * if more clients are added this possibly has to be increased further...
	 */
	sleep(30);

	rv = invoke_uproxy(e, "list", NULL, -1, invoke_cb);
	assert_int_equal(rv, 0);

	assert_int_not_equal(e->reconnect_count, 0);

	rv = invoke_uproxy(e, "remove", IP, -1, NULL);
	assert_int_equal(rv, 0);

	rv = invoke_uproxy(e, "list", NULL, -1, invoke_cb);
	assert_int_equal(rv, 0);
	assert_false(e->match);
}


/**
 * This is run once before all group tests
 */
static int group_setup (void** state) {
	struct test_env *e = calloc(1, sizeof(struct test_env));

	if (!e)
		return 1;

	e->ctx = ubus_connect(NULL);
	if (!e->ctx)
		return 1;

	invoke_uproxy(e, "remove", IP, -1, NULL);

	*state = e;
	return 0;
}

/**
 * This is run once before one given test
 */
static int setup (void** state) {
	struct test_env *e = (struct test_env *) *state;

	invoke_uproxy(e, "remove", IP, -1, NULL);

    e->match = false;
	e->reconnect_count = 0;
	return 0;
}

/**
 * This is run once after all group tests
 */
static int group_teardown (void** state) {
	struct test_env *e = (struct test_env *) *state;

	ubus_free(e->ctx);
	free(e);
	return 0;
}

int main(void) {

	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(test_ubus_add, setup),
		cmocka_unit_test_setup(test_ubus_remove_by_ip, setup),
		cmocka_unit_test_setup(test_ubus_remove_by_index, setup),
		cmocka_unit_test_setup(test_ubus_retry, setup),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
