#
# SYNOPSIS
#
#   MP_WITH_NCURSES
#
# DESCRIPTION
#
#   Detect SysV compatible curses, such as ncurses.
#
#   Defines HAVE_NCURSES_H if ncurses is found. NCURSES_LIB is also set with
#   the required libary, but is not appended to LIBS automatically. If no
#   working curses libary is found NCURSES_LIB will be left blank.
#   
#   This macro was modified to just check for ncurses. The original was
#   taken from http://autoconf-archive.cryp.to/mp_with_curses.html
#
# LAST MODIFICATION
#
#   2008-08-26  Holger.Kiehl@dwd.de
#
# COPYLEFT
#
#   Copyright (c) 2008 Mark Pulford <mark@kyne.com.au>
#
#   This program is free software: you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation, either version 3 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <http://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Macro Archive. When you make and
#   distribute a modified version of the Autoconf Macro, you may extend this
#   special exception to the GPL to apply to your modified version as well.

AC_DEFUN([MP_WITH_NCURSES],
  [mp_save_LIBS="$LIBS"
   NCURSES_LIB=""
   AC_CACHE_CHECK([for working ncurses], mp_cv_ncurses,
     [LIBS="$mp_save_LIBS -lncurses"
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <ncurses.h>]], [[chtype a; int b=A_STANDOUT, c=KEY_LEFT; initscr(); ]])],[mp_cv_ncurses=yes],[mp_cv_ncurses=no])])
   if test "$mp_cv_ncurses" = yes
   then
     AC_DEFINE([HAVE_NCURSES_H],[1],[Define if you have ncurses.h])
     NCURSES_LIB="-lncurses"
   fi
   LIBS="$mp_save_LIBS"
])dnl
