#if OWSD_JSON_VALIDATION
#ifndef UTIL_JSON_VALIDATION_H
#define UTIL_JSON_VALIDATION_H
#include <json-validator.h>

int validate_json_object(struct json_object *in, const char *object, const char *method, enum schema_call_t type);
int validate_json_call_string(char *j_str);
#endif /* UTIL_JSON_VALIDATION_H */
#endif /* WSD_HAVE_JSON_VALIDATION */
