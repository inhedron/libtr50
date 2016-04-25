/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 ILS Technology, LLC
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

#include <stdlib.h>
#include <string.h>
#include <tr50/tr50.h>
#include "sy/portab.h"
#include <sylib.h>
#include <dm_ext.h>

int g_memory_handle_count = 0;

void *_memory_malloc_ex(int size, const char *file, int line) {
	++g_memory_handle_count;
	return SYMalloc(size);
}

void *_memory_realloc_ex(void *ptr, size_t size, const char *file, int line) {
	return SYRealloc(ptr, size);
}

void _memory_free_ex(void *ptr, const char *file, int line) {
	--g_memory_handle_count;
	SYFree(ptr);
}

void *_memory_memset(void *dest, int value, int size) {
	return SYmemset(dest, value, size);
}

void *_memory_memcpy(void *dest, void *src, int size) {
	return SYmemcpy(dest, src, size);
}

void *_memory_clone_ex(void *ptr, size_t size, const char *file, int line) {
	char *clone = (char *)_memory_malloc_ex(size + 1, file, line);
	if (clone) {
		_memory_memcpy(clone, ptr, size);
		clone[size] = 0;
	}
	return clone;
}

int _memory_handle_count() {
	return g_memory_handle_count;
}

