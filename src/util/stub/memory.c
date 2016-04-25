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

MEMORY_FUNCTIONS g_memory_functions = {
	malloc,
	realloc,
	free,
	memset,
	memcpy
};

void memory_functions(MEMORY_FUNCTIONS functions) {
	g_memory_functions.mallocFunc = functions.mallocFunc;
	g_memory_functions.reallocFunc = functions.reallocFunc;
	g_memory_functions.freeFunc = functions.freeFunc;
	g_memory_functions.memsetFunc = functions.memsetFunc;
	g_memory_functions.memcpyFunc = functions.memcpyFunc;
}

void *_memory_malloc(int size) {
	return g_memory_functions.mallocFunc(size);
}

void *_memory_realloc(void *ptr, size_t size) {
	return g_memory_functions.reallocFunc(ptr, size);
}

void _memory_free(void *ptr) {
	g_memory_functions.freeFunc(ptr);
}

void *_memory_memset(void *dest, int value, int size) {
	return g_memory_functions.memsetFunc(dest, value, size);
}

void *_memory_memcpy(void *dest, void *src, int size) {
	return g_memory_functions.memcpyFunc(dest, src, size);
}

void *_memory_clone(void *ptr, size_t size) {
	char *clone = (char *)_memory_malloc(size + 1);
	if (clone) {
		_memory_memcpy(clone, ptr, size);
		clone[size] = 0;
	}
	return clone;
}

