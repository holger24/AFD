/*
 *  mode_t2str.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2018 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "afddefs.h"

DESCR__S_M3
/*
 ** NAME
 **   mode_t2str - converts a mode_t variable to readable string
 **
 ** SYNOPSIS
 **   void mode_t2str(mode_t mode, char *mstr)
 **
 ** DESCRIPTION
 **   This functions converts the variable mode to a human readable
 **   string presenting the file type and mode. The caller must
 **   ensure that mstr is at least 11 bytes long.
 **
 ** RETURN VALUES
 **   The function returns in mstr 11 bytes of the mode variable.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.08.2018 H.Kiehl Created
 **   19.03.2025 H.Kiehl When type is not set return ' ' (space)
 **                      instead of '?'.
 **
 */
DESCR__E_M3


#include <sys/stat.h>    /* S_IFMT, S_IFREG, ... */

/* Local function prototypes. */
static char mode_t2type(mode_t);


/*############################## mode_t2str() ###########################*/
void
mode_t2str(mode_t mode, char *mstr)
{
   *mstr = mode_t2type(mode);
   mstr++;
   *mstr = (mode & 0400) ? 'r' : '-';
   mstr++;
   *mstr = (mode & 0200) ? 'w' : '-';
   mstr++;
   if (mode & 04000)
   {
      *mstr = (mode & 0100) ? 's' : 'S';
   }
   else
   {
      *mstr = (mode & 0100) ? 'x' : '-';
   }
   mstr++;
   *mstr = (mode & 040) ? 'r' : '-';
   mstr++;
   *mstr = (mode & 020) ? 'w' : '-';
   mstr++;
   if (mode & 02000)
   {
      *mstr = (mode & 010) ? 's' : 'S';
   }
   else
   {
      *mstr = (mode & 010) ? 'x' : '-';
   }
   mstr++;
   *mstr = (mode & 04) ? 'r' : '-';
   mstr++;
   *mstr = (mode & 02) ? 'w' : '-';
   mstr++;
   if (mode & 01000)
   {
      *mstr = (mode & 01) ? 't' : 'T';
   }
   else
   {
      *mstr = (mode & 01) ? 'x' : '-';
   }
   mstr++;
   *mstr = '\0';

   return;
}


/*---------------------------- mode_t2type() ----------------------------*/
static char
mode_t2type(mode_t mode)
{
   switch(mode & S_IFMT)
   {
      case S_IFREG : return '-';
      case S_IFDIR : return 'd';
      case S_IFLNK : return 'l';
#ifdef S_IFSOCK
      case S_IFSOCK: return 's';
#endif
      case S_IFCHR : return 'c';
      case S_IFBLK : return 'b';
      case S_IFIFO : return 'p';
      default      : return ' ';
   }
}
