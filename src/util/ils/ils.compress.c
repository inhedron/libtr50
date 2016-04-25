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

#include <zlib.h>

#include <tr50/error.h>

#include <tr50/util/blob.h>
#include <tr50/util/memory.h>

int _compress_is_supported() {
	return 1;
}

int _compress_deflate(const char *in, int in_len, char **out, int *out_len) {
	int ret;
	unsigned char *cbuffer;
	unsigned long   cbuffer_len;

	cbuffer_len = compressBound(in_len);

	if ((cbuffer = _memory_malloc(cbuffer_len)) == NULL) {
		return ERR_TR50_MALLOC;
	}

	if ((ret = compress2(cbuffer, &cbuffer_len, (const Bytef *)in, in_len, Z_BEST_COMPRESSION)) != Z_OK) {
		return ERR_TR50_COMPRESS_DEFLATE;
	}

	*out = (char *)cbuffer;
	*out_len = cbuffer_len;

	return 0;
}

#define STOMP_COMPRESSION_CHUNK	16384

int _compress_inflate(const char *in, int in_len, char **out, int *out_len) {
	int ret;
	void *blob;
	z_stream strm;
	unsigned char *cbuffer;
	unsigned long  cbuffer_len;

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;

	inflateInit(&strm);
	_blob_create(&blob, 256);

	if ((cbuffer = _memory_malloc(STOMP_COMPRESSION_CHUNK)) == NULL) {
		return ERR_TR50_MALLOC;
	}

	cbuffer_len = STOMP_COMPRESSION_CHUNK;

	strm.avail_in = (unsigned int)in_len;
	strm.next_in = (unsigned char *)in;

	do {
		strm.avail_out = STOMP_COMPRESSION_CHUNK;
		strm.next_out = cbuffer;
#if defined(_VXWORKS)
		ret = dwinflate(&strm, Z_NO_FLUSH);
#else
		ret = inflate(&strm, Z_NO_FLUSH);
#endif
		switch (ret) {
		case Z_NEED_DICT:
			ret = Z_DATA_ERROR;     /* and fall through */
		case Z_DATA_ERROR:
		case Z_MEM_ERROR:
			inflateEnd(&strm);
			return ERR_TR50_COMPRESS_INFLATE;
		}
		_blob_append(blob, (char *)cbuffer, STOMP_COMPRESSION_CHUNK - strm.avail_out);
	} while (strm.avail_out == 0);

	inflateEnd(&strm);

	if (cbuffer != NULL) {
		_memory_free(cbuffer);
	}

	*out = _blob_get_buffer(blob);
	*out_len = _blob_get_length(blob);
	_blob_delete_object(blob);

	return 0;
}
