#include <libwebsockets.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "wsubus.h"
#include "ubusx_acl.h"
#include "config.h"

int new_vhinfo_list(struct vhinfo_list **currvh)
{
	   int rc = 0;

	   struct vhinfo_list *newvh = malloc(sizeof *newvh);
	   if (!newvh) {
			   lwsl_err("OOM vhinfo init\n");
			   rc = -1;
			   goto error;
	   }
	   *newvh = (struct vhinfo_list){{0}};
	   INIT_LIST_HEAD(&newvh->vh_ctx.origins);
	   INIT_LIST_HEAD(&newvh->vh_ctx.users);
	   newvh->vh_ctx.name = "";
	   newvh->vh_info.options |= LWS_SERVER_OPTION_DISABLE_IPV6;
	   newvh->vh_ctx.restrict_origins = 1;

	   // add this listening vhost into our list
	   newvh->next = *currvh;
	   *currvh = newvh;
error:
	   return rc;
}

int append_origins(struct json_object *cfg, struct vhinfo_list *currvh)
{
	struct json_object *elem;
	int i;

	if (!cfg || !json_object_is_type(cfg, json_type_array))
		return -1;

	for (i = 0; i < json_object_array_length(cfg); i++) {
		elem = json_object_array_get_idx(cfg, i);
		if (!elem)
			continue;

		struct str_list *str = malloc(sizeof *str);
		if (!str)
			return -1;

		str->str = json_object_get_string(elem);
		list_add_tail(&str->list, &currvh->vh_ctx.origins);
	}

	return 0;
}

int create_vhost_by_json(char *key, struct json_object *obj, struct vhinfo_list **currvh, struct global_config *cfg)
{
	struct json_object *port, *interface, *arg, *a, *k, *c;
	int rv = -1, i;

	json_object_object_get_ex(obj, "port", &port);
	if (!port)
		goto out;

	json_object_object_get_ex(obj, "interface", &interface);
	if (!interface)
		goto out;

	rv = new_vhinfo_list(currvh);
	if (rv) {
		lwsl_err("new_vhinfo_list failed memory alloc\n");
		goto out;
	}

	(*currvh)->vh_info.port = json_object_get_int(port);
	(*currvh)->vh_ctx.name = key;
	(*currvh)->vh_info.vhost_name = key;
	(*currvh)->vh_info.iface = json_object_get_string(interface);

	json_object_object_get_ex(obj, "ca", &a);
	json_object_object_get_ex(obj, "key", &k);
	json_object_object_get_ex(obj, "cert", &c);
	if (a && k && c) {
		(*currvh)->vh_info.ssl_cert_filepath = json_object_get_string(c);
		(*currvh)->vh_info.ssl_private_key_filepath = json_object_get_string(k);
		(*currvh)->vh_info.ssl_ca_filepath = json_object_get_string(a);
		cfg->any_ssl = true;
		(*currvh)->vh_info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	}

	json_object_object_get_ex(obj, "ubusx_acl", &arg);
	if (arg) {
		lwsl_notice("ubusx ACL: \"%s\"\n", json_object_get_string(arg));
		ubusx_acl__add_objects((char *)json_object_get_string(arg));
	}

	json_object_object_get_ex(obj, "origin", &arg);
	if (arg)
		append_origins(arg, *currvh);

	json_object_object_get_ex(obj, "origin_check", &arg);
	if (arg && !json_object_get_boolean(arg))
		(*currvh)->vh_ctx.restrict_origins = 0;

	json_object_object_get_ex(obj, "ipv6", &arg);
	if (arg && json_object_get_boolean(arg)) {
		(*currvh)->vh_info.options &= ~LWS_SERVER_OPTION_DISABLE_IPV6;
	}

	json_object_object_get_ex(obj, "ipv6only", &arg);
	if (arg && json_object_get_boolean(arg)) {
		(*currvh)->vh_info.options &= ~LWS_SERVER_OPTION_DISABLE_IPV6;
		(*currvh)->vh_info.options |= LWS_SERVER_OPTION_IPV6_V6ONLY_MODIFY | LWS_SERVER_OPTION_IPV6_V6ONLY_VALUE;
	}

	json_object_object_get_ex(obj, "restrict_to_user", &arg);
	if (arg && json_object_is_type(arg, json_type_array)) {
		for (i = 0; i < json_object_array_length(arg); i++) {
			struct json_object *elem = json_object_array_get_idx(arg, i);
			if (!elem)
				continue;

			struct str_list *str = malloc(sizeof *str);
			if (!str)
				continue;

			str->str = json_object_get_string(elem);
			list_add_tail(&str->list, &(*currvh)->vh_ctx.users);
		}
	}

out:
	return rv;
}

char *file_to_str(char *filename)
{
	char *buffer = NULL;
	int string_size, read_size;
	FILE *handler = fopen(filename, "r");

	if (!handler)
		return NULL;

	fseek(handler, 0, SEEK_END);
	string_size = ftell(handler);
	rewind(handler);

	buffer = (char*) calloc(1, sizeof(char) * (string_size + 1) );

	read_size = fread(buffer, sizeof(char), string_size, handler);

	if (string_size != read_size) {
		free(buffer);
		buffer = NULL;
	}

	fclose(handler);
	return buffer;
}

int parse_global_json(struct json_object *global, struct global_config *cfg) {
	struct json_object *arg;

	json_object_object_get_ex(global, "socket", &arg);
	if (arg)
		cfg->ubus_sock_path = json_object_get_string(arg);

	json_object_object_get_ex(global, "www", &arg);
	if (arg)
		cfg->www_dirpath = json_object_get_string(arg);

	json_object_object_get_ex(global, "redirect", &arg);
	if (arg) {
		cfg->redir_to = strchr(json_object_get_string(arg), ':');
		if (cfg->redir_to) {
			*cfg->redir_to++ = '\0';
			cfg->redir_from = (char *) json_object_get_string(arg);
		}
	}

	json_object_object_get_ex(global, "www_maxage", &arg);
	if (arg)
		cfg->www_maxage = json_object_get_int(arg);

	return 0;
}

int parse_ubusx_json(struct json_object *ubusx, struct global_config *cfg) {
	struct json_object *arg, *cert, *key, *ca, *elem;
	int i;

	json_object_object_get_ex(ubusx, "peer_cert", &cert);
	json_object_object_get_ex(ubusx, "peer_key", &key);
	json_object_object_get_ex(ubusx, "peer_ca", &ca);
	if (cert && key && ca) {
		wsubus_client_set_cert_filepath(json_object_get_string(cert));
		wsubus_client_set_private_key_filepath(json_object_get_string(key));
		wsubus_client_set_ca_filepath(json_object_get_string(ca));
	}

	cfg->any_ssl = true;
	wsubus_client_enable_proxy();

	json_object_object_get_ex(ubusx, "object", &arg);
	if (arg && json_object_is_type(arg, json_type_array)) {
		for (i = 0; i < json_object_array_length(arg); i++) {
			elem = json_object_array_get_idx(arg, i);
			if (!elem)
				continue;

			wsubus_client_path_pattern_add(json_object_get_string(elem));
		}
	}

	json_object_object_get_ex(ubusx, "peer", &arg);
	if (arg) {
		for (i = 0; i < json_object_array_length(arg); i++) {
			elem = json_object_array_get_idx(arg, i);
			if (!elem)
				continue;

			const char *proto, *addr, *path;
			int port;
			if (!lws_parse_uri((char *) json_object_get_string(elem), &proto, &addr, &port, &path) &&
					strncmp(proto, "wss", 4) == 0) {
				cfg->any_ssl = true;
				wsubus_client_create(addr, port, path, CLIENT_FROM_PROGARG);
			}
		}
	}

	json_object_object_get_ex(ubusx, "prefix", &arg);
	if (arg) {
		const char *prefix = json_object_get_string(arg);
		if (prefix)
			if (strncmp(prefix, "mac", 4) == 0)
				ubusx_prefix = 1;//UBUSX_PREFIX_MAC;
	}

	json_object_object_get_ex(ubusx, "rpcd_integration", &arg);
	if (arg && json_object_get_boolean(arg))
		wsubus_client_set_rpcd_integration(true);

	json_object_object_get_ex(ubusx, "reconnect_timeout", &arg);
	if (arg) {
		extern int wsbus_client_connnection_retry_timeout;
		int time_out = json_object_get_int(arg);
		if(time_out > 0)
			wsbus_client_connnection_retry_timeout = time_out;
	}

	return 0;
}


int parse_vhosts_json(struct json_object *vhosts, struct vhinfo_list **currvh, struct global_config *cfg) {
	json_object_object_foreach(vhosts, key, val) {
		struct json_object *elem;
		int i;

		if (!val || !json_object_is_type(val, json_type_array))
			continue;

		for (i = 0; i < json_object_array_length(val); i++) {
			elem = json_object_array_get_idx(val, i);
			if (!elem)
				continue;

			create_vhost_by_json(key, elem, currvh, cfg);
		}
	}

	return 0;
}

struct json_object *parse_json_cfg(char *path, struct vhinfo_list **currvh, struct global_config *cfg)
{
	struct json_object *obj = NULL, *global, *ubusx, *vhosts;
	char *json;

	json = file_to_str(path);
	if (!json)
		goto out;

	obj = json_tokener_parse(json);
	if (!obj)
		goto out_free;

	json_object_object_get_ex(obj, "global", &global);
	json_object_object_get_ex(obj, "ubusproxy", &ubusx);
	json_object_object_get_ex(obj, "owsd-listen", &vhosts);
	if (global)
		parse_global_json(global, cfg);
	if (ubusx)
		parse_ubusx_json(ubusx, cfg);
	if (vhosts)
		parse_vhosts_json(vhosts, currvh, cfg);

out_free:
	free(json);
out:
	return obj;
}