/*
 * Copyright (c) 2009 Dave Gamble
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef JSON_H_
#define JSON_H_

#if defined(_WIN32)
#  if defined(EXPORT_TR50_SYMS)
#    define TR50_EXPORT __declspec(dllexport)
#  else
#    define TR50_EXPORT __declspec(dllimport)
#  endif
#else
#  define TR50_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define JSON_FALSE    0
#define JSON_TRUE     1
#define JSON_NULL     2
#define JSON_NUMBER   3
#define JSON_STRING   4
#define JSON_ARRAY    5
#define JSON_OBJECT   6
#define JSON_INTEGER   7

#define tr50_json_is_reference 256

typedef struct _JSON {
	struct _JSON *next, *prev;
	struct _JSON *child;

	int type;

	char *valuestring;
	int valueint;
	unsigned int valueuint;
	double valuedouble;
	char *string;
	long long valuelonglong;

} JSON;

// Supply a block of JSON, and this returns a JSON object you can interrogate. Call json_Delete when finished.
TR50_EXPORT JSON *tr50_json_parse(const char *value);
// Render a JSON entity to text for transfer/storage. Free the char* when finished.
TR50_EXPORT char  *tr50_json_print(JSON *item);
// Render a JSON entity to text for transfer/storage without any formatting. Free the char* when finished.
TR50_EXPORT char  *tr50_json_print_unformatted(JSON *item);
// Delete a JSON entity and all subentities.
TR50_EXPORT void   tr50_json_delete(JSON *c);

// Returns the number of items in an array (or object).
TR50_EXPORT int	  tr50_json_get_array_size(JSON *array);
// Retrieve item number "item" from array "array". Returns NULL if unsuccessful.
TR50_EXPORT JSON *tr50_json_get_array_item(JSON *array, int item);
// Looping through the children of an object or array.
typedef void(*json_fefunc)(JSON *item, void *custom);
TR50_EXPORT int	 tr50_json_object_for_each(JSON *object, json_fefunc func, void *custom);
// Get item "string" from object. Case insensitive.
TR50_EXPORT JSON *tr50_json_get_object_item(JSON *object, const char *string);

TR50_EXPORT const char   *tr50_json_get_object_item_as_string(JSON *object, const char *string);
TR50_EXPORT int          *tr50_json_get_object_item_as_int(JSON *object, const char *string);
TR50_EXPORT long long    *tr50_json_get_object_item_as_longlong(JSON *object, const char *string);
TR50_EXPORT unsigned int *tr50_json_get_object_item_as_uint(JSON *object, const char *string);
TR50_EXPORT double       *tr50_json_get_object_item_as_double(JSON *object, const char *string);
TR50_EXPORT int          *tr50_json_get_object_item_as_bool(JSON *object, const char *string);

TR50_EXPORT const char   *tr50_json_get_array_item_as_string(JSON *object, int item);
TR50_EXPORT int          *tr50_json_get_array_item_as_int(JSON *object, int item);
TR50_EXPORT long long          *tr50_json_get_array_item_as_longlong(JSON *object, int item);
TR50_EXPORT unsigned int *tr50_json_get_array_item_as_uint(JSON *object, int item);
TR50_EXPORT double       *tr50_json_get_array_item_as_double(JSON *object, int item);
TR50_EXPORT int          *tr50_json_get_array_item_as_bool(JSON *object, int item);

// These calls create a JSON item of the appropriate type.
TR50_EXPORT JSON *tr50_json_create_null();
TR50_EXPORT JSON *tr50_json_create_true();
TR50_EXPORT JSON *tr50_json_create_false();
TR50_EXPORT JSON *tr50_json_create_integer(long long num);
TR50_EXPORT JSON *tr50_json_create_number(double num);
TR50_EXPORT JSON *tr50_json_create_string(const char *string);
TR50_EXPORT JSON *tr50_json_create_array();
TR50_EXPORT JSON *tr50_json_create_object();

// These utilities create an Array of count items.
TR50_EXPORT JSON *tr50_json_create_bool_array(char *numbers, int count);
TR50_EXPORT JSON *tr50_json_create_longlong_array(long long *numbers, int count);
TR50_EXPORT JSON *tr50_json_create_int_array(int *numbers, int count);
TR50_EXPORT JSON *tr50_json_create_float_array(float *numbers, int count);
TR50_EXPORT JSON *tr50_json_create_double_array(double *numbers, int count);
TR50_EXPORT JSON *tr50_json_create_string_array(const char **strings, int count);

// Append item to the specified array/object.
TR50_EXPORT void    tr50_json_add_item_to_array(JSON *array, JSON *item);
TR50_EXPORT void	tr50_json_add_item_to_object(JSON *object, const char *string, JSON *item);
// Append reference to item to the specified array/object. Use this when you want to add an existing JSON to a new JSON, but don't want to corrupt your existing JSON.
TR50_EXPORT void    tr50_json_add_item_reference_to_array(JSON *array, JSON *item);
TR50_EXPORT void	tr50_json_add_item_reference_to_object(JSON *object, const char *string, JSON *item);

// Remove/Detatch items from Arrays/Objects.
TR50_EXPORT JSON *tr50_json_detach_item_from_array(JSON *array, int which);
TR50_EXPORT void   tr50_json_delete_item_from_array(JSON *array, int which);
TR50_EXPORT JSON *tr50_json_detach_item_from_object(JSON *object, const char *string);
TR50_EXPORT void   tr50_json_delete_item_from_object(JSON *object, const char *string);

// Update array items.
TR50_EXPORT void tr50_json_replace_item_in_array(JSON *array, int which, JSON *newitem);
TR50_EXPORT void tr50_json_replace_item_in_object(JSON *object, const char *string, JSON *newitem);

#define tr50_json_add_null_to_object(object,name)	tr50_json_add_item_to_object(object, name, tr50_json_create_null())
#define tr50_json_add_true_to_object(object,name)	tr50_json_add_item_to_object(object, name, tr50_json_create_true())
#define tr50_json_add_false_to_object(object,name)		tr50_json_add_item_to_object(object, name, tr50_json_create_false())
#define tr50_json_add_bool_to_object(object,name,v)	tr50_json_add_item_to_object(object, name, (v)?tr50_json_create_true():tr50_json_create_false())
#define tr50_json_add_number_to_object(object,name,n)	tr50_json_add_item_to_object(object, name, tr50_json_create_number(n))
#define tr50_json_add_string_to_object(object,name,s)	tr50_json_add_item_to_object(object, name, tr50_json_create_string(s))
#define tr50_json_add_integer_to_object(object,name,n)	tr50_json_add_item_to_object(object, name, tr50_json_create_integer(n))

#ifdef __cplusplus
}
#endif

#endif /*TR50_UTIL_json_H_*/
