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

#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <tr50/util/json.h>
#include <tr50/util/memory.h>
#include <tr50/util/platform.h>

#define TRUE		1
#define FALSE		0

int tr50_json_str_case_cmp(const char *s1,const char *s2) {
	if (!s1) return (s1==s2)?0:1;if (!s2) return 1;
	for(; tolower(*s1) == tolower(*s2); ++s1, ++s2)	if(*s1 == 0)	return 0;
	return tolower(*(const unsigned char *)s1) - tolower(*(const unsigned char *)s2);
}

char* tr50_json_strdup(const char* str) {
	size_t len;
	char* copy;
	if(str==NULL) str="";
	len = strlen(str) + 1;
	if (!(copy = (char*)_memory_malloc(len))) return 0;
	_memory_memcpy(copy,(char*)str,len);
	return copy;
}

// Internal constructor.
JSON *tr50_json_new_item() {
	JSON* node = (JSON*)_memory_malloc(sizeof(JSON));
	if(node) _memory_memset(node,0,sizeof(JSON));
	return node;
}

// Delete a JSON structure.
void tr50_json_delete(JSON *c) {
	JSON *next;
	if(c==NULL) return;
	while(c) {
		next=c->next;
		if (!(c->type&tr50_json_is_reference) && c->child) tr50_json_delete(c->child);
		if (!(c->type&tr50_json_is_reference) && c->valuestring) _memory_free(c->valuestring);
		if(c->string) _memory_free(c->string);
		_memory_free(c);
		c=next;
	}
}

// Parse the input text to generate a number, and populate the result into item.
const char *parse_number(JSON *item,const char *num)
{
	int flag = FALSE;
	double n = 0, sign = 1, scale = 0; int subscale = 0, signsubscale = 1;
	long long ln = 0;
	// Could use sscanf for this?
	if (*num == '-') sign = -1, num++;	// Has sign?
	if (*num == '0') num++;			// is zero
	if (*num >= '1' && *num <= '9') {
		do {
			ln = (ln * 10) + (*num++ - '0');
		} while (*num >= '0' && *num <= '9');	// Number?
	}
	n = (double)ln;
	if (*num == '.') { num++; flag = TRUE;   do	n = (n*10.0) + (*num++ - '0'), scale--; while (*num >= '0' && *num <= '9'); }	// Fractional part?
	if (*num == 'e' || *num == 'E')		// Exponent?
	{
		flag = TRUE; num++; if (*num == '+') num++;	else if (*num == '-') signsubscale = -1, num++;		// With sign?
		while (*num >= '0' && *num <= '9') subscale = (subscale * 10) + (*num++ - '0');	// Number?
	}
	ln = sign*ln;
	n = sign*n*pow(10.0, (scale + subscale*signsubscale));	// number = +/- number.fraction * 10^+/- exponent
	item->valuedouble = n;
	if (flag == FALSE) { item->valueint = (int)ln; } else { item->valueint = (int)n; }
	if (flag == FALSE) { item->valuelonglong = (long long)ln; } else { item->valuelonglong = (long long)n; }
	if (flag == FALSE) { item->valueuint = (unsigned int)ln; } else { item->valueuint = (unsigned int)n; }
	item->type = JSON_NUMBER;
	return num;
}

// Render the number nicely from the given item into a string.
char *print_number(JSON *item)
{
	char *str;
	double d=item->valuedouble;
	if (fabs(((double)item->valuelonglong)-d)<=DBL_EPSILON)
	{
		str=(char*)_memory_malloc(21);	// 2^64+1 can be represented in 21 chars.
		sprintf(str,"%lld",item->valuelonglong);
	}
	else
	{
		str=(char*)_memory_malloc(64);	// This is a nice tradeoff.
		if (fabs(floor(d)-d)<=DBL_EPSILON)			sprintf(str,"%.0f",d);
		else if (fabs(d)<1.0e-6 || fabs(d)>1.0e9)	sprintf(str,"%e",d);
		else										sprintf(str,"%f",d);
	}
	return str;
}

// Parse the input text into an unescaped cstring, and populate item.
const char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
const char *parse_string(JSON *item,const char *str)
{
	const char *ptr=str+1;char *ptr2;char *out;int len=0;unsigned uc;
	if (*str!='\"') return 0;	// not a string!

	while (*ptr!='\"' && (unsigned char)*ptr>31 && ++len) if (*ptr++ == '\\') ptr++;	// Skip escaped quotes.

	out=(char*)_memory_malloc(len+1);	// This is how long we need for the string, roughly.
	if (!out) return 0;

	ptr=str+1;ptr2=out;
	while (*ptr!='\"' && (unsigned char)*ptr>31)
	{
		if (*ptr!='\\') *ptr2++=*ptr++;
		else
		{
			ptr++;
			switch (*ptr)
			{
			case 'b': *ptr2++='\b';	break;
			case 'f': *ptr2++='\f';	break;
			case 'n': *ptr2++='\n';	break;
			case 'r': *ptr2++='\r';	break;
			case 't': *ptr2++='\t';	break;
			case 'u':	 // transcode utf16 to utf8. DOES NOT SUPPORT SURROGATE PAIRS CORRECTLY.
				sscanf(ptr+1,"%4x",&uc);	// get the unicode char.
				len=3;if (uc<0x80) len=1;else if (uc<0x800) len=2;ptr2+=len;

				switch (len) {
				case 3: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
				case 2: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
				case 1: *--ptr2 =(uc | firstByteMark[len]);
				}
				ptr2+=len;ptr+=4;
				break;
			default:  *ptr2++=*ptr; break;
			}
			ptr++;
		}
	}
	*ptr2=0;
	if (*ptr=='\"') ptr++;
	item->valuestring=out;
	item->type=JSON_STRING;
	return ptr;
}

// Render the cstring provided to an escaped version that can be printed.
char *print_string_ptr(const char *str)
{
	const char *ptr;char *ptr2,*out;int len=0;

	if (!str) return tr50_json_strdup("");
	ptr=str;while (*ptr && ++len) {if ((unsigned char)*ptr<32 || *ptr=='\"' || *ptr=='\\') len++;ptr++;}

	out=(char*)_memory_malloc(len+3);
	ptr2=out;ptr=str;
	*ptr2++='\"';
	while (*ptr)
	{
		if ((unsigned char)*ptr>31 && *ptr!='\"' && *ptr!='\\') *ptr2++=*ptr++;
		else
		{
			*ptr2++='\\';
			switch (*ptr++)
			{
			case '\\':	*ptr2++='\\';	break;
			case '\"':	*ptr2++='\"';	break;
			case '\b':	*ptr2++='b';	break;
			case '\f':	*ptr2++='f';	break;
			case '\n':	*ptr2++='n';	break;
			case '\r':	*ptr2++='r';	break;
			case '\t':	*ptr2++='t';	break;
			default: ptr2--;	break;	// eviscerate with prejudice.
			}
		}
	}
	*ptr2++='\"';*ptr2++=0;
	return out;
}
// Invote print_string_ptr (which is useful) on an item.
char *print_string(JSON *item)	{return print_string_ptr(item->valuestring);}

// Predeclare these prototypes.
const char *parse_value(JSON *item,const char *value);
char *print_value(JSON *item,int depth,int fmt);
const char *parse_array(JSON *item,const char *value);
char *print_array(JSON *item,int depth,int fmt);
const char *parse_object(JSON *item,const char *value);
char *print_object(JSON *item,int depth,int fmt);

// Utility to jump whitespace and cr/lf
const char *skip(const char *in) {
	if(in==NULL) return NULL;
	while(*in!=0 && (unsigned char)*in<=32) {
		in++;
	}
	return in;
}

// Parse an object - create a new root, and populate.
JSON *tr50_json_parse(const char *value)
{
	JSON *c = tr50_json_new_item();
	if (!c) return 0;       /* memory fail */

	if (!parse_value(c, skip(value))) { tr50_json_delete(c); return 0; }
	return c;
}

// Render a JSON item/entity/structure to text.
char *tr50_json_print(JSON *item) { return print_value(item, 0, 0); }
char *tr50_json_print_unformatted(JSON *item) { return print_value(item, 0, 0); }

// Parser core - when encountering text, process appropriately.
const char *parse_value(JSON *item,const char *value)
{
	if (!value)						return 0;	// Fail on null.
	if (!strncmp(value,"null",4))	{ item->type=JSON_NULL;  return value+4; }
	if (!strncmp(value,"false",5))	{ item->type=JSON_FALSE; return value+5; }
	if (!strncmp(value,"true",4))	{ item->type=JSON_TRUE; item->valueint=1;	return value+4; }
	if (*value=='\"')				{ return parse_string(item,value); }
	if (*value=='-' || (*value>='0' && *value<='9'))	{ return parse_number(item,value); }
	if (*value=='[')				{ return parse_array(item,value); }
	if (*value=='{')				{ return parse_object(item,value); }

	return 0;	// failure.
}

// Render a value to text.
char *print_value(JSON *item,int depth,int fmt)
{
	char *out=0;
	if (!item) return 0;
	switch ((item->type)&255)
	{
	case JSON_NULL:	out = tr50_json_strdup("null");	break;
	case JSON_FALSE:	out = tr50_json_strdup("false"); break;
	case JSON_TRUE:	out = tr50_json_strdup("true"); break;
	case JSON_NUMBER:	out=print_number(item);break;
	case JSON_STRING:	out=print_string(item);break;
	case JSON_ARRAY:	out=print_array(item,depth,fmt);break;
	case JSON_OBJECT:	out=print_object(item,depth,fmt);break;
	}
	return out;
}

// Build an array from input text.
const char *parse_array(JSON *item,const char *value)
{
	JSON *child;
	if (*value!='[')	return 0;	// not an array!

	item->type=JSON_ARRAY;
	value=skip(value+1);
	if (*value==']') return value+1;	// empty array.

	item->child = child = tr50_json_new_item();
	if (!item->child) return 0;		 // memory fail
	value=skip(parse_value(child,skip(value)));	// skip any spacing, get the value.
	if (!value) return 0;

	while (*value==',')
	{
		JSON *new_item;
		if (!(new_item = tr50_json_new_item())) return 0; 	// memory fail
		child->next=new_item;new_item->prev=child;child=new_item;
		value=skip(parse_value(child,skip(value+1)));
		if (!value) return 0;	// memory fail
	}

	if (*value==']') return value+1;	// end of array
	return 0;	// malformed.
}

// Render an array to text
char *print_array(JSON *item,int depth,int fmt)
{
	char **entries;
	char *out=0,*ptr,*ret;int len=5;
	JSON *child=item->child;
	int numentries=0,i=0,fail=0;

	// How many entries in the array?
	while (child) numentries++,child=child->next;
	// If nothing, just close it with brackets
	if (!numentries) return _memory_clone("[]",2);
	// Allocate an array to hold the values for each
	entries=(char**)_memory_malloc(numentries*sizeof(char*));
	_memory_memset(entries,0,numentries*sizeof(char*));
	// Retrieve all the results:
	child=item->child;
	while (child && !fail)
	{
		ret=print_value(child,depth+1,fmt);
		entries[i++]=ret;
		if (ret) len+=strlen(ret)+2+(fmt?1:0); else fail=1;
		child=child->next;
	}

	// If we didn't fail, try to malloc the output string
	if (!fail) out=_memory_malloc(len);
	// If that fails, we fail.
	if (!out) fail=1;

	// Handle failure.
	if (fail)
	{
		for (i=0;i<numentries;i++) if (entries[i]) _memory_free(entries[i]);
		_memory_free(entries);
		return 0;
	}

	// Compose the output array.
	*out='[';
	ptr=out+1;*ptr=0;
	for (i=0;i<numentries;i++)
	{
		strcpy(ptr,entries[i]);ptr+=strlen(entries[i]);
		if (i!=numentries-1) {*ptr++=',';if(fmt)*ptr++=' ';*ptr=0;}
		_memory_free(entries[i]);
	}
	_memory_free(entries);
	*ptr++=']';*ptr++=0;
	return out;
}

// Build an object from the text.
const char *parse_object(JSON *item,const char *value) {
	JSON *child;
	if (*value!='{')	return 0;	// not an object!

	item->type=JSON_OBJECT;
	value=skip(value+1);
	if (*value=='}') return value+1;	// empty array.

	item->child = child = tr50_json_new_item();
	value=skip(parse_string(child,skip(value)));
	if (!value) return 0;
	child->string=child->valuestring;child->valuestring=0;
	if (*value!=':') return 0;	// fail!
	value=skip(parse_value(child,skip(value+1)));	// skip any spacing, get the value.
	if (!value) return 0;

	while(*value==',') {
		JSON *new_item;
		if (!(new_item = tr50_json_new_item()))	return 0; // memory fail
		child->next=new_item;new_item->prev=child;child=new_item;
		value=skip(parse_string(child,skip(value+1)));
		if (!value) return 0;
		child->string=child->valuestring;child->valuestring=0;
		if (*value!=':') return 0;	// fail!
		value=skip(parse_value(child,skip(value+1)));	// skip any spacing, get the value.
		if (!value) return 0;
	}

	if (*value=='}') return value+1;	// end of array
	return 0;	// malformed.
}

// Render an object to text.
char *print_object(JSON *item,int depth,int fmt) {
	char **entries=0,**names=0;
	char *out=0,*ptr,*ret,*str;int len=7,i=0,j;
	JSON *child=item->child;
	int numentries=0,fail=0;
	// Count the number of entries.
	while (child) numentries++,child=child->next;
	// Allocate space for the names and the objects
	entries=(char**)_memory_malloc((1+numentries)*sizeof(char*));
	if (!entries) return 0;
	names=(char**)_memory_malloc((1+numentries)*sizeof(char*));
	if (!names) {_memory_free(entries);return 0;}
	_memory_memset(entries,0,sizeof(char*)*numentries);
	_memory_memset(names,0,sizeof(char*)*numentries);

	// Collect all the results into our arrays:
	child=item->child;depth++;if (fmt) len+=depth;
	while(child) {
		names[i]=str=print_string_ptr(child->string);
		entries[i++]=ret=print_value(child,depth,fmt);
		if (str && ret) len+=strlen(ret)+strlen(str)+2+(fmt?2+depth:0); else fail=1;
		child=child->next;
	}

	// Try to allocate the output string
	if(!fail) out=(char*)_memory_malloc(len);
	if(!out) fail=1;

	// Handle failure
	if(fail) {
		for (i=0;i<numentries;i++) {if (names[i]) _memory_free(names[i]);if (entries[i]) _memory_free(entries[i]);}
		_memory_free(names);_memory_free(entries);
		return 0;
	}

	// Compose the output:
	*out='{';ptr=out+1;if (fmt)*ptr++='\n';*ptr=0;
	for (i=0;i<numentries;i++) {
		if (fmt) for (j=0;j<depth;j++) *ptr++='\t';
		strcpy(ptr,names[i]);ptr+=strlen(names[i]);
		*ptr++=':';if (fmt) *ptr++='\t';
		strcpy(ptr,entries[i]);ptr+=strlen(entries[i]);
		if (i!=numentries-1) *ptr++=',';
		if (fmt) *ptr++='\n';*ptr=0;
		_memory_free(names[i]);_memory_free(entries[i]);
	}

	_memory_free(names);_memory_free(entries);
	if (fmt) for (i=0;i<depth-1;i++) *ptr++='\t';
	*ptr++='}';*ptr++=0;
	return out;
}

// Get Array size/item / object item.
int   tr50_json_get_array_size(JSON *array) { JSON *c; int i = 0; if (array == NULL) return 0; c = array->child; while (c)i++, c = c->next; return i; }
JSON *tr50_json_get_array_item(JSON *array, int item) { JSON *c; if (array == NULL) return NULL; c = array->child;  while (c && item>0) item--, c = c->next; return c; }
JSON *tr50_json_get_object_item(JSON *object, const char *string) { JSON *c; if (object == NULL) return NULL; c = object->child; while (c && tr50_json_str_case_cmp(c->string, string)) c = c->next; return c; }

const char *tr50_json_get_object_item_as_string(JSON *object, const char *string) {
	JSON *c = tr50_json_get_object_item(object, string);
	if(c==NULL) return NULL;
	if(c->type==JSON_STRING) {
		return c->valuestring;
	} else {
		return NULL;
	}
}

int *tr50_json_get_object_item_as_int(JSON *object, const char *string) {
	JSON *c = tr50_json_get_object_item(object, string);
	if(c==NULL) return NULL;
	if(c->type==JSON_NUMBER) {
		return &c->valueint;
	} else {
		return NULL;
	}
}

long long *tr50_json_get_object_item_as_longlong(JSON *object, const char *string) {
	JSON *c = tr50_json_get_object_item(object, string);
	if (c == NULL) return NULL;
	if (c->type == JSON_NUMBER) {
		return &c->valuelonglong;
	} else {
		return NULL;
	}
}

unsigned int *tr50_json_get_object_item_as_uint(JSON *object, const char *string) {
	JSON *c = tr50_json_get_object_item(object, string);
	if(c==NULL) return NULL;
	if(c->type==JSON_NUMBER) {
		return &c->valueuint;
	} else {
		return NULL;
	}
}

double *tr50_json_get_object_item_as_double(JSON *object, const char *string) {
	JSON *c = tr50_json_get_object_item(object, string);
	if(c==NULL) return NULL;
	if(c->type==JSON_NUMBER) {
		return &c->valuedouble;
	} else {
		return NULL;
	}
}

int *tr50_json_get_object_item_as_bool(JSON *object, const char *string) {
	JSON *c = tr50_json_get_object_item(object, string);
	if(c==NULL) return NULL;
	if(c->type==JSON_TRUE) {
		c->valueint=TRUE;
		return &c->valueint;
	} else if(c->type==JSON_FALSE) {
		c->valueint=FALSE;
		return &c->valueint;
	} else {
		return NULL;
	}
}

const char *tr50_json_get_array_item_as_string(JSON *object, int item) {
	JSON *c = tr50_json_get_array_item(object, item);
	if(c==NULL) return NULL;
	if(c->type==JSON_STRING) {
		return c->valuestring;
	} else {
		return NULL;
	}
}

int *tr50_json_get_array_item_as_int(JSON *object, int item) {
	JSON *c = tr50_json_get_array_item(object, item);
	if(c==NULL) return NULL;
	if(c->type==JSON_NUMBER) {
		return &c->valueint;
	} else {
		return NULL;
	}
}
long long *tr50_json_get_array_item_as_longlong(JSON *object, int item) {
	JSON *c = tr50_json_get_array_item(object, item);
	if (c == NULL) return NULL;
	if (c->type == JSON_NUMBER) {
		return &c->valuelonglong;
	} else {
		return NULL;
	}
}

unsigned int *tr50_json_get_array_item_as_uint(JSON *object, int item) {
	JSON *c = tr50_json_get_array_item(object, item);
	if(c==NULL) return NULL;
	if(c->type==JSON_NUMBER) {
		return &c->valueuint;
	} else {
		return NULL;
	}
}

double *tr50_json_get_array_item_as_double(JSON *object, int item) {
	JSON *c = tr50_json_get_array_item(object, item);
	if(c==NULL) return NULL;
	if(c->type==JSON_NUMBER) {
		return &c->valuedouble;
	} else {
		return NULL;
	}
}

int *tr50_json_get_array_item_as_bool(JSON *object, int item) {
	JSON *c = tr50_json_get_array_item(object, item);
	if(c==NULL) return NULL;
	if(c->type==JSON_TRUE) {
		c->valueint=TRUE;
		return &c->valueint;
	} else if(c->type==JSON_FALSE) {
		c->valueint=FALSE;
		return &c->valueint;
	} else {
		return NULL;
	}
}

int json_ObjectForeach(JSON *object, json_fefunc callback, void *custom) {
	JSON *ptr=object->child;
	while(ptr!=NULL) {
		callback(ptr,custom);
		ptr=ptr->next;
	}
	return 0;
}

// Utility for array list handling.
void suffix_object(JSON *prev,JSON *item) {prev->next=item;item->prev=prev;}
// Utility for handling references.
JSON *create_reference(JSON *item) { JSON *ref = tr50_json_new_item(); memcpy(ref, item, sizeof(JSON)); ref->string = 0; ref->type |= tr50_json_is_reference; ref->next = ref->prev = 0; return ref; }

// Add item to array/object.
void	tr50_json_add_item_to_array(JSON *array, JSON *item) { JSON *c = array->child; if (!c) { array->child = item; } else { while (c && c->next) c = c->next; suffix_object(c, item); } }
void	tr50_json_add_item_to_object(JSON *object, const char *string, JSON *item) { if (item->string) _memory_free(item->string); item->string = tr50_json_strdup(string); tr50_json_add_item_to_array(object, item); }
void	tr50_json_add_item_reference_to_array(JSON *array, JSON *item) { tr50_json_add_item_to_array(array, create_reference(item)); }
void	tr50_json_add_item_reference_to_object(JSON *object, const char *string, JSON *item) { tr50_json_add_item_to_object(object, string, create_reference(item)); }

JSON *tr50_json_detach_item_from_array(JSON *array, int which) {
	JSON *c = array->child; while (c && which>0) c = c->next, which--; if (!c) return 0;
if (c->prev) c->prev->next=c->next;if (c->next) c->next->prev=c->prev;if (c==array->child) array->child=c->next;c->prev=c->next=0;return c;}
void   tr50_json_delete_item_from_array(JSON *array, int which) { tr50_json_delete(tr50_json_detach_item_from_array(array, which)); }
JSON *tr50_json_detach_item_from_object(JSON *object, const char *string) { int i = 0; JSON *c = object->child; while (c && tr50_json_str_case_cmp(c->string, string)) i++, c = c->next; if (c) return tr50_json_detach_item_from_array(object, i); return 0; }
void   tr50_json_delete_item_from_object(JSON *object, const char *string) { tr50_json_delete(tr50_json_detach_item_from_object(object, string)); }

// Replace array/object items with new ones.
void   tr50_json_replace_item_in_array(JSON *array, int which, JSON *newitem) {
	JSON *c = array->child; while (c && which>0) c = c->next, which--; if (!c) return;
newitem->next=c->next;newitem->prev=c->prev;if (newitem->next) newitem->next->prev=newitem;
if (c == array->child) array->child = newitem; else newitem->prev->next = newitem; c->next = c->prev = 0; tr50_json_delete(c);
}
void   tr50_json_replace_item_in_object(JSON *object, const char *string, JSON *newitem) { int i = 0; JSON *c = object->child; while (c && tr50_json_str_case_cmp(c->string, string))i++, c = c->next; if (c) { newitem->string = tr50_json_strdup(string); tr50_json_replace_item_in_array(object, i, newitem); } }

// Create basic types:
JSON *tr50_json_create_null() { JSON *item = tr50_json_new_item(); item->type = JSON_NULL; return item; }
JSON *tr50_json_create_true() { JSON *item = tr50_json_new_item(); item->type = JSON_TRUE; return item; }
JSON *tr50_json_create_false() { JSON *item = tr50_json_new_item(); item->type = JSON_FALSE; return item; }
JSON *tr50_json_create_integer(long long num) { JSON *item = tr50_json_new_item(); item->type = JSON_NUMBER; item->valuedouble = (double)num; item->valueint = (int)num; item->valueuint = (unsigned int)num; item->valuelonglong = (long long)num;  return item; }
JSON *tr50_json_create_number(double num) { JSON *item = tr50_json_new_item(); item->type = JSON_NUMBER; item->valuedouble = num; item->valueint = (int)num; item->valueuint = (unsigned int)num; item->valuelonglong = (long long)num; return item; }
JSON *tr50_json_create_string(const char *string) { JSON *item = tr50_json_new_item(); item->type = JSON_STRING; item->valuestring = tr50_json_strdup(string); return item; }
JSON *tr50_json_create_array() { JSON *item = tr50_json_new_item(); item->type = JSON_ARRAY; return item; }
JSON *tr50_json_create_object() { JSON *item = tr50_json_new_item(); item->type = JSON_OBJECT; return item; }

// Create Arrays:
JSON *tr50_json_create_bool_array(char *numbers, int count) { int i; JSON *n = 0, *p = 0, *a = tr50_json_create_array(); for (i = 0; i<count; i++) { n = numbers[i] ? tr50_json_create_true() : tr50_json_create_false(); if (!i)a->child = n; else suffix_object(p, n); p = n; }return a; }
JSON *tr50_json_create_int_array(int *numbers, int count) { int i; JSON *n = 0, *p = 0, *a = tr50_json_create_array(); for (i = 0; i<count; i++) { n = tr50_json_create_number(numbers[i]); if (!i)a->child = n; else suffix_object(p, n); p = n; }return a; }
JSON *tr50_json_create_longlong_array(long long  *numbers, int count) { int i; JSON *n = 0, *p = 0, *a = tr50_json_create_array(); for (i = 0; i<count; i++) { n = tr50_json_create_integer(numbers[i]); if (!i)a->child = n; else suffix_object(p, n); p = n; }return a; }
JSON *tr50_json_create_float_array(float *numbers, int count) { int i; JSON *n = 0, *p = 0, *a = tr50_json_create_array(); for (i = 0; i<count; i++) { n = tr50_json_create_number(numbers[i]); if (!i)a->child = n; else suffix_object(p, n); p = n; }return a; }
JSON *tr50_json_create_double_array(double *numbers, int count) { int i; JSON *n = 0, *p = 0, *a = tr50_json_create_array(); for (i = 0; i<count; i++) { n = tr50_json_create_number(numbers[i]); if (!i)a->child = n; else suffix_object(p, n); p = n; }return a; }
JSON *tr50_json_create_string_array(const char **strings, int count) { int i; JSON *n = 0, *p = 0, *a = tr50_json_create_array(); for (i = 0; i<count; i++) { n = tr50_json_create_string(strings[i]); if (!i)a->child = n; else suffix_object(p, n); p = n; }return a; }
