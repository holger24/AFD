/*
 *  tfilter.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **   tfilter - test pattern matching function pmatch()
 **
 ** SYNOPSIS
 **   tfilter[ -l loops][ <filter> <file-name>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.12.1998 H.Kiehl Created
 **   24.03.2000 H.Kiehl Added -l loop option.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>    /* malloc()                                       */
#include <unistd.h>    /* STDERR_FILENO                                  */
#include <errno.h>

/* Global variables. */
int        sys_log_fd = STDERR_FILENO;
char       **fl = NULL, /* File name list */
           *hl = NULL,  /* Hit list */
           **pl = NULL, /* Pattern list */
           *p_work_dir;
const char *pmatch_file = "pmatch.data",
           *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static int store_pmatch_data(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ tfilter() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          no_of_patterns,
                patterns_checked,
                ret;
   register int i, j,
                loops = 1;
   char         filename[MAX_FILENAME_LENGTH];

   if ((get_arg(&argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      (void)fprintf(stderr,
                    "Usage: %s [-l <loops>][ <pattern> <file-name>]\n",
                    argv[0]);
      return(0);
   }
   if (get_arg(&argc, argv, "-l", filename, MAX_INT_LENGTH) == SUCCESS)
   {
      loops = atoi(filename);
   }
   if (get_arg(&argc, argv, "-f", filename, MAX_FILENAME_LENGTH) != SUCCESS)
   {
      (void)strcpy(filename, "pmatch.data");
   }
   if (argc == 3)
   {
      ret = strlen(argv[1]);
      if ((argv[1][0] == '"') && (argv[1][ret - 1] == '"'))
      {
         RT_ARRAY(pl, 1, ret - 1, char);
         (void)strncpy(pl[0], &argv[1][1], ret - 2);
         pl[0][ret - 2] = '\0';
      }
      else
      {
         RT_ARRAY(pl, 1, ret + 1, char);
         (void)strcpy(pl[0], argv[1]);
      }
      ret = strlen(argv[2]);
      RT_ARRAY(fl, 1, ret + 1, char);
      (void)strcpy(fl[0], argv[2]);
      if ((hl = malloc(1 * sizeof(char))) == NULL)
      {
         (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         return(1);
      }
      hl[0] = '?';
      no_of_patterns = 1;
   }
   else if (argc == 1)
        {
           no_of_patterns = store_pmatch_data(filename);
        }
        else
        {
           (void)fprintf(stderr,
                         "Usage: %s [-l <loops>][ <pattern> <file-name>]\n",
                         argv[0]);
           return(0);
        }

   patterns_checked = 0;
   for (i = 0; i < loops; i++)
   {
      for (j = 0; j < no_of_patterns; j++)
      {
         ret = pmatch(pl[j], fl[j], NULL);
         switch (hl[j])
         {
            case 'h': /* Expect Hit. */
               if (ret != 0)
               {
                  (void)printf("Expecting a hit but got a miss for %s|%s\n",
                               pl[j], fl[j]);
               }
               else
               {
                  if (i == 0)
                  {
                     patterns_checked++;
                  }
               }
               break;

            case 'm': /* Expect Miss. */
               if (ret == 0)
               {
                  (void)printf("Expecting a miss but got a hit for %s|%s\n",
                               pl[j], fl[j]);
               }
               else
               {
                  if (i == 0)
                  {
                     patterns_checked++;
                  }
               }
               break;

            default : /* Show result. */
               if (ret == 0)
               {
                  (void)printf("%s|%s|h\n", pl[j], fl[j]);
               }
               else
               {
                  (void)printf("%s|%s|m\n", pl[j], fl[j]);
               }
               if (i == 0)
               {
                  patterns_checked++;
               }
               break;
         }
      }
   }
   if (no_of_patterns > 1)
   {
      if (i > 1)
      {
         (void)printf("Have successfully checked %d patterns in %d loops.\n",
                       patterns_checked, i);
      }
      else
      {
         (void)printf("Have successfully checked %d patterns.\n",
                       patterns_checked);
      }
   }

   return(0);
}


/*######################### store_pmatch_data() #########################*/
static int
store_pmatch_data(char *file_name)
{
   off_t bl; /* Buffer length */
   char  *file_buffer = NULL;

   if ((bl = read_file(file_name, &file_buffer)) > 0)
   {
      int  length,
           max_pl, /* Max pattern length */
           max_fl, /* Max file name length */
           no_of_patterns;
      char *ptr,
           *p_start;

      no_of_patterns = max_pl = max_fl = 0;
      ptr = file_buffer;
      do
      {
         p_start = ptr;
         while ((*ptr != '|') && (*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
         if (*ptr == '|')
         {
            ptr++;
            length = ptr - p_start;
            if (length > max_pl)
            {
               max_pl = length;
            }
            p_start = ptr;
            while ((*ptr != '|') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
            if ((*ptr == '|') || (*ptr == '\n'))
            {
               if (*ptr == '|')
               {
                  ptr++;
                  while ((*ptr != '\n') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
               }
               else
               {
                  ptr++;
               }
               length = ptr - p_start;
               if (length > max_fl)
               {
                  max_fl = length;
               }
               no_of_patterns++;
            }
         }
         else if (*ptr == '\n')
              {
                 ptr++;
              }
      } while (*ptr != '\0');

      if (no_of_patterns > 0)
      {
         int i;

         RT_ARRAY(pl, no_of_patterns, max_pl, char);
         RT_ARRAY(fl, no_of_patterns, max_fl, char);
         if ((hl = malloc(no_of_patterns * sizeof(char))) == NULL)
         {
            (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            return(0);
         }

         ptr = file_buffer;
         i = 0;
         do
         {
            length = 0;
            while ((*(ptr + length) != '|') && (*(ptr + length) != '\n') &&
                   (*(ptr + length) != '\0'))
            {
               pl[i][length] = *(ptr + length);
               length++;
            }
            ptr += length;
            if (*ptr == '|')
            {
               ptr++;
               pl[i][length] = '\0';
               length = 0;
               while ((*(ptr + length) != '|') && (*(ptr + length) != '\n') &&
                      (*(ptr + length) != '\0'))
               {
                  fl[i][length] = *(ptr + length);
                  length++;
               }
               ptr += length;
               if ((*ptr == '|') || (*ptr == '\n'))
               {
                  if (*ptr == '|')
                  {
                     hl[i] = *(ptr + 1);
                     ptr += 2;
                     while ((*ptr != '\n') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                  }
                  else
                  {
                     hl[i] = '?';
                     ptr++;
                  }
                  fl[i][length] = '\0';
                  i++;
               }
            }
            else if (*ptr == '\n')
                 {
                    ptr++;
                 }
         } while (*ptr != '\0');
      }

      free(file_buffer);
      return(no_of_patterns);
   }

   return(0);
}
