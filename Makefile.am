ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST =			\
	aclocal.m4		\
	AUTHORS			\
	ChangeLog		\
	cleanup			\
	config.h.in		\
	configure		\
	configure.ac		\
	configure.bat		\
	configure.w32		\
	COPYING			\
	INSTALL			\
	Makefile.am		\
	Makefile.in		\
	Makefile.vxworks	\
	Makefile.w32		\
	NEWS			\
	README			\
	VERSION

DISTDIRS =			\
	build-aux		\
	examples		\
	include			\
	m4			\
	MSVC			\
	src

dist-hook: 
	(builddir=`pwd` ; cd $(srcdir) && ( \
		for dir in $(DISTDIRS); do \
			find $$dir -depth -print | egrep -v '(~$$)|(/\.git)|(/\.#)|(/\.deps)' \
			| cpio -pdum $$builddir/$(distdir) 2> /dev/null; \
		done \
	))

SUBDIRS = include src @BUILD_EXAMPLES@

distclean-local:
	-rm -f examples/.deps/linux.sysinfo.Po examples/.deps/sample.main.Po examples/Makefile
