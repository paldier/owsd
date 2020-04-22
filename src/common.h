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
#pragma once

#include "owsd-config.h"
#include "wsubus_client.h"

#include <libwebsockets.h>
#include <libubox/uloop.h>
#include <stddef.h>

struct prog_context {
	struct uloop_fd **ufds;
	size_t num_ufds;
	size_t num_active_ses;

	int req_max;
	int total_req_max;

	struct uloop_timeout utimer;

	struct lws_context *lws_ctx;

#if WSD_HAVE_UBUS
	struct ubus_context *ubus_ctx;
#endif
#if WSD_HAVE_DBUS
	struct DBusConnection *dbus_ctx;
#endif

	const char *www_path;
	const char *redir_from;
	const char *redir_to;
};

/* each listen vhost keeps origin whitelist */
struct vh_context {
	struct list_head origins;
	struct list_head users;
	char *name;
	bool restrict_origins;
};

struct str_list {
	struct list_head list;
	const char *str;
};
