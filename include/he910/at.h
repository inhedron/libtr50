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

#define AT_TOKEN_PARAM_MAX			32

#define ERR_AT_TOKEN_PARAM_MAX		-19001
#define ERR_AT_TOKEN_CMD_NOT_MATCH	-19002
#define ERR_AT_CMD_NOT_SUPPORTED	-19003

#ifndef _AT_H_
#define _AT_H_

#if defined(_WIN32)
#  if defined(EXPORT_TR50_SYMS)
#    define TR50_EXPORT __declspec(dllexport)
#  else
#    define TR50_EXPORT __declspec(dllimport)
#  endif
#else
#  define TR50_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*tr50_at_unsol_callback)(const char *at_input, int at_input_len);
TR50_EXPORT int tr50_at_init(tr50_at_unsol_callback callback);
TR50_EXPORT int tr50_at_process(const char *at_input, char **at_output, int *at_output_len, char **data, int *data_len);
TR50_EXPORT void *tr50_at_handle();

#ifdef __cplusplus
}
#endif

#endif // !_AT_H_

#define TR50_AT_SEND_MODE_DELIMITED		1
#define TR50_AT_SEND_MODE_RAW			2
#define TR50_AT_SEND_MODE_METHOD		3

#if defined(_AT_SIMULATOR)
typedef char INT8;
typedef unsigned char UINT8;
typedef unsigned int UINT32;
#define DW_URL_LENGTH		256
#define DW_MAX_TOKEN_LENGTH	256

#define DWCfg				1

#define DWUrlId				1
#define DWdevideIdSelId		2
#define DWappTokenId		3
#define DWorgTokenId		4
#define DWsecurityId		5
#define DWheartBeatId		6
#define DWautoReconnId		7
#define DWoverflowHandlId	8

#define DWFeatureAt			21

#define DW_URL_DEF				"open01.devicewise.com"
#define DW_DEV_ID_SEL_DEF		0
#define DW_APP_TOKEN_DEF		"trzNXgN7WUNSg1DB"
#define DW_SEC_DEF				0
#define DW_HEART_BEAT_DEF		60
#define DW_AUTO_RECONN_DEF		3
#define DW_OVERFL_HANDL_DEF		0

int NVMRead(int family, int id, INT8 *buffer);

#else
#include "at/atparser.h"
#include <nvmpst.h>
#include <atdw.h>
#endif

int tr50_at_run_recv(const char *output, int output_len);
void tr50_at_run_send_back( void );

int at_token_create(void **handle, const char *at_string);
int at_token_continue_with(void *handle, const char *str);
int at_token_has_ended(void *handle);
const char *at_token_parameter(void *handle, int index);
char *at_token_data(void *handle);
int at_token_count(void *handle);
int at_token_delete(void *handle);

int _tr50_at_storage_put(void *message, int mode, int id, int *len);
int _tr50_at_storage_get(int id, int mode, int *status, char **output, int *output_len);
void _tr50_at_storage_list(void *blob);
int _tr50_at_storage_is_full();

int _tr50_at_cmd_dwconn_set(void *token, void *output_blob);
int _tr50_at_cmd_dwconn_get(void *token, void *output_blob);
int _tr50_at_cmd_dwsendr_set(void *input_token, void *output_blob);
int _tr50_at_cmd_dwsend_set(void *input_token, void *output_blob);
int _tr50_at_cmd_dwlrcv_get(void *token, void *output_blob);
int _tr50_at_cmd_dwrcvr_set(void *token, void *output_blob);
int _tr50_at_cmd_dwrcv_set(void *token, void *output_blob);
int _tr50_at_cmd_dwstatus_get(void *token, void *output_blob);

const char *_tr50_at_info_imei(char *buffer32);
const char *_tr50_at_info_ccid(char *buffer32);
const char *_tr50_at_info_model(char *buffer32);
const char *_tr50_at_info_version(char *buffer64);

int _tr50_at_feature_enabled(int id);
void _tr50_at_feature_option_get(int id, int option_id, char *buffer);

typedef struct {
	void *					tr50;
	tr50_at_unsol_callback	unsol_callback;
	void *					at_run_buffer;
	void *					at_run_mux;
	int						at_connected_once;
} _AT_WRAPPER;
