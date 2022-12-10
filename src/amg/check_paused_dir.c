/*
 *  check_paused_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2022 Deutscher Wetterdienst (DWD),
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
 **   check_paused_dir - checks if there is a directory with files
 **                      for a specific host
 **
 ** SYNOPSIS
 **   char *check_paused_dir(struct directory_entry *p_de,
 **                          int                    *nfg,
 **                          int                    *dest_count,
 **                          int                    *pdf)
 **
 ** DESCRIPTION
 **   This function checks the user directory for any paused
 **   directories. A paused directory consists of the hostname
 **   starting with a dot.
 **
 ** RETURN VALUES
 **   The first hostname it finds in this directory. If no host is
 **   found NULL is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.03.1996 H.Kiehl Created
 **   04.01.2001 H.Kiehl Don't check duplicate directories.
 **   29.05.2001 H.Kiehl Delete queue when host is disabled.
 **   05.02.2004 H.Kiehl Also check for dummy remote paused directories.
 **   09.04.2005 H.Kiehl Added function remove_paused_dir() to
 **                      reduce files_queued and bytes_in_queue in FRA.
 **   31.07.2018 H.Kiehl Added a timeout for deleting files in
 **                      remove_paused_dir(). If the directory is full
 **                      it can take a long time for the function before
 **                      it returns.
 **   03.08.2018 H.Kiehl Lets not always call time() for each file deleted.
 **                      Most the time deleting files is very fast.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* NULL                               */
#include <string.h>                /* strlen(), strcpy(), strerror()     */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>                /* Definition of AT_* constants       */
#endif
#include <sys/stat.h>              /* stat(), S_ISREG()                  */
#include <dirent.h>                /* opendir(), readdir(), closedir()   */
#include <unistd.h>
#include <time.h>                  /* time()                             */
#include <errno.h>
#include "amgdefs.h"

#define REMOVE_PAUSED_DIR_TIMEOUT 30

/* External global variables. */
#ifdef WITH_ERROR_QUEUE
extern int                        fsa_fd;
#endif
extern int                        fra_fd;
extern struct instant_db          *db;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;

/* Local function prototypes. */
static int                        remove_paused_dir(char *, int);


/*######################### check_paused_dir() ##########################*/
char *
check_paused_dir(struct directory_entry *p_de,
                 int                    *nfg,
                 int                    *dest_count,
                 int                    *pdf)
{
   register int i, j;

   /*
    * First check if this is a dummy remote dir. This needs to be handled
    * differently.
    */
   if (fra[p_de->fra_pos].fsa_pos != -1)
   {
      if ((fsa[fra[p_de->fra_pos].fsa_pos].host_status & PAUSE_QUEUE_STAT) == 0)
      {
#ifdef HAVE_STATX
         struct statx stat_buf;
#else
         struct stat  stat_buf;
#endif

#ifdef HAVE_STATX
         if (statx(0, p_de->paused_dir, AT_STATX_SYNC_AS_STAT,
                   STATX_MODE, &stat_buf) == 0)
#else
         if (stat(p_de->paused_dir, &stat_buf) == 0)
#endif
         {
#ifdef HAVE_STATX
            if (S_ISDIR(stat_buf.stx_mode))
#else
            if (S_ISDIR(stat_buf.st_mode))
#endif
            {
               if (fsa[fra[p_de->fra_pos].fsa_pos].special_flag & HOST_DISABLED)
               {
                  if (remove_paused_dir(p_de->paused_dir,
                                        p_de->fra_pos) < 0)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Failed to remove %s", p_de->paused_dir);
                  }
                  return(NULL);
               }
               else
               {
                  return(fsa[fra[p_de->fra_pos].fsa_pos].host_alias);
               }
            }
         }
      }
      else
      {
         if (pdf != NULL)
         {
            *pdf = YES;
         }
      }
   }

   for (i = *nfg; i < p_de->nfg; i++)
   {
      for (j = *dest_count; j < p_de->fme[i].dest_count; j++)
      {
         /* Is queue stopped? (ie PAUSE_QUEUE_STAT, AUTO_PAUSE_QUEUE_STAT */
         /* or DANGER_PAUSE_QUEUE_STAT is set)                            */
         if ((db[p_de->fme[i].pos[j]].dup_paused_dir == NO) &&
             ((((fsa[db[p_de->fme[i].pos[j]].position].host_status & PAUSE_QUEUE_STAT) == 0) &&
               ((fsa[db[p_de->fme[i].pos[j]].position].host_status & AUTO_PAUSE_QUEUE_STAT) == 0) &&
#ifdef WITH_ERROR_QUEUE
               ((fsa[db[p_de->fme[i].pos[j]].position].host_status & ERROR_QUEUE_SET) == 0) &&
#endif
               ((fsa[db[p_de->fme[i].pos[j]].position].host_status & DANGER_PAUSE_QUEUE_STAT) == 0)) ||
              ((fsa[db[p_de->fme[i].pos[j]].position].special_flag & HOST_DISABLED) &&
               ((fsa[db[p_de->fme[i].pos[j]].position].host_status & PAUSE_QUEUE_STAT) ||
#ifdef WITH_ERROR_QUEUE
                (fsa[db[p_de->fme[i].pos[j]].position].host_status & ERROR_QUEUE_SET) ||
#endif
                (fsa[db[p_de->fme[i].pos[j]].position].host_status & AUTO_PAUSE_QUEUE_STAT) ||
                (fsa[db[p_de->fme[i].pos[j]].position].host_status & DANGER_PAUSE_QUEUE_STAT)))))
         {
#ifdef HAVE_STATX
            struct statx stat_buf;
#else
            struct stat  stat_buf;
#endif

#ifdef HAVE_STATX
            if (statx(0, db[p_de->fme[i].pos[j]].paused_dir,
                      AT_STATX_SYNC_AS_STAT, STATX_MODE, &stat_buf) == 0)
#else
            if (stat(db[p_de->fme[i].pos[j]].paused_dir, &stat_buf) == 0)
#endif
            {
#ifdef HAVE_STATX
               if (S_ISDIR(stat_buf.stx_mode))
#else
               if (S_ISDIR(stat_buf.st_mode))
#endif
               {
                  if (fsa[db[p_de->fme[i].pos[j]].position].special_flag & HOST_DISABLED)
                  {
#ifdef WITH_ERROR_QUEUE
                     if (fsa[db[p_de->fme[i].pos[j]].position].host_status & ERROR_QUEUE_SET)
                     {
                        (void)remove_from_error_queue(db[p_de->fme[i].pos[j]].job_id,
                                                      &fsa[db[p_de->fme[i].pos[j]].position],
                                                      db[p_de->fme[i].pos[j]].position,
                                                      fsa_fd);
                     }
#endif
                     if (remove_paused_dir(db[p_de->fme[i].pos[j]].paused_dir,
                                           p_de->fra_pos) < 0)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Failed to remove %s",
                                   db[p_de->fme[i].pos[j]].paused_dir);
                     }
                     return(NULL);
                  }
                  else
                  {
                     *nfg = i;
                     *dest_count = j + 1;
                     return(db[p_de->fme[i].pos[j]].host_alias);
                  }
               }
            }
         }
         else
         {
            if ((pdf != NULL) &&
                ((fsa[db[p_de->fme[i].pos[j]].position].host_status & PAUSE_QUEUE_STAT) ||
                 (fsa[db[p_de->fme[i].pos[j]].position].host_status & AUTO_PAUSE_QUEUE_STAT) ||
#ifdef WITH_ERROR_QUEUE
                 (fsa[db[p_de->fme[i].pos[j]].position].host_status & ERROR_QUEUE_SET) ||
#endif
                 (fsa[db[p_de->fme[i].pos[j]].position].host_status & DANGER_PAUSE_QUEUE_STAT)))
            {
               *pdf = YES;
            }
         }
      }
      *dest_count = 0;
   }

   return(NULL);
}


/*+++++++++++++++++++++++++ remove_paused_dir() +++++++++++++++++++++++++*/
static int
remove_paused_dir(char *dirname, int fra_pos)
{
   int           addchar = NO,
                 counter = 0,
                 files_deleted = 0,
                 timed_out = NO;
   off_t         file_size_deleted = 0;
   time_t        start_time;
   char          *ptr;
   struct dirent *dirp;
   DIR           *dp;
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif

   ptr = dirname + strlen(dirname);

   if ((dp = opendir(dirname)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to opendir() <%s> : %s", dirname, strerror(errno));
      return(INCORRECT);
   }
   if (*(ptr - 1) != '/')
   {
      *(ptr++) = '/';
      addchar = YES;
   }

   start_time = time(NULL);

   while ((dirp = readdir(dp)) != NULL)
   {
      if ((dirp->d_name[0] == '.') && ((dirp->d_name[1] == '\0') ||
          ((dirp->d_name[1] == '.') && (dirp->d_name[2] == '\0'))))
      {
         continue;
      }
      (void)strcpy(ptr, dirp->d_name);
#ifdef HAVE_STATX
      if (statx(0, dirname, AT_STATX_SYNC_AS_STAT, STATX_SIZE, &stat_buf) == -1)
#else
      if (stat(dirname, &stat_buf) == -1)
#endif
      {
         if (errno != ENOENT)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                       "Failed to statx() `%s' : %s",
#else
                       "Failed to stat() `%s' : %s",
#endif
                       dirname, strerror(errno));
         }
      }
      else
      {
         if (unlink(dirname) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Failed to unlink() `%s' : %s",
                       dirname, strerror(errno));
         }
         else
         {
            files_deleted++;
#ifdef HAVE_STATX
            file_size_deleted += stat_buf.stx_size;
#else
            file_size_deleted += stat_buf.st_size;
#endif
         }
      }

      if (counter > 20)
      {
         counter = 0;
         if ((time(NULL) - start_time) > REMOVE_PAUSED_DIR_TIMEOUT)
         {
            timed_out = YES;
            break;
         }
      }
      else
      {
         counter++;
      }
   }
   if (addchar == YES)
   {
      ptr[-1] = 0;
   }
   else
   {
      *ptr = '\0';
   }
   if (closedir(dp) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to closedir() <%s> : %s", dirname, strerror(errno));
      return(INCORRECT);
   }
   if (timed_out == NO)
   {
      if (rmdir(dirname) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to rmdir() <%s> : %s", dirname, strerror(errno));
      }
      else
      {
#ifdef LOCK_DEBUG
         lock_region_w(fra_fd,
                       (char *)&fra[fra_pos].files_queued - (char *)fra,
                       __FILE__, __LINE__);
#else
         lock_region_w(fra_fd,
                       (char *)&fra[fra_pos].files_queued - (char *)fra);
#endif
         fra[fra_pos].files_queued = 0;
         fra[fra_pos].bytes_in_queue = 0;
#ifdef LOCK_DEBUG
         unlock_region(fra_fd,
                       (char *)&fra[fra_pos].files_queued - (char *)fra,
                       __FILE__, __LINE__);
#else
         unlock_region(fra_fd,
                       (char *)&fra[fra_pos].files_queued - (char *)fra);
#endif
         files_deleted = 0;
      }
   }
   else
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Unable to delete all files in %s due to timeout %d (REMOVE_PAUSED_DIR_TIMEOUT) seconds.",
                 dirname, REMOVE_PAUSED_DIR_TIMEOUT);
   }
   if (files_deleted > 0)
   {
      ABS_REDUCE_QUEUE(fra_pos, files_deleted, file_size_deleted);
   }

   return(SUCCESS);
}
