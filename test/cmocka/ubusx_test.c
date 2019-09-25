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

#include "ubusx_acl.h"

struct test_env {
	int __remote_obj_counter;
	int __successful_invoke;
	int __successful_event;
	struct ubus_event_handler listener;
	struct ubus_context *ctx;
	char ip[16];
};

static void send_interrupt(struct uloop_timeout *t);

struct uloop_timeout interrupt = {
	.cb = send_interrupt
};

static void send_interrupt(struct uloop_timeout *t)
{
	kill(getpid(), 2);
}

/* look for any ip prefixed object, store ip in ip if empty */
static void uobjx_lookup(struct ubus_context *ctx, struct ubus_object_data *obj, void *priv)
{
	struct test_env *e = (struct test_env *) priv;
	const char *token = "/";
	char *p;

	p = strstr(obj->path, token);
	if (p)
		e->__remote_obj_counter++;
	if (!strlen(e->ip) && p) {
		memset(e->ip, 0, sizeof(e->ip));
		snprintf(e->ip, p - obj->path + 1, "%s", obj->path);
	}
}

/* see if any prefixed objects on ubus list */
static void test_proxied_objects(void **state) {
	struct test_env *e = (struct test_env *) *state;

	ubus_lookup(e->ctx, NULL, uobjx_lookup, e);
	assert_int_not_equal(e->__remote_obj_counter, 0);
}

static void invoke_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	struct test_env *e = (struct test_env *) req->priv;
	struct json_object *obj, *tmp;
	char *str;
	const char *test;

	e->__successful_invoke++;

	str = blobmsg_format_json(msg, true);
	assert_non_null(str);

	obj = json_tokener_parse(str);
	assert_non_null(obj);

	json_object_object_get_ex(obj, "test", &tmp);
	assert_non_null(tmp);

	test = json_object_get_string(tmp);
	assert_string_equal(test, "success");

	json_object_put(obj);
	free(str);
}


/* invoke a proxied template object */
static void test_proxied_access_ubusx_acl_success(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct blob_buf bb = {0};
	uint32_t obj_id;
	char obj[64];
	int rv;

	ubus_lookup(e->ctx, NULL, uobjx_lookup, e);

	assert_int_not_equal(strlen(e->ip), 0);

	snprintf(obj, sizeof(obj), "%s/template", e->ip);
	rv = ubus_lookup_id(e->ctx, obj, &obj_id);

	assert_int_equal(rv, 0);

	rv = blob_buf_init(&bb, 0);
	assert_int_equal(rv, 0);

	rv = ubus_invoke(e->ctx, obj_id, "test", bb.head, invoke_cb, e, 1500);
	assert_int_equal(rv, 0);

	assert_int_equal(e->__successful_invoke, 1);
	blob_buf_free(&bb);

}

/* attempt to access an object which is not exposed via ubusx acl */
static void test_proxied_access_ubusx_acl_fail(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct blob_buf bb = {0};
	uint32_t obj_id = 0;
	char obj[64];
	int rv;

	ubus_lookup(e->ctx, NULL, uobjx_lookup, e);

	assert_int_not_equal(strlen(e->ip), 0);

	snprintf(obj, sizeof(obj), "%s/restricted_template", e->ip);
	rv = ubus_lookup_id(e->ctx, obj, &obj_id);

	assert_int_not_equal(rv, 0);

	rv = blob_buf_init(&bb, 0);
	assert_int_equal(rv, 0);

	rv = ubus_invoke(e->ctx, obj_id, "test", bb.head, invoke_cb, e, 1500);
	assert_int_not_equal(rv, 0);

	assert_int_equal(e->__successful_invoke, 0);
	blob_buf_free(&bb);
}

/* record event and send interrupt signal to interrupt uloop and continue test */
void ev_handler(struct ubus_context *ctx, struct ubus_event_handler *ev, const char *type, struct blob_attr *msg)
{
	struct test_env *e = container_of(ev, struct test_env, listener);

	e->__successful_event++;
	kill(getpid(), 2);
}

/* record event and send interrupt signal to interrupt uloop and continue test */
void uobj_ev_handler(struct ubus_context *ctx, struct ubus_event_handler *ev, const char *type, struct blob_attr *msg)
{
	struct test_env *e = container_of(ev, struct test_env, listener);
	struct json_object *j_obj, *path;
	char *str, obj[128];

	str = blobmsg_format_json(msg, true);
	assert_non_null(str);

	j_obj = json_tokener_parse(str);
	assert_non_null(j_obj);

	if (!j_obj)
		assert_int_equal(1, 0);

	json_object_object_get_ex(j_obj, "path", &path);
	assert_non_null(path);

	ubus_lookup(e->ctx, NULL, uobjx_lookup, e);

	snprintf(obj, sizeof(obj) - 1, "%s/toggling_template", e->ip);

	strncmp(json_object_get_string(path), obj, sizeof(obj));

	e->__successful_event++;
	json_object_put(j_obj);
	free(str);
	kill(getpid(), 2);
}


/* register listener of provided type, prefixed by whatever is found first on ubus */
static void register_listener(struct test_env *e, char *type, struct ubus_event_handler *listener, void *cb)
{
	char obj[64];
	int rv;

	ubus_lookup(e->ctx, NULL, uobjx_lookup, e);

	assert_int_not_equal(strlen(e->ip), 0);

	snprintf(obj, 64, "%s/%s", e->ip, type);

	listener->cb = cb;

	rv = ubus_register_event_handler(e->ctx, listener, obj);
	assert_int_equal(rv, 0);
}

/* listen for template event */
static void test_proxied_events_acl_success(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	int rv;

	register_listener(*state, "template", &e->listener, ev_handler);

	uloop_timeout_set(&interrupt, 4 * 1000);

	uloop_run();

	rv = ubus_unregister_event_handler(e->ctx, &e->listener);
	assert_int_equal(rv, 0);

	assert_int_not_equal(e->__successful_event, 0);
}

/* listen for restricted_template event */
static void test_proxied_events_acl_fail(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	int rv;

	register_listener(*state, "restricted_template", &e->listener, ev_handler);

	uloop_timeout_set(&interrupt, 4 * 1000);

	uloop_run();

	rv = ubus_unregister_event_handler(e->ctx, &e->listener);
	assert_int_equal(rv, 0);

	assert_int_equal(e->__successful_event, 0);
}

/* attempt to access an object which is not exposed via ubusx acl */
static void test_call_toggling_object(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	struct blob_buf bb = {0};
	uint32_t obj_id = 0;
	char obj[64];
	int rv, id_cnt, outer_cnt = 0;

	blob_buf_init(&bb, 0);
	ubus_lookup(e->ctx, NULL, uobjx_lookup, e);

	assert_int_not_equal(strlen(e->ip), 0);

	snprintf(obj, sizeof(obj), "%s/toggling_template", e->ip);

	do {
		id_cnt = 0;
		while ((rv = ubus_lookup_id(e->ctx, obj, &obj_id)) && id_cnt < 4) {
			printf("No %s was not found on attempt: %d, try again in 1 seconds\n", obj, id_cnt);
			sleep(1);
			id_cnt++;
		}

		assert_int_equal(rv, 0);
		outer_cnt++;
	} while ((rv = ubus_invoke(e->ctx, obj_id, "test", bb.head, invoke_cb, e, 1500) && outer_cnt <= 3));
	assert_int_equal(rv, 0);

	assert_int_equal(e->__successful_invoke, 1);
	blob_buf_free(&bb);
}

/* listen for template event */
static void test_observe_toggling_add(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	int rv;

	e->listener.cb = uobj_ev_handler;
	rv = ubus_register_event_handler(e->ctx, &e->listener, "ubus.object.add");

	uloop_timeout_set(&interrupt, 6 * 1000);

	uloop_run();

	rv = ubus_unregister_event_handler(e->ctx, &e->listener);
	assert_int_equal(rv, 0);

	assert_int_not_equal(e->__successful_event, 0);
}

/* listen for template event */
static void test_observe_toggling_remove(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	int rv;

	e->listener.cb = uobj_ev_handler;
	rv = ubus_register_event_handler(e->ctx, &e->listener, "ubus.object.remove");

	uloop_timeout_set(&interrupt, 6 * 1000);

	uloop_run();

	rv = ubus_unregister_event_handler(e->ctx, &e->listener);
	assert_int_equal(rv, 0);

	assert_int_not_equal(e->__successful_event, 0);
}

/* initialize ubusx access lists, placeholder test for coverage */
static void test_ubusx_acl_init(void **state)
{
	(void) state;

	ubusx_acl__init();
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

	ubus_add_uloop(e->ctx);

	*state = e;
	return 0;
}

/**
 * This is run once before one given test
 */
static int setup (void** state) {
	struct test_env *e = (struct test_env *) *state;
	e->__successful_invoke = 0;
	e->__remote_obj_counter = 0;
	e->__successful_event = 0;
	memset(e->ip, 0, sizeof(e->ip));
	return 0;
}

/**
 * This is run once after one given test
 */
static int teardown (void** state) {
	struct test_env *e = (struct test_env *) *state;
	e->__successful_invoke = 0;
	e->__remote_obj_counter = 0;
	e->__successful_event = 0;
	memset(e->ip, 0, sizeof(e->ip));
	return 0;
}

/**
 * This is run once after all group tests
 */
static int group_teardown (void** state) {
	struct test_env *e = (struct test_env *) *state;

	uloop_done();
	ubus_free(e->ctx);
	free(e);
	return 0;
}

int main(void) {

	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_proxied_objects, setup, teardown),
		cmocka_unit_test_setup_teardown(test_proxied_access_ubusx_acl_success, setup, teardown),
		cmocka_unit_test_setup_teardown(test_proxied_access_ubusx_acl_fail, setup, teardown),
		cmocka_unit_test_setup_teardown(test_proxied_events_acl_fail, setup, teardown),
		cmocka_unit_test_setup_teardown(test_proxied_events_acl_success, setup, teardown),
		cmocka_unit_test_setup_teardown(test_call_toggling_object, setup, teardown),
		cmocka_unit_test_setup_teardown(test_observe_toggling_add, setup, teardown),
		cmocka_unit_test_setup_teardown(test_observe_toggling_remove, setup, teardown),
		cmocka_unit_test(test_ubusx_acl_init)
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
