dnl
dnl
dnl AC_CHECK_LESSTIF_VERSION : determine if we have Lesstif and which version
dnl
AC_DEFUN([AC_CHECK_LESSTIF_VERSION],
[
AC_MSG_CHECKING([for Lesstif])
savedLIBS="$LIBS"
savedCPPFLAGS="$CPPFLAGS"
savedINCLUDES="$INCLUDES"
LIBS="$LIBS $link_motif $X_LIBS -lXt $X_PRE_LIBS $AFD_XP_LIB -lX11 $X_EXTRA_LIBS"
CPPFLAGS="$CPPFLAGS $include_motif $X_CFLAGS"
INCLUDES="$INCLUDES $include_motif $X_CFLAGS"
AC_RUN_IFELSE([AC_LANG_SOURCE([[
              #include <stdio.h>
              #include <string.h>
              #include "Xm/Xm.h"
              #define DATA "conftest.lesstifver"
              int main(void)
              {
                 FILE *fd;

                 if ((fd = fopen(DATA, "w")) != NULL)
                 {
                    if (fprintf(fd , "%d %d.%d\n", LesstifVersion, LESSTIF_VERSION, LESSTIF_REVISION) >= 0)
                    {
                       return(0);
                    }
                 }
                 return(1);
              }]])],[lesstif_library_ver=`cat conftest.lesstifver`],[lesstif_library_ver=0.0],[])
LIBS="$savedLIBS"
CPPFLAGS="$savedCPPFLAGS"
INCLUDES="$savedINCLUDES"
case "${lesstif_library_ver}"
in
   0.0)
      AC_MSG_RESULT([no])
      ;;
   *)
      AC_MSG_RESULT([${lesstif_library_ver}])
      AC_DEFINE(LESSTIF_WORKAROUND, 1, [Define this if you have Lesstif])
      ;;
esac
])
