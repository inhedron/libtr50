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

#include <pthread.h>
#include <stdio.h>

#include <tr50/tr50.h>
#include <tr50/error.h>

#include <tr50/internal/tr50.h>

#include <tr50/util/log.h>
#include <tr50/util/json.h>
#include <tr50/util/event.h>
#include <tr50/util/memory.h>

#define EVENT_NAME_LEN	64
#define ETIMEDOUT       138

 

typedef struct _EVENT_S {
	pthread_mutex_t mux;
	pthread_cond_t cond;
	int is_locked;
	int wait_count;
	int usage_count;
} _EVENT;

int _tr50_event_create(void **handle) {
	_EVENT *evt;

	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if ((*handle = _memory_malloc(sizeof(_EVENT))) == NULL) {
		return ERR_TR50_MALLOC;
	}
	_memory_memset(*handle, 0, sizeof(_EVENT));
	evt = *handle;

	evt->is_locked = TRUE;

	if ((pthread_mutex_init(&evt->mux, NULL)) != 0) {
		return ERR_TR50_OS;
	}
	if ((pthread_cond_init(&evt->cond, NULL)) != 0) {
		return ERR_TR50_OS;
	}

	return 0;
}

int _tr50_event_wait(void *handle) {
	_EVENT *evt = handle;
	int ret;
 
	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	++evt->usage_count;

	if ((ret = pthread_mutex_lock(&evt->mux)) != 0) {
		return ERR_TR50_OS;
	}
 
	while (evt->is_locked) {
		++evt->wait_count;
		if ((ret = pthread_cond_wait(&evt->cond, &evt->mux)) != 0) {
			--evt->wait_count;
			pthread_mutex_unlock(&evt->mux);
			return ERR_TR50_OS;
		}
		--evt->wait_count;
	}

	if ((ret = pthread_mutex_unlock(&evt->mux)) != 0) {
		return ERR_TR50_OS;
	}
	return 0;
}

int _tr50_event_signal(void *handle) {

	_EVENT *evt = handle;

	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if ((pthread_mutex_lock(&evt->mux)) != 0) {
		return ERR_TR50_OS;
	}

	evt->is_locked = FALSE;
	if ((pthread_cond_broadcast(&evt->cond)) != 0) {
		return ERR_TR50_OS;
	}

	if ((pthread_mutex_unlock(&evt->mux)) != 0) {
		return ERR_TR50_OS;
	}
	return 0;
}

int _tr50_event_delete(void *handle) {
	_EVENT *evt = handle;
	int ret;
	if (handle == NULL) {
		return ERR_TR50_TIMEOUT;
	}

	if ((ret = pthread_cond_destroy(&evt->cond)) != 0) {
		log_should_not_happen("event_delete(): pthread_cond_destroy failed[%d]\n", ret);
	}
	if ((ret = pthread_mutex_destroy(&evt->mux)) != 0) {
		log_should_not_happen("event_delete(): pthread_mutex_destroy failed[%d]\n", ret);
	}

	_memory_free(handle);
	return 0;
}
