AC_PREREQ([2.69])
AC_INIT([libtr50], [m4_esyscmd([tr -d '\n' < VERSION])], [tr50@ilstechnology.com])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/tr50.c])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.14 foreign silent-rules subdir-objects -Wall -Werror])
AM_MAINTAINER_MODE
AM_PROG_AR

LT_PREREQ([2.2])
LT_INIT([dlopen shared])

# Command-line Option Checks

# The user may specify an Operating System implementation for the specific
# set of function abstractions required by the TR50 library. By default,
# the library build is set to "linux", with the accompanying C Language
# implementation files located in the src/linux sub-directory.
#
# For additional information regarding existing Operating System support, as
# well as extending support to new Operating Systems, please refer to the
# README file which should have been included with the distribution of
# the TR50 library code.
AC_ARG_WITH([util], [AS_HELP_STRING([--with-util=[DIR]], [Specify the OS abstraction utility layer])], [util=${withval}])

# Attempt to autoconfigure our OS abstraction utility layer.
AC_CANONICAL_HOST
if test -z "$util"; then
	# Check for which host we are on and setup a few things
	# specifically based on the host
	case $host_os in
	darwin*)
		UTIL_OS_ABS="darwin"
		AC_MSG_ERROR([Your platform is not currently supported])
		;;
	linux*)
		UTIL_OS_ABS="linux"
		;;
	freebsd*)
		UTIL_OS_ABS="freebsd"
		;;
	*)
		AC_MSG_ERROR([Your platform is not currently supported])
		;;
	esac
else
	UTIL_OS_ABS="$util"
fi

# Build example programs
AC_ARG_WITH([examples], [AS_HELP_STRING([--with-examples], [Build optional example code])], [], [with_examples=no])
if test "x$with_examples" != xno; then
	BUILD_EXAMPLES="examples"
else
	BUILD_EXAMPLES=""
fi

AC_ARG_ENABLE([versioning], AS_HELP_STRING([--disable-versioning], [Disable libtool versioning of shared libraries]))
if test "x$enable_versioning" != "xno"; then
	BUILD_VERSIONING="-release"
	if -e "VERSION"; then
		BUILD_VERSIONING="${BUILD_VERSIONING} $(tr -d '\n' < VERSION)"
	else
		if test -n "${PACKAGE_VERSION}"; then
			BUILD_VERSIONING="${BUILD_VERSIONING} ${PACKAGE_VERSION}"
		else
			# If all else fails, set the version to something
			# slightly spurious (and perhaps disconcerting).
			BUILD_VERSIONING="${BUILD_VERSIONING} 0.0"
		fi
	fi
else
	BUILD_VERSIONING=""
fi

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for libraries.
AC_SEARCH_LIBS([pthread_create], [pthread])

AC_CHECK_LIB([dl], [dlopen])
AC_CHECK_LIB([crypto], [AES_encrypt], [], [AC_MSG_FAILURE([could not find SSL])])
AC_CHECK_LIB([ssl], [SSL_library_init], [], [AC_MSG_FAILURE([could not find SSL])])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([floor gethostbyname gettimeofday memset pow select socket strchr strrchr strstr])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h float.h limits.h netdb.h netinet/in.h pthread.h stddef.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Determine endianness.
AC_ARG_WITH([endian], [AS_HELP_STRING([--with-endian], [Supply value of 'big' or 'little' to force endianess])],
[ if test "$withval" = "big"; then
	AC_MSG_CHECKING([checking whether byte ordering is bigendian])
	AC_MSG_RESULT([yes (forced)])
	ac_cv_c_bigendian="yes"
else
	if test "$withval" = "little"; then
		AC_MSG_CHECKING([checking whether byte ordering is bigendian])
		AC_MSG_RESULT([no (forced)])
		ac_cv_c_bigendian="no"
	else
		AC_C_BIGENDIAN()
	fi
fi ], [AC_C_BIGENDIAN()])

AC_ARG_WITH([cross-endian-doubles], [AS_HELP_STRING([--with-cross-endian-doubles], [Enable cross endian doubles])],
[ if test "$withval" = "yes"; then
	CROSS_ENDIAN_DOUBLES="#define CROSS_ENDIAN_DOUBLES 1"
else
	CROSS_ENDIAN_DOUBLES="#undef CROSS_ENDIAN_DOUBLES"
fi], [CROSS_ENDIAN_DOUBLES="#undef CROSS_ENDIAN_DOUBLES"])

AC_C_CONST
AC_C_INLINE

if test "$ac_cv_c_bigendian" = yes; then
        AC_SUBST(BIG_BYTEORDER, ["#define _TR50_BIG_ENDIAN 1"])
        AC_SUBST(LITTLE_BYTEORDER, ["#undef _TR50_LITTLE_ENDIAN"])
else
        AC_SUBST(BIG_BYTEORDER, ["#undef _TR50_BIG_ENDIAN"])
        AC_SUBST(LITTLE_BYTEORDER, ["#define _TR50_LITTLE_ENDIAN 1"])
fi

AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_LDFLAGS])
AC_SUBST([CROSS_ENDIAN_DOUBLES])
AC_SUBST([LIBS])
AC_SUBST([UTIL_OS_ABS])
AC_SUBST([BUILD_EXAMPLES])
AC_SUBST([BUILD_VERSIONING])

AC_CONFIG_FILES([Makefile examples/Makefile include/Makefile src/Makefile include/tr50/util/platform.h])
AC_OUTPUT
