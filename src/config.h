#ifndef CONFIG_H
#define CONFIG_H

struct vhinfo_list {
	struct lws_context_creation_info vh_info;
	struct vhinfo_list *next;
	struct vh_context vh_ctx;
};

struct global_config {
	#if WSD_HAVE_UBUS
	const char *ubus_sock_path;
	#endif
	const char *www_dirpath;
	int www_maxage;
	char *redir_from;
	char *redir_to;
	char *cgi_from;
	char *cgi_to;
	bool any_ssl;
};

int new_vhinfo_list(struct vhinfo_list **currvh);
int append_origins(struct json_object *cfg, struct vhinfo_list *currvh);
int create_vhost_by_json(char *key, struct json_object *obj, struct vhinfo_list **currvh, struct global_config *cfg);
char *file_to_str(char *filename);
int parse_global_json(struct json_object *global, struct global_config *cfg);
int parse_ubusx_json(struct json_object *ubusx, struct global_config *cfg);
int parse_vhosts_json(struct json_object *vhosts, struct vhinfo_list **currvh, struct global_config *cfg);
struct json_object *parse_json_cfg(char *path, struct vhinfo_list **currvh, struct global_config *cfg);

#endif