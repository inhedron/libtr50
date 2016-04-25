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

#include <ils/util/everything.h>

#include <tr50/util/thread.h>

int _thread_create(void **handle, const char *name, _thread_function func, void *arg) {
	int ret;
	THREAD_EX_ARGS *ex;

	thread_ex_create(&ex);
	ex->stack = 32768;
	ret = thread_create_ex(handle, func, arg, ex);
	thread_ex_delete(ex);

	return ret;
}

int _thread_join(void *handle) {
	void *result;

	return thread_join(handle, &result);
}

int _thread_delete(void *handle) {
	return thread_delete(handle);
}

void _thread_sleep(int ms) {
	thread_sleep(ms);
}

int _thread_id(int *id) {
	if (!id) {
		return ERR_MALLOC;
	}

	*id = thread_id(NULL);

	return 0;
}
