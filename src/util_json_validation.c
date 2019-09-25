#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <json-c/json.h>
#include <string.h>

#include "owsd-config.h"
#include "util_json_validation.h"

int validate_json_object(struct json_object *in, const char *object, const char *method, enum schema_call_t type)
{
	return schema_validator_validate_jobj(in, object, method, type);
}

int validate_json_call_string(char *j_str)
{
	struct json_object *in;
	struct json_object *type, *params, *object, *method, *args;
	const char *obj_str;
	bool rv = true;

	in = json_tokener_parse(j_str);
	if (!in)
		return rv;

	if (!json_object_object_get_ex(in, "method", &type))
		goto out;
	if (strncmp(json_object_get_string(type), "call", 5) != 0)
		goto out;
	if (!json_object_object_get_ex(in, "params", &params))
		goto out;

	object = json_object_array_get_idx(params, 1);
	if (!object)
		goto out;
	obj_str = json_object_get_string(object);
	if (!obj_str)
		goto out;
	method = json_object_array_get_idx(params, 2);
	if (!method)
		goto out;

	args = json_object_array_get_idx(params, 3);
	/* TODO: should no args be valid or not? */
	/* if (!args)
	 *	goto out;
	 */

	rv = schema_validator_validate_jobj(args, obj_str, json_object_get_string(method), SCHEMA_INPUT_CALL);

out:
	json_object_put(in);
	return rv;
}
