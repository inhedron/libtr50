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

#include <tr50/util/memory.h>
#include <tx_api.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int _tr50_mutex_create_internal(void **mux, const char *file, int line) {
	TX_MUTEX *handle = (TX_MUTEX *)_memory_malloc(sizeof(TX_MUTEX));
	tx_mutex_create(handle, "", TX_INHERIT);
	*mux = handle;
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int _tr50_mutex_lock_internal(void *mux, const char *file, int line) {
	return tx_mutex_get(mux, TX_WAIT_FOREVER);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int _tr50_mutex_unlock_internal(void *mux, const char *file, int line) {
	return tx_mutex_put(mux);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int _tr50_mutex_delete_internal(void *mux, const char *file, int line) {
	tx_mutex_delete(mux);
	_memory_free(mux);
	return 0;
}

