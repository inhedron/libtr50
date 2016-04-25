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

#include <tr50/internal/tr50.h>

#include <tr50/util/compress.h>
#include <tr50/util/memory.h>

// Free all allocated parameters.
void _tr50_config_delete(_TR50_CONFIG *config) {
	if (config->client_id) {
		_memory_free(config->client_id);
	}
	if (config->host) {
		_memory_free(config->host);
	}
	if (config->username) {
		_memory_free(config->username);
	}
	if (config->password) {
		_memory_free(config->password);
	}
	if (config->proxy_addr) {
		_memory_free(config->proxy_addr);
	}
	if (config->proxy_username) {
		_memory_free(config->proxy_username);
	}
	if (config->proxy_password) {
		_memory_free(config->proxy_password);
	}
	_memory_memset(config, 0, sizeof(_TR50_CONFIG));
}

// Setter
int tr50_config_set_host(void *tr50, const char *host) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	if (config->host) {
		_memory_free(config->host);
	}
	config->host = _memory_clone((void *)host, strlen(host));
	return 0;
}

int tr50_config_set_port(void *tr50, int port) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	config->port = port;
	return 0;
}

int tr50_config_set_keeplive(void *tr50, int keepalive_in_ms) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	config->keepalive_in_ms = keepalive_in_ms;
	return 0;
}

int tr50_config_set_client_id(void *tr50, const char *client_id) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	if (config->client_id) {
		_memory_free(config->client_id);
	}
	config->client_id = _memory_clone((void *)client_id, strlen(client_id));
	return 0;
}

int tr50_config_set_timeout(void *tr50, int timeout_in_ms) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	config->timeout_in_ms = timeout_in_ms;
	return 0;
}

int tr50_config_set_username(void *tr50, const char *username) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	if (config->username) {
		_memory_free(config->username);
	}
	if (username) {
		config->username = _memory_clone((void *)username, strlen(username));
	}
	return 0;
}
int tr50_config_set_password(void *tr50, const char *password) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	if (config->password) {
		_memory_free(config->password);
	}
	if (password) {
		config->password = _memory_clone((void *)password, strlen(password));
	}
	return 0;
}

int tr50_config_set_proxy(void *tr50, int type, const char *addr, const char *username, const char *password) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	config->proxy_type = type;
	if (config->proxy_addr) {
		_memory_free(config->proxy_addr);
	}
	if (config->proxy_username) {
		_memory_free(config->proxy_username);
	}
	if (config->proxy_password) {
		_memory_free(config->proxy_password);
	}
	config->proxy_addr = NULL;
	config->proxy_username = NULL;
	config->proxy_password = NULL;
	if (addr) {
		config->proxy_addr = _memory_clone((void *)addr, strlen(addr));
	}
	if (username) {
		config->proxy_username = _memory_clone((void *)username, strlen(username));
	}
	if (password) {
		config->proxy_password = _memory_clone((void *)password, strlen(password));
	}
	return 0;
}

int tr50_config_set_non_api_handler(void *tr50, tr50_async_non_api_callback callback, void *custom) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	config->non_api_handler = callback;
	config->non_api_handler_custom = custom;
	return 0;
}

int tr50_config_set_state_change_handler(void *tr50, tr50_async_state_change_callback callback, void *custom) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	config->state_change_handler = callback;
	config->state_change_handler_custom = custom;
	return 0;
}

int	tr50_config_set_api_watcher_handler(void *tr50, tr50_async_api_watcher_callback callback) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	config->api_watcher_handler = callback;
	return 0;
}

int tr50_config_set_ssl(void *tr50, int enabled) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	config->ssl = enabled;
	return 0;
}

int tr50_config_set_https_proxy(void *tr50, int enabled) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	config->https_proxy = enabled;
	return 0;
}

int tr50_config_set_compress(void *tr50, int enabled) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	if (!_compress_is_supported()) {
		return ERR_TR50_COMPRESS_NOT_SUPPORTED;
	}
	config->compress = enabled;
	return 0;
}

int	tr50_config_set_should_reconnect_handler(void *tr50, tr50_async_should_reconnect_callback callback, void *custom) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	config->should_reconnect_callback = callback;
	config->should_reconnect_custom = custom;
	return 0;
}

// Getter
const char *tr50_config_get_host(void *tr50) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	return config->host;
}

int tr50_config_get_port(void *tr50) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	return config->port;
}

int tr50_config_get_keeplive(void *tr50) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	return config->keepalive_in_ms;
}

const char *tr50_config_get_client_id(void *tr50) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	return config->client_id;
}

int tr50_config_get_timeout(void *tr50) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	return config->timeout_in_ms;
}

const char *tr50_config_get_username(void *tr50) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	return config->username;
}

const char *tr50_config_get_password(void *tr50) {
	_TR50_CONFIG *config = &((_TR50_CLIENT *)tr50)->config;
	return config->password;
}

