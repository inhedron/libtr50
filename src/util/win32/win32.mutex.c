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

#include <Windows.h>

#include <tr50/error.h>

#include <tr50/util/log.h>
#include <tr50/util/memory.h>
#include <tr50/util/mutex.h>
#include <tr50/util/thread.h>

typedef struct {
	void	*handle;
} _MUTEX;

int _tr50_mutex_create_internal(void **mux, const char *file, int line) {
	_MUTEX *mutex;
	int options = 0;

	if (mux == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if ((*mux = malloc(sizeof(_MUTEX))) == NULL) {
		return ERR_TR50_MALLOC;
	}
	_memory_memset(*mux, 0, sizeof(_MUTEX));
	mutex = *mux;

	if ((mutex->handle = CreateMutex(NULL, FALSE, NULL)) == NULL) {
		return ERR_TR50_OS;
	}
	return 0;
}

int _tr50_mutex_lock_internal(void *mux, const char *file, int line) {
	_MUTEX *mutex = mux;

	if (mux == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if (WaitForSingleObject(mutex->handle, -1) != WAIT_OBJECT_0) {
		return ERR_TR50_OS;
	}

	return 0;
}

int _tr50_mutex_unlock_internal(void *mux, const char *file, int line) {
	_MUTEX *mutex = mux;
	if (mux == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if (!ReleaseMutex(mutex->handle)) {
		return ERR_TR50_OS;
	}
	return 0;
}

int _tr50_mutex_delete_internal(void *mux, const char *file, int line) {
	_MUTEX *muxtex = mux;

	if (mux == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if (!CloseHandle(muxtex->handle)) {
		log_should_not_happen("mutex_delete(): CloseHandle failed[%d]\n", GetLastError());
	}

	_memory_free(mux);
	return 0;
}
