dnl
dnl
dnl AC_CHECK_MOTIF_VERSION : determine if we have the correct Motif Version
dnl
AC_DEFUN([AC_CHECK_MOTIF_VERSION],
[
AC_MSG_CHECKING([for Motif version])
savedLIBS="$LIBS"
savedCPPFLAGS="$CPPFLAGS"
savedINCLUDES="$INCLUDES"
LIBS="$LIBS $link_motif $X_LIBS -lXt $X_PRE_LIBS $AFD_XP_LIB -lX11 $X_EXTRA_LIBS"
CPPFLAGS="$CPPFLAGS $include_motif $X_CFLAGS"
INCLUDES="$INCLUDES $include_motif $X_CFLAGS"
AC_TRY_RUN([
              #include <stdio.h>
              #include <string.h>
              #include "Xm/Xm.h"
              #define DATA "conftest.motifver"
              int main(void)
              {
                 FILE *fd;

                 if ((fd = fopen(DATA, "w")) != NULL)
                 {
                    if (fprintf(fd , "%d.%d\n", XmVERSION, XmREVISION) >= 0)
                    {
                       return(0);
                    }
                 }
                 return(1);
              }],
           [motif_library_ver=`cat conftest.motifver`],
           [motif_library_ver=0.0])
LIBS="$savedLIBS"
CPPFLAGS="$savedCPPFLAGS"
INCLUDES="$savedINCLUDES"
case "${motif_library_ver}"
in
   0.0)
      AC_MSG_RESULT([Unable to determine version])
      ;;
   0.* | 1.0* | 1.1*)
      AC_MSG_ERROR(Motif Version to old)
      ;;
   *)
      AC_MSG_RESULT([${motif_library_ver}])
      ;;
esac
])
