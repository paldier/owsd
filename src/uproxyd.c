/*
 * uproxyd -- daemon dispatching ubusproxy client events to owsd
 *
 * Copyright (C) 2018 iopsys Software Solutions AB. All rights reserved.
 *
 * Author:
	Ionut-Alex Oprea <ionutalexoprea@gmail.com>
	Reidar Cederqvist <reidar.cederqvist@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <json-c/json.h>

#define UNUSED(x) (void)(x)

#define OWSD_OBJECT_NAME "owsd.ubusproxy"
#define OWSD_METHOD_NAME_ADD "add"
#define OWSD_METHOD_NAME_REMOVE "remove"
#define OWSD_METHOD_NAME_LIST "list"
#define CLIENT_EVENT_NAME "client"
#define UBUS_CALL_TIMEOUT (2 * 1000)
#define CLEAN_TIMEOUT (UBUS_CALL_TIMEOUT + (3 * 1000))
#define MAX_RECONNECT_COUNT 10

/** structure of a ubus callback function
 * static void cb_fn(struct ubus_request *req, int type, struct blob_attr *msg)
 **/

static struct ubus_context *ctx;

int json_get_int(struct json_object *object, const char *key)
{
	json_bool ret;
	struct json_object *value;

	if (!object || !json_object_is_type(object, json_type_object))
		return -1;

	ret = json_object_object_get_ex(object, key, &value);
	if (!ret || !value || !json_object_is_type(value, json_type_int))
		return -1;

	return json_object_get_int(value);
}

int json_get_bool(struct json_object *object, const char *key)
{
	json_bool ret;
	struct json_object *value;

	if (!object || !json_object_is_type(object, json_type_object))
		return -1;

	ret = json_object_object_get_ex(object, key, &value);
	if (!ret || !value || !json_object_is_type(value, json_type_boolean))
		return -1;

	return json_object_get_boolean(value) ? 1 : 0;
}

const char *json_get_string(struct json_object *object, const char *key)
{
	json_bool ret;
	struct json_object *value;

	if (!object || !json_object_is_type(object, json_type_object))
		return NULL;

	ret = json_object_object_get_ex(object, key, &value);
	if (!ret || !value || !json_object_is_type(value, json_type_string))
		return NULL;

	return json_object_get_string(value);
}

static int init_ubus()
{
	int ret = 0;

	ret = uloop_init();
	if (ret)
		return ret;
	ctx = ubus_connect(NULL);
	if (!ctx) {
		fprintf(stderr, "Failed to connect to ubus\n");
		return 1;
	}
	ubus_add_uloop(ctx);
	return 0;
}

static void owsd_add_client(const char *ip)
{
	uint32_t id;
	struct blob_buf arg_buf = {};
	int ret;

	ubus_lookup_id(ctx, OWSD_OBJECT_NAME, &id);
	if (!id)
		return;

	blob_buf_init(&arg_buf, 0);
	blobmsg_add_string(&arg_buf, "ip", ip);

	ret = ubus_invoke(ctx, id, OWSD_METHOD_NAME_ADD, arg_buf.head,
			NULL, NULL, UBUS_CALL_TIMEOUT);

	if (ret) {
		id = 0;
		fprintf(stderr, "Couldn't invoke "OWSD_OBJECT_NAME" "
				OWSD_METHOD_NAME_ADD" for ip %s\n", ip);
	}
	blob_buf_free(&arg_buf);
}

static void owsd_remove_client(int index, const char *ip)
{
	uint32_t id;
	struct blob_buf arg_buf = {};
	int ret;

	ubus_lookup_id(ctx, OWSD_OBJECT_NAME, &id);
	if (!id)
		return;

	blob_buf_init(&arg_buf, 0);
	if (ip)
		blobmsg_add_string(&arg_buf, "ip", ip);
	else
		blobmsg_add_u32(&arg_buf, "index", index);

	ret = ubus_invoke(ctx, id, OWSD_METHOD_NAME_REMOVE, arg_buf.head,
			NULL, NULL, UBUS_CALL_TIMEOUT);

	if (ret) {
		id= 0;
		fprintf(stderr, "Couldn't invoke "OWSD_OBJECT_NAME" "
				OWSD_METHOD_NAME_REMOVE" for index %d\n", index);
	}
	blob_buf_free(&arg_buf);
}

/**
 * receive an event:
 * { "client": {"action":"connect","macaddr":"b4:3a:28:ff:58:dc",
 *	"ipaddr":"192.168.1.211","network":"lan"} }
 **/
static void receive_event_cb(struct ubus_context *ctx,
		struct ubus_event_handler *eh, const char *type,
		struct blob_attr *msg)
{
	const char *action, *ipaddr;
	char *json_str;
	struct json_object *json_msg;

	json_str = blobmsg_format_json(msg, true);
	if (!json_str)
		return;

	json_msg = json_tokener_parse(json_str);
	if (!json_msg)
		goto out_str;

	if (!json_object_is_type(json_msg, json_type_object))
		goto out_json;

	action = json_get_string(json_msg, "action");
	if (!action)
		goto out_json;

	ipaddr = json_get_string(json_msg, "ipaddr");
	if (!ipaddr)
		goto out_json;

	if (action && strcmp(action, "connect") == 0)
		owsd_add_client(ipaddr);
	else if (action && strcmp(action, "disconnect") == 0) {
		owsd_remove_client(0, ipaddr);
	}

out_json:
	json_object_put(json_msg);
out_str:
	free(json_str);
}

static void clean_stale_clients_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	char *json_str;
	struct json_object *json_msg;
	int reconnect_count, index;

	UNUSED(req);
	UNUSED(type);

	json_str = blobmsg_format_json(msg, true);
	if (!json_str)
		return;

	json_msg = json_tokener_parse(json_str);

	if (!json_msg)
		goto out_str;

	if (!json_object_is_type(json_msg, json_type_object))
		goto out_json;

	json_object_object_foreach(json_msg, name, client) {
		UNUSED(name);
		reconnect_count = json_get_int(client, "reconnect_count");
		if (reconnect_count <= MAX_RECONNECT_COUNT)
			continue;
		index = json_get_int(client, "index");
		if (index >= 0)
			owsd_remove_client(index, NULL);
	}
out_json:
	json_object_put(json_msg);
out_str:
	free(json_str);
}

static void clean_stale_clients(struct uloop_timeout *timer)
{
	static uint32_t id;
	int ret;
	struct blob_buf bb = {};

	if (!id) {
		ubus_lookup_id(ctx, OWSD_OBJECT_NAME, &id);
		if (!id)
			goto out;
	}

	blob_buf_init(&bb, 0);

	ret = ubus_invoke(ctx, id, OWSD_METHOD_NAME_LIST, bb.head,
			clean_stale_clients_cb, NULL, UBUS_CALL_TIMEOUT);

	if (ret) {
		id = 0;
		fprintf(stderr, "Couldn't get hosts from router.network "
				"hosts ret = %d\n", ret);
	}
	blob_buf_free(&bb);
out:
	uloop_timeout_set(timer, CLEAN_TIMEOUT);
}

static void add_all_clients_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	char *json_str;
	const char *ipaddr;
	struct json_object *json_msg, *elem, *hosts;
	int connected, i;

	UNUSED(req);
	UNUSED(type);

	json_str = blobmsg_format_json(msg, true);
	if (!json_str)
		return;

	json_msg = json_tokener_parse(json_str);

	if (!json_msg)
		goto out_str;

	if(!json_object_is_type(json_msg, json_type_object))
		goto out_json;

	json_object_object_get_ex(json_msg, "hosts", &hosts);
	if (!hosts || !json_object_is_type(hosts, json_type_array))
		goto out_json;

	for (i = 0; i < json_object_array_length(hosts); i++) {
		elem = json_object_array_get_idx(hosts, i);
		if (!elem)
			continue;

		connected = json_get_bool(elem, "connected");
		if (!connected)
			continue;
		ipaddr = json_get_string(elem, "ipaddr");
		if (ipaddr)
			owsd_add_client(ipaddr);
	}

out_json:
	json_object_put(json_msg);
out_str:
	free(json_str);
}

static void add_all_clients(struct uloop_timeout *timer)
{
	uint32_t id;
	int ret;
	struct blob_buf bb = {};

	ubus_lookup_id(ctx, "router.network", &id);
	if (!id) {
		fprintf(stderr, "ubus_lookup_id for router.network failed\n");
		goto retry;
	}

	blob_buf_init(&bb, 0);
	ret = ubus_invoke(ctx, id, "hosts", bb.head, add_all_clients_cb,
			NULL, UBUS_CALL_TIMEOUT);
	blob_buf_free(&bb);

	if (ret)
		fprintf(stderr, "Couln't get hosts from router.network hosts ret = %d\n", ret);

retry:
	uloop_timeout_set(timer, 5000 /*msecs*/);
	uloop_timeout_add(timer);
}

static int start_ubus_listen(struct ubus_event_handler *listener)
{
	int rv;

	listener->cb = receive_event_cb;
	rv = ubus_register_event_handler(ctx, listener, CLIENT_EVENT_NAME);
	if (rv)
		fprintf(stderr, "Couldn't register to ubus events. error %d\n",
				rv);

	return rv;
}

static void prepare_timers(struct uloop_timeout *add_timer, struct uloop_timeout *clean_timer)
{
	add_timer->cb = add_all_clients;
	uloop_timeout_set(add_timer, 1000 /*msecs*/);
	uloop_timeout_add(add_timer);

	clean_timer->cb = clean_stale_clients;
	uloop_timeout_set(clean_timer, CLEAN_TIMEOUT);
	uloop_timeout_add(clean_timer);
}

int main(int argc, char **argv)
{
	struct ubus_event_handler listener = {};
	struct uloop_timeout add_timer = {}, clean_timer = {};
	int rv = 0;

	rv = init_ubus();
	if (rv)
		goto out;

	rv = start_ubus_listen(&listener);
	if (rv)
		goto out_ubus;

	prepare_timers(&add_timer, &clean_timer);

	uloop_run();
	ubus_unregister_event_handler(ctx, &listener);

out_ubus:
	ubus_free(ctx);
out:
	return rv;
}
