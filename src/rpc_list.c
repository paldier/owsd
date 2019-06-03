/*
 * Copyright (C) 2018 iopsys Software Solutions AB. All rights reserved.
 *
 * Author: Denis Osvald <denis.osvald@sartura.hr>
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

/*
 * ubus over websocket - ubus list
 */
#include "rpc_list.h"

#include "common.h"
#include "wsubus.impl.h"
#include "rpc.h"

#if WSD_HAVE_DBUS
#include "rpc_list_dbus.h"
#endif

#if WSD_HAVE_UBUS
#include "rpc_list_ubus.h"
#endif

#include <libwebsockets.h>

#include <assert.h>

/**
 * \brief fills in allocated struct
 */
static int ubusrpc_blob_list_parse_(struct ubusrpc_blob_list *ubusrpc, struct blob_attr *blob)
{
	if (blob_id(blob) != BLOBMSG_TYPE_ARRAY) {
		ubusrpc->src_blob = NULL;
		ubusrpc->pattern = NULL;
		return 0;
	}

	static const struct blobmsg_policy rpc_ubus_param_policy[] = {
		[0] = { .type = BLOBMSG_TYPE_UNSPEC }, // session ID, IGNORED to keep compat
		[1] = { .type = BLOBMSG_TYPE_STRING }, // ubus-object pattern
	};
	enum { __RPC_U_MAX = (sizeof rpc_ubus_param_policy / sizeof rpc_ubus_param_policy[0]) };
	struct blob_attr *tb[__RPC_U_MAX];

	struct blob_attr *dup_blob = blob_memdup(blob);
	if (!dup_blob) {
		return -100;
	}

	// TODO<blob> blob_(data|len) vs blobmsg_xxx usage, what is the difference
	// and which is right here? (uhttpd ubus uses blobmsg_data for blob which
	// comes from another blob's table... here and so do we)
	blobmsg_parse_array(rpc_ubus_param_policy, __RPC_U_MAX, tb, blobmsg_data(dup_blob), (unsigned)blobmsg_len(dup_blob));

	if (!tb[1]) {
		free(dup_blob);
		return -2;
	}

	ubusrpc->src_blob = dup_blob;
	ubusrpc->sid = tb[0] ? blobmsg_get_string(tb[0]) : UBUS_DEFAULT_SID;
	ubusrpc->pattern = blobmsg_get_string(tb[1]);

	return 0;
}

struct ubusrpc_blob* ubusrpc_blob_list_parse(struct blob_attr *blob)
{
	struct ubusrpc_blob_list *ubusrpc = calloc(1, sizeof *ubusrpc);
	if (!ubusrpc)
		return NULL;

	if (ubusrpc_blob_list_parse_(ubusrpc, blob) != 0) {
		free(ubusrpc);
		return NULL;
	}

	return &ubusrpc->_base;
}

int ubusrpc_handle_list(struct lws *wsi, struct ubusrpc_blob *ubusrpc, struct blob_attr *id)
{
	// in short:
	// ubus list can only be done synchronously, so if we have ubus, then we do it first.
	// After that create a dbus list and complete it asynchronously, appending to the results buffer


#if WSD_HAVE_DBUS
	struct ws_request_base *req = calloc(1, sizeof(struct wsd_list_ctx));
#else
	struct ws_request_base *req = calloc(1, sizeof(struct ws_request_base));
#endif

	// set up result blob buffer
	req->id = blob_memdup(id);
	req->wsi = wsi;
	blob_buf_init(&req->retbuf, 0);

#if WSD_HAVE_DBUS
	req->cancel_and_destroy = wsd_list_ctx_cancel_and_destroy;
#if WSD_HAVE_UBUS
	// false tells it to just write result, not output it to client
	handle_list_ubus(req, wsi, ubusrpc, id, false);
#endif
	// dbus will add its own data and then send result
	handle_list_dbus(req, wsi, ubusrpc, id);
#elif WSD_HAVE_UBUS && !WSD_HAVE_DBUS

	// otherwise let ubus send the reply
	handle_list_ubus(req, wsi, ubusrpc, id, true);
	ubusrpc_blob_destroy_default(ubusrpc);
	blob_buf_free(&req->retbuf);
	free(req->id);
	free(req);
#endif

	return 0;
}
