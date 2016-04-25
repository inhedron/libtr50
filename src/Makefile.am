lib_LTLIBRARIES = libtr50.la
libtr50_la_SOURCES = \
	$(top_builddir)/include/tr50/error.h \
	$(top_builddir)/include/tr50/tr50.h \
	$(top_builddir)/include/tr50/internal/tr50.h \
	$(top_builddir)/include/tr50/mqtt/mqtt.h \
	$(top_builddir)/include/tr50/util/blob.h \
	$(top_builddir)/include/tr50/util/compress.h \
	$(top_builddir)/include/tr50/util/dictionary.h \
	$(top_builddir)/include/tr50/util/event.h \
	$(top_builddir)/include/tr50/util/json.h \
	$(top_builddir)/include/tr50/util/log.h \
	$(top_builddir)/include/tr50/util/memory.h \
	$(top_builddir)/include/tr50/util/mutex.h \
	$(top_builddir)/include/tr50/util/platform.h \
	$(top_builddir)/include/tr50/util/swap.h \
	$(top_builddir)/include/tr50/util/tcp.h \
	$(top_builddir)/include/tr50/util/thread.h \
	$(top_builddir)/include/tr50/util/time.h \
	tr50.api.async.c \
	tr50.c \
	tr50.command.c \
        tr50.method.c \
	tr50.config.c \
	tr50.mailbox.c \
	tr50.message.c \
	tr50.payload.c \
	tr50.pending.c \
	tr50.stats.c \
	tr50.worker.c \
	tr50.worker.extended.c \
	mqtt/mqtt.async.c \
	mqtt/mqtt.c \
	mqtt/mqtt.msg.c \
	mqtt/mqtt.recv.c \
	mqtt/mqtt.qos.c \
	util/common/tr50.json.c \
	util/common/tr50.blob.c \
	util/@UTIL_OS_ABS@/@UTIL_OS_ABS@.blob.c \
	util/@UTIL_OS_ABS@/@UTIL_OS_ABS@.compress.c \
	util/@UTIL_OS_ABS@/@UTIL_OS_ABS@.event.c \
	util/@UTIL_OS_ABS@/@UTIL_OS_ABS@.log.c \
	util/@UTIL_OS_ABS@/@UTIL_OS_ABS@.memory.c \
	util/@UTIL_OS_ABS@/@UTIL_OS_ABS@.mutex.c \
	util/@UTIL_OS_ABS@/@UTIL_OS_ABS@.tcp.c \
	util/@UTIL_OS_ABS@/@UTIL_OS_ABS@.tcp_proxy.c \
	util/@UTIL_OS_ABS@/@UTIL_OS_ABS@.tcp_ssl.c \
	util/@UTIL_OS_ABS@/@UTIL_OS_ABS@.thread.c \
	util/@UTIL_OS_ABS@/@UTIL_OS_ABS@.time.c
libtr50_la_LDFLAGS = -lm -module -avoid-version -shared @BUILD_VERSIONING@
libtr50_la_CPPFLAGS = -I$(top_srcdir)/include

.PHONY: lib-copy clean-lib-copy

lib-copy: $(lib_LTLIBRARIES)
	@for soname in `echo | $(EGREP) "^dlname=" $^ | $(SED) -e "s|^dlname='\(.*\)'|\1|"`; do \
		echo "$$soname: copying $(abs_builddir)/.libs/$$soname to $(top_builddir)/$$soname"; \
		test -e $(abs_builddir)/.libs/$$soname && \
		cd $(top_builddir) && \
		rm -f $(top_builddir)/$$soname; \
		cp -f $(abs_builddir)/.libs/$$soname $$soname || true; \
		base_soname=`echo $$soname | $(SED) -e "s|\-@VERSION@||"`; \
		test -e $(abs_builddir)/.libs/$$base_soname && \
		echo "$$base_soname: copying $(abs_builddir)/.libs/$$base_soname to $(top_builddir)/$$base_soname"; \
		rm -f $(top_builddir)/$$base_soname; \
		cp -f $(abs_builddir)/.libs/$$base_soname $$base_soname || true; \
		test -e $(abs_builddir)/.libs/$$soname -a ! -e $(abs_builddir)/.libs/$$base_soname && \
		echo "$$base_soname: copying $(abs_builddir)/.libs/$$soname to $(top_builddir)/$$base_soname"; \
		rm -f $(top_builddir)/$$base_soname; \
		cp -f $(abs_builddir)/.libs/$$soname $$base_soname || true; \
	done

clean-lib-copy:
	@for soname in `echo $(lib_LTLIBRARIES) | $(SED) -e "s|\.la||"`; do \
		soname=$$soname-@VERSION@.so; \
		echo "$$soname: removing copy"; \
		test -e $(top_builddir)/$$soname && rm -f $(top_builddir)/$$soname || true; \
		base_soname=`echo $$soname | $(SED) -e "s|\-@VERSION@||"`; \
		echo "$$base_soname: removing copy"; \
		test -e $(top_builddir)/$$base_soname && rm -f $(top_builddir)/$$base_soname || true; \
	done

all-local: lib-copy

clean-local: clean-lib-copy
