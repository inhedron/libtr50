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

#define LOG_TYPE_SHOULD_NOT_HAPPEN		0
#define LOG_TYPE_NEED_INVESTIGATION		1
#define LOG_TYPE_IMPORTANT_INFO			2
#define LOG_TYPE_DEBUG					3
#define LOG_TYPE_LOW_LEVEL				4

#if defined (_NO_VA_ARGS)
void log_should_not_happen(const char *msg, ...);
void log_need_investigation(const char *msg, ...);
void log_important_info(const char *msg, ...);
void log_debug(const char *msg, ...);
void log_low_level(const char *msg, ...);
#else
#define log_should_not_happen(msg,...)		_log_this(LOG_TYPE_SHOULD_NOT_HAPPEN,__FILE__,__LINE__,msg,##__VA_ARGS__)
#define log_need_investigation(msg,...)		_log_this(LOG_TYPE_NEED_INVESTIGATION,__FILE__,__LINE__,msg,##__VA_ARGS__)
#define log_important_info(msg,...)			_log_this(LOG_TYPE_IMPORTANT_INFO,__FILE__,__LINE__,msg,##__VA_ARGS__)
#define log_debug(msg,...)					_log_this(LOG_TYPE_DEBUG,__FILE__,__LINE__,msg,##__VA_ARGS__)
#define log_low_level(msg,...)				_log_this(LOG_TYPE_LOW_LEVEL,__FILE__,__LINE__,msg,##__VA_ARGS__)
#endif
void log_hexdump(int type, const char *msg, const char *buffer, int len);
void log_recurring(int type, const char *filename, int line, int minutes_between_logs, int check_for_changes, const char *msg, ...);

void log_filter_maximum_log_level(int level);

void _log_this(int type, const char *file, int line, const char *msg, ...);
