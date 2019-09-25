#include <libubox/avl.h>
#include <libubox/avl-cmp.h>
#include <libubox/blobmsg.h>
#include <libubox/blobmsg_json.h>
#include <libubox/utils.h>
#include <libubus.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "schema.h"

#define JSON_SCHEMA_DIR "/usr/share/rpcd/schemas"

static void schema_flush_method(struct schema_method *s_method)
{
    lwsl_notice("cleaning method %s\n", (char *)s_method->avl.key);

    if (s_method->input)
        free(s_method->input);
    if (s_method->output)
        free(s_method->output);
    if (s_method->avl.key)
        free((void *) s_method->avl.key);

    free(s_method);
	return;
}

static void schema_flush_methods(struct schema_object *s_object)
{
    struct schema_method *s_method, *tmp;

    lwsl_notice("flushing all methods of object %s\n", (char *)s_object->avl.key);

    avl_for_each_element_safe(&s_object->schema_methods, s_method, avl, tmp) {
        avl_delete(&s_object->schema_methods, &s_method->avl);
        schema_flush_method(s_method);
    }

	return;
}

static void schema_flush_object(struct schema_object *s_object)
{
    lwsl_notice("cleaning object %s\n", (char *)s_object->avl.key);

    schema_flush_methods(s_object);
    free((void *)s_object->avl.key);
    if (s_object->definitions)
        free(s_object->definitions);

	if (s_object->regex)
		regfree(&s_object->regex_exp);

    free(s_object);

	return;
}

void schema_flush_objects(struct prog_context *prog)
{
    struct schema_object *s_object, *tmp;

    lwsl_notice("cleaning all schema objects\n");

    avl_for_each_element_safe(&prog->schema_objects, s_object, avl, tmp) {
        avl_delete(&prog->schema_objects, &s_object->avl);
        schema_flush_object(s_object);
    }
	return;
}

static void schema_parse_object(struct prog_context *prog, struct schema_object *schema_object, struct blob_buf *acl, struct blob_attr *object)
{
	struct blob_attr *method, *properties;
	int rem, rem2;
	struct schema_method *s_method;

	blobmsg_for_each_attr(properties, object, rem) {
		if (strncmp(blobmsg_name(properties), "properties", 11))
			continue;

		//lwsl_notice("%s %d method name = %s\n", __func__, __LINE__, blobmsg_name(object));
		s_method = calloc(1, sizeof(*s_method));
		if (!s_method)
			continue;

		s_method->avl.key = strdup(blobmsg_name(object));
		avl_insert(&schema_object->schema_methods, &s_method->avl);

		blobmsg_for_each_attr(method, properties, rem2) {
			//lwsl_notice("%s %d name = %s\n", __func__, __LINE__, blobmsg_name(method));
			if (!strncmp(blobmsg_name(method), "input", 6)) {
				s_method->input = blobmsg_format_json(method, true);
                //lwsl_notice("input = %s\n", s_method->input);
			}
			else if (!strncmp(blobmsg_name(method), "output", 7)) {
				s_method->output = blobmsg_format_json(method, true);
                //lwsl_notice("output = %s\n", s_method->output);
			}
		}
	}
}

static void
schema_setup_file(struct prog_context *prog, const char *path)
{
	struct blob_buf acl = { 0 };
	struct blob_attr *root, *object;
	int rem, rem2;
	struct schema_object *schema_object;
    char *definitions = NULL;

	blob_buf_init(&acl, 0);

	if (!blobmsg_add_json_from_file(&acl, path)) {
		fprintf(stderr, "Failed to parse %s\n", path);
		goto out;
	}

	schema_object = calloc(1, sizeof(*schema_object));
	if (!schema_object)
		goto out;

	avl_init(&schema_object->schema_methods, avl_strcmp, false, NULL);

	/* find the key properties of root */
	blob_for_each_attr(root, acl.head, rem) {
		if (!strncmp(blobmsg_name(root), "regex", 6)) {
			schema_object->regex = blobmsg_get_bool(root);
			lwsl_notice("%s: regex enabled %d\n", __func__, schema_object->regex);

		}

		if (!strncmp(blobmsg_name(root), "object", 7)) {
            schema_object->avl.key = strdup(blobmsg_data(root));
            if (!schema_object->avl.key)
                goto out_flush;
        }

		if (!strncmp(blobmsg_name(root), "definitions", 12))
			definitions = blobmsg_format_json(root, true);

		if (strncmp(blobmsg_name(root), "properties", 11))
			continue;

		/* iterate all methods */
		blobmsg_for_each_attr(object, root, rem2) {
			if (blobmsg_type(object) != BLOBMSG_TYPE_TABLE)
				continue;

			schema_parse_object(prog, schema_object, &acl, object);
		}
	}

	if (schema_object->regex) {
		int rv;
		lwsl_notice("parsing regex for %s!\n", schema_object->avl.key );
		rv = regcomp(&schema_object->regex_exp, (char *) schema_object->avl.key, 0);
		if (rv) {
			lwsl_notice("%s: invalid regex: %s, flushing validation schema!\n", __func__, (char *) schema_object->avl.key);
			goto out_flush;
		}
	}

	if (!schema_object->avl.key)
		goto out_flush;

	schema_object->definitions = definitions;
	avl_insert(&prog->schema_objects, &schema_object->avl);
    /*avl_for_each_element(&prog->schema_objects, schema_object, avl) {
        lwsl_notice("%s %d key = %s\n", __func__, __LINE__, (char *) schema_object->avl.key);
    }*/
out:
	blob_buf_free(&acl);
	return;
out_flush:
	schema_flush_object(schema_object);
}

struct schema_method *schema_get_method_schema(struct prog_context *prog, const char *object, const char *method)
{
    struct schema_object *s_object;
    struct schema_method *s_method;

    lwsl_notice("%s: object = %s method = %s\n", __func__, object, method);

    s_object = schema_get_object_schema(prog, object);
    if (!s_object)
        return NULL;

    avl_for_each_element(&s_object->schema_methods, s_method, avl) {
        lwsl_notice("%s: method = %s, avl.key = %s\n", __func__, method, (char *)s_method->avl.key);
        if (!strncmp(method, (char *) s_method->avl.key, METHOD_NAME_MAX_LEN))
            return s_method;
    }
    return NULL;
}

const char *schema_get_definitions_schema(struct prog_context *prog, const char *object)
{
    struct schema_object *s_object;

    s_object = schema_get_object_schema(prog, object);
    if (!s_object)
        return NULL;

    return s_object->definitions;
}

struct schema_object *schema_get_object_schema(struct prog_context *prog, const char *object)
{
    struct schema_object *s_object;

    lwsl_notice("%s: object = %s\n", __func__, object);

    avl_for_each_element(&prog->schema_objects, s_object, avl) {
        lwsl_notice("%s: object = %s, avl.key = %s, regex = %d\n", __func__, object, (char *)s_object->avl.key, s_object->regex);
		if (s_object->regex) {
			if (!regexec(&s_object->regex_exp, object, 0, NULL, 0))
				return s_object;
		} else {
			if (!strncmp(object, (char *) s_object->avl.key, OBJECT_NAME_MAX_LEN))
				return s_object;
		}
    }

    return NULL;
}

void
schema_setup_json(struct prog_context *prog)
{
	int i;
	glob_t gl;

	avl_init(&prog->schema_objects, avl_strcmp, false, NULL);

	if (glob(JSON_SCHEMA_DIR "/*.json", 0, NULL, &gl))
		return;

	for (i = 0; i < gl.gl_pathc; i++) {
        lwsl_notice("path = %s\n", gl.gl_pathv[i]);
		schema_setup_file(prog, gl.gl_pathv[i]);
    }

	globfree(&gl);
}
