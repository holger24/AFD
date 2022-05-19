dnl
dnl Check for libXp
dnl In fact this check ensures that
dnl  - <X11/extensions/Print.h> and
dnl  - both libXp libXext
dnl are in place
dnl Note that a simpler check only for the libraries would not
dnl be sufficient perhaps.
dnl If the test succeeds it defines Have_Libxp within our
dnl Makefiles. Perhaps one should immediately add those libs
dnl to link commands which include libXm version2.1?!
dnl
dnl (NOTE: This was taken from Xbae ac_find_motif.m4 macro)
dnl
AC_DEFUN([AC_FIND_LIBXP],
[AC_REQUIRE([AC_PATH_XTRA])
AC_CACHE_CHECK(whether libXp is available, lt_cv_libxp,
[lt_save_CFLAGS="$CFLAGS"
lt_save_CPPFLAGS="$CPPFLAGS"
lt_save_LIBS="$LIBS"
LIBS="$X_LIBS -lXp -lXext -lXt $X_PRE_LIBS -lX11 $X_EXTRA_LIBS $LIBS"
CFLAGS="$X_CFLAGS $CFLAGS"
CPPFLAGS="$X_CFLAGS $CPPFLAGS"
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <X11/Intrinsic.h>
#include <X11/extensions/Print.h>
]], [[
int main() {
Display *display=NULL;
short   major_version, minor_version;
Status rc;
rc=XpQueryVersion(display, &major_version, &minor_version);
exit(0);
}
]])],[lt_cv_libxp=yes],[lt_cv_libxp=no])
])
if test "$lt_cv_libxp" = "yes"; then
  AC_DEFINE(HAVE_LIB_XP, [], [With -lXp support])
  AFD_XP_LIB="-lXp"
else
  AFD_XP_LIB=""
fi
AC_SUBST(AFD_XP_LIB)
CFLAGS="$lt_save_CFLAGS"
CPPFLAGS="$lt_save_CPPFLAGS"
LIBS="$lt_save_LIBS"
])
