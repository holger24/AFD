/*
 *  eval_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2012 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   rr_file_name_file - modifies file with filenames
 **
 ** SYNOPSIS
 **   void rr_file_name_file(char *file_name, struct data *p_db)
 **
 ** DESCRIPTION
 **   This function applies the given rename-rule to all the filenames
 **   in file_name_file, appending the target/remote name to the local name.
 **   If there already is a rename-name, it is overwritten.
 **   Afterwards the file_name_file is re-written with the changes.
 **
 ** RETURN VALUES
 **   SUCCESS or INCORRECT.
 **
 ** AUTHOR
 **   A.Maul
 **
 ** HISTORY
 **   02.01.2019 A.Maul  Created.
 */
DESCR__E_M3

#include <stdio.h>                 /* stderr, fprintf()                  */
#include <stdlib.h>                /* atoi()                             */
#include <string.h>                /* strcpy(), strerror(), strcmp()     */
#include <unistd.h>                /* getpid(), write(), close()         */
#include <fcntl.h>
#include <errno.h>

extern char file_name_file[], file_name_file_copy[];

/*####################### _rr_file_name_file() #############################*/
int rr_file_name_file(struct rule *rule, int rule_no, char *filter, char *rename_to)
{
   int ret;
   char *buffer, *modbuf;
   off_t buffer_sz, modbuf_sz;
   struct stat foo;
   int fd;

   if (file_name_file_copy[0] == '\0')
   {
      (void) strcpy(file_name_file_copy, file_name_file);
      (void) strcat(file_name_file_copy, ".rr");
   }
   if (stat(file_name_file_copy, &foo) != 0)
   {
      if ((buffer_sz = read_file_no_cr(file_name_file, &buffer, NO, __FILE__, __LINE__)) == INCORRECT)
      {
         ret = INCORRECT;
      }
      if ((fd = open(file_name_file_copy, (O_WRONLY | O_CREAT | O_TRUNC), (S_IRUSR | S_IWUSR))) == -1)
      {
         (void) fprintf(stderr, "Failed to open() `%s' : %s (%s %d)\n", file_name_file_copy, strerror(errno), __FILE__,
               __LINE__);
         return (INCORRECT);
      }
      else
      {
         if (write(fd, buffer, buffer_sz) != buffer_sz)
         {
            (void) fprintf(stderr, "Failed to write() to `%s' : %s (%s %d)\n", file_name_file_copy, strerror(errno),
                  __FILE__, __LINE__);
            return (INCORRECT);
         }
         if (close(fd) == -1)
         {
            (void) fprintf(stderr, "Failed to close() `%s' : %s (%s %d)\n", file_name_file_copy, strerror(errno),
                  __FILE__, __LINE__);
         }
         ret = SUCCESS;
      }
   }
   else
   {
      if ((buffer_sz = read_file_no_cr(file_name_file_copy, &buffer, NO, __FILE__, __LINE__)) == INCORRECT)
      {
         ret = INCORRECT;
      }
      ret = SUCCESS;
   }
   if (ret != INCORRECT)
   {
      char *ptr = buffer;
      char *p_fn, filename[MAX_PATH_LENGTH], realname[MAX_PATH_LENGTH];
      size_t length;
      register int i, k;

      modbuf_sz = 0;
      modbuf = NULL;
      length = 0;

      do
      {
         i = 0;
         while ((*ptr != '\n') && (*ptr != '|') && (*ptr != '\0'))
         {
            filename[i] = *ptr;
            ptr++;
            i++;
         }
         if (*ptr == '\n')
         {
            ptr++;
         }
         filename[i] = '\0';
         if (*ptr == '|')
         {
            i = 0;
            ptr++;
            while ((*ptr != '\n') && (*ptr != '\0'))
            {
               realname[i] = *ptr;
               ptr++;
               i++;
            }
            if (*ptr == '\n')
            {
               ptr++;
            }
            realname[i] = '\0';
            if (!(p_fn = strrchr(realname, '/')))
            {
               p_fn = realname;
            }
         }
         else
         {
            realname[0] = '\0';
            if (!(p_fn = strrchr(filename, '/')))
            {
               p_fn = filename;
            }
         }
         if (*p_fn == '/')
         {
            p_fn++;
         }
         int counter_fd = -1, *unique_counter;
         if (rule_no >= 0)
         {
            /* find and use rule from file rename.rule */
            for (k = 0; k < rule[rule_no].no_of_rules; k++)
            {
               if (pmatch(rule[rule_no].filter[k], p_fn, NULL ) == 0)
               {
                  change_name(p_fn, rule[rule_no].filter[k], rule[rule_no].rename_to[k], realname, MAX_PATH_LENGTH,
                        &counter_fd, &unique_counter, 0);
                  break;
               }
            }
         }
         else
            if ((filter != NULL )&& (rename_to != NULL) && (filter[0] != '\0') && (rename_to[0] != '\0')){
            /* use single rename */
            change_name(p_fn, filter, rename_to, realname, MAX_PATH_LENGTH,
                  &counter_fd, &unique_counter, 0);
         }
         if ((length + strlen(filename) + strlen(realname) + 2) >= modbuf_sz)
         {
            modbuf_sz += 2 * MAX_PATH_LENGTH;
            if (!(modbuf = (char*) realloc(modbuf, modbuf_sz * sizeof(char))))
            {
               (void) fprintf(stderr, "Could not realloc() memory : %s [%s %d]\n", strerror(errno), __FILE__, __LINE__);
               return (INCORRECT);
            }
         }
         if (realname[0])
         {
            length += sprintf(&modbuf[length], "%s|%s\n", filename, realname);
         }
         else
         {
            length += sprintf(&modbuf[length], "%s\n", filename);
         }
      } while ((*ptr != '\0') && (ptr < buffer + buffer_sz));

      if ((fd = open(file_name_file, (O_WRONLY | O_CREAT | O_TRUNC), (S_IRUSR | S_IWUSR))) == -1)
      {
         (void) fprintf(stderr, "Failed to open() `%s' : %s (%s %d)\n", file_name_file, strerror(errno), __FILE__,
               __LINE__);
         return (INCORRECT);
      }
      else
      {
         if (write(fd, modbuf, length) != length)
         {
            (void) fprintf(stderr, "Failed to write() to `%s' : %s (%s %d)\n", file_name_file, strerror(errno),
                  __FILE__, __LINE__);
            return (INCORRECT);
         }
         if (close(fd) == -1)
         {
            (void) fprintf(stderr, "Failed to close() `%s' : %s (%s %d)\n", file_name_file, strerror(errno), __FILE__,
                  __LINE__);
         }
      }
      ret = SUCCESS;
      free(buffer);
      free(modbuf);
   }

   return (ret);
}
