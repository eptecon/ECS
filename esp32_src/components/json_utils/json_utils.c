#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "json_utils.h"

#if 0
#define JSMN_MAX_TOKEN_COUNT 300
static jsmntok_t t[JSMN_MAX_TOKEN_COUNT];
static jsmn_parser jsmn;
static char json_buf[128];
#endif

/*
 * jsmn_convert_line_breaks() - convert the strings "\\n" into a "\n" (C-string representation).
 * @out_buf: converted output buffer
 * @buf: non-converted input buffer
 * @size: a size of input buffer
 */
void jsmn_convert_line_breaks(char *out_buf, const char *buf, size_t size) {

	uintptr_t ptr = (uintptr_t)buf;
	uintptr_t ptr_next;
	int out_ptr = 0;

	while (true) {

		/*
		 * find a next "\n" substring
		 */	
		ptr_next = (uintptr_t)strstr((char*)ptr, "\\n");
	
		/*
		 * if it hasn't been found or it's out of the size allowed,
		 * then copy the rest of the input buf and go out
		 */
		if (ptr_next - (uintptr_t)buf >= size || (void*)ptr_next == NULL) {
			sprintf(&out_buf[out_ptr], "%.*s\n",(uintptr_t)buf + size - (uintptr_t)ptr, (char*)ptr);
			break;
		}

		/*
		 * copy the chunk into out_buf
		 */
		sprintf(&out_buf[out_ptr], "%.*s\n", (int)(ptr_next - ptr), (char*)(ptr));

		/*
		 * offset + "\n"
		 */
		out_ptr += ptr_next - ptr + 1;
	
		/*
		 * index of \n + 2
		 */
		ptr = ptr_next + 2;
	}
}


/*
 * jsmn_array_join_lines() - join the lines (strings) of the JSON array object.
 * @array_index: index of the array in jsmn tokens array.
 * @t: a pointer to jsmn tokens array.
 * @json_doc: a pointer to JSON document
 * @out_buf: buffer to where the joined strings will be placed
 *
 * The strings are joined with addition of \n (newline) on the end of each one.
 * The main purpose of this function is to bypass the lack of multiline representation
 * in JSON format.
 * This way, the only useful case is to split JSON array elements of string type together.
 * Other token types also may be splitted together, but in 99.(9)% of cases it's pointless.
 */
void jsmn_array_join_lines(int array_index, const jsmntok_t *t, const char *json_doc, char *out_buf) {
	
	int current_element_index;
	int buf_ptr = 0;
	
	for (int i = 0; i < t[array_index].size; i++) {
		/*
		 * array_index points to array object itself,
		 * array_index + 1 points to the first element on JSON array
		 */
		current_element_index = array_index + 1 + i;

		/* 
		 * copy the current element into out buffer with buf_ptr offset
		 */
		memcpy(out_buf + buf_ptr, json_doc + t[current_element_index].start, t[current_element_index].end - t[current_element_index].start);
			
		/*
		 * increase buf_ptr value by the length of the current element size
		 */
		buf_ptr += t[current_element_index].end - t[current_element_index].start;
	
		/*
		 * add newline to the end of the current line and increase buf_ptr by 1
		 */
		out_buf[buf_ptr++] = '\n';
	}
}


/*
 * jsmn_get_key_index() - Find a key index in object.
 * @key: required key string.
 * @object index: index of an object within which we're making a lookup.
 * @t: pointer to tokenized json values array.
 * @json_doc: pointer to json document string.
 * 
 * Return: Number of matching token found, -1 if not found, -2 if token passed 
 * is not an object type.
 */
int jsmn_get_key_index(char *key, int object_index, jsmntok_t *t, const char *json_doc) {
	int i = object_index;
	int root_end = t[i].end;
	int local_end;

	if (t[i].type != JSMN_OBJECT)
		return -2;

	/*
	 * As the initial index points to json object itself,
	 * then the next index will point to its key.
	 * json_doc + t[i] = "{\"object_name\" : {\"key\": \"value\", ...}}
	 * json_doc + t[i + 1] = \"object_name\"
	 */
	i++;

	for (; t[i].end <= root_end; i++) {
		/*
		 * Check if the current token is not empty
		 */
		if (t[i].start == t[i].end)
			continue;

		if (strncmp(json_doc + t[i].start, (char*)key, t[i].end - t[i].start) == 0) {
			return i;
		}	

		/*
		 * If the current token is an object
		 * and its key value is not matching 'key' string,
		 * then pass this object without going into it.
		 */
		if (t[i + 1].type == JSMN_OBJECT) {
			if (strncmp(t[i + 2].start + json_doc, (char*)key, t[i].end - t[i].start) == 0) {
				return i;
			}

			local_end = t[i + 1].end;

			do { 
				i++; 
			} while (t[i].end < local_end);
		}
	}
	/* 
	 * We haven't return in a cycle, it means no key found
	 */
	return -1;
}
#if 0
static void _debug_output_tokens(char *payload, jsmntok_t *t, int tok_num) {
	int i;
	for (i = 0; i < tok_num; i++) {
		ESP_LOGI(TAG, "t[%d].type = %d, size = %d, %.*s", i, t[i].type, t[i].size, t[i].end - t[i].start, (char*)(payload + t[i].start) );
	}
}
#endif
