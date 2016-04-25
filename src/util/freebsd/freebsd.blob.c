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

#include <tr50/util/blob.h>
 
int _blob_format_append(void *handle, const char *msg, ...) {
	va_list pArgs;
	_BLOB *blob = handle;
	int format_len = 0;
	int vsnl = -1;

	if (handle == NULL) {
		return ERR_TR50_BADHANDLE;
	}

	if (msg == NULL) {
		return ERR_TR50_PARMS;
	}

	va_start(pArgs, msg);

	vsnl = vsnprintf(blob->buffer + blob->len, blob->unused, msg, pArgs);

	if (vsnl == -1) {
		do {
			_blob_grow(blob, blob->size);
			vsnl = vsnprintf(blob->buffer + blob->len, blob->unused, msg, pArgs);
		} while (vsnl == -1);
	} else if (vsnl >= blob->unused) {
		_blob_grow(blob, vsnl + 1);
		vsnl = vsnprintf(blob->buffer + blob->len, blob->unused, msg, pArgs);
	}

	format_len = vsnl;

	va_end(pArgs);

	blob->len += format_len;
	blob->unused -= format_len;

	return 0;

}
 
