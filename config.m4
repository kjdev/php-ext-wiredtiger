dnl config.m4 for extension wiredtiger

dnl Check PHP version:
AC_MSG_CHECKING(PHP version)
if test ! -z "$phpincludedir"; then
    PHP_VERSION=`grep 'PHP_VERSION ' $phpincludedir/main/php_version.h | sed -e 's/.*"\([[0-9\.]]*\)".*/\1/g' 2>/dev/null`
elif test ! -z "$PHP_CONFIG"; then
    PHP_VERSION=`$PHP_CONFIG --version 2>/dev/null`
fi

if test x"$PHP_VERSION" = "x"; then
    AC_MSG_WARN([none])
else
    PHP_MAJOR_VERSION=`echo $PHP_VERSION | sed -e 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\1/g' 2>/dev/null`
    PHP_MINOR_VERSION=`echo $PHP_VERSION | sed -e 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\2/g' 2>/dev/null`
    PHP_RELEASE_VERSION=`echo $PHP_VERSION | sed -e 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\3/g' 2>/dev/null`
    AC_MSG_RESULT([$PHP_VERSION])
fi

if test $PHP_MAJOR_VERSION -lt 5; then
    AC_MSG_ERROR([need at least PHP 5 or newer])
fi

dnl WiredTiger Extension
PHP_ARG_ENABLE(wiredtiger, whether to enable wiredtiger support,
[  --enable-wiredtiger      Enable wiredtiger support])

if test "$PHP_WIREDTIGER" != "no"; then

    dnl Check for WiredTiger header
    PHP_ARG_WITH(wiredtiger-includedir, for WiredTiger header,
    [ --with-wiredtiger-includedir=DIR WiredtTger header path], yes)

    if test "$PHP_WIREDTIGER_INCLUDEDIR" != "no" && test "$PHP_WIREDTIGER_INCLUDEDIR" != "yes"; then
        if test -r "$PHP_WIREDTIGER_INCLUDEDIR/wiredtiger.h"; then
            WIREDTIGER_INCLUDES="$PHP_WIREDTIGER_INCLUDEDIR"
        else
            AC_MSG_ERROR([Can't find WiredTiger headers under "$PHP_WIREDTIGER_INCLUDEDIR"])
        fi
    else
        SEARCH_PATH="/usr/local /usr"
        SEARCH_FOR="/include/wiredtiger.h"
        if test -r $PHP_WIREDTIGER/$SEARCH_FOR; then
            WIREDTIGER_INCLUDES="$PHP_WIREDTIGER/include"
        else
            AC_MSG_CHECKING([for WiredTiger header files in default path])
            for i in $SEARCH_PATH ; do
                if test -r $i/$SEARCH_FOR; then
                    WIREDTIGER_INCLUDES="$i/include"
                    AC_MSG_RESULT(found in $i)
                fi
            done
        fi
    fi

    if test -z "$WIREDTIGER_INCLUDES"; then
        AC_MSG_RESULT([not found])
        AC_MSG_ERROR([Can't find WiredTiger headers])
    fi

    PHP_ADD_INCLUDE($WIREDTIGER_INCLUDES)

    dnl Check for WiredTiger library
    PHP_ARG_WITH(wiredtiger-libdir, for WiredTiger library,
    [ --with-wiredtiger-libdir=DIR WiredTiger library path], yes)

    LIBNAME_C=wiredtiger
    AC_MSG_CHECKING([for WiredTiger])
    AC_LANG_SAVE

    save_CFLAGS="$CFLAGS"
    wiredtiger_CFLAGS="-I$WIREDTIGER_INCLUDES"
    CFLAGS="$save_CFLAGS $wiredtiger_CFLAGS"

    save_LDFLAGS="$LDFLAGS"
    wiredtiger_LDFLAGS="-L$PHP_WIREDTIGER_LIBDIR -l$LIBNAME_C"
    LDFLAGS="$save_LDFLAGS $wiredtiger_LDFLAGS"

    AC_TRY_LINK(
    [
        #include "wiredtiger.h"
    ],[
        WT_CONNECTION *conn;
        wiredtiger_open(".", NULL, NULL, &conn);
    ],[
        AC_MSG_RESULT(yes)
        PHP_ADD_LIBRARY_WITH_PATH($LIBNAME_C, $PHP_WIREDTIGER_LIBDIR, WIREDTIGER_SHARED_LIBADD)
        AC_DEFINE(HAVE_WIREDTIGERLIB,1,[ ])
    ],[
        AC_MSG_RESULT([error])
        AC_MSG_ERROR([wrong WiredTiger lib version or lib not found])
    ])
    CFLAGS="$save_CFLAGS"
    LDFLAGS="$save_LDFLAGS"

    PHP_SUBST(WIREDTIGER_SHARED_LIBADD)

    dnl PHP Extension
    PHP_NEW_EXTENSION(wiredtiger, wiredtiger.c exception.c db.c cursor.c, $ext_shared)
fi

dnl coverage
PHP_ARG_ENABLE(coverage, whether to enable coverage support,
[  --enable-coverage     Enable coverage support], no, no)

if test "$PHP_COVERAGE" != "no"; then
    EXTRA_CFLAGS="--coverage"
    PHP_SUBST(EXTRA_CFLAGS)
fi
