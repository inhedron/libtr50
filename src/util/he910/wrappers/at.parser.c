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

#include <string.h>

#include <tr50/util/memory.h>
#include <tr50/util/platform.h>

#if defined(_AT_SIMULATOR)
#include <he910/at.h>
#else
#include <tr50/wrappers/at.h>
#endif

#include <tr50/tr50.h>

typedef struct {
	char			*at_string;
	int				at_string_len;
	char			*walker;
	char			*params[AT_TOKEN_PARAM_MAX];
	int				params_count;
	int				is_tokenized;
} _AT_TOKEN;

#define AT_TOKEN_DELIMITERS		",\r\n"

int at_token_create(void **handle, const char *at_string) {
	_AT_TOKEN *token;
	int len = strlen(at_string);
	if (len == 0) {
		return ERR_TR50_PARMS;
	}

	if ((token = _memory_malloc(sizeof(_AT_TOKEN))) == NULL) {
		return ERR_TR50_PARMS;
	}
	_memory_memset(token, 0, sizeof(_AT_TOKEN));

	token->at_string_len = len;
	if ((token->at_string = _memory_clone((void *)at_string, len)) == NULL) {
		return ERR_TR50_MALLOC;
	}
	token->walker = token->at_string;
	*handle = token;
	return 0;
}

int at_token_continue_with(void *handle, const char *str) {
	_AT_TOKEN *token = (_AT_TOKEN *)handle;
	int len = strlen(str);
	if (strncmp(token->walker, str, len) != 0) {
		return 0;
	}
	token->walker += len;
	return 1;
}

int at_token_has_ended(void *handle) {
	_AT_TOKEN *token = (_AT_TOKEN *)handle;
	if (token->at_string + token->at_string_len == token->walker) {
		return 1;
	}
	return 0;
}

int _at_token_parse_parameters(_AT_TOKEN *token) {
	char *reent = NULL, *ptr = NULL;
#if !defined(_AT_SIMULATOR)
	UINT8 lookForClosingQuote = 0;
#endif
	ptr = strtok_r(token->walker, AT_TOKEN_DELIMITERS, &reent);
	while (ptr) {
#if !defined(_AT_SIMULATOR)
		char * firstQuote;
#endif
		if (token->params_count >= AT_TOKEN_PARAM_MAX) {
			return ERR_AT_TOKEN_PARAM_MAX;
		}
#if !defined(_AT_SIMULATOR)
		/* Normal case: no quotes found */
		if (!lookForClosingQuote) {
			if ((firstQuote = (char *)SYstrchr(ptr, '"')) != NULL) {
				/* we have at least a double quote. If if find a second one in the    *
				* same token it means that it doesn't contain a comma. Otherwise we  *
				* have to look for the closing quotes in the following tokens.       */
				if (SYstrrchr(ptr, '"') == firstQuote) {
					/* The reverse search has found the same double quotes! Remember it *
					*in the following loops.                                           */
					lookForClosingQuote = 1;
				}
				/* else... no problem. The token is already OK (no commas in it).     */
			}

			/* Do this anyway. In case of string with commas we need it to remember *
			* where the total string starts.                                       */
			token->params[token->params_count++] = ptr;
		} else {
			UINT8 negOffset = 1;

			/* Restore comma at the end of previous string instead of increasing    *
			* params counter. If we had more than one comma, we will have a '\0'   *
			* character followed by N commas. Search for the first and substitute  *
			* it with a comma.                                                     */
			while (*(ptr - negOffset) != '\0')
				negOffset++;
			*(ptr - negOffset) = ',';

			if (SYstrchr(ptr, '"') != NULL) /* Found final quotes! */
			{
				UINT8 newLen;
				char * curPar = token->params[token->params_count - 1];

				lookForClosingQuote = 0;
				/* Lets remove quotes by copying from char 2 to char len-1. */
				newLen = SYstrlen(curPar) - 2;
				SYstrncpy(curPar, curPar + 1, newLen);

				/* Put terminator */
				*(curPar + newLen) = 0;
			}
		}
#else
		token->params[token->params_count++] = ptr;
#endif

		ptr = strtok_r(NULL, AT_TOKEN_DELIMITERS, &reent);
	}
	token->is_tokenized = 1;
	return 0;
}

int at_token_delete(void *handle) {
	_AT_TOKEN *token = (_AT_TOKEN *)handle;
	if (token) {
		if (token->at_string) {
			_memory_free(token->at_string);
		}
		_memory_free(token);
	}
	return 0;
}

const char *at_token_parameter(void *handle, int index) {
	_AT_TOKEN *token = (_AT_TOKEN *)handle;
	if (!token->is_tokenized) {
		_at_token_parse_parameters(token);
	}

	if (index >= token->params_count) {
		return 0;
	}
	return token->params[index];
}

int at_token_count(void *handle) {
	_AT_TOKEN *token = (_AT_TOKEN *)handle;
	if (!token->is_tokenized) {
		_at_token_parse_parameters(token);
	}
	return token->params_count;
}

char *at_token_data(void *handle) {
	_AT_TOKEN *token = (_AT_TOKEN *)handle;
	return token->at_string;
}
