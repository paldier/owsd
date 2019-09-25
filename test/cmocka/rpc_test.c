#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <json-c/json.h>
#include <libwebsockets.h>
#include <time.h>

#include "common.h"
#include "config.h"

#define USERNAME "user"
#define PASSWORD "user"
#define IP "127.0.0.1"
#define WS_PORT 80
#define MAX_MSG_LEN 1024
#define RX_BUFFER_BYTES (1024 * 64)


struct test_env {
	char *session_id;
	struct lws_context *context;
	struct lws *web_socket;

	struct json_object *ptr;

	int test_no;

	bool sent;
	bool received;

	bool event;
	bool unsubbed;

	int result;
	char type[32];
};

static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + MAX_MSG_LEN + LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
	struct lws_context *context;
	struct test_env *e;
	size_t w_n;
	size_t n;

	context = lws_get_context(wsi);
	if (!context) {
		printf("No context found!\n");
		return 0;
	}

	e = (struct test_env *) lws_context_user(context);

	switch (reason) {
		case LWS_CALLBACK_CLIENT_ESTABLISHED: {
			printf("Connection established, waiting for writeable...\n");
			lws_callback_on_writable(wsi);
		} break;

		case LWS_CALLBACK_CLIENT_RECEIVE: {
			struct json_object *jobj = json_tokener_parse((char *) in);
			if (jobj) {
				struct json_object *result_jobj = json_object_object_get(jobj, "result");
				struct json_object *method_jobj = json_object_object_get(jobj, "method");
				struct json_object *error_jobj = json_object_object_get(jobj, "error");
				int id = json_object_get_int(json_object_object_get(jobj, "id"));

				//Debug any incoming data
				printf("Object received: %s\n", json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_SPACED));

				if (method_jobj) {
					struct json_object *params_jobj = json_object_object_get(jobj, "params");
					struct json_object *type_jobj = json_object_object_get(params_jobj, "type");
					const char *m_type = json_object_get_string(method_jobj);

					if (m_type && !strncmp(m_type, "event", 6)) {
						strncpy(e->type, json_object_get_string(type_jobj), sizeof(e->type) - 1);
						e->event = true;
					} if (m_type && !strncmp(m_type, "subscribe-list", 15)) {
						strncpy(e->type, json_object_get_string(type_jobj), sizeof(e->type) - 1);
						e->received = true;
					}
				}

				if (result_jobj) {
					struct json_object *array0_jobj = json_object_array_get_idx(result_jobj, 0);

					e->result = json_object_get_int(array0_jobj);
					if (e->result == 0) {
						struct json_object *array1_jobj = json_object_array_get_idx(result_jobj, 1);
						struct json_object *ubus_rpc_session_jobj = json_object_object_get(array1_jobj, "ubus_rpc_session");
						const char *session_id_tmp = NULL;

						e->ptr = json_object_get(array1_jobj);

						session_id_tmp = json_object_get_string(ubus_rpc_session_jobj);

						if (session_id_tmp) {
							//printf("Got session id: %s\n", session_id_tmp);
							e->session_id = calloc(1, (strlen(session_id_tmp) + 1));
							strcpy(e->session_id, session_id_tmp);
						}

						/* hard-code unsubscribe IDs... */
						if (id == 4 || id == 6 || id == 11)
							e->unsubbed = true;
					}
					e->received = true;
				} else if (error_jobj) {
					struct json_object *code;

					json_object_object_get_ex(error_jobj, "code", &code);
					if (code)
						e->result = json_object_get_int(code);

					e->received = true;
				}

				json_object_put(jobj);
			}

		} break;

		case LWS_CALLBACK_CLIENT_WRITEABLE: {
			if (!e->session_id) {
				char *login_call;

				// jsonrpc + session id + new_state
				login_call = malloc(170 + 32 + strlen(USERNAME) + strlen(PASSWORD));

				sprintf(login_call,
						"{ \
						\"jsonrpc\": \"2.0\", \
						\"method\": \"call\", \
						\"params\": [\"00000000000000000000000000000000\", \"session\", \
						\"login\", { \"username\": \"%s\", \"password\": \"%s\" }], \"id\": \"0\" \
					}",
						"user",
						"user");

				n = sprintf((char *) p, "%s", login_call);
				w_n = lws_write(wsi, p, n, LWS_WRITE_TEXT);
				if (w_n == n)
					printf("Login request sent!\n\n");
			}
			/* valid method call */
			else if (e->test_no == 1) {
				printf("Requesting restricted_template test method...\n");
				n = sprintf((char *) p,
							"{ \
					\"jsonrpc\": \"2.0\", \
					\"method\": \"call\", \
					\"params\": [\"%s\", \"restricted_template\", \"test\", {}], \
					\"id\": \"1\" \
				}",
							e->session_id);
				w_n = lws_write(wsi, p, n, LWS_WRITE_TEXT);
				if (w_n == n)
					printf("Restricted restricted_template test call sent! test_no = %d\n\n", e->test_no);
			}
			/* denied method call */
			else if (e->test_no == 2) {
				printf("Requesting template test method...\n");
				n = sprintf((char *) p,
							"{ \
					\"jsonrpc\": \"2.0\", \
					\"method\": \"call\", \
					\"params\": [\"%s\", \"template\", \"test\", {}], \
					\"id\": \"2\" \
				}",
							e->session_id);
				w_n = lws_write(wsi, p, n, LWS_WRITE_TEXT);
				if (w_n == n)
					printf("Restricted template test call sent! test_no = %d\n\n", e->test_no);
			}
			/* valid subscribe and unsub */
			else if (e->test_no == 3) {
				if (!e->sent) {
					printf("Sending subscribe request to restricted_template events...\n");
					n = sprintf((char *) p,
								"{ \
						\"jsonrpc\": \"2.0\", \
						\"method\": \"subscribe\", \
						\"params\": [\"%s\", \"restricted_template\"], \
						\"id\": \"3\" \
					}",
								e->session_id);
					w_n = lws_write(wsi, p, n, LWS_WRITE_TEXT);
					if (w_n == n)
						printf("Subscribe request sent! test_no = %d\n\n", e->test_no);
				} else {
					printf("Sending unsubscribe request for restricted_template...\n");
					n = sprintf((char *) p,
								"{ \
						\"jsonrpc\": \"2.0\", \
						\"method\": \"unsubscribe\", \
						\"params\": [\"%s\", \"restricted_template\"], \
						\"id\": \"4\" \
					}",
								e->session_id);
					w_n = lws_write(wsi, p, n, LWS_WRITE_TEXT);
					if (w_n == n)
						printf("Unsubscribe request sent for restricted_template! test_no = %d\n\n", e->test_no);
					//e->unsubbed = true;
				}
			}
			/* invalid subscribe (and unsub) */
			else if (e->test_no == 4) {
				if (!e->sent) {
					printf("Sending subscribe request to template events...\n");
					n = sprintf((char *) p,
								"{ \
						\"jsonrpc\": \"2.0\", \
						\"method\": \"subscribe\", \
						\"params\": [\"%s\", \"template\"], \
						\"id\": \"5\" \
					}",
								e->session_id);
					w_n = lws_write(wsi, p, n, LWS_WRITE_TEXT);
					if (w_n == n)
						printf("Subscribe request sent for template events! test_no = %d\n\n", e->test_no);
				} else {
					printf("Sending unsubscribe request for template\n");
					n = sprintf((char *) p,
								"{ \
						\"jsonrpc\": \"2.0\", \
						\"method\": \"unsubscribe\", \
						\"params\": [\"%s\", \"template\"], \
						\"id\": \"6\" \
					}",
								e->session_id);
					w_n = lws_write(wsi, p, n, LWS_WRITE_TEXT);
					if (w_n == n)
						printf("Unsubscribe request sent for restricted_template! test_no = %d\n\n", e->test_no);
					//e->unsubbed = true;
				}
			}
			/* valid input format with schema */
			else if (e->test_no == 5) {
				printf("Requesting restricted test method with valid input\n");
				n = sprintf((char *) p,
							"{ \
					\"jsonrpc\": \"2.0\", \
					\"method\": \"call\", \
					\"params\": [\"%s\", \"restricted_template\", \"test\", {}], \
					\"id\": \"7\" \
				}",
							e->session_id);
				w_n = lws_write(wsi, p, n, LWS_WRITE_TEXT);
				if (w_n == n)
					printf("Restricted template test call sent! test_no = %d\n\n", e->test_no);
			}
			/* invalid input format with schema */
			else if (e->test_no == 6) {
				printf("Requesting invalid_template test method with invalid input\n");
				n = sprintf((char *) p,
							"{ \
					\"jsonrpc\": \"2.0\", \
					\"method\": \"call\", \
					\"params\": [\"%s\", \"invalid_template\", \"test\", {}], \
					\"id\": \"8\" \
				}",
							e->session_id);
				w_n = lws_write(wsi, p, n, LWS_WRITE_TEXT);
				if (w_n == n)
					printf("Invalid template test call sent! test_no = %d\n\n", e->test_no);
			}
			/* valid output format with schema */
			else if (e->test_no == 7) {
				printf("Requesting invalid_template test method\n");
				n = sprintf((char *) p,
							"{ \
					\"jsonrpc\": \"2.0\", \
					\"method\": \"call\", \
					\"params\": [\"%s\", \"restricted_template\", \"test\", {}], \
					\"id\": \"9\" \
				}",
							e->session_id);
				w_n = lws_write(wsi, p, n, LWS_WRITE_TEXT);
				if (w_n == n)
					printf("Restricted restrictedtemplate test call sent! test_no = %d\n\n", e->test_no);
			}
			/* invalid output format with schema */
			else if (e->test_no == 8) {
				printf("calling invalid_template with valid input\n");
				n = sprintf((char *) p,
							"{ \
					\"jsonrpc\": \"2.0\", \
					\"method\": \"call\", \
					\"params\": [\"%s\", \"invalid_template\", \"test\", {\"test\":5}], \
					\"id\": \"10\" \
				}",
							e->session_id);
				w_n = lws_write(wsi, p, n, LWS_WRITE_TEXT);
				if (w_n == n)
					printf("Restricted invalid_template test call sent! test_no = %d\n\n", e->test_no);
			}
			/* call with incomplete jsonrpc */
			else if (e->test_no == 9) {
				printf("calling with incomplete json\n");
				n = sprintf((char *) p,
							"{ \
					\"jsonrpc\": \"2.0\", \
					\"method\": \"call\", \
					\"params\": [\"%s\", \"invalid_template\", \"",
					e->session_id);
				w_n = lws_write(wsi, p, n, LWS_WRITE_TEXT);
				if (w_n == n)
					printf("Restricted invalid_template test call sent! test_no = %d\n\n", e->test_no);
			}
			/* valid subscribe and sucscribe-list unsub */
			else if (e->test_no == 10) {
				if (!e->sent) {
					printf("Sending subscribe request to test events...\n");
					n = sprintf((char *) p,
								"{ \
						\"jsonrpc\": \"2.0\", \
						\"method\": \"subscribe\", \
						\"params\": [\"%s\", \"test\"], \
						\"id\": \"11\" \
						}",
						e->session_id);
					w_n = lws_write(wsi, p, n, LWS_WRITE_TEXT);
					if (w_n == n)
						printf("Subscribe request sent! test_no = %d\n\n", e->test_no);
				}
			}
			else if (e->test_no == 11) {
				if (!e->sent) {
					printf("Sending subscribe request to test events...\n");
					n = sprintf((char *) p,
								"{ \
						\"jsonrpc\": \"2.0\", \
						\"method\": \"subscribe-list\", \
						\"params\": [\"%s\"], \
						\"id\": \"12\" \
					}",
							e->session_id);
					w_n = lws_write(wsi, p, n, LWS_WRITE_TEXT);
					if (w_n == n)
						printf("Subscribe request sent! test_no = %d\n\n", e->test_no);
				} else {
					printf("Sending unsubscribe request for test...\n");
					n = sprintf((char *) p,
								"{ \
						\"jsonrpc\": \"2.0\", \
						\"method\": \"unsubscribe\", \
						\"params\": [\"%s\", \"test\"], \
						\"id\": \"13\" \
						}",
						e->session_id);
					w_n = lws_write(wsi, p, n, LWS_WRITE_TEXT);
					if (w_n == n)
						printf("Unsubscribe request sent for test! test_no = %d\n\n", e->test_no);
					//e->unsubbed = true;
				}
			}
			e->sent = true;
		} break;

		case LWS_CALLBACK_CLOSED: {
			printf("Websocket connection closed..\n");
			e->web_socket = NULL;
		} break;

		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR: {
			printf("Not connected..\n");
			e->web_socket = NULL;
		} break;
		default:
			break;
	}

	return 0;
}

static void test_rpc_login(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	time_t seconds = time(NULL);

	lws_callback_on_writable(e->web_socket);

	while ((!e->sent || !e->received) && seconds > time(NULL) - 4)
		lws_service(e->context, /* timeout_ms = */ 1000);

	assert_non_null(e->session_id);
	assert_int_equal(e->result, 0);
}

static void test_rpc_method_call(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	time_t seconds = time(NULL);

	lws_callback_on_writable(e->web_socket);

	while ((!e->sent || !e->received) && seconds > time(NULL) - 4)
		lws_service(e->context, /* timeout_ms = */ 1000);

	assert_non_null(e->session_id);
	assert_int_equal(e->result, 0);
}

static void test_rpc_method_call_denied(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	time_t seconds = time(NULL);

	lws_callback_on_writable(e->web_socket);

	while ((!e->sent || !e->received) && seconds > time(NULL) - 4)
		lws_service(e->context, /* timeout_ms = */ 1000);

	assert_non_null(e->session_id);
	assert_int_equal(e->result, 6);
}

static void test_rpc_subscribe(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	time_t seconds = time(NULL);

	lws_callback_on_writable(e->web_socket);

	while ((!e->sent || !e->received || seconds > time(NULL) - 4) && !e->event)
		lws_service(e->context, /* timeout_ms = */ 1000);

	assert_non_null(e->session_id);
	assert_string_equal(e->type, "restricted_template");

	lws_callback_on_writable(e->web_socket);
	seconds = time(NULL);

	while (!e->unsubbed && seconds > time(NULL) - 4)
		lws_service(e->context, /* timeout_ms = */ 1000);

	assert_true(e->unsubbed);
}

static void test_rpc_subscribe_denied(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	time_t seconds = time(NULL);

	lws_callback_on_writable(e->web_socket);

	while ((!e->sent || !e->received || seconds > time(NULL) - 4) && !e->event)
		lws_service(e->context, /* timeout_ms = */ 1000);

	assert_non_null(e->session_id);
	assert_string_equal(e->type, "");

	lws_callback_on_writable(e->web_socket);
	seconds = time(NULL);
	while (!e->unsubbed && seconds > time(NULL) - 4)
		lws_service(e->context, /* timeout_ms = */ 1000);
}

static void test_rpc_valid_input_format(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	time_t seconds = time(NULL);

	lws_callback_on_writable(e->web_socket);

	while ((!e->sent || !e->received) && seconds > time(NULL) - 4)
		lws_service(e->context, /* timeout_ms = */ 1000);

	assert_non_null(e->session_id);
	assert_int_equal(e->result, 0);
}

static void test_rpc_valid_output_format(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	time_t seconds = time(NULL);

	lws_callback_on_writable(e->web_socket);

	while ((!e->sent || !e->received) && seconds > time(NULL) - 4)
		lws_service(e->context, /* timeout_ms = */ 1000);

	assert_non_null(e->session_id);
	assert_int_equal(e->result, 0);
}

static void test_rpc_invalid_output_format(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	time_t seconds = time(NULL);

	lws_callback_on_writable(e->web_socket);

	while ((!e->sent || !e->received) && seconds > time(NULL) - 4)
		lws_service(e->context, /* timeout_ms = */ 1000);

	assert_non_null(e->session_id);
	assert_int_equal(e->result, -32605);
}

static void test_rpc_invalid_input_format(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	time_t seconds = time(NULL);

	lws_callback_on_writable(e->web_socket);

	while ((!e->sent || !e->received) && seconds > time(NULL) - 4)
		lws_service(e->context, /* timeout_ms = */ 1000);

	assert_non_null(e->session_id);
	assert_int_equal(e->result, -32604);
}

static void test_rpc_parse_error(void **state)
{
	struct test_env *e = (struct test_env *) *state;
	time_t seconds = time(NULL);

	lws_callback_on_writable(e->web_socket);

	while ((!e->sent || !e->received) && seconds > time(NULL) - 4)
		lws_service(e->context, /* timeout_ms = */ 1000);

	assert_non_null(e->session_id);
	assert_int_equal(e->result, -32700);
}

static void test_rpc_subscribe_list(void **state)
{
	struct json_object *elem, *sess, *pattern;
	struct test_env *e = (struct test_env *) *state;
	time_t seconds = time(NULL);

	lws_callback_on_writable(e->web_socket);

	while ((!e->sent || !e->received || seconds > time(NULL) - 4) && !e->event)
		lws_service(e->context, /* timeout_ms = */ 1000);

	assert_non_null(e->session_id);
	assert_int_equal(e->result, 0);

	/* manually reset sent variable and increase counter  */
	e->sent = false;
	e->test_no++;
	e->received = false;
	lws_callback_on_writable(e->web_socket);
	seconds = time(NULL);

	while (!e->received && seconds > time(NULL) - 4)
		lws_service(e->context, /* timeout_ms = */ 1000);

	assert_int_equal(e->result, 0);
	assert_non_null(e->ptr);
	assert_true(json_object_is_type(e->ptr, json_type_array));

	elem = json_object_array_get_idx(e->ptr, 0);
	assert_non_null(elem);

	sess = json_object_object_get(elem, "ubus_rpc_session");
	pattern = json_object_object_get(elem, "pattern");

	assert_non_null(pattern);
	assert_non_null(sess);
	assert_string_equal(json_object_get_string(pattern), "test");
	assert_string_equal(json_object_get_string(sess), e->session_id);
	lws_callback_on_writable(e->web_socket);
	seconds = time(NULL);

	while (!e->unsubbed && seconds > time(NULL) - 4)
		lws_service(e->context, /* timeout_ms = */ 1000);

	assert_true(e->unsubbed);
}

static int setup (void** state) {
	struct test_env *e = (struct test_env *) *state;

	e->sent = false;
	e->received = false;
	e->unsubbed = false;
	e->event = false;
	e->result = -1;
	memset(e->type, 0, sizeof(e->type));

	return 0;
}

/**
 * This is run once after one given test
 */
static int teardown (void** state) {
	struct test_env *e = (struct test_env *) *state;

	e->test_no++;

	return 0;
}

enum protocols { PROTOCOL_WS = 0, PROTOCOL_COUNT };

static struct lws_protocols protocols[] = {
	{
		"ubus-json",
		ws_callback,
		0,
		RX_BUFFER_BYTES,
	},
	{NULL, NULL, 0, 0} /* terminator */
};

static int group_setup (void** state)
{
	struct lws_context_creation_info info;
	struct test_env *e;
	time_t seconds = time(NULL);

	memset(&info, 0, sizeof(info));
	e = calloc(1, sizeof(struct test_env));
	e->result = -1;

	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = protocols;
	info.gid = -1;
	info.uid = -1;
	info.user = e;

	*state = e;

	e->context = lws_create_context(&info);
	while (!e->web_socket && seconds > time(NULL) - 10) {
		sleep(1);
		printf("Connecting...\n");
		struct lws_client_connect_info ccinfo = {0};
		ccinfo.context = e->context;
		ccinfo.address = IP;
		ccinfo.port = WS_PORT;
		ccinfo.path = "/";
		ccinfo.host = lws_canonical_hostname(e->context);
		ccinfo.origin = IP;
		ccinfo.protocol = protocols[PROTOCOL_WS].name;
		e->web_socket = lws_client_connect_via_info(&ccinfo);

		lws_service(e->context, /* timeout_ms = */ 1000);
	}

	return !e->web_socket;
}

static int group_teardown(void** state)
{
	struct test_env *e = (struct test_env *) *state;

	lws_context_destroy(e->context);
	free(e->session_id);
	free(e);
	return 0;
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_rpc_login, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rpc_method_call, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rpc_method_call_denied, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rpc_subscribe, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rpc_subscribe_denied, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rpc_valid_input_format, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rpc_invalid_input_format, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rpc_valid_output_format, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rpc_invalid_output_format, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rpc_parse_error, setup, teardown),
		cmocka_unit_test_setup_teardown(test_rpc_subscribe_list, setup, teardown),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
