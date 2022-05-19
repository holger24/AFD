dnl @synopsis ETR_SOCKET_NSL
dnl
dnl This macro figures out what libraries are required on this platform
dnl to link sockets programs.  It's usually -lsocket and/or -lnsl or
dnl neither.  We test for all three combinations.
dnl
dnl @version $Id: etr_socket_nsl.m4,v 1.1 2001/06/07 08:31:03 simons Exp $
dnl @author Warren Young <warren@etr-usa.com>
dnl
dnl Modified by changing CFLAGS to LIBS by Holger.Kiehl@dwd.de
dnl
AC_DEFUN([ETR_SOCKET_NSL],
[
AC_CACHE_CHECK(for libraries containing socket functions,
ac_cv_socket_libs, [
        oLIBS=$LIBS

        AC_LINK_IFELSE([AC_LANG_PROGRAM([[
                        #include <sys/types.h>
                        #include <sys/socket.h>
                        #include <netinet/in.h>
                        #include <arpa/inet.h>
                ]], [[
                        struct in_addr add;
                        int sd = socket(AF_INET, SOCK_STREAM, 0);
                        inet_ntoa(add);
                ]])],[ac_cv_socket_libs=-lc],[ac_cv_socket_libs=no])

        if test x"$ac_cv_socket_libs" = "xno"
        then
                LIBS="$oLIBS -lsocket"
                AC_LINK_IFELSE([AC_LANG_PROGRAM([[
                                #include <sys/types.h>
                                #include <sys/socket.h>
                                #include <netinet/in.h>
                                #include <arpa/inet.h>
                        ]], [[
                                struct in_addr add;
                                int sd = socket(AF_INET, SOCK_STREAM, 0);
                                inet_ntoa(add);
                        ]])],[ac_cv_socket_libs=-lsocket],[ac_cv_socket_libs=no])
        fi

        if test x"$ac_cv_socket_libs" = "xno"
        then
                LIBS="$oLIBS -lsocket -lnsl"
                AC_LINK_IFELSE([AC_LANG_PROGRAM([[
                                #include <sys/types.h>
                                #include <sys/socket.h>
                                #include <netinet/in.h>
                                #include <arpa/inet.h>
                        ]], [[
                                struct in_addr add;
                                int sd = socket(AF_INET, SOCK_STREAM, 0);
                                inet_ntoa(add);
                        ]])],[ac_cv_socket_libs="-lsocket -lnsl"],[ac_cv_socket_libs=no])
        fi

        LIBS=$oLIBS
])

        if test x"$ac_cv_socket_libs" = "xno"
        then
                AC_MSG_ERROR([Cannot find socket libraries])
        elif test x"$ac_cv_socket_libs" = "x-lc"
        then
                EXTRA_SOCKET_LIBS=""
        else
                EXTRA_SOCKET_LIBS="$ac_cv_socket_libs"
        fi

        AC_SUBST(EXTRA_SOCKET_LIBS)
]) dnl ETR_SOCKET_NSL
