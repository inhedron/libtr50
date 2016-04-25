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

#include <windows.h>

#include "tr50\error.h"

#include "tr50\util\event.h"
#include "tr50\util\json.h"
#include "tr50\util\log.h"
#include "tr50\util\memory.h"

#define EVT_WAIT_TIMEOUT -1

typedef struct _EVENT_S {
	void *handle;
} _EVENT;

int _tr50_event_create(void **handle) {
	_EVENT *evt;
	int options = 0;

	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if ((*handle = _memory_malloc(sizeof(_EVENT))) == NULL) {
		return ERR_TR50_MALLOC;
	}
	_memory_memset(*handle, 0, sizeof(_EVENT));
	evt = *handle;

	if ((evt->handle = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL) {
		return ERR_TR50_OS;
	}
	return 0;
}

int _tr50_event_wait(void *handle) {
	_EVENT *evt = handle;
	int timeout = EVT_WAIT_TIMEOUT;
	int ret;

	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	ret = WaitForSingleObject(evt->handle, timeout);
	if (ret == WAIT_TIMEOUT) {
		return ERR_TR50_REQ_TIMEOUT;
	} else if (ret != WAIT_OBJECT_0) {
		return ERR_TR50_OS;
	}
	return 0;
}

int _tr50_event_signal(void *handle) {
	_EVENT *evt = handle;

	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if (!SetEvent(evt->handle)) {
		return ERR_TR50_OS;
	}
	return 0;
}

int _tr50_event_delete(void *handle) {
	_EVENT *evt = handle;

	if (handle == NULL) {
		return ERR_TR50_TIMEOUT;
	}

	if (!CloseHandle(evt->handle)) {
		log_should_not_happen("event_delete(): CloseHandle failed[%d]\n", GetLastError());
	}

	_memory_free(handle);
	return 0;
}
