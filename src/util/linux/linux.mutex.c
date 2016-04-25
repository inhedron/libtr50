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

#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#include <tr50/error.h>
#include <tr50/util/memory.h>
#include <tr50/util/log.h>

typedef struct {
	pthread_mutex_t handle;
} _MUTEX;

int _tr50_mutex_create_internal(void **mutex, const char *file, int line) {
	_MUTEX *mux;

	pthread_mutex_t tmp = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

	if (mutex == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if ((mux = _memory_malloc(sizeof(_MUTEX))) == NULL) {
		return ERR_TR50_MALLOC;
	}

	_memory_memset(mux, 0, sizeof(_MUTEX));

	mux->handle = tmp;
	*mutex = mux;

	return 0;
}

int _tr50_mutex_lock_internal(void *mux, const char *file, int line) {
	_MUTEX *mutex = mux;
	int ret;

	if (mux == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if ((ret = pthread_mutex_lock((pthread_mutex_t *)&mutex->handle)) != 0) {
		return ERR_TR50_OS;
	}

	return 0;
}

int _tr50_mutex_unlock_internal(void *mux, const char *file, int line) {
	_MUTEX *mutex = mux;
	int ret;

	if (mux == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if ((ret = pthread_mutex_unlock((pthread_mutex_t *)&mutex->handle)) != 0) {
		return ERR_TR50_OS;
	}

	return 0;
}

int _tr50_mutex_delete_internal(void *mux, const char *file, int line) {
	_MUTEX *mutex = mux;
	int ret;

	if (mux == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if ((ret = pthread_mutex_destroy((pthread_mutex_t *)&mutex->handle)) != 0) {
		if ((ret = pthread_mutex_unlock((pthread_mutex_t *)&mutex->handle)) != 0) {
			log_should_not_happen("mutex_delete(): pthread_mutex_unlock failed[%d]\n", ret);
		}

		if ((ret = pthread_mutex_destroy((pthread_mutex_t *)&mutex->handle)) != 0) {
			log_should_not_happen("mutex_delete(): pthread_mutex_destroy failed[%d]\n", ret);
		}
	}
	_memory_free(mux);

	return 0;
}

