#ifndef SCHEMA_H
#define SCHEMA_H

#define METHOD_NAME_MAX_LEN 64
#define OBJECT_NAME_MAX_LEN 64

#include <regex.h>

struct schema_method {
	char method_name[METHOD_NAME_MAX_LEN];

	//struct blob_attr input;
	//struct blob_attr output;

	char *input;
	char *output;

	struct avl_node avl;
};

struct schema_object {
	char object_name[OBJECT_NAME_MAX_LEN];

	char *definitions;

	bool regex;
	regex_t regex_exp;

	struct avl_tree schema_methods;
	struct avl_node avl;
};


struct schema_method *schema_get_method_schema(struct prog_context *prog, const char *object, const char *method);
const char *schema_get_definitions_schema(struct prog_context *prog, const char *object);
struct schema_object *schema_get_object_schema(struct prog_context *prog, const char *object);
void schema_setup_json(struct prog_context *prog);
void schema_flush_objects(struct prog_context *prog);

#endif