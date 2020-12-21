/*
 *  eval_dupcheck_options.c - Part of AFD, an automatic file distribution
 *                            program.
 *  Copyright (c) 2005 - 2020 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   eval_dupcheck_options - evaluate dupcheck options
 **
 ** SYNOPSIS
 **   char *eval_dupcheck_options(char         *ptr,
 **                               time_t       *timeout,
 **                               unsigned int *flag,
 **                               int          *warn)
 **
 ** DESCRIPTION
 **   This function evaluates the dupcheck option that can be specified
 **   under [dir options] and [options] which has the following format:
 **
 **   dupcheck[ <timeout>[ <check type>[ <action>[ <CRC type>]]]]
 **
 **        <timeout>    - Time in seconds when this CRC value is to be
 **                       discarded. (Default 3600)
 **        <check type> - What type of check is to be performed, the
 **                       following values are possible:
 **                         1 - Filename only. (default)
 **                         2 - File content.
 **                         3 - Filename and file content.
 **                         4 - Filname without last suffix.
 **                         5 - Filename and file size.
 **        <action>     - What action is to be taken when we find a
 **                       duplicate. The following values are possible:
 **                        24 - Delete. (default)
 **                        25 - Store the duplicate file in the following
 **                             directory: $AFD_WORK_DIR/files/store/<id>
 **                             Where <id> is the directoy id when this
 **                             is a input duplicate check or the job id
 **                             when it is an output duplicate check.
 **                        26 - Only warn is SYSTEM_LOG.
 **                        33 - Delete and warn.
 **                        34 - Store and warn.
 **        <CRC type>   - What type of CRC check is to be performed.
 **                        16 - CRC-32
 **                        17 - CRC-32c
 **
 ** RETURN VALUES
 **   Ruturns a ptr to the end of the given string.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.06.2005 H.Kiehl Created
 **   20.12.2006 H.Kiehl Added option filename without last suffix.
 **   20.05.2007 H.Kiehl Added warn option so caller knows if we had a
 **                      warning.
 **   24.11.2012 H.Kiehl Added support for CRC-32c.
 **
 */
DESCR__E_M3

#include <stdlib.h>
#include <ctype.h>


/*####################### eval_dupcheck_options() #######################*/
char *
eval_dupcheck_options(char         *ptr,
                      time_t       *timeout,
                      unsigned int *flag,
                      int          *warn)
{
   int  length = 0;
   char number[MAX_LONG_LENGTH + 1];

   ptr += DUPCHECK_ID_LENGTH;
   while ((*ptr == ' ') || (*ptr == '\t'))
   {
      ptr++;
   }
   while ((isdigit((int)(*ptr))) && (length < MAX_LONG_LENGTH))
   {
      number[length] = *ptr;
      ptr++; length++;
   }
   if ((length > 0) && (length != MAX_LONG_LENGTH))
   {
      number[length] = '\0';
      *timeout = str2timet(number, NULL, 10);
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      length = 0;
      while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
      {
         number[length] = *ptr;
         ptr++; length++;
      }
      if ((length > 0) && (length != MAX_INT_LENGTH))
      {
         int val;

         number[length] = '\0';
         val = atoi(number);
         if (val == DC_FILENAME_ONLY_BIT)
         {
            *flag = DC_FILENAME_ONLY;
         }
         else if (val == DC_FILENAME_AND_SIZE_BIT)
              {
                 *flag = DC_FILENAME_AND_SIZE;
              }
         else if (val == DC_NAME_NO_SUFFIX_BIT)
              {
                 *flag = DC_NAME_NO_SUFFIX;
              }
         else if (val == DC_FILE_CONTENT_BIT)
              {
                 *flag = DC_FILE_CONTENT;
              }
         else if (val == DC_FILE_CONT_NAME_BIT)
              {
                 *flag = DC_FILE_CONT_NAME;
              }
              else
              {
                 *flag = DC_FILENAME_ONLY;
                 if (warn == NULL)
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               _("Unkown duplicate check type %d using default %d."),
                               val, DC_FILENAME_ONLY_BIT);
                 }
                 else
                 {
                    system_log(WARN_SIGN, __FILE__, __LINE__,
                               _("Unkown duplicate check type %d."), val);
                 }
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            _("Possible types are: %d (filename only), %d (filename and size), %d (filename no suffix), %d (file content only) and %d (filename and content)."),
                            DC_FILENAME_ONLY_BIT, DC_FILENAME_AND_SIZE_BIT,
                            DC_NAME_NO_SUFFIX_BIT, DC_FILE_CONTENT_BIT,
                            DC_FILE_CONT_NAME_BIT);
                 if (warn != NULL)
                 {
                    *warn = 1;
                 }
              }
         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         length = 0;
         while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
         {
            number[length] = *ptr;
            ptr++; length++;
         }
         if ((length > 0) && (length != MAX_INT_LENGTH))
         {
            number[length] = '\0';
            val = atoi(number);
            if (val == DC_DELETE_BIT)
            {
               *flag |= DC_DELETE;
            }
            else if (val == DC_STORE_BIT)
                 {
                    *flag |= DC_STORE;
                 }
            else if (val == DC_WARN_BIT)
                 {
                    *flag |= DC_WARN;
                 }
            else if (val == DC_DELETE_WARN_BIT)
                 {
                    *flag |= (DC_DELETE | DC_WARN);
                 }
            else if (val == DC_STORE_WARN_BIT)
                 {
                    *flag |= (DC_STORE | DC_WARN);
                 }
                 else
                 {
                    *flag |= DC_DELETE;
                    if (warn == NULL)
                    {
                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                  _("Unkown duplicate check action %d using default %d."),
                                  val, DC_DELETE);
                    }
                    else
                    {
                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                  _("Unkown duplicate check action %d."), val);
                       *warn = 1;
                    }
                 }

            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }
            length = 0;
            while ((isdigit((int)(*ptr))) && (length < MAX_INT_LENGTH))
            {
               number[length] = *ptr;
               ptr++; length++;
            }
            if ((length > 0) && (length != MAX_INT_LENGTH))
            {
               number[length] = '\0';
               val = atoi(number);
               if (val == DC_CRC32_BIT)
               {
                  *flag |= DC_CRC32;
               }
               else if (val == DC_CRC32C_BIT)
                    {
                       *flag |= DC_CRC32C;
                    }
               else if (val == DC_MURMUR3_BIT)
                    {
                       *flag |= DC_MURMUR3;
                    }
                    else
                    {
                       *flag |= DC_CRC32;
                       if (warn == NULL)
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     _("Unkown duplicate check CRC type %d using default %d."),
                                     val, DC_CRC32_BIT);
                       }
                       else
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     _("Unkown duplicate check CRC type %d."), val);
                          *warn = 1;
                       }
                    }
            }
            else
            {
               *flag |= DC_CRC32;
               if (length == MAX_INT_LENGTH)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             _("Integer value for duplicate check CRC type to large."));
                  if (warn != NULL)
                  {
                     *warn = 1;
                  }
               }
            }
         }
         else
         {
            *flag |= (DC_DELETE | DC_CRC32);
            if (length == MAX_INT_LENGTH)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Integer value for duplicate check action to large."));
               if (warn != NULL)
               {
                  *warn = 1;
               }
            }
         }
      }
      else
      {
         *flag = DC_FILENAME_ONLY | DC_CRC32 | DC_DELETE;
         if (length == MAX_INT_LENGTH)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Integer value for duplicate check type to large."));
            if (warn != NULL)
            {
               *warn = 1;
            }
         }
      }
   }
   else
   {
      *timeout = DEFAULT_DUPCHECK_TIMEOUT;
      *flag = DC_FILENAME_ONLY | DC_CRC32 | DC_DELETE;
      if (length == MAX_LONG_LENGTH)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Long integer value for duplicate check timeout to large."));
         if (warn != NULL)
         {
            *warn = 1;
         }
      }
   }
   while ((*ptr != '\n') && (*ptr != '\0'))
   {
      ptr++;
   }

   return(ptr);
}
