#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include "jsmn.h"

int jsmn_get_key_index(char *key, int object_index, jsmntok_t *t, const char *json_doc);
void jsmn_array_join_lines(int array_index, const jsmntok_t *t, const char *json_doc, char *out_buf);
void jsmn_convert_line_breaks(char *out_buf, const char *buf, size_t size);

#endif /* JSON_UTILS_H  */
