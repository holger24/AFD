/*
 *  handle_ls_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   attach_ls_data - attaches to ls data file
 **   detach_ls_data - detaches from ls data file
 **   reset_ls_data - resets all ls data values
 **
 ** SYNOPSIS
 **   int  attach_ls_data(struct fileretrieve_status *fra,
 **                       unsigned int               special_flag,
 **                       int                        create)
 **   void detach_ls_data(int remove)
 **   int  reset_ls_data(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.06.2009 H.Kiehl Created
 **   28.07.2014 H.Kiehl Added reset_ls_data().
 **   19.08.2014 H.Kiehl Added detach_ls_data().
 **   04.09.2014 H.Kiehl In stupid mode store names in a separate file.
 **   12.11.2014 H.Kiehl Undo above change and instead store data in
 **                      an malloc() area.
 **   18.03.2017 H.Kiehl Let user specify the name of the ls data file.
 **   06.01.2019 H.Kiehl Make this function set usable for users without
 **                      struct job.
 **                      Store last modification time of the directory.
 **                      Add parameter create ot attach_ls_data().
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>                /* atoi(), malloc(), free()           */
#include <string.h>                /* strcpy(), strerror()               */
#include <time.h>                  /* mktime()                           */ 
#include <sys/time.h>              /* struct tm                          */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                /* unlink()                           */
#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include "fddefs.h"


/* Local global variables. */
static char                 *list_file = NULL;
static size_t               list_file_length = 0;

/* External global variables. */
extern int                  *current_no_of_listed_files,
                            no_of_listed_files,
                            rl_fd;
extern char                 *p_work_dir;
extern off_t                rl_size;
extern struct retrieve_list *rl;


/*########################## attach_ls_data() ###########################*/
int
attach_ls_data(struct fileretrieve_status *fra,
               unsigned int               special_flag,
               int                        create)
{
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
   if ((fra->stupid_mode == YES) || (fra->remove == YES))
   {
      if (rl == NULL)
      {
         char *ptr;

         rl_size = (RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                   AFD_WORD_OFFSET;
         if ((ptr = malloc(rl_size)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "malloc() error : %s", strerror(errno));
            return(INCORRECT);
         }
         no_of_listed_files = *(int *)ptr = 0;
         current_no_of_listed_files = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         rl = (struct retrieve_list *)ptr;
      }
   }
   else
   {
#endif /* DO_NOT_PARALLELIZE_ALL_FETCH */
      if (rl_fd == -1)
      {
         time_t       *create_time;
         char         *ptr;
#ifdef HAVE_STATX
         struct statx stat_buf;
#else
         struct stat  stat_buf;
#endif

         if (rl != NULL)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Hmmm. Seems as if retrieve list pointer has still an assignment (fsa_pos=%d dir_alias=%s).",
                       fra->fsa_pos, fra->dir_alias);
         }
         if (list_file == NULL)
         {
            list_file_length = strlen(p_work_dir) + AFD_FILE_DIR_LENGTH +
                               INCOMING_DIR_LENGTH + LS_DATA_DIR_LENGTH + 1 +
                               MAX_DIR_ALIAS_LENGTH + 1 + MAX_INT_LENGTH + 1;
            if ((list_file = malloc(list_file_length)) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "malloc() error : %s", strerror(errno));
               return(INCORRECT);
            }
         }
         if (fra->ls_data_alias[0] == '\0')
         {
            ptr = fra->dir_alias;
         }
         else
         {
            ptr = fra->ls_data_alias;
         }
         (void)snprintf(list_file, list_file_length, "%s%s%s%s/%s",
                        p_work_dir, AFD_FILE_DIR, INCOMING_DIR, LS_DATA_DIR,
                        ptr);
         if ((rl_fd = open(list_file,
                           (create == YES) ? (O_RDWR | O_CREAT) : O_RDWR,
                           FILE_MODE)) == -1)
         {
            if ((errno != ENOENT) || (create == YES))
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to open() `%s' : %s",
                          list_file, strerror(errno));
            }
            return(INCORRECT);
         }
#ifdef HAVE_STATX
         if (statx(rl_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &stat_buf) == -1)
#else
         if (fstat(rl_fd, &stat_buf) == -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                       "Failed to statx() `%s' : %s",
#else
                       "Failed to fstat() `%s' : %s",
#endif
                       list_file, strerror(errno));
            return(INCORRECT);
         }
#ifdef HAVE_STATX
         if (stat_buf.stx_size == 0)
#else
         if (stat_buf.st_size == 0)
#endif
         {
            rl_size = (RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                      AFD_WORD_OFFSET;
            if (lseek(rl_fd, rl_size - 1, SEEK_SET) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to lseek() in `%s' : %s",
                          list_file, strerror(errno));
               return(INCORRECT);
            }
            if (write(rl_fd, "", 1) != 1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to write() to `%s' : %s",
                          list_file, strerror(errno));
               return(INCORRECT);
            }
         }
         else
         {
#ifdef HAVE_STATX
            rl_size = stat_buf.stx_size;
#else
            rl_size = stat_buf.st_size;
#endif
         }
#ifdef HAVE_MMAP
         if ((ptr = mmap(NULL, rl_size, (PROT_READ | PROT_WRITE),
                         MAP_SHARED, rl_fd, 0)) == (caddr_t) -1)
#else
         if ((ptr = mmap_emu(NULL, rl_size, (PROT_READ | PROT_WRITE),
                             MAP_SHARED, list_file, 0)) == (caddr_t) -1)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to mmap() to `%s' : %s",
                       list_file, strerror(errno));
            return(INCORRECT);
         }
         current_no_of_listed_files = (int *)ptr;
         no_of_listed_files = *(int *)ptr;
         create_time = (time_t *)(ptr + SIZEOF_INT + 4);
         ptr += AFD_WORD_OFFSET;
         rl = (struct retrieve_list *)ptr;
#ifdef HAVE_STATX
         if (stat_buf.stx_size == 0)
#else
         if (stat_buf.st_size == 0)
#endif
         {
            no_of_listed_files = *(int *)((char *)rl - AFD_WORD_OFFSET)  = 0;
            *(ptr - AFD_WORD_OFFSET + SIZEOF_INT) = 0;         /* Not used. */
            *(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 1) = 0;     /* Not used. */
            *(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 1 + 1) = 0; /* Not used. */
            *(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 1 + 1 + 1) = CURRENT_RL_VERSION;
            *create_time = time(NULL);
         }
         if (((special_flag & DISTRIBUTED_HELPER_JOB) == 0) &&
             ((special_flag & OLD_ERROR_JOB) == 0))
         {
            if (no_of_listed_files < 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Hmmm, no_of_listed_files = %d", no_of_listed_files);
               no_of_listed_files = *(int *)((char *)rl - AFD_WORD_OFFSET)  = 0;
            }
            else
            {
               int   i;
               off_t must_have_size;

               if (((rl_size - AFD_WORD_OFFSET) % sizeof(struct retrieve_list)) != 0)
               {
                  off_t old_calc_size,
                        old_int_calc_size;

                  old_calc_size = (((no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                                   RETRIEVE_LIST_STEP_SIZE * sizeof(struct old_retrieve_list)) +
                                  8;
                  old_int_calc_size = (((no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                                       RETRIEVE_LIST_STEP_SIZE * sizeof(struct old_int_retrieve_list)) +
                                      8;
                  if (rl_size == old_calc_size)
                  {
                     int                      new_rl_fd,
                                              *no_of_new_listed_files,
                                              no_of_old_listed_files;
                     time_t                   *new_create_time,
                                              old_create_time;
                     char                     *new_list_file,
                                              *new_ptr;
                     struct old_retrieve_list *orl;
                     struct retrieve_list     *nrl;
                     struct tm                bd_time;

                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Converting old retrieve list %s", list_file);
                     no_of_old_listed_files = no_of_listed_files;
                     old_create_time = *create_time;
                     ptr -= AFD_WORD_OFFSET;
                     ptr += 8;
                     orl = (struct old_retrieve_list *)ptr;

                     if ((new_list_file = malloc((list_file_length + 1))) == NULL)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "malloc() error : %s", strerror(errno));
                        return(INCORRECT);
                     }
                     (void)snprintf(new_list_file, list_file_length + 1,
                                    "%s%s%s%s/.%s",
                                    p_work_dir, AFD_FILE_DIR,
                                    INCOMING_DIR, LS_DATA_DIR,
                                    (fra->ls_data_alias[0] == '\0') ? fra->dir_alias : fra->ls_data_alias);
                     if ((new_rl_fd = open(new_list_file,
                                           O_RDWR | O_CREAT | O_TRUNC,
                                           FILE_MODE)) == -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to open() `%s' : %s",
                                   new_list_file, strerror(errno));
                        free(new_list_file);
                        return(INCORRECT);
                     }
                     rl_size = (((no_of_old_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                                RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                               AFD_WORD_OFFSET;
                     if (lseek(new_rl_fd, rl_size - 1, SEEK_SET) == -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to lseek() in `%s' : %s",
                                   new_list_file, strerror(errno));
                        (void)close(new_rl_fd);
                        free(new_list_file);
                        return(INCORRECT);
                     }
                     if (write(new_rl_fd, "", 1) != 1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to write() to `%s' : %s",
                                   new_list_file, strerror(errno));
                        (void)close(new_rl_fd);
                        free(new_list_file);
                        return(INCORRECT);
                     }
#ifdef HAVE_MMAP
                     if ((new_ptr = mmap(NULL, rl_size,
                                         (PROT_READ | PROT_WRITE), MAP_SHARED,
                                         new_rl_fd, 0)) == (caddr_t) -1)
#else
                     if ((new_ptr = mmap_emu(NULL, rl_size,
                                             (PROT_READ | PROT_WRITE), MAP_SHARED,
                                             new_list_file, 0)) == (caddr_t) -1)
#endif
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to mmap() to `%s' : %s",
                                   new_list_file, strerror(errno));
                        (void)close(new_rl_fd);
                        free(new_list_file);
                        return(INCORRECT);
                     }
                     no_of_new_listed_files = (int *)new_ptr;
                     *(new_ptr + SIZEOF_INT) = 0;          /* Not used. */
                     *(new_ptr + SIZEOF_INT + 1) = 0;      /* Not used. */
                     *(new_ptr + SIZEOF_INT + 1 + 1) = 0;  /* Not used. */
                     *(new_ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_RL_VERSION;
                     new_create_time = (time_t *)(new_ptr + SIZEOF_INT + 4);
                     new_ptr += AFD_WORD_OFFSET;
                     nrl = (struct retrieve_list *)new_ptr;
                     *no_of_new_listed_files = no_of_old_listed_files;
                     *new_create_time = old_create_time;

                     for (i = 0; i < no_of_old_listed_files; i++)
                     {
                        (void)strcpy(nrl[i].file_name, orl[i].file_name);
#ifdef _WITH_EXTRA_CHECK
                        nrl[i].extra_data[0] = '\0';
#endif
                        nrl[i].retrieved = orl[i].retrieved;
                        nrl[i].in_list = orl[i].in_list;
                        nrl[i].size = orl[i].size;
                        if (orl[i].date[0] == '\0')
                        {
                           nrl[i].file_mtime = -1;
                           nrl[i].got_date = NO;
                        }
                        else
                        {
                           bd_time.tm_sec  = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 2]);
                           orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 2] = '\0';
                           bd_time.tm_min  = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 4]);
                           orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 4] = '\0';
                           bd_time.tm_hour = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 6]);
                           orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 6] = '\0';
                           bd_time.tm_mday = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 8]);
                           orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 8] = '\0';
                           bd_time.tm_mon = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 10]) - 1;
                           orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 10] = '\0';
                           bd_time.tm_year = atoi(orl[i].date) - 1900;
                           bd_time.tm_isdst = 0;
                           nrl[i].file_mtime = mktime(&bd_time);
                           nrl[i].got_date = YES;
                        }
                     }
                     ptr -= 8;
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
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Failed to munmap() %s : %s",
                                   list_file, strerror(errno));
                     }
                     if (close(rl_fd) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Failed to close() %s : %s",
                                   list_file, strerror(errno));
                     }
                     if (unlink(list_file) == -1)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Failed to unlink() %s : %s",
                                   list_file, strerror(errno));
                     }
                     if (rename(new_list_file, list_file) == -1)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Failed to rename() %s to %s : %s",
                                   new_list_file, list_file, strerror(errno));
                     }
                     free(new_list_file);
                     rl_fd = new_rl_fd;
                     rl = nrl;
                     ptr = new_ptr;
                     no_of_listed_files = *(int *)((char *)rl - AFD_WORD_OFFSET) = *no_of_new_listed_files;
                     current_no_of_listed_files = (int *)((char *)rl - AFD_WORD_OFFSET);
                     create_time = new_create_time;
                  }
                  else if (rl_size == old_int_calc_size)
                       {
                          int                          new_rl_fd,
                                                       *no_of_new_listed_files,
                                                       no_of_old_listed_files;
                          time_t                       *new_create_time,
                                                       old_create_time;
                          char                         *new_list_file,
                                                       *new_ptr;
                          struct old_int_retrieve_list *orl;
                          struct retrieve_list         *nrl;
                          struct tm                    bd_time;

                          system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                     "Converting old retrieve list %s", list_file);
                          no_of_old_listed_files = no_of_listed_files;
                          old_create_time = *create_time;
                          ptr -= AFD_WORD_OFFSET;
                          ptr += 8;
                          orl = (struct old_int_retrieve_list *)ptr;

                          if ((new_list_file = malloc((list_file_length + 1))) == NULL)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "malloc() error : %s", strerror(errno));
                             return(INCORRECT);
                          }
                          (void)snprintf(new_list_file, list_file_length + 1,
                                         "%s%s%s%s/.%s",
                                         p_work_dir, AFD_FILE_DIR,
                                         INCOMING_DIR, LS_DATA_DIR,
                                         (fra->ls_data_alias[0] == '\0') ? fra->dir_alias : fra->ls_data_alias);
                          if ((new_rl_fd = open(new_list_file,
                                                O_RDWR | O_CREAT | O_TRUNC,
                                                FILE_MODE)) == -1)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to open() `%s' : %s",
                                        new_list_file, strerror(errno));
                             free(new_list_file);
                             return(INCORRECT);
                          }
                          rl_size = (((no_of_old_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                                     RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                                    AFD_WORD_OFFSET;
                          if (lseek(new_rl_fd, rl_size - 1, SEEK_SET) == -1)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to lseek() in `%s' : %s",
                                        new_list_file, strerror(errno));
                             (void)close(new_rl_fd);
                             free(new_list_file);
                             return(INCORRECT);
                          }
                          if (write(new_rl_fd, "", 1) != 1)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to write() to `%s' : %s",
                                        new_list_file, strerror(errno));
                             (void)close(new_rl_fd);
                             free(new_list_file);
                             return(INCORRECT);
                          }
#ifdef HAVE_MMAP
                          if ((new_ptr = mmap(NULL, rl_size,
                                              (PROT_READ | PROT_WRITE),
                                              MAP_SHARED, new_rl_fd,
                                              0)) == (caddr_t) -1)
#else
                          if ((new_ptr = mmap_emu(NULL, rl_size,
                                                  (PROT_READ | PROT_WRITE),
                                                  MAP_SHARED, new_list_file,
                                                  0)) == (caddr_t) -1)
#endif
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Failed to mmap() to `%s' : %s",
                                        new_list_file, strerror(errno));
                             (void)close(new_rl_fd);
                             free(new_list_file);
                             return(INCORRECT);
                          }
                          no_of_new_listed_files = (int *)new_ptr;
                          *(new_ptr + SIZEOF_INT) = 0;          /* Not used. */
                          *(new_ptr + SIZEOF_INT + 1) = 0;      /* Not used. */
                          *(new_ptr + SIZEOF_INT + 1 + 1) = 0;  /* Not used. */
                          *(new_ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_RL_VERSION;
                          new_create_time = (time_t *)(new_ptr + SIZEOF_INT + 4);
                          new_ptr += AFD_WORD_OFFSET;
                          nrl = (struct retrieve_list *)new_ptr;
                          *no_of_new_listed_files = no_of_old_listed_files;
                          *new_create_time = old_create_time;

                          for (i = 0; i < no_of_old_listed_files; i++)
                          {
                             (void)strcpy(nrl[i].file_name, orl[i].file_name);
#ifdef _WITH_EXTRA_CHECK
                             nrl[i].extra_data[0] = '\0';
#endif
                             nrl[i].retrieved = orl[i].retrieved;
                             nrl[i].in_list = orl[i].in_list;
                             nrl[i].size = orl[i].size;
                             if (orl[i].date[0] == '\0')
                             {
                                nrl[i].file_mtime = -1;
                                nrl[i].got_date = NO;
                             }
                             else
                             {
                                bd_time.tm_sec  = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 2]);
                                orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 2] = '\0';
                                bd_time.tm_min  = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 4]);
                                orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 4] = '\0';
                                bd_time.tm_hour = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 6]);
                                orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 6] = '\0';
                                bd_time.tm_mday = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 8]);
                                orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 8] = '\0';
                                bd_time.tm_mon = atoi(&orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 10]) - 1;
                                orl[i].date[(OLD_MAX_FTP_DATE_LENGTH - 1) - 10] = '\0';
                                bd_time.tm_year = atoi(orl[i].date) - 1900;
                                bd_time.tm_isdst = 0;
                                nrl[i].file_mtime = mktime(&bd_time);
                                nrl[i].got_date = YES;
                             }
                          }
                          ptr -= 8;
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
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Failed to munmap() %s : %s",
                                        list_file, strerror(errno));
                          }
                          if (close(rl_fd) == -1)
                          {
                             system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                        "Failed to close() %s : %s",
                                        list_file, strerror(errno));
                          }
                          if (unlink(list_file) == -1)
                          {
                             system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                        "Failed to unlink() %s : %s",
                                        list_file, strerror(errno));
                          }
                          if (rename(new_list_file, list_file) == -1)
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Failed to rename() %s to %s : %s",
                                        new_list_file, list_file,
                                        strerror(errno));
                          }
                          free(new_list_file);
                          rl_fd = new_rl_fd;
                          rl = nrl;
                          ptr = new_ptr;
                          no_of_listed_files = *(int *)((char *)rl - AFD_WORD_OFFSET) = *no_of_new_listed_files;
                          current_no_of_listed_files = (int *)((char *)rl - AFD_WORD_OFFSET);
                          create_time = new_create_time;
                       }
                       else
                       {
                          if (*(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 1 + 1 + 1) != CURRENT_RL_VERSION)
                          {
                             if ((ptr = convert_ls_data(rl_fd,
                                                        list_file,
                                                        &rl_size,
                                                        no_of_listed_files,
                                                        ptr,
                                                        *(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 1 + 1 + 1),
                                                        CURRENT_RL_VERSION)) == NULL)
                             {
                                system_log(ERROR_SIGN, __FILE__, __LINE__,
                                           "Failed to convert AFD ls data file %s.",
                                           list_file);
                                return(INCORRECT);
                             }
                             else
                             {
                                no_of_listed_files = *(int *)ptr;
                                current_no_of_listed_files = (int *)ptr;
                                create_time = (time_t *)(ptr + SIZEOF_INT + 4);
                                ptr += AFD_WORD_OFFSET;
                                rl = (struct retrieve_list *)ptr;
                             }
                          }
                          else
                          {
                             off_t calc_size;

                             calc_size = (((no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                                          RETRIEVE_LIST_STEP_SIZE *
                                          sizeof(struct retrieve_list)) +
                                         AFD_WORD_OFFSET;
                             system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                                        "Hmm, LS data file %s has incorrect size (%ld != %ld, %ld, %ld), removing it.",
#else
                                        "Hmm, LS data file %s has incorrect size (%lld != %lld, %lld, %lld), removing it.",
#endif
#ifdef HAVE_STATX
                                        list_file, (pri_off_t)stat_buf.stx_size,
#else
                                        list_file, (pri_off_t)stat_buf.st_size,
#endif
                                        (pri_off_t)calc_size,
                                        (pri_off_t)old_calc_size,
                                        (pri_off_t)old_int_calc_size);
                             ptr -= AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
                             if (munmap(ptr, rl_size) == -1)
#else
                             if (munmap_emu(ptr) == -1)
#endif
                             {
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           "Failed to munmap() %s : %s",
                                           list_file, strerror(errno));
                             }
                             if (close(rl_fd) == -1)
                             {
                                system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                           "Failed to close() %s : %s",
                                           list_file, strerror(errno));
                             }
                             if ((rl_fd = open(list_file,
                                               O_RDWR | O_CREAT | O_TRUNC,
                                               FILE_MODE)) == -1)
                             {
                                system_log(ERROR_SIGN, __FILE__, __LINE__,
                                           "Failed to open() `%s' : %s",
                                           list_file, strerror(errno));
                                return(INCORRECT);
                             }
                             rl_size = (RETRIEVE_LIST_STEP_SIZE *
                                        sizeof(struct retrieve_list)) +
                                       AFD_WORD_OFFSET;
                             if (lseek(rl_fd, rl_size - 1, SEEK_SET) == -1)
                             {
                                system_log(ERROR_SIGN, __FILE__, __LINE__,
                                           "Failed to lseek() in `%s' : %s",
                                           list_file, strerror(errno));
                                return(INCORRECT);
                             }
                             if (write(rl_fd, "", 1) != 1)
                             {
                                system_log(ERROR_SIGN, __FILE__, __LINE__,
                                           "Failed to write() to `%s' : %s",
                                           list_file, strerror(errno));
                                return(INCORRECT);
                             }
#ifdef HAVE_MMAP
                             if ((ptr = mmap(NULL, rl_size,
                                             (PROT_READ | PROT_WRITE), MAP_SHARED,
                                             rl_fd, 0)) == (caddr_t) -1)
#else
                             if ((ptr = mmap_emu(NULL, rl_size,
                                                 (PROT_READ | PROT_WRITE), MAP_SHARED,
                                                 list_file, 0)) == (caddr_t) -1)
#endif
                             {
                                system_log(ERROR_SIGN, __FILE__, __LINE__,
                                           "Failed to mmap() to `%s' : %s",
                                           list_file, strerror(errno));
                                return(INCORRECT);
                             }
                             no_of_listed_files = *(int *)ptr = 0;
                             current_no_of_listed_files = (int *)ptr;
                             *(ptr + SIZEOF_INT) = 0;         /* Not used. */
                             *(ptr + SIZEOF_INT + 1) = 0;     /* Not used. */
                             *(ptr + SIZEOF_INT + 1 + 1) = 0; /* Not used. */
                             *(ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_RL_VERSION;
                             create_time = (time_t *)(ptr + SIZEOF_INT + 4);
                             ptr += AFD_WORD_OFFSET;
                             rl = (struct retrieve_list *)ptr;
                             *create_time = time(NULL);
                          }
                       }
               }
               else
               {
                  if (*(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 1 + 1 + 1) != CURRENT_RL_VERSION)
                  {
                     if ((ptr = convert_ls_data(rl_fd, list_file, &rl_size,
                                                no_of_listed_files, ptr,
                                                *(ptr - AFD_WORD_OFFSET + SIZEOF_INT + 1 + 1 + 1),
                                                CURRENT_RL_VERSION)) == NULL)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to convert AFD ls data file %s.",
                                   list_file);
                        return(INCORRECT);
                     }
                     else
                     {
                        no_of_listed_files = *(int *)ptr;
                        current_no_of_listed_files = (int *)ptr;
                        create_time = (time_t *)(ptr + SIZEOF_INT + 4);
                        ptr += AFD_WORD_OFFSET;
                        rl = (struct retrieve_list *)ptr;
                     }
                  }
               }

               /*
                * Check if the file does have the correct step size. If
                * not resize it to the correct size.
                */
               must_have_size = (((no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                                 RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                                AFD_WORD_OFFSET;
               if (rl_size < must_have_size)
               {
                  ptr = (char *)rl - AFD_WORD_OFFSET;
                  if ((ptr = mmap_resize(rl_fd, ptr, must_have_size)) == (caddr_t) -1)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "mmap_resize() error : %s", strerror(errno));
                     return(INCORRECT);
                  }
                  rl_size = must_have_size;
                  no_of_listed_files = *(int *)ptr;
                  current_no_of_listed_files = (int *)ptr;
                  create_time = (time_t *)(ptr + SIZEOF_INT + 4);
                  ptr += AFD_WORD_OFFSET;
                  rl = (struct retrieve_list *)ptr;
                  if (no_of_listed_files < 0)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Hmmm, no_of_listed_files = %d",
                                no_of_listed_files);
                     no_of_listed_files = *(int *)((char *)rl - AFD_WORD_OFFSET) = 0;
                  }
               }

               for (i = 0; i < no_of_listed_files; i++)
               {
                  rl[i].in_list = NO;
               }
            }
         }
      }
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
   }
#endif

   return(SUCCESS);
}


/*########################## detach_ls_data() ###########################*/
void
detach_ls_data(int remove)
{
   if (rl_fd != -1)
   {
      if (rl != NULL)
      {
         char *ptr = (char *)rl - AFD_WORD_OFFSET;

#ifdef HAVE_MMAP
         if (msync(ptr, rl_size, MS_SYNC) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "msync() error : %s", strerror(errno));
         }
         if (munmap(ptr, rl_size) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "munmap() error : %s", strerror(errno));
         }
         else
         {
            rl = NULL;
            rl_size = 0;
         }
#else
         if (msync_emu(ptr) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "msync_emu() error : %s", strerror(errno));
         }
         if (munmap_emu(ptr) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "munmap_emu() error : %s", strerror(errno));
         }
         else
         {
            rl = NULL;
            rl_size = 0;
         }
#endif
      }
      if (close(rl_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
      else
      {
         rl_fd = -1;
      }
      current_no_of_listed_files = NULL;
      if ((remove == YES) && (list_file != NULL))
      {
         if (unlink(list_file) == -1)
         {
            if (errno != ENOENT)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("Failed to unlink() %s : %s"),
                          list_file, strerror(errno));
            }
         }
      }
   }
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
   else if (rl != NULL)
        {
           free((char *)rl - AFD_WORD_OFFSET);
           rl = NULL;
        }
#endif

   return;
}


/*########################### reset_ls_data() ###########################*/
int
reset_ls_data(void)
{
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
   if ((fra->stupid_mode == YES) || (fra->remove == YES))
   {
      off_t rl_size;
      char  *ptr;

      if (rl != NULL)
      {
         free((char *)rl - AFD_WORD_OFFSET);
      }
      rl_size = (RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                AFD_WORD_OFFSET;
      if ((ptr = malloc(rl_size)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
         return(INCORRECT);
      }
      no_of_listed_files = *(int *)ptr = 0;
      current_no_of_listed_files = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      rl = (struct retrieve_list *)ptr;
   }
   else
   {
#endif /* DO_NOT_PARALLELIZE_ALL_FETCH */
      if ((rl_fd != -1) && (no_of_listed_files > 0))
      {
         size_t new_size,
                old_size;

         new_size = (RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                    AFD_WORD_OFFSET;
         old_size = (((no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                     RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                    AFD_WORD_OFFSET;

         if (old_size != new_size)
         {
            char *ptr;

            ptr = (char *)rl - AFD_WORD_OFFSET;
            if ((ptr = mmap_resize(rl_fd, ptr, new_size)) == (caddr_t) -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "mmap_resize() error : %s", strerror(errno));
               return(INCORRECT);
            }
            rl_size = new_size;
            *(int *)ptr = 0;
            current_no_of_listed_files = (int *)ptr;
            ptr += AFD_WORD_OFFSET;
            rl = (struct retrieve_list *)ptr;
         }
         no_of_listed_files = 0;
      }
#ifdef DO_NOT_PARALLELIZE_ALL_FETCH
   }
#endif

   return(SUCCESS);
}
