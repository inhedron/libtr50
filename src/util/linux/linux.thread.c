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

#include <errno.h>
#include <stdio.h>
#include <limits.h>
#include <pthread.h>

#include <tr50/error.h>

#include <tr50/internal/tr50.h>

#include <tr50/util/log.h>
#include <tr50/util/memory.h>
#include <tr50/util/thread.h>

/* Thread Running Priorities */
#define THREAD_PRIO_HIGHEST		 2
#define THREAD_PRIO_HIGH		 1
#define THREAD_PRIO_NORMAL		 0
#define THREAD_PRIO_LOW			-1
#define THREAD_PRIO_LOWEST		-2

#define STACK_MAX_SIZE 1048576
#define STACK_MIN_SIZE 0

int g_thread_default_prio = 0;

typedef struct {
	pthread_t handle;
	int err;
} _THREAD_OBJECT;

typedef struct {
	const char *name;	/* Thread Name */
	int stack;			/* Thread Stack Size */
	int priority;		/* Thread Running Priority */
} THREAD_EX_ARGS;

int _thread_create(void **handle, const char *name, _thread_function thread, void *args) {
	THREAD_EX_ARGS ex;
	_THREAD_OBJECT *thd;
	pthread_t tid;
	pthread_attr_t tattr;
	int ret;
	struct sched_param param;

	ex.name = NULL;
	ex.stack = STACK_MAX_SIZE;
	ex.priority = THREAD_PRIO_NORMAL;

	if ((*handle = _memory_malloc(sizeof(_THREAD_OBJECT))) == NULL) {
		return ERR_TR50_MALLOC;
	}
	_memory_memset(*handle, 0, sizeof(_THREAD_OBJECT));

	thd = *handle;

	if ((ret = pthread_attr_init(&tattr)) != 0) {
		log_should_not_happen("thread_create_ex(): pthread_attr_init failed[%d]\n", ret);
	}

	if (ex.stack < STACK_MIN_SIZE) {
		ex.stack = STACK_MIN_SIZE;
	} else if (ex.stack < PTHREAD_STACK_MIN) {
		ex.stack = PTHREAD_STACK_MIN;
	}

	if ((ret = pthread_attr_setstacksize(&tattr, ex.stack)) != 0) {
		log_should_not_happen("thread_create_ex(): pthread_attr_setstacksize failed[%d]\n", ret);
	}

	if (ex.priority != THREAD_PRIO_NORMAL || g_thread_default_prio != 0) {
		if ((ret = pthread_attr_getschedparam(&tattr, &param)) != 0) {
			log_should_not_happen("thread_create_ex(): pthread_attr_getschedparam failed[%d]\n", ret);
		}

		if (param.sched_priority != 0) {
			switch (ex.priority) {
			case THREAD_PRIO_HIGHEST:
				param.sched_priority += 40;
				break;
			case THREAD_PRIO_HIGH:
				param.sched_priority += 20;
				break;
			case THREAD_PRIO_LOW:
				param.sched_priority -= 10;
				break;
			case THREAD_PRIO_LOWEST:
				param.sched_priority -= 20;
				break;
			}
		}

		if ((ret = pthread_attr_setschedparam(&tattr, &param)) != 0) {
			log_should_not_happen("thread_create_ex(%d): pthread_attr_setschedparam(%d) failed[%d]\n", ex.priority, param.sched_priority, ret);
		}
	}

	if (pthread_create(&tid, &tattr, thread, args) < 0) {
		pthread_attr_destroy(&tattr);
		thd->err = errno;
		log_should_not_happen("thread_create_ex(): pthread_create failed[%d]\n", thd->err);
		return ERR_TR50_OS;
	}

	thd->handle = tid;

	if ((ret = pthread_attr_destroy(&tattr)) != 0) {
		log_should_not_happen("thread_create_ex(): pthread_attr_destroy failed[%d]\n", ret);
	}

	return 0;
}

int _thread_join(void *handle) {
	_THREAD_OBJECT *thd = handle;
	void **result = NULL;

	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if (pthread_join((pthread_t)thd->handle, result) != 0) {
		thd->err = errno;
		log_should_not_happen("thread_join(): pthread_join failed[%d]\n", thd->err);
		return ERR_TR50_OS;
	}

	return 0;
}

int _thread_delete(void *handle) {
	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	_memory_free(handle);

	return 0;
}

void _thread_sleep(int ms) {
	struct timespec tss;

	if (ms == -1) {
		tss.tv_sec = 1000000;
		tss.tv_nsec = 0;

		/* Sleep forever. */
		while (1) {
			nanosleep(&tss, NULL);
		}
	} else if (ms != 0) {
		tss.tv_sec = ms / 1000;
		tss.tv_nsec = ms % 1000 * 1000000;

		while (1) {
			if (nanosleep(&tss, &tss) == -1) {
				if (errno == EINTR) {
					continue;
				} else {
					break;
				}
			} else {
				break;
			}
		}
	}

	return;
}

int _thread_id(int *id) {
	*id = pthread_self();
	return 0;
}

