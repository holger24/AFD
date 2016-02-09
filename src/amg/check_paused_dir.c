/*
 *  check_paused_dir.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2008 Deutscher Wetterdienst (DWD),
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
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* NULL                               */
#include <string.h>                /* strlen(), strcpy(), strerror()     */
#include <sys/types.h>
#include <sys/stat.h>              /* stat(), S_ISREG()                  */
#include <dirent.h>                /* opendir(), readdir(), closedir()   */
#include <unistd.h>
#include <errno.h>
#include "amgdefs.h"

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
         struct stat stat_buf;

         if (stat(p_de->paused_dir, &stat_buf) == 0)
         {
            if (S_ISDIR(stat_buf.st_mode))
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
            struct stat stat_buf;

            if (stat(db[p_de->fme[i].pos[j]].paused_dir, &stat_buf) == 0)
            {
               if (S_ISDIR(stat_buf.st_mode))
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
                 files_deleted = 0;
   off_t         file_size_deleted = 0;
   char          *ptr;
   struct dirent *dirp;
   DIR           *dp;
   struct stat   stat_buf;

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

   while ((dirp = readdir(dp)) != NULL)
   {
      if ((dirp->d_name[0] == '.') && ((dirp->d_name[1] == '\0') ||
          ((dirp->d_name[1] == '.') && (dirp->d_name[2] == '\0'))))
      {
         continue;
      }
      (void)strcpy(ptr, dirp->d_name);
      if (stat(dirname, &stat_buf) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to stat() `%s' : %s", dirname, strerror(errno));
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
            file_size_deleted += stat_buf.st_size;
         }
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
   if (rmdir(dirname) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to rmdir() <%s> : %s", dirname, strerror(errno));
      return(INCORRECT);
   }
   if (files_deleted > 0)
   {
      ABS_REDUCE_QUEUE(fra_pos, files_deleted, file_size_deleted);
   }

   return(SUCCESS);
}
