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

#include <stdio.h>

#define _memory_malloc(size) _memory_malloc_ex((size), __FILE__, __LINE__)
#define _memory_realloc(ptr,size) _memory_realloc_ex((ptr),(size), __FILE__, __LINE__)
#define _memory_free(ptr) _memory_free_ex((ptr), __FILE__, __LINE__)
#define _memory_clone(ptr,size) _memory_clone_ex((ptr),(size), __FILE__, __LINE__)

void *_memory_malloc_ex(size_t size, const char *file, int line);
void *_memory_realloc_ex(void* ptr, size_t size, const char *file, int line);
void _memory_free_ex(void *ptr, const char *file, int line);
void *_memory_clone_ex(void *ptr, size_t size, const char *file, int line);
void *_memory_memset(void *ptr, int value, size_t size);
void *_memory_memcpy(void *dest, void *src, size_t size);

int _memory_handle_count();
