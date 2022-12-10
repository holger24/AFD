/*
 *  check_old_time_jobs.c - Part of AFD, an automatic file distribution
 *                          program.
 *  Copyright (c) 1999 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_old_time_jobs - checks if there are any old time jobs
 **                         after a DIR_CONFIG update
 **
 ** SYNOPSIS
 **   void check_old_time_jobs(int no_of_jobs, char *time_dir)
 **
 ** DESCRIPTION
 **   The function check_old_time_jobs() searches the time directory
 **   for any old time jobs after a DIR_CONFIG update. If it finds any
 **   old job, it tries to locate that job in the current job list
 **   and move all the files to the new time job directory. If it
 **   fails to locate the job in the current or old job list, all
 **   files will be deleted and logged in the delete log.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.05.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>            /* rename()                                */
#include <string.h>           /* strerror(), strcpy(), strlen()          */
#include <stdlib.h>           /* strtoul()                               */
#include <ctype.h>            /* isxdigit()                              */
#include <sys/types.h>
#ifndef LINUX
# ifdef HAVE_STATX
#  include <fcntl.h>          /* Definition of AT_* constants            */
# endif
# include <sys/stat.h>        /* S_ISDIR()                               */
#endif
#include <dirent.h>           /* opendir(), readdir(), closedir()        */
#include <unistd.h>           /* rmdir(), unlink()                       */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                 *no_of_job_ids,
                           no_of_time_jobs,
                           *time_job_list;
extern struct instant_db   *db;
extern struct job_id_data  *jd;
extern struct dir_name_buf *dnb;

/* Local function prototype. */
static void move_time_dir(char *, unsigned int);

/* #define _STRONG_OPTION_CHECK 1 */


/*####################### check_old_time_jobs() #########################*/
void
check_old_time_jobs(int no_of_jobs, char *time_dir)
{
   DIR *dp;

#ifdef _MAINTAINER_LOG
   maintainer_log(DEBUG_SIGN, NULL, 0,
                  _("%s starting time dir check in %s . . ."),
                  DIR_CHECK, time_dir);
#endif

   /*
    * Search time pool directory and see if there are any old jobs
    * from the last DIR_CONFIG that still have to be processed.
    */
   if ((dp = opendir(time_dir)) == NULL)
   {
      system_log((errno == ENOENT) ? DEBUG_SIGN : WARN_SIGN, __FILE__, __LINE__,
                 _("Failed to opendir() `%s' : %s"), time_dir, strerror(errno));
   }
   else
   {
      int           i;
      char          *ptr,
                    str_number[MAX_INT_LENGTH],
                    *time_dir_ptr;
      struct dirent *p_dir;

      time_dir_ptr = time_dir + strlen(time_dir);
      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            continue;
         }

         ptr = p_dir->d_name;
         i = 0;
         while ((*ptr != '\0') && (i < MAX_INT_LENGTH))
         {
            if (isxdigit((int)(*ptr)) == 0)
            {
               break;
            }
            str_number[i] = *ptr;
            i++; ptr++;
         }
         if (*ptr == '\0')
         {
#ifndef LINUX
# ifdef HAVE_STATX
            struct statx stat_buf;
# else
            struct stat  stat_buf;
# endif
#endif

            (void)strcpy(time_dir_ptr, p_dir->d_name);
#ifdef LINUX
            if (p_dir->d_type == DT_DIR)
#else
# ifdef HAVE_STATX
            if (statx(0, time_dir, AT_STATX_SYNC_AS_STAT,
                      STATX_MODE, &stat_buf) == -1)
# else
            if (stat(time_dir, &stat_buf) == -1)
# endif
            {
               if (errno != ENOENT)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
# ifdef HAVE_STATX
                             _("Failed to statx() `%s' : %s"),
# else
                             _("Failed to stat() `%s' : %s"),
# endif
                             strerror(errno));
               }
            }
            else
            {
# ifdef HAVE_STATX
               if (S_ISDIR(stat_buf.stx_mode))
# else
               if (S_ISDIR(stat_buf.st_mode))
# endif
#endif
               {
                  int          gotcha = NO;
                  unsigned int job_id;

                  str_number[i] = '\0';
                  job_id = (unsigned int)strtoul(str_number, NULL, 16);

                  for (i = 0; i < no_of_time_jobs; i++)
                  {
                     if (db[time_job_list[i]].job_id == job_id)
                     {
                        gotcha = YES;
                        break;
                     }
                  }
                  if (gotcha == NO)
                  {
                     /*
                      * Befor we try to determine the new JID number lets
                      * try to delete the directory. If we are successful
                      * then there where no files and we can save us a
                      * complex search for the new one.
                      */
                     if ((rmdir(time_dir) == -1) &&
                         ((errno == ENOTEMPTY) || (errno == EEXIST)))
                     {
                        int jid_pos = -1;

                        /* Locate the lost job in structure job_id_data. */
                        for (i = 0; i < *no_of_job_ids; i++)
                        {
                           if (jd[i].job_id == job_id)
                           {
                              jid_pos = i;
                              break;
                           }
                        }
                        if (jid_pos == -1)
                        {
                           /*
                            * The job cannot be found in the JID structure!?
                            * The only thing we can do now is remove them.
                            */
#ifdef _DELETE_LOG
                           remove_time_dir("-", time_dir, YES, -1, -1,
                                           JID_LOOKUP_FAILURE_DEL,
                                           __FILE__, __LINE__);
#else
                           remove_time_dir("-", time_dir, YES, -1);
#endif
                        }
                        else
                        {
                           unsigned int new_job_id;
                           int          new_job_gotcha = NO;

                           /*
                            * Hmmm. Now comes the difficult part! Try find
                            * a current job in the JID structure that resembles
                            * the lost job. Question is how close should the
                            * new job resemble the old one? If we resemble it
                            * to close we loose more data, since we do not find
                            * a job matching. On the other hand the less we
                            * care how well the job resembles, the higher will
                            * be the chance that we catch the wrong job in the
                            * JID structure. The big problem here are the
                            * options. Well, some options we do not need to
                            * worry about: priority, time and lock. If these
                            * options cause difficulties, then the DIR_CONFIG
                            * configuration is broke!
                            */
                           for (i = 0; i < no_of_jobs; i++)
                           {
                              if ((db[i].job_id != job_id) &&
                                  (dnb[jd[jid_pos].dir_id_pos].dir_id == db[i].dir_id) &&
                                  (jd[jid_pos].file_mask_id == db[i].file_mask_id) &&
                                  (CHECK_STRCMP(jd[jid_pos].recipient, db[i].recipient) == 0))
                              {
#ifdef _STRONG_OPTION_CHECK
                                 if (jd[jid_pos].no_of_loptions == db[i].no_of_loptions)
                                 {
                                    char *p_loptions_db = db[i].loptions,
                                         *p_loptions_jd = jd[jid_pos].loptions;

                                    for (j = 0; j < jd[jid_pos].no_of_loptions; j++)
                                    {
                                       if ((CHECK_STRCMP(p_loptions_jd, p_loptions_db) != 0) &&
                                           (strncmp(p_loptions_jd, TIME_ID, TIME_ID_LENGTH) != 0))
                                       {
                                          gotcha = YES;
                                          break;
                                       }
                                       NEXT(p_loptions_db);
                                       NEXT(p_loptions_jd);
                                    }
                                    if (gotcha == NO)
                                    {
                                       if (jd[jid_pos].no_of_soptions == db[i].no_of_soptions)
                                       {
                                          if (jd[jid_pos].no_of_soptions > 0)
                                          {
                                             if (CHECK_STRCMP(jd[jid_pos].soptions, db[i].soptions) != 0)
                                             {
                                                gotcha = YES;
                                             }
                                          }
                                          if (gotcha == NO)
                                          {
                                             new_job_id = db[i].job_id;
                                             new_job_gotcha = YES;
                                             break;
                                          }
                                       }
                                    }
                                 }
#else
                                 new_job_id = db[i].job_id;
                                 new_job_gotcha = YES;
                                 break;
#endif
                              }
                           } /* for (i = 0; i < no_of_jobs; i++) */

                           if (new_job_gotcha == NO)
                           {
                              /*
                               * Since we were not able to locate a similar
                               * job lets assume it has been taken from the
                               * DIR_CONFIG. In that case we have to remove
                               * all files.
                               */
#ifdef _DELETE_LOG
                             remove_time_dir(jd[jid_pos].host_alias, time_dir,
                                             YES, jd[jid_pos].job_id,
                                             jd[jid_pos].dir_id,
                                             JID_LOOKUP_FAILURE_DEL,
                                             __FILE__, __LINE__);
#else
                             remove_time_dir(jd[jid_pos].host_alias, time_dir,
                                             YES, jd[jid_pos].job_id);
#endif
                           }
                           else
                           {
                              /*
                               * Lets move all files from the old job directory
                               * to the new one.
                               */
                              move_time_dir(time_dir, new_job_id);
                           }
                        }
                     } /* if ((rmdir(time_dir) == -1) && (errno == ENOTEMPTY)) */
                  } /* if (gotcha == NO) */
               } /* if (S_ISDIR(stat_buf.st_mode)) */
#ifndef LINUX
            } /* stat() successful */
#endif
         } /* if (*ptr == '\0') */
         errno = 0;
      } /* while ((p_dir = readdir(dp)) != NULL) */

      *time_dir_ptr = '\0';
      if (errno)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to readdir() `%s' : %s"),
                    time_dir, strerror(errno));
      }
      if (closedir(dp) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to closedir() `%s' : %s"),
                    time_dir, strerror(errno));
      }
   }
#ifdef _MAINTAINER_LOG
   maintainer_log(DEBUG_SIGN, NULL, 0,
                  _("%s time dir check done."), DIR_CHECK);
#endif

   return;
}


/*+++++++++++++++++++++++++++ move_time_dir() +++++++++++++++++++++++++++*/
static void
move_time_dir(char *time_dir, unsigned int job_id)
{
#ifdef _CHECK_TIME_DIR_DEBUG
   system_log(INFO_SIGN, __FILE__, __LINE__,
              _("Moving time directory `%s' to %x"), time_dir, job_id);
#else
   DIR *dp;

#ifdef _MAINTAINER_LOG
   maintainer_log(INFO_SIGN, __FILE__, __LINE__,
                  _("Moving time directory `%s' to %x"), time_dir, job_id);
#endif
   if ((dp = opendir(time_dir)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to opendir() `%s' to move old time jobs : %s"),
                 time_dir, strerror(errno));
   }
   else
   {
      char          *ptr,
                    *tmp_ptr,
                    to_dir[MAX_PATH_LENGTH],
                    *to_dir_ptr;
      struct dirent *p_dir;

      ptr = time_dir + strlen(time_dir);
      *(ptr++) = '/';
      tmp_ptr = ptr - 2;
      while ((*tmp_ptr != '/') && (tmp_ptr > time_dir))
      {
         tmp_ptr--;
      }
      if (*tmp_ptr == '/')
      {
         char tmp_char;

         tmp_ptr++;
         tmp_char = *tmp_ptr;
         *tmp_ptr = '\0';
         to_dir_ptr = to_dir + snprintf(to_dir, MAX_PATH_LENGTH,
                                        "%s%x", time_dir, job_id);
         *tmp_ptr = tmp_char;
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Hmmm.. , something is wrong here!?"));
         *(ptr - 1) = '\0';
         if (closedir(dp) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Could not closedir() `%s' : %s"),
                       time_dir, strerror(errno));
         }
         return;
      }
      if (mkdir(to_dir, DIR_MODE) == -1)
      {
         if (errno != EEXIST)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Could not mkdir() `%s' to move old time job : %s"),
                       to_dir, strerror(errno));
            *(ptr - 1) = '\0';
            if (closedir(dp) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to closedir() `%s' : %s"),
                          time_dir, strerror(errno));
            }
            return;
         }
      }
      *(to_dir_ptr++) = '/';

      errno = 0;
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            continue;
         }
         (void)strcpy(ptr, p_dir->d_name);
         (void)strcpy(to_dir_ptr, p_dir->d_name);

         if (rename(time_dir, to_dir) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Failed to rename() `%s' to `%s' : %s"),
                       time_dir, to_dir, strerror(errno));
            if (unlink(time_dir) == -1)
            {
               if (errno != ENOENT)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             _("Failed to unlink() `%s' : %s"),
                             time_dir, strerror(errno));
               }
            }
         }
         errno = 0;
      }

      *(ptr - 1) = '\0';
      if (errno)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not readdir() `%s' : %s"),
                    time_dir, strerror(errno));
      }
      if (closedir(dp) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not closedir() `%s' : %s"),
                    time_dir, strerror(errno));
      }
      if (rmdir(time_dir) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not rmdir() `%s' [to_dir = `%s'] : %s"),
                    time_dir, to_dir, strerror(errno));
      }
   }
#endif

   return;
}
