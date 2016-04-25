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

#include <tx_api.h>

#include <tr50/error.h>
#include <tr50/util/memory.h>
#include <tr50/util/thread.h>

typedef struct {
	char name[32];
	void *stack;
	TX_THREAD tx;
	_thread_function func;
	void *arg;
} _TELIT_THREAD;

void _thread_handler(void *handle) {
	_TELIT_THREAD *thread = (_TELIT_THREAD *)handle;
	thread->func(thread->arg);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int _thread_create(void **handle, const char *name, _thread_function func, void *arg) {
	_TELIT_THREAD *thread = (_TELIT_THREAD *)_memory_malloc(sizeof(_TELIT_THREAD));
	_memory_memset(thread, 0, sizeof(_TELIT_THREAD));

	thread->stack = (void *)_memory_malloc(32768);
	_memory_memset(thread->stack, 0, 32768);

	thread->func = func;
	thread->arg = arg;

	strcpy(thread->name, name);
	/* Create the task */
	if (tx_thread_create(&thread->tx, thread->name,
						 (VOID (*)(ULONG))_thread_handler, (ULONG) thread,
						 thread->stack, 32768,
						 105, 105,
						 2, TX_AUTO_START) != TX_SUCCESS) {
		//log_should_not_happen("tx_thread_create(): failed.");
		return -1;
	}
	*handle = thread;
	return 0;
}

int _thread_join(void *handle) {
	_thread_sleep(5000);
	return 0;
}

int _thread_delete(void *handle) {
	_TELIT_THREAD *thread = (_TELIT_THREAD *)handle;
	tx_thread_terminate(&thread->tx);
	tx_thread_delete(&thread->tx);

	_memory_free(thread->stack);
	_memory_free(thread);
	return 0;
}

void _thread_sleep(int ms) {
	UtaOsSleep(ms, ms);
}

int _thread_id(int *id) {
	return ERR_TR50_NOPORT;
}

