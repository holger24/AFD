/*
 *  handle_info_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_info_file - checks if the info file changes and rereads it
 **                     if it does
 **   write_info_file - writes the information to the given information
 **                     file
 **
 ** SYNOPSIS
 **   int check_info_file(char *alias_name,
 **                       char *central_info_filename,
 **                       int  check_mtime)
 **   void write_info_file(Widget w,
 **                        char   *alias_name,
 **                        char   *central_info_filename)
 **
 ** DESCRIPTION
 **   The function check_info_file() reads the contents of the file
 **   info_file into the buffer host_info_data when the modification
 **   time has changed. If there is no change in time, the file is
 **   left untouched.
 **
 ** RETURN VALUES
 **   check_info_file() returns YES when no error has occurred and the
 **   info file has changed. Otherwise it will return NO.
 **   write_info_file() writes the data to the given info file.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.11.1996 H.Kiehl Created
 **   09.02.2005 H.Kiehl Added support for central host info file.
 **   20.09.2010 H.Kiehl Added function write_info_file().
 **   25.12.2019 H.Kiehl Handle separate info directory.
 **
 */
DESCR__E_M3

#include <stdio.h>      /* NULL                                          */
#include <string.h>     /* strerror()                                    */
#include <unistd.h>     /* read(), close(), eaccess()                    */
#include <sys/types.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>  /* mmap(), munmap()                              */
#endif
#include <sys/stat.h>
#include <stdlib.h>     /* free(), malloc()                              */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "motif_common_defs.h"

#include <Xm/Xm.h>
#include <Xm/Text.h>

#define IN_ETC_DIR 111

/* External global variables. */
extern char *info_data,
            *p_work_dir;

/* Local global variables. */
static int  data_from_central_info_file = NO;

/* Local function prototypes. */
static void fill_default_info(void);


/*########################### write_info_file() #########################*/
void
write_info_file(Widget w, char *alias_name, char *central_info_filename)
{
   int  data_changed = NO,
        fd;
   char info_file[MAX_PATH_LENGTH];

   (void)snprintf(info_file, MAX_PATH_LENGTH, "%s%s/%s", p_work_dir,
                  ETC_DIR, central_info_filename);
   if ((data_from_central_info_file == YES) ||
       ((data_from_central_info_file == NEITHER) &&
        (eaccess(info_file, R_OK | W_OK) == 0)))
   {
      if ((fd = lock_file(info_file, ON)) < 0)
      {
         (void)xrec(ERROR_DIALOG,
                    "Failed to lock_file() %s (%s %d)",
                     info_file, __FILE__, __LINE__);
      }
      else
      {
#ifdef HAVE_STATX
         struct statx stat_buf;
#else
         struct stat stat_buf;
#endif

#ifdef HAVE_STATX
         if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &stat_buf) == -1)
#else
         if (fstat(fd, &stat_buf) == -1)
#endif
         {
            (void)xrec(ERROR_DIALOG,
                       "Failed to access %s : %s (%s %d)",
                       info_file, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
#ifdef HAVE_STATX
            if (stat_buf.stx_size > 0)
#else
            if (stat_buf.st_size > 0)
#endif
            {
               size_t length;
               char   *buffer = NULL,
                      *p_end,
                      *p_start,
                      *search_str_end,
                      *search_str_start;

#ifdef HAVE_STATX
               if ((buffer = malloc(stat_buf.stx_size + 1)) == NULL)
#else
               if ((buffer = malloc(stat_buf.st_size + 1)) == NULL)
#endif
               {
                  (void)xrec(ERROR_DIALOG,
                             "Failed to allocate memory : %s (%s %d)",
                             strerror(errno), __FILE__, __LINE__);
                  (void)close(fd);
                  return;
               }
#ifdef HAVE_STATX
               if (readn(fd, buffer, stat_buf.stx_size,
                         DEFAULT_TRANSFER_TIMEOUT) != stat_buf.stx_size)
#else
               if (readn(fd, buffer, stat_buf.st_size,
                         DEFAULT_TRANSFER_TIMEOUT) != stat_buf.st_size)
#endif
               {
                  (void)xrec(ERROR_DIALOG,
                             "Failed to read file to memory : %s (%s %d)",
                             strerror(errno), __FILE__, __LINE__);
                  free(buffer);
                  (void)close(fd);
                  return;
               }
#ifdef HAVE_STATX
               buffer[stat_buf.stx_size] = '\0';
#else
               buffer[stat_buf.st_size] = '\0';
#endif

               length = strlen(alias_name);
               if ((search_str_end = malloc((3 + length + 2))) == NULL)
               {
                  (void)xrec(ERROR_DIALOG,
                             "Failed to allocate memory : %s (%s %d)",
                             strerror(errno), __FILE__, __LINE__);
                  free(buffer);
                  (void)close(fd);
                  return;
               }
               search_str_end[0] = '\n';
               search_str_end[1] = '<';
               search_str_end[2] = '/';
               (void)strcpy(&search_str_end[3], alias_name);
               search_str_end[3 + length] = '>';
               search_str_end[3 + length + 1] = '\0';
               if ((search_str_start = malloc((1 + length + 2))) == NULL)
               {
                  (void)xrec(ERROR_DIALOG,
                             "Failed to allocate memory : %s (%s %d)",
                             strerror(errno), __FILE__, __LINE__);
                  free(buffer);
                  free(search_str_end);
                  (void)close(fd);
                  return;
               }
               search_str_start[0] = '<';
               (void)strcpy(&search_str_start[1], &search_str_end[3]);
               if (((p_start = posi(buffer, search_str_start)) != NULL) &&
                   ((p_end = posi(p_start, search_str_end)) != NULL))
               {
                  free(info_data);
                  if ((info_data = XmTextGetString(w)))
                  {
                     size_t data_length;

                     if (lseek(fd, 0, SEEK_SET) < 0)
                     {
                        (void)xrec(ERROR_DIALOG,
                                   "Failed to lseek() %s to 0 : %s (%s %d)",
                                   info_file, strerror(errno),
                                   __FILE__, __LINE__);
                        exit(INCORRECT);
                     }
                     if (ftruncate(fd, 0) == -1)
                     {
                        (void)xrec(ERROR_DIALOG,
                                   "Failed to ftruncate() %s to 0 : %s (%s %d)",
                                   info_file, strerror(errno),
                                   __FILE__, __LINE__);
                     }
                     data_length = p_start - buffer;
                     if (writen(fd, buffer, data_length, 0) != data_length)
                     {
                        (void)xrec(ERROR_DIALOG,
                                   "Failed to writen() to %s : %s (%s %d)",
                                   info_file, strerror(errno),
                                   __FILE__, __LINE__);
                     }
                     data_length = strlen(info_data);
                     if (writen(fd, info_data, data_length, 0) != data_length)
                     {
                        (void)xrec(ERROR_DIALOG,
                                   "Failed to writen() to %s : %s (%s %d)",
                                   info_file, strerror(errno),
                                   __FILE__, __LINE__);
                     }
                     XtFree(info_data);
                     info_data = NULL;
                     data_length = strlen((p_end - (3 + length + 1)));
                     if (writen(fd, p_end - (3 + length + 1), data_length,
                                0) != data_length)
                     {
                        (void)xrec(ERROR_DIALOG,
                                   "Failed to writen() to %s : %s (%s %d)",
                                   info_file, strerror(errno),
                                   __FILE__, __LINE__);
                     }
                     data_changed = YES;
                  }
               }
               else if (p_start == NULL)
                    {
                       if ((info_data = XmTextGetString(w)))
                       {
                          size_t data_length;
                          char   tmp_buffer[2];

                          if (lseek(fd, -2, SEEK_END) < 0)
                          {
                             (void)xrec(ERROR_DIALOG,
                                        "Failed to lssek() to end of file %s : %s (%s %d)",
                                        info_file, strerror(errno),
                                        __FILE__, __LINE__);
                          }
                          if (read(fd, tmp_buffer, 2) != 2)
                          {
                              (void)xrec(ERROR_DIALOG,
                                         "Failed to read() one byte of file %s : %s (%s %d)",
                                         info_file, strerror(errno),
                                         __FILE__, __LINE__);
                          }
                          if (tmp_buffer[0] != '\n')
                          {
                             tmp_buffer[0] = '\n';
                             if (tmp_buffer[1] != '\n')
                             {
                                tmp_buffer[1] = '\n';
                                data_length = 2;
                             }
                             else
                             {
                                data_length = 1;
                             }
                             if (write(fd, tmp_buffer, data_length) != data_length)
                             {
                                (void)xrec(ERROR_DIALOG,
                                           "Failed to write() to %s : %s (%s %d)",
                                           info_file, strerror(errno),
                                           __FILE__, __LINE__);
                             }
                          }
                          else
                          {
                             if (tmp_buffer[1] != '\n')
                             {
                                tmp_buffer[1] = '\n';
                                if (write(fd, tmp_buffer, 2) != 2)
                                {
                                   (void)xrec(ERROR_DIALOG,
                                              "Failed to write() to %s : %s (%s %d)",
                                              info_file, strerror(errno),
                                              __FILE__, __LINE__);
                                }
                             }
                          }
                          search_str_start[1 + length + 1] = '\n';
                          if (write(fd, search_str_start, (1 + length + 2)) != (1 + length + 2))
                          {
                             (void)xrec(ERROR_DIALOG,
                                        "Failed to write() to %s : %s (%s %d)",
                                        info_file, strerror(errno),
                                        __FILE__, __LINE__);
                          }
                          data_length = strlen(info_data);
                          if (writen(fd, info_data, data_length, 0) != data_length)
                          {
                             (void)xrec(ERROR_DIALOG,
                                        "Failed to writen() to %s : %s (%s %d)\n",
                                        info_file, strerror(errno),
                                        __FILE__, __LINE__);
                          }
                          XtFree(info_data);
                          info_data = NULL;
                          search_str_end[3 + length + 1] = '\n';
                          if (write(fd, search_str_end, (3 + length + 2)) != (3 + length + 2))
                          {
                             (void)xrec(ERROR_DIALOG,
                                        "Failed to write() to %s : %s (%s %d)",
                                        info_file, strerror(errno),
                                        __FILE__, __LINE__);
                          }
                          data_changed = YES;
                       }
                    }
               free(buffer);
               free(search_str_end);
               free(search_str_start);
            }
         }
         if (close(fd) == -1)
         {
            (void)xrec(WARN_DIALOG, "Failed to close() %s : %s (%s %d)",
                       info_file, strerror(errno), __FILE__, __LINE__);
         }
      }
   }
   else
   {
      if (data_from_central_info_file == IN_ETC_DIR)
      {
         (void)snprintf(info_file, MAX_PATH_LENGTH, "%s%s/%s%s", p_work_dir,
                        ETC_DIR, INFO_IDENTIFIER, alias_name);
      }
      else
      {
         (void)snprintf(info_file, MAX_PATH_LENGTH, "%s%s%s/%s", p_work_dir,
                        ETC_DIR, INFO_DIR, alias_name);
      }
      if ((fd = open(info_file, O_WRONLY | O_CREAT | O_TRUNC,
                     FILE_MODE)) == -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to open() %s : %s (%s %d)",
                    info_file, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         free(info_data);
         if ((info_data = XmTextGetString(w)))
         {
            size_t data_length;

            data_length = strlen(info_data);
            if (writen(fd, info_data, data_length, 0) != data_length)
            {
               (void)xrec(ERROR_DIALOG,
                          "Failed to write() to %s : %s (%s %d)\n",
                          info_file, strerror(errno),
                          __FILE__, __LINE__);
            }
            XtFree(info_data);
            info_data = NULL;
            data_changed = YES;
         }
      }
      (void)close(fd);
   }

   if (data_changed == YES)
   {
      config_log(EC_HOST, ET_MAN, EA_CHANGE_INFO, alias_name, NULL);
   }

   return;
}


/*########################### check_info_file() #########################*/
int
check_info_file(char *alias_name, char *central_info_filename, int check_mtime)
{
   static int    first_time = YES;
   static time_t last_mtime_central = 0,
                 last_mtime_host = 0;
   int           file_changed = NO,
                 ret,
                 src_fd;
   char          alias_info_file[MAX_PATH_LENGTH],
                 central_info_file[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif

   (void)snprintf(central_info_file, MAX_PATH_LENGTH, "%s%s/%s", p_work_dir,
                  ETC_DIR, central_info_filename);
#ifdef HAVE_STATX
   if ((statx(0, central_info_file, AT_STATX_SYNC_AS_STAT,
              STATX_SIZE | STATX_MTIME, &stat_buf) == 0) &&
       (stat_buf.stx_size > 0))
#else
   if ((stat(central_info_file, &stat_buf) == 0) && (stat_buf.st_size > 0))
#endif
   {
#ifdef HAVE_STATX
      if ((check_mtime == NO) || (stat_buf.stx_mtime.tv_sec > last_mtime_central))
#else
      if ((check_mtime == NO) || (stat_buf.st_mtime > last_mtime_central))
#endif
      {
#ifdef HAVE_STATX
         last_mtime_central = stat_buf.stx_mtime.tv_sec;
#else
         last_mtime_central = stat_buf.st_mtime;
#endif

         if ((src_fd = open(central_info_file, O_RDONLY)) < 0)
         {
            (void)xrec(ERROR_DIALOG, "Failed to open() %s : %s (%s %d)",
                       central_info_file, strerror(errno), __FILE__, __LINE__);
         }
         else
         {
            char *ptr;

#ifdef HAVE_MMAP
            if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                            stat_buf.stx_size, PROT_READ,
# else
                            stat_buf.st_size, PROT_READ,
# endif
                            MAP_SHARED, src_fd, 0)) == (caddr_t) -1)
#else
            if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                stat_buf.stx_size, PROT_READ, MAP_SHARED,
# else
                                stat_buf.st_size, PROT_READ, MAP_SHARED,
# endif
                                central_info_file, 0)) == (caddr_t) -1)
#endif
            {
               (void)xrec(ERROR_DIALOG, "Failed to mmap() %s : %s (%s %d)",
                          central_info_file, strerror(errno),
                          __FILE__, __LINE__);
            }
            else
            {
               size_t length;
               char   *p_end,
                      *p_start,
                      *search_str_end,
                      *search_str_start;

               length = strlen(alias_name);
               if ((search_str_end = malloc((3 + length + 2))) == NULL)
               {
                  (void)xrec(ERROR_DIALOG,
                             "Failed to allocate memory : %s (%s %d)",
                             strerror(errno), __FILE__, __LINE__);
                  (void)close(src_fd);
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
                  if (munmap(ptr, stat_buf.stx_size) == -1)
# else
                  if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
                  if (munmap_emu(ptr) == -1)
#endif
                  {
                     (void)xrec(WARN_DIALOG, "Failed to munmap() from %s : %s",
                                central_info_file, strerror(errno));
                  }
                  return(NO);
               }
               search_str_end[0] = '\n';
               search_str_end[1] = '<';
               search_str_end[2] = '/';
               (void)strcpy(&search_str_end[3], alias_name);
               search_str_end[3 + length] = '>';
               search_str_end[3 + length + 1] = '\0';
               if ((search_str_start = malloc((1 + length + 2))) == NULL)
               {
                  (void)xrec(ERROR_DIALOG,
                             "Failed to allocate memory : %s (%s %d)",
                             strerror(errno), __FILE__, __LINE__);
                  free(search_str_end);
                  (void)close(src_fd);
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
                  if (munmap(ptr, stat_buf.stx_size) == -1)
# else
                  if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
                  if (munmap_emu(ptr) == -1)
#endif
                  {
                     (void)xrec(WARN_DIALOG, "Failed to munmap() from %s : %s",
                                central_info_file, strerror(errno));
                  }
                  return(NO);
               }
               search_str_start[0] = '<';
               (void)strcpy(&search_str_start[1], &search_str_end[3]);
               if (((p_start = posi(ptr, search_str_start)) != NULL) &&
                   ((p_end = posi(p_start, search_str_end)) != NULL))
               {
                  length = p_end - p_start - (length + 4);

                  free(info_data);
                  if ((info_data = malloc(length + 1)) == NULL)
                  {
                     (void)xrec(ERROR_DIALOG,
                                "Failed to allocate memory : %s (%s %d)",
                                strerror(errno), __FILE__, __LINE__);
                     ret = NO;
                  }
                  else
                  {
                     (void)memcpy(info_data, p_start, length);
                     info_data[length] = '\0';
                     first_time = YES;
                     data_from_central_info_file = YES;
                     ret = YES;
                  }
                  (void)close(src_fd);
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
                  if (munmap(ptr, stat_buf.stx_size) == -1)
# else
                  if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
                  if (munmap_emu(ptr) == -1)
#endif
                  {
                     (void)xrec(WARN_DIALOG, "Failed to munmap() from %s : %s",
                                central_info_file, strerror(errno));
                  }
                  free(search_str_end);
                  free(search_str_start);

                  return(ret);
               }
               free(search_str_end);
               free(search_str_start);
            }
         }
      }
      else
      {
         first_time = YES;
         return(NO);
      }
   }

   /*
    * No central Info file or alias not found in it. So lets
    * search for alias info file.
    */
   (void)snprintf(alias_info_file, MAX_PATH_LENGTH, "%s%s%s/%s", p_work_dir,
                  ETC_DIR, INFO_DIR, alias_name);
#ifdef HAVE_STATX
   if ((ret = statx(0, alias_info_file, AT_STATX_SYNC_AS_STAT,
                    STATX_SIZE | STATX_MTIME, &stat_buf)) == 0)
#else
   if ((ret = stat(alias_info_file, &stat_buf)) == 0)
#endif
   {
      data_from_central_info_file = NO;
   }
   else
   {
      (void)snprintf(alias_info_file, MAX_PATH_LENGTH, "%s%s/%s%s", p_work_dir,
                     ETC_DIR, INFO_IDENTIFIER, alias_name);
#ifdef HAVE_STATX
      if ((ret = statx(0, alias_info_file, AT_STATX_SYNC_AS_STAT,
                       STATX_SIZE | STATX_MTIME, &stat_buf)) == 0)
#else
      if ((ret = stat(alias_info_file, &stat_buf)) == 0)
#endif
      {
         data_from_central_info_file = IN_ETC_DIR;
      }
   }
#ifdef HAVE_STATX
   if ((ret == 0) && (stat_buf.stx_size > 0))
#else
   if ((ret == 0) && (stat_buf.st_size > 0))
#endif
   {
#ifdef HAVE_STATX
      if ((check_mtime == NO) || (stat_buf.stx_mtime.tv_sec > last_mtime_host))
#else
      if ((check_mtime == NO) || (stat_buf.st_mtime > last_mtime_host))
#endif
      {
#ifdef HAVE_STATX
         last_mtime_host = stat_buf.stx_mtime.tv_sec;
#else
         last_mtime_host = stat_buf.st_mtime;
#endif

         if ((src_fd = open(alias_info_file, O_RDONLY)) < 0)
         {
            if (first_time == YES)
            {
               fill_default_info();
               first_time = NO;
               file_changed = YES;
               if (errno == ENOENT)
               {
                  data_from_central_info_file = NEITHER;
               }
            }
            (void)xrec(ERROR_DIALOG, "Failed to open() %s : %s (%s %d)",
                       alias_info_file, strerror(errno), __FILE__, __LINE__);
            return(file_changed);
         }
         free(info_data);
#ifdef HAVE_STATX
         if ((info_data = malloc(stat_buf.stx_size + 1)) == NULL)
#else
         if ((info_data = malloc(stat_buf.st_size + 1)) == NULL)
#endif
         {
            (void)xrec(ERROR_DIALOG, "Failed to allocate memory : %s (%s %d)",
                       strerror(errno), __FILE__, __LINE__);
            (void)close(src_fd);
            return(NO);
         }

         /* Read file in one chunk. */
#ifdef HAVE_STATX
         if (read(src_fd, info_data, stat_buf.stx_size) != stat_buf.stx_size)
#else
         if (read(src_fd, info_data, stat_buf.st_size) != stat_buf.st_size)
#endif
         {
            (void)xrec(ERROR_DIALOG,
                       "read() error when reading from %s : %s (%s %d)",
                       alias_info_file, strerror(errno), __FILE__, __LINE__);
            (void)close(src_fd);
            return(NO);
         }
         (void)close(src_fd);
#ifdef HAVE_STATX
         info_data[stat_buf.stx_size] = '\0';
#else
         info_data[stat_buf.st_size] = '\0';
#endif

         first_time = YES;
         file_changed = YES;
      }
      else
      {
         first_time = YES;
         file_changed = NO;
      }
   }
   else
   {
      int tmp_errno = errno;

      if ((check_mtime == NO) || (first_time == YES))
      {
         fill_default_info();
         first_time = NO;
         file_changed = YES;
         if (tmp_errno == ENOENT)
         {
            data_from_central_info_file = NEITHER;
         }
      }
      if ((ret == -1) && (tmp_errno != ENOENT))
      {
         (void)xrec(WARN_DIALOG, "Failed to stat() %s : %s (%s %d)",
                    alias_info_file, strerror(tmp_errno), __FILE__, __LINE__);
      }
   }

   return(file_changed);
}


/*++++++++++++++++++++++++ fill_default_info() ++++++++++++++++++++++++++*/
static void
fill_default_info(void)
{
   /* Free previous memory chunk. */
   free(info_data);

   if ((info_data = malloc(384)) == NULL)
   {
      (void)xrec(ERROR_DIALOG, "Failed to allocate memory : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   (void)snprintf(info_data, 384, "\n\n\n\n\n                             %s\n",
                  NO_INFO_AVAILABLE);

   return;
}
