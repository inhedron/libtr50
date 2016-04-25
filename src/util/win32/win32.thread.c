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
#include <Windows.h>

#include <tr50/error.h>

#include <tr50/util/memory.h>
#include <tr50/util/log.h>
#include <tr50/util/thread.h>

#define THREAD_PRIO_HIGHEST		2  ///<The thread is run at the highest priority
#define THREAD_PRIO_HIGH		1  ///<The thread is run at a high priority
#define THREAD_PRIO_NORMAL		0  ///<The thread is run at a normal priority
#define THREAD_PRIO_LOW		   -1  ///<The thread is run at a low priority
#define THREAD_PRIO_LOWEST	   -2  ///<The thread is run at a the lowest priority

#define EXCEPTION_EXECUTE_HANDLER       1
#define EXCEPTION_CONTINUE_SEARCH       0
#define EXCEPTION_CONTINUE_EXECUTION    -1

#define STACK_MAX_SIZE 1048576

typedef void(*thread_exception_handler)(int code, void *custom);

thread_exception_handler g_thread_default_exception_handler = NULL;
int g_thread_exception_intercept = EXCEPTION_EXECUTE_HANDLER;
void *g_thread_default_exception_handler_args = NULL;

//use all
typedef struct {
	_thread_function func;
	void *args;
	int priority;
	char name[32];
} _THREAD_WRAPPER_EXEC;

//use all
typedef struct {
	const char *name;   ///<The name of the thread.
	int stack;          ///<The stack size to be used in the thread.
	int priority;       ///<The priority at which the thread is run.
} THREAD_EX_ARGS;

//use all
typedef struct tagTHREADNAME_INFO {
	DWORD dwType; // must be 0x1000
	LPCSTR szName; // pointer to name (in user addr space)
	DWORD dwThreadID; // thread ID (-1=caller thread)
	DWORD dwFlags; // reserved for future use, must be zero
} THREADNAME_INFO;

//use all
typedef struct {
	void *handle;
	int err;
} _THREAD_OBJECT;

void _thread_name(const char *name) {
	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = name;
	info.dwThreadID = -1;
	info.dwFlags = 0;

	__try {
		RaiseException(0x406D1388, 0, sizeof(info) / sizeof(DWORD), (const DWORD *)&info);
	} __except (EXCEPTION_CONTINUE_EXECUTION) { }
}

int thread_filter_exception(int code) {
	if (IsDebuggerPresent()) {
		return EXCEPTION_CONTINUE_SEARCH;
	}
	switch (code) {
	case STATUS_ACCESS_VIOLATION:
		return g_thread_exception_intercept;
	default:
		return EXCEPTION_CONTINUE_SEARCH;
	}
}

void thread_set_exception_intercept(int value) {
	if (value == TRUE) {
		g_thread_exception_intercept = EXCEPTION_EXECUTE_HANDLER;
	} else {
		g_thread_exception_intercept = EXCEPTION_CONTINUE_SEARCH;
	}
}

void thread_set_exception_handler(thread_exception_handler handler, void *custom) {
	g_thread_default_exception_handler = handler;
	g_thread_default_exception_handler_args = custom;
}

void *_thread_wrapper(void *args) {
	_thread_function func = ((_THREAD_WRAPPER_EXEC *)args)->func;
	void *fargs = ((_THREAD_WRAPPER_EXEC *)args)->args;
	if (((_THREAD_WRAPPER_EXEC *)args)->name[0] != '\0') {
		_thread_name(((_THREAD_WRAPPER_EXEC *)args)->name);
	}

	if (((_THREAD_WRAPPER_EXEC *)args)->priority != THREAD_PRIO_NORMAL) {
		SetThreadPriority(GetCurrentThread(), ((_THREAD_WRAPPER_EXEC *)args)->priority);
	}

	_memory_free(args);
	__try {
		return func(fargs);
	} __except (thread_filter_exception(GetExceptionCode())) {
		printf("============================================\n");
		printf("=== Runtime Exception Caught: 0x%8X ===\n", GetExceptionCode());
		printf("============================================\n");
		if (g_thread_default_exception_handler != NULL) {
			g_thread_default_exception_handler(GetExceptionCode(), g_thread_default_exception_handler_args);
		} else {
			exit(0);
		}
		return NULL;
	}
}

int _thread_create(void **handle, const char *name, _thread_function thread, void *args) {
	THREAD_EX_ARGS ex;
	ex.name = NULL;
	ex.stack = STACK_MAX_SIZE;
	ex.priority = THREAD_PRIO_NORMAL;

	_THREAD_OBJECT *thd;
	_THREAD_WRAPPER_EXEC *tse;

	if ((*handle = malloc(sizeof(_THREAD_OBJECT))) == NULL) {
		return ERR_TR50_MALLOC;
	}
	_memory_memset(*handle, 0, sizeof(_THREAD_OBJECT));
	thd = *handle;

	if ((tse = malloc(sizeof(_THREAD_WRAPPER_EXEC))) == NULL) {
		return ERR_TR50_MALLOC;
	}
	_memory_memset(tse, 0, sizeof(_THREAD_WRAPPER_EXEC));

	tse->func = thread;
	tse->args = args;
	tse->priority = ex.priority;
	if (ex.name != NULL) {
		strncpy(tse->name, ex.name, 31);
	}

	if (ex.stack < 0) {
		ex.stack = 0;
	}
	if ((thd->handle = CreateThread(NULL, ex.stack, (LPTHREAD_START_ROUTINE)_thread_wrapper, tse, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL)) == NULL) {
		thd->err = errno;
		log_should_not_happen("thread_create_ex(): CreateThread failed[%d]\n", thd->err);
		return ERR_TR50_OS;
	}

	return 0;
}

int _thread_join(void *handle) {
	_THREAD_OBJECT *thd = handle;
	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if (WaitForSingleObject(thd->handle, -1) != WAIT_OBJECT_0) {
		thd->err = GetLastError();
		log_should_not_happen("thread_join(): WaitForSingleObject failed[%d]\n", thd->err);
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
	Sleep(ms);
	return;
}

int _thread_id(int *id) {
	*id = GetCurrentThreadId();
	return 0;
}
