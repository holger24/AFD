/*
 *  view_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   view_data - calls a configured program to view data
 **
 ** SYNOPSIS
 **   void view_data(char *fullname, char *file_name)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.04.2007 H.Kiehl Created
 **   04.09.2013 A.Maul  Moved duplicate code to extra function
 **                      add_default_afd_hex_print().
 **   12.09.2013 A.Maul  The last filter is not terminated correctly.
 **                      This could lead to a SIGSEGV.
 **   29.04.2014 H.Kiehl When SUID bit is set we must release the
 **                      effective UID and set the real user ID, in case
 **                      we call some X program.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>            /* strerror()                             */
#include <stdlib.h>            /* realloc()                              */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>            /* seteuid()                              */
#include <errno.h>
#include "ui_common_defs.h"

/* External global variables. */
extern char                     font_name[],
                                *p_work_dir;

/* Local global variables. */
static int                      no_of_vdp;
static struct view_process_list *vdpl;

/* Local function prototypes. */
static void                     add_default_afd_hex_print(char *, char *);


/*############################# view_data() #############################*/
void
view_data(char *fullname, char *file_name)
{
   int  i,
        j;
   char afd_config_file[MAX_PATH_LENGTH];

   (void)snprintf(afd_config_file, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if (eaccess(afd_config_file, F_OK) == 0)
   {
      int          argcounter,
                   fd,
                   with_show_cmd;
      char         *buffer,
                   *ptr,
                   *p_start,
                   view_data_prog_def[VIEW_DATA_PROG_DEF_LENGTH + 2];
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat  stat_buf;
#endif

      if ((fd = open(afd_config_file, O_RDONLY)) == -1)
      {
         (void)fprintf(stderr,
                       "Failed to open() `%s' for reading : %s (%s %d)\n",
                       afd_config_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         (void)fprintf(stderr,
                       "Failed to access `%s' : %s (%s %d)\n",
                       afd_config_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

#ifdef HAVE_STATX
      if ((buffer = malloc(1 + stat_buf.stx_size + 1)) == NULL)
#else
      if ((buffer = malloc(1 + stat_buf.st_size + 1)) == NULL)
#endif
      {
         (void)fprintf(stderr,
#if SIZEOF_OFF_T == 4
                       "Failed to malloc() %ld bytes : %s (%s %d)\n",
#else
                       "Failed to malloc() %lld bytes : %s (%s %d)\n",
#endif
#ifdef HAVE_STATX
                       (pri_off_t)(1 + stat_buf.stx_size + 1),
#else
                       (pri_off_t)(1 + stat_buf.st_size + 1),
#endif
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
#ifdef HAVE_STATX
      if (read(fd, &buffer[1], stat_buf.stx_size) == -1)
#else
      if (read(fd, &buffer[1], stat_buf.st_size) == -1)
#endif
      {
         (void)fprintf(stderr,
#if SIZEOF_OFF_T == 4
                       "Failed to read() %ld bytes from `%s' : %s (%s %d)\n",
#else
                       "Failed to read() %lld bytes from `%s' : %s (%s %d)\n",
#endif
#ifdef HAVE_STATX
                       (pri_off_t)stat_buf.stx_size, afd_config_file,
#else
                       (pri_off_t)stat_buf.st_size, afd_config_file,
#endif
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      buffer[0] = '\n';
#ifdef HAVE_STATX
      buffer[stat_buf.stx_size + 1] = '\0';
#else
      buffer[stat_buf.st_size + 1] = '\0';
#endif

      (void)strcpy(&view_data_prog_def[1], VIEW_DATA_PROG_DEF);
      view_data_prog_def[0] = '\n';
      if (no_of_vdp > 0)
      {
         for (i = 0; i < no_of_vdp; i++)
         {
            free(vdpl[i].progname);
            vdpl[i].progname = NULL;
            FREE_RT_ARRAY(vdpl[i].filter);
            vdpl[i].filter = NULL;
            free(vdpl[i].args);
            vdpl[i].args = NULL;
         }
         free(vdpl);
      }
      no_of_vdp = 0;
      vdpl = NULL;
      ptr = buffer;
      do
      {
         if ((ptr = posi(ptr, view_data_prog_def)) != NULL)
         {
            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }

            /* Check if we wish to add show_cmd (--with-show_cmd). */
            if ((*ptr == '-') && (*(ptr + 1) == '-') &&
                (*(ptr + 2) == 'w') && (*(ptr + 3) == 'i') &&
                (*(ptr + 4) == 't') && (*(ptr + 5) == 'h') &&
                (*(ptr + 6) == '-') && (*(ptr + 7) == 's') &&
                (*(ptr + 8) == 'h') && (*(ptr + 9) == 'o') &&
                (*(ptr + 10) == 'w') && (*(ptr + 11) == '_') &&
                (*(ptr + 12) == 'c') && (*(ptr + 13) == 'm') &&
                (*(ptr + 14) == 'd') &&
                ((*(ptr + 15) == ' ') || (*(ptr + 15) == '\t')))
            {
               with_show_cmd = YES;
               ptr += 15;
               while ((*ptr == ' ') || (*ptr == '\t'))
               {
                  ptr++;
               }
            }
            else
            {
               with_show_cmd = NO;
            }
            p_start = ptr;
            argcounter = 0;
            if (*ptr == '"')
            {
               ptr++;
               p_start = ptr;
               while ((*ptr != '"') && (*ptr != '\n') && (*ptr != '\0') &&
                      (*ptr != '\r'))
               {
                  if ((*ptr == ' ') || (*ptr == '\t'))
                  {
                     argcounter++;
                  }
                  ptr++;
               }
               if (*ptr == '"')
               {
                  *ptr = ' ';
               }
            }
            else
            {
               while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
                      (*ptr != '\r') && (*ptr != '\0'))
               {
                  ptr++;
               }
            }
            if (ptr != p_start)
            {
               int length,
                   max_length;

               if ((vdpl = realloc(vdpl, ((no_of_vdp + 1) * sizeof(struct view_process_list)))) == NULL)
               {
                  (void)fprintf(stderr, "realloc() error : %s (%s %d)\n",
                                strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
               length = ptr - p_start;
               if (with_show_cmd == YES)
               {
                  int  extra_length;
                  char *p_arg,
                       *p_read,
                       *p_write,
                       tmp_char;

                  length += SHOW_CMD_LENGTH + 1 + WORK_DIR_ID_LENGTH + 1 +
                            strlen(p_work_dir) + 1 + 2 + 1 + 2 + 1 +
                            strlen(font_name) + 1 + 4 + MAX_PATH_LENGTH +
                            1 + strlen(file_name) + 1;
                  if ((vdpl[no_of_vdp].progname = malloc(length + 1)) == NULL)
                  {
                     (void)fprintf(stderr,
                                   "Failed to malloc() %d bytes : %s (%s %d)\n",
                                   length + 1, strerror(errno),
                                   __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  extra_length = snprintf(vdpl[no_of_vdp].progname, length + 1,
                                          "%s %s %s -b -f %s \"",
                                          SHOW_CMD, WORK_DIR_ID, p_work_dir,
                                          font_name);
                  if (extra_length > (length + 1))
                  {
                     (void)fprintf(stderr, "Buffer to small %d > %d (%s %d)\n",
                                   extra_length, length + 1,
                                   __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  p_write = vdpl[no_of_vdp].progname + extra_length;
                  extra_length = 0;
                  tmp_char = *ptr;
                  *ptr = '\0';
                  p_read = p_start;
                  length = strlen(fullname);
                  while (*p_read != '\0')
                  {
                     if ((*p_read == '%') && (*(p_read + 1) == 's') &&
                         ((extra_length + length) < MAX_PATH_LENGTH))
                     {
                        (void)strcpy(p_write, fullname);
                        p_read += 2;
                        p_write += length;
                        extra_length += length;
                     }
                     else
                     {
                        *p_write = *p_read;
                        p_write++; p_read++;
                     }
                  }
                  *ptr = tmp_char;

                  /* If no %s is found, add fullname to the end. */
                  if (extra_length == 0)
                  {
                     *p_write = ' ';
                     p_write++;
                     (void)strcpy(p_write, fullname);
                     p_write += length;
                  }

                  /* For show_cmd write as last parameter the file name. */
                  *p_write = ' ';
                  p_write++;
                  (void)strcpy(p_write, file_name);
                  p_write += strlen(file_name);
                  *p_write = '"';
                  p_write++;
                  *p_write = '\0';

                  if ((vdpl[no_of_vdp].args = malloc(8 * sizeof(char *))) == NULL)
                  {
                     (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                                   strerror(errno), __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  vdpl[no_of_vdp].args[0] = vdpl[no_of_vdp].progname;
                  p_arg = vdpl[no_of_vdp].progname;
                  p_arg += SHOW_CMD_LENGTH;
                  *p_arg = '\0';
                  p_arg++;
                  vdpl[no_of_vdp].args[1] = p_arg;
                  p_arg += WORK_DIR_ID_LENGTH;
                  *p_arg = '\0';
                  p_arg++;
                  vdpl[no_of_vdp].args[2] = p_arg;
                  while (*p_arg != ' ')
                  {
                     p_arg++;
                  }
                  *p_arg = '\0';
                  p_arg++;
                  vdpl[no_of_vdp].args[3] = p_arg;
                  p_arg += 2;
                  *p_arg = '\0';
                  p_arg++;
                  vdpl[no_of_vdp].args[4] = p_arg;
                  p_arg += 2;
                  *p_arg = '\0';
                  p_arg++;
                  vdpl[no_of_vdp].args[5] = p_arg;
                  while (*p_arg != ' ')
                  {
                     p_arg++;
                  }
                  *p_arg = '\0';
                  p_arg++;
                  vdpl[no_of_vdp].args[6] = p_arg;
                  vdpl[no_of_vdp].args[7] = NULL;
               }
               else
               {
                  if ((vdpl[no_of_vdp].progname = malloc(length + 1)) == NULL)
                  {
                     (void)fprintf(stderr,
                                   "Failed to malloc() %d bytes : %s (%s %d)\n",
                                   length + 1, strerror(errno),
                                   __FILE__, __LINE__);
                     exit(INCORRECT);
                  }
                  (void)strncpy(vdpl[no_of_vdp].progname, p_start, length);
                  vdpl[no_of_vdp].progname[length] = '\0';

                  /* Cut out the args. */
                  if (argcounter > 0)
                  {
                     int  filename_set = NO;
                     char *p_arg;

                     if ((vdpl[no_of_vdp].args = malloc((argcounter + 3) * sizeof(char *))) == NULL)
                     {
                        (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                                      strerror(errno), __FILE__, __LINE__);
                        exit(INCORRECT);
                     }
                     vdpl[no_of_vdp].args[0] = vdpl[no_of_vdp].progname;
                     p_arg = vdpl[no_of_vdp].progname;
                     for (i = 1; i < (argcounter + 1); i++)
                     {
                        while ((*p_arg != ' ') && (*p_arg != '\t'))
                        {
                           p_arg++;
                        }
                        *p_arg = '\0';
                        p_arg++;
                        vdpl[no_of_vdp].args[i] = p_arg;
                        if ((*p_arg == '%') && (*(p_arg + 1) == 's'))
                        {
                           filename_set = YES;
                           vdpl[no_of_vdp].args[i] = fullname;
                        }
                     }
                     while ((*p_arg != ' ') && (*p_arg != '\t') &&
                            (*p_arg != '\n') && (*p_arg != '\r') &&
                            (*p_arg != '\0'))
                     {
                        p_arg++;
                     }
                     if (*p_arg != '\0')
                     {
                        *p_arg = '\0';
                     }
                     if (filename_set == NO)
                     {
                        vdpl[no_of_vdp].args[i] = fullname;
                        i += 1;
                     }
                     vdpl[no_of_vdp].args[i] = NULL;
                  }
                  else
                  {
                     if ((vdpl[no_of_vdp].args = malloc(3 * sizeof(char *))) == NULL)
                     {
                        (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                                      strerror(errno), __FILE__, __LINE__);
                        exit(INCORRECT);
                     }
                     vdpl[no_of_vdp].args[0] = vdpl[no_of_vdp].progname;
                     vdpl[no_of_vdp].args[1] = fullname;
                     vdpl[no_of_vdp].args[2] = NULL;
                  }
               }

               /* Cut out filters. */
               while ((*ptr == ' ') || (*ptr == '\t'))
               {
                  ptr++;
               }
               p_start = ptr;
               length = 0;
               max_length = 0;
               vdpl[no_of_vdp].no_of_filters = 0;
               while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
               {
                  if (*ptr == '|')
                  {
                     vdpl[no_of_vdp].no_of_filters++;
                     if (length > max_length)
                     {
                        max_length = length;
                     }
                     length = 0;
                     ptr++;
                  }
                  else
                  {
                     ptr++; length++;
                  }
               }
               if (ptr != p_start)
               {
                  vdpl[no_of_vdp].no_of_filters++;
                  if (length > max_length)
                  {
                     max_length = length;
                  }
                  RT_ARRAY(vdpl[no_of_vdp].filter,
                           vdpl[no_of_vdp].no_of_filters,
                           max_length + 1, char);

                  ptr = p_start;
                  length = 0;
                  max_length = 0;
                  while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                  {
                     if (*ptr == '|')
                     {
                        vdpl[no_of_vdp].filter[max_length][length] = '\0';
                        length = 0;
                        max_length++;
                        ptr++;
                     }
                     else
                     {
                        vdpl[no_of_vdp].filter[max_length][length] = *ptr;
                        ptr++; length++;
                     }
                  }
                  vdpl[no_of_vdp].filter[max_length][length] = '\0';
               }
               no_of_vdp++;
            }
         }
      } while (ptr != NULL);
      free(buffer);

      /* Add as default program afd_hex_print. */
      add_default_afd_hex_print(fullname, file_name);

      if (close(fd) == -1)
      {
         (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
      }
   }

   if (no_of_vdp == 0)
   {
      add_default_afd_hex_print(fullname, file_name);
   }

   for (i = 0; i < no_of_vdp; i++)
   {
      for (j = 0; j < vdpl[i].no_of_filters; j++)
      {
         if (pmatch(vdpl[i].filter[j], file_name, NULL) == 0)
         {
            uid_t euid, /* Effective user ID. */
                  ruid; /* Real user ID. */

            if (i == (no_of_vdp - 1))
            {
               (void)sprintf(vdpl[i].args[6], "\"%s %s %s\"",
                             HEX_PRINT, fullname, file_name);
            }

            /*
             * SSH wants to look at .Xauthority and with setuid flag
             * set we cannot do that. So when we initialize X lets temporaly
             * disable it. After XtAppInitialize() we set it back.
             */
            euid = geteuid();
            ruid = getuid();
            if (euid != ruid)
            {
               if (seteuid(ruid) == -1)
               {
                  (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                                ruid, strerror(errno), __FILE__, __LINE__);
               }
            }

            make_xprocess(vdpl[i].progname, vdpl[i].progname, vdpl[i].args, -1);

            if (euid != ruid)
            {
               if (seteuid(euid) == -1)
               {
                  (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                                euid, strerror(errno), __FILE__, __LINE__);
               }
            }

            return;
         }
      }
   }

   return;
}


/*+++++++++++++++++++++ add_default_afd_hex_print() +++++++++++++++++++++*/
static void
add_default_afd_hex_print(char *fullname, char *file_name)
{
   char *p_arg;

   /* Add as default program afd_hex_print. */
   if ((vdpl = realloc(vdpl, ((no_of_vdp + 1) * sizeof(struct view_process_list)))) == NULL)
   {
      (void)fprintf(stderr, "realloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   vdpl[no_of_vdp].no_of_filters = 1;
   RT_ARRAY(vdpl[no_of_vdp].filter, 1, 2, char);
   vdpl[no_of_vdp].filter[0][0] = '*';
   vdpl[no_of_vdp].filter[0][1] = '\0';
   if ((vdpl[no_of_vdp].progname = malloc(MAX_PATH_LENGTH)) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   (void)snprintf(vdpl[no_of_vdp].progname, MAX_PATH_LENGTH,
                  "%s %s %s -b -f %s \"%s %s %s\"",
                  SHOW_CMD, WORK_DIR_ID, p_work_dir, font_name,
                  HEX_PRINT, fullname, file_name);

   if ((vdpl[no_of_vdp].args = malloc(8 * sizeof(char *))) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   vdpl[no_of_vdp].args[0] = vdpl[no_of_vdp].progname;
   p_arg = vdpl[no_of_vdp].progname;
   p_arg += SHOW_CMD_LENGTH;
   *p_arg = '\0';
   p_arg++;
   vdpl[no_of_vdp].args[1] = p_arg;
   p_arg += WORK_DIR_ID_LENGTH;
   *p_arg = '\0';
   p_arg++;
   vdpl[no_of_vdp].args[2] = p_arg;
   while (*p_arg != ' ')
   {
      p_arg++;
   }
   *p_arg = '\0';
   p_arg++;
   vdpl[no_of_vdp].args[3] = p_arg;
   p_arg += 2;
   *p_arg = '\0';
   p_arg++;
   vdpl[no_of_vdp].args[4] = p_arg;
   p_arg += 2;
   *p_arg = '\0';
   p_arg++;
   vdpl[no_of_vdp].args[5] = p_arg;
   while (*p_arg != ' ')
   {
      p_arg++;
   }
   *p_arg = '\0';
   p_arg++;
   vdpl[no_of_vdp].args[6] = p_arg;
   vdpl[no_of_vdp].args[7] = NULL;
   no_of_vdp++;

   return;
}
