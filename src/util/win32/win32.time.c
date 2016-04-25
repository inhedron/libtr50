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

#include <Windows.h>
#include <time.h>
#include <stdio.h>

#  define snprintf _snprintf

#include <tr50/util/time.h>

#ifndef __GNUC__
#define EPOCHFILETIME (116444736000000000i64)
#else
#define EPOCHFILETIME (116444736000000000LL)
#endif
#ifndef TM_YEAR_BASE
#  define TM_YEAR_BASE 1900
#endif
#define ALT_E			0x01
#define ALT_O			0x02
#define	LEGAL_ALT(x)		{ if(alt_format & ~(x)) return 0; }
static int _strings_conv_num(const char **buf, int *dest, int llim, int ulim);
static const char *g_strings_day[7] = {
	"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
	"Friday", "Saturday"
};
static const char *g_strings_abday[7] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char *g_strings_mon[12] = {
	"January", "February", "March", "April", "May", "June", "July",
	"August", "September", "October", "November", "December"
};
static const char *g_strings_abmon[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char *g_strings_am_pm[2] = { "AM", "PM" };
#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#    define strcasecmp(one,two) _strcmpi(one,two)
#  else
#    define strcasecmp(one,two) strcmpi(one,two)
#  endif

#  define strncasecmp(one,two,len) _strnicmp(one,two,len)

//__inline int gettimeofday(struct timeval *tv, struct timezone *tz);

_Check_return_ _CRT_INSECURE_DEPRECATE_GLOBALS(_get_daylight) _CRTIMP int *__cdecl __daylight(void);
#define _daylight (*__daylight())

/* difference in seconds between GMT and local time */
_Check_return_ _CRT_INSECURE_DEPRECATE_GLOBALS(_get_timezone) _CRTIMP long *__cdecl __timezone(void);
#define _timezone (*__timezone())

struct timezone {
	int tz_minuteswest; /* minutes W of Greenwich */
	int tz_dsttime;     /* type of dst correction */
};


__inline int gettimeofday(struct timeval *tv, struct timezone *tz) {
	FILETIME        ft;
	LARGE_INTEGER   li;
	__int64         t;
	static int      tzflag;

	if (tv) {
		GetSystemTimeAsFileTime(&ft);
		li.LowPart = ft.dwLowDateTime;
		li.HighPart = ft.dwHighDateTime;
		t = li.QuadPart;       /* In 100-nanosecond intervals */
		t -= EPOCHFILETIME;     /* Offset to the Epoch time */
		t /= 10;                /* In microseconds */
		tv->tv_sec = (long)(t / 1000000);
		tv->tv_usec = (long)(t % 1000000);
	}

	if (tz) {
		if (!tzflag) {
			_tzset();
			++tzflag;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}

	return 0;
}

long long _time_now() {
	struct timeval		ltime;
	long long ret;
	gettimeofday(&ltime, NULL);
	ret = ltime.tv_sec;
	ret *= 1000;
	ret += ltime.tv_usec / 1000;
	return ret;
}

int time_now_in_sec() {
	struct timeval		ltime;
	gettimeofday(&ltime, NULL);
	return ltime.tv_sec;
}
int _time_now_in_sec() {
	return time_now_in_sec();
}
void tr50_time_sprintf2(char *buffer, const char *time_format, long long mstime, int use_gmt) {

	SYSTEMTIME st;
	FILETIME ft;
	struct tm tim;
	int ms = mstime % 1000;

	if (mstime < 0) ms += 1000;

	mstime *= 10000;
	(unsigned long long)mstime += ((unsigned long long)116444736000000000);
	ft.dwLowDateTime = (DWORD)mstime;
	ft.dwHighDateTime = (DWORD)(mstime >> 32);
	if (use_gmt) {
		FileTimeToSystemTime(&ft, &st);
	}
	else {
		SYSTEMTIME tmp;
		FileTimeToSystemTime(&ft, &tmp);
		SystemTimeToTzSpecificLocalTime(NULL, &tmp, &st);
	}

	//ilsMemset(&tim, 0, sizeof(tim));
	tim.tm_hour = st.wHour;
	tim.tm_min = st.wMinute;
	tim.tm_mday = st.wDay;
	tim.tm_mon = st.wMonth - 1;
	tim.tm_sec = st.wSecond;
	tim.tm_year = st.wYear - 1900;
	tim.tm_isdst = -1;

	mktime(&tim);
	if (time_format == NULL) {
		int len;
		strftime((char *)buffer, 64, "%Y-%m-%d %H:%M:%S", &tim);
		len = strlen(buffer);
		snprintf(buffer + len, 64 - len, ".%03d", ms);
	}
	else {
		strftime((char *)buffer, 64, time_format, &tim);
	}
}

void tr50_time_sprintf(char *buffer, const char *time_format, long long mstime) {
	tr50_time_sprintf2(buffer, time_format, mstime, FALSE);
}

char *strptime(const char *buf, const char *fmt, struct tm *tm) {
	char c;
	const char *bp;
	size_t len = 0;
	int alt_format, i, split_year = 0;

	bp = buf;

	while ((c = *fmt) != '\0') {
		/* Clear `alternate' modifier prior to new conversion. */
		alt_format = 0;

		/* Eat up white-space. */
		if (isspace(c)) {
			while (isspace(*bp)) ++bp;
			++fmt;
			continue;
		}

		if ((c = *fmt++) != '%')
			goto literal;

	again:		switch (c = *fmt++) {
	case '%':	/* "%%" is converted to "%". */
		literal :
		if (c != *bp++) return 0;
		break;

		/*
		* "Alternative" modifiers. Just set the appropriate flag
		* and start over again.
		*/
	case 'E':	/* "%E?" alternative conversion modifier. */
		LEGAL_ALT(0);
		alt_format |= ALT_E;
		goto again;

	case 'O':	/* "%O?" alternative conversion modifier. */
		LEGAL_ALT(0);
		alt_format |= ALT_O;
		goto again;

		/*
		* "Complex" conversion rules, implemented through recursion.
		*/
	case 'c':	/* Date and time, using the locale's format. */
		LEGAL_ALT(ALT_E);
		if (!(bp = strptime(bp, "%x %X", tm))) return 0;
		break;

	case 'D':	/* The date as "%m/%d/%y". */
		LEGAL_ALT(0);
		if (!(bp = strptime(bp, "%m/%d/%y", tm))) return 0;
		break;

	case 'R':	/* The time as "%H:%M". */
		LEGAL_ALT(0);
		if (!(bp = strptime(bp, "%H:%M", tm))) return 0;
		break;

	case 'r':	/* The time in 12-hour clock representation. */
		LEGAL_ALT(0);
		if (!(bp = strptime(bp, "%I:%M:%S %p", tm))) return 0;
		break;

	case 'T':	/* The time as "%H:%M:%S". */
		LEGAL_ALT(0);
		if (!(bp = strptime(bp, "%H:%M:%S", tm))) return 0;
		break;

	case 'X':	/* The time, using the locale's format. */
		LEGAL_ALT(ALT_E);
		if (!(bp = strptime(bp, "%H:%M:%S", tm))) return 0;
		break;

	case 'x':	/* The date, using the locale's format. */
		LEGAL_ALT(ALT_E);
		if (!(bp = strptime(bp, "%m/%d/%y", tm))) return 0;
		break;

		/*
		* "Elementary" conversion rules.
		*/
	case 'A':	/* The day of week, using the locale's form. */
	case 'a':
		LEGAL_ALT(0);
		for (i = 0; i < 7; ++i) {
			/* Full name. */
			len = strlen(g_strings_day[i]);
			if (strncasecmp(g_strings_day[i], bp, len) == 0)
				break;

			/* Abbreviated name. */
			len = strlen(g_strings_abday[i]);
			if (strncasecmp(g_strings_abday[i], bp, len) == 0)
				break;
		}

		/* Nothing matched. */
		if (i == 7) return 0;

		tm->tm_wday = i;
		bp += len;
		break;

	case 'B':	/* The month, using the locale's form. */
	case 'b':
	case 'h':
		LEGAL_ALT(0);
		for (i = 0; i < 12; ++i) {
			/* Full name. */
			len = strlen(g_strings_mon[i]);
			if (strncasecmp(g_strings_mon[i], bp, len) == 0)
				break;

			/* Abbreviated name. */
			len = strlen(g_strings_abmon[i]);
			if (strncasecmp(g_strings_abmon[i], bp, len) == 0)
				break;
		}

		/* Nothing matched. */
		if (i == 12) return 0;

		tm->tm_mon = i;
		bp += len;
		break;

	case 'C':	/* The century number. */
		LEGAL_ALT(ALT_E);
		if (!(_strings_conv_num(&bp, &i, 0, 99))) return 0;

		if (split_year) {
			tm->tm_year = (tm->tm_year % 100) + (i * 100);
		} else {
			tm->tm_year = i * 100;
			split_year = 1;
		}
		break;

	case 'd':	/* The day of month. */
	case 'e':
		LEGAL_ALT(ALT_O);
		if (!(_strings_conv_num(&bp, &tm->tm_mday, 1, 31))) return 0;
		break;

	case 'k':	/* The hour (24-hour clock representation). */
		LEGAL_ALT(0);
		/* FALLTHROUGH */
	case 'H':
		LEGAL_ALT(ALT_O);
		if (!(_strings_conv_num(&bp, &tm->tm_hour, 0, 23))) return 0;
		break;

	case 'l':	/* The hour (12-hour clock representation). */
		LEGAL_ALT(0);
		/* FALLTHROUGH */
	case 'I':
		LEGAL_ALT(ALT_O);
		if (!(_strings_conv_num(&bp, &tm->tm_hour, 1, 12))) return 0;
		if (tm->tm_hour == 12)
			tm->tm_hour = 0;
		break;

	case 'j':	/* The day of year. */
		LEGAL_ALT(0);
		if (!(_strings_conv_num(&bp, &i, 1, 366))) return 0;
		tm->tm_yday = i - 1;
		break;

	case 'M':	/* The minute. */
		LEGAL_ALT(ALT_O);
		if (!(_strings_conv_num(&bp, &tm->tm_min, 0, 59))) return 0;
		break;

	case 'm':	/* The month. */
		LEGAL_ALT(ALT_O);
		if (!(_strings_conv_num(&bp, &i, 1, 12))) return 0;
		tm->tm_mon = i - 1;
		break;

	case 'p':	/* The locale's equivalent of AM/PM. */
		LEGAL_ALT(0);
		/* AM? */
		if (strcasecmp(g_strings_am_pm[0], bp) == 0) {
			if (tm->tm_hour > 11) return 0;

			bp += strlen(g_strings_am_pm[0]);
			break;
		}
		/* PM? */
		else if (strcasecmp(g_strings_am_pm[1], bp) == 0) {
			if (tm->tm_hour > 11) return 0;

			tm->tm_hour += 12;
			bp += strlen(g_strings_am_pm[1]);
			break;
		}

		/* Nothing matched. */
		return 0;

	case 'S':	/* The seconds. */
		LEGAL_ALT(ALT_O);
		if (!(_strings_conv_num(&bp, &tm->tm_sec, 0, 61))) return 0;
		break;

	case 'U':	/* The week of year, beginning on sunday. */
	case 'W':	/* The week of year, beginning on monday. */
		LEGAL_ALT(ALT_O);
		/*
		* XXX This is bogus, as we can not assume any valid
		* information present in the tm structure at this
		* point to calculate a real value, so just check the
		* range for now.
		*/
		if (!(_strings_conv_num(&bp, &i, 0, 53))) return 0;
		break;

	case 'w':	/* The day of week, beginning on sunday. */
		LEGAL_ALT(ALT_O);
		if (!(_strings_conv_num(&bp, &tm->tm_wday, 0, 6))) return 0;
		break;

	case 'Y':	/* The year. */
		LEGAL_ALT(ALT_E);
		if (!(_strings_conv_num(&bp, &i, 0, 9999))) return 0;

		tm->tm_year = i - TM_YEAR_BASE;
		break;

	case 'y':	/* The year within 100 years of the epoch. */
		LEGAL_ALT(ALT_E | ALT_O);
		if (!(_strings_conv_num(&bp, &i, 0, 99))) return 0;

		if (split_year) {
			tm->tm_year = ((tm->tm_year / 100) * 100) + i;
			break;
		}
		split_year = 1;
		if (i <= 68)
			tm->tm_year = i + 2000 - TM_YEAR_BASE;
		else
			tm->tm_year = i + 1900 - TM_YEAR_BASE;
		break;

		/*
		* Miscellaneous conversions.
		*/
	case 'n':	/* Any kind of white-space. */
	case 't':
		LEGAL_ALT(0);
		while (isspace(*bp)) ++bp;
		break;


	default:	/* Unknown/unsupported conversion. */
		return 0;
	}


	}

	return ((char *)bp);
}

static int _strings_conv_num(const char **buf, int *dest, int llim, int ulim) {
	int result = 0;

	/* The limit also determines the number of valid digits. */
	int rulim = ulim;

	if (**buf < '0' || **buf > '9') return 0;

	do {
		result *= 10;
		result += *(*buf)++ - '0';
		rulim /= 10;
	} while ((result * 10 <= ulim) && rulim && **buf >= '0' && **buf <= '9');

	if (result < llim || result > ulim) return 0;

	*dest = result;
	return 1;
}
 
void tr50_time_strptime(const char *timestamp_str, const char *time_format, long long *mstime) {
	struct tm tim;
	*mstime = 0LL;

	if (strptime(timestamp_str, time_format ? time_format : "%Y-%m-%dT%H:%M:%SZ", &tim) == NULL) return ;

	tim.tm_isdst = -1;
	*mstime = mktime(&tim);
	*mstime *= 1000;
	return ;
}
