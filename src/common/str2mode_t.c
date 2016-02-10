/*
 *  str2mode_t.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   str2mode_t - converts a octal mode string to mode_t
 **
 ** SYNOPSIS
 **   mode_t str2mode_t(char *mode_str)
 **
 ** DESCRIPTION
 **   This functions converts the octal mode string mode_str to
 *    to a mode_t value.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to perform the action. Otherwise
 **   0 is returned and the file size 'file_size' if the size of the
 **   files have been changed due to the action (compress, uncompress,
 **   gzip, gunzip, tiff2gts and extract).
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.06.2015 H.Kiehl Created
 **
 */
DESCR__E_M3



/*############################## str2mode_t() ###########################*/
mode_t
str2mode_t(char *mode_str)
{
   int    n = strlen(mode_str);
   mode_t mode = 0;
   char   *ptr = mode_str;

   if ((n == 3) || (n == 4))
   {
      if (n == 4)
      {
         switch (*ptr)
         {
            case '7' :
               mode |= S_ISUID | S_ISGID | S_ISVTX;
               break;
            case '6' :
               mode |= S_ISUID | S_ISGID;
               break;
            case '5' :
               mode |= S_ISUID | S_ISVTX;
               break;
            case '4' :
               mode |= S_ISUID;
               break;
            case '3' :
               mode |= S_ISGID | S_ISVTX;
               break;
            case '2' :
               mode |= S_ISGID;
               break;
            case '1' :
               mode |= S_ISVTX;
               break;
            case '0' : /* Nothing to be done here. */
               break;
            default :
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Incorrect mode %c%c%c%c",
                          ptr, ptr + 1, ptr + 2, ptr + 3);
               break;
         }
         ptr++;
      }
      switch (*ptr)
      {
         case '7' :
            mode |= S_IRUSR | S_IWUSR | S_IXUSR;
            break;
         case '6' :
            mode |= S_IRUSR | S_IWUSR;
            break;
         case '5' :
            mode |= S_IRUSR | S_IXUSR;
            break;
         case '4' :
            mode |= S_IRUSR;
            break;
         case '3' :
            mode |= S_IWUSR | S_IXUSR;
            break;
         case '2' :
            mode |= S_IWUSR;
            break;
         case '1' :
            mode |= S_IXUSR;
            break;
         case '0' : /* Nothing to be done here. */
            break;
         default :
            if (n == 4)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Incorrect mode %c%c%c%c",
                          ptr - 1, ptr, ptr + 1, ptr + 2);
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Incorrect mode %c%c%c", ptr, ptr + 1, ptr + 2);
            }
            break;
      }
      ptr++;
      switch (*ptr)
      {
         case '7' :
            mode |= S_IRGRP | S_IWGRP | S_IXGRP;
            break;
         case '6' :
            mode |= S_IRGRP | S_IWGRP;
            break;
         case '5' :
            mode |= S_IRGRP | S_IXGRP;
            break;
         case '4' :
            mode |= S_IRGRP;
            break;
         case '3' :
            mode |= S_IWGRP | S_IXGRP;
            break;
         case '2' :
            mode |= S_IWGRP;
            break;
         case '1' :
            mode |= S_IXGRP;
            break;
         case '0' : /* Nothing to be done here. */
            break;
         default :
            if (n == 4)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Incorrect mode %c%c%c%c",
                          ptr - 2, ptr - 1, ptr, ptr + 1);
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Incorrect mode %c%c%c", ptr - 1, ptr, ptr + 1);
            }
            break;
      }
      ptr++;
      switch (*ptr)
      {
         case '7' :
            mode |= S_IROTH | S_IWOTH | S_IXOTH;
            break;
         case '6' :
            mode |= S_IROTH | S_IWOTH;
            break;
         case '5' :
            mode |= S_IROTH | S_IXOTH;
            break;
         case '4' :
            mode |= S_IROTH;
            break;
         case '3' :
            mode |= S_IWOTH | S_IXOTH;
            break;
         case '2' :
            mode |= S_IWOTH;
            break;
         case '1' :
            mode |= S_IXOTH;
            break;
         case '0' : /* Nothing to be done here. */
            break;
         default :
            if (n == 4)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Incorrect mode %c%c%c%c",
                          ptr - 3, ptr - 2, ptr - 1, ptr);
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Incorrect mode %c%c%c", ptr - 2, ptr - 1, ptr);
            }
            break;
      }
   }

   return(mode);
}
