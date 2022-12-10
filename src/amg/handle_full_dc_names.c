/*
 *  handle_full_dc_names.c - Part of AFD, an automatic file distribution
 *                           program.
 *  Copyright (c) 2014 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_full_dc_names - get all DIR_CONFIG names from the given filter
 **   check_full_dc_name_changes - check if this filter has any changes
 **   purge_full_dc_names - remove a 'filter'-DIR_CONFIG
 **
 ** SYNOPSIS
 **   void get_full_dc_names(char *dc_filter, off_t *db_size)
 **   int  check_full_dc_name_changes(char *dc_filter)
 **   int  purge_full_dc_name_changes(void)
 **
 ** DESCRIPTION
 **   The function get_full_dc_names() searches the directory 'dc_filter'
 **   for DIR_CONFIG's. If it finds any, these will be stored in the
 **   structure dc_filter_list dcfl.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.08.2014 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* fprintf(), sprintf()               */
#include <string.h>                /* strcpy(), strerror()               */
#include <stdlib.h>                /* exit()                             */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>                /* Definition of AT_* constants       */
#endif
#include <sys/stat.h>              /* stat(), S_ISREG()                  */
#include <dirent.h>                /* opendir(), closedir(), readdir(),  */
                                   /* DIR, struct dirent                 */
#include <errno.h>
#include "amgdefs.h"


/* External global variables. */
extern int                   no_of_dc_filters,
                             no_of_dir_configs;
extern struct dir_config_buf *dc_dcl;
extern struct dc_filter_list *dcfl;

/* Local function prototypes. */
static int                   check_full_dc_name(char *),
                             purge_full_dc_names(void);


/*######################## get_full_dc_names() ##########################*/
void
get_full_dc_names(char *dc_filter, off_t *db_size)
{
   char fullname[MAX_PATH_LENGTH],
        *work_ptr;

   (void)strcpy(fullname, dc_filter);
   work_ptr = fullname + strlen(fullname);
   while ((*work_ptr != '/') && (work_ptr != fullname))
   {
      work_ptr--;
   }
   if (*work_ptr == '/')
   {
      char          filter[MAX_FILENAME_LENGTH + 1];
#ifdef HAVE_STATX
      struct statx  stat_buf;
#else
      struct stat   stat_buf;
#endif
      DIR           *dp;
      struct dirent *p_dir;

      work_ptr++;
      (void)strcpy(filter, work_ptr);
      *work_ptr = '\0';

      if ((dp = opendir(fullname)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to opendir() `%s' : %s"),
                    fullname, strerror(errno));
         return;
      }

      while ((p_dir = readdir(dp)) != NULL)
      {
#ifdef LINUX
         if ((p_dir->d_type != DT_REG) || (p_dir->d_name[0] == '.'))
#else
         if (p_dir->d_name[0] == '.')
#endif
         {
            errno = 0;
            continue;
         }

         (void)strcpy(work_ptr, p_dir->d_name);
#ifdef HAVE_STATX
         if (statx(0, fullname, AT_STATX_SYNC_AS_STAT,
# ifndef LINUX
                   STATX_MODE |
# endif
                   STATX_SIZE | STATX_MTIME, &stat_buf) == -1)
#else
         if (stat(fullname, &stat_buf) == -1)
#endif
         {
            if (errno != ENOENT)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Failed to stat() `%s' : %s"),
                          fullname, strerror(errno));
            }
            errno = 0;
            continue;
         }

#ifndef LINUX
         /* Sure it is a normal file? */
# ifdef HAVE_STATX
         if (S_ISREG(stat_buf.stx_mode))
# else
         if (S_ISREG(stat_buf.st_mode))
# endif
         {
#endif
            if (pmatch(filter, p_dir->d_name, NULL) == 0)
            {
               int length;

               /* Check space for dc_dcl structure. */
               if ((no_of_dir_configs % DIR_CONFIG_NAME_STEP_SIZE) == 0)
               {
                  size_t new_size;

                  new_size = ((no_of_dir_configs / DIR_CONFIG_NAME_STEP_SIZE) + 1) * DIR_CONFIG_NAME_STEP_SIZE * sizeof(struct dir_config_buf);

                  if ((dc_dcl = realloc(dc_dcl, new_size)) == NULL)
                  {
                     system_log(FATAL_SIGN, __FILE__, __LINE__,
                                "Could not realloc() memory : %s",
                                strerror(errno));
                     exit(INCORRECT);
                  }
               }

               length = strlen(fullname) + 1;
               if ((dc_dcl[no_of_dir_configs].dir_config_file = malloc(length)) == NULL)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             "Could not malloc() %d bytes : %s",
                             length, strerror(errno));
                  exit(INCORRECT);
               }
               (void)memcpy(dc_dcl[no_of_dir_configs].dir_config_file,
                            fullname, length);
#ifdef HAVE_STATX
               *db_size += stat_buf.stx_size;
               dc_dcl[no_of_dir_configs].dc_old_time = stat_buf.stx_mtime.tv_sec;
#else
               *db_size += stat_buf.st_size;
               dc_dcl[no_of_dir_configs].dc_old_time = stat_buf.st_mtime;
#endif
               dc_dcl[no_of_dir_configs].is_filter = YES;
               no_of_dir_configs++;
            }
#ifndef LINUX
         }
#endif
      }

      /* When using readdir() it can happen that it always returns     */
      /* NULL, due to some error. We want to know if this is the case. */
      if (errno == EBADF)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to readdir() `%s' : %s"), fullname, strerror(errno));
      }

      /* Don't forget to close the directory. */
      if (closedir(dp) == -1)
      {
         *work_ptr = '\0';
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed  to closedir() `%s' : %s"),
                    fullname, strerror(errno));
      }
   }

   return;
}


/*################### check_full_dc_name_changes() #####################*/
int
check_full_dc_name_changes(void)
{
   int changed = NO,
       i;

   for (i = 0; i < no_of_dir_configs; i++)
   {
      if (dc_dcl[i].is_filter == YES)
      {
         dc_dcl[i].in_list = NO;
      }
   }

   for (i = 0; i < no_of_dc_filters; i++)
   {
      if (dcfl[i].is_filter == YES)
      {
         if (check_full_dc_name(dcfl[i].dc_filter) == YES)
         {
            changed = YES;
         }
      }
   }

   if (purge_full_dc_names() == YES)
   {
      changed = YES;
   }

   return(changed);
}


/*++++++++++++++++++++++++ check_full_dc_name() ++++++++++++++++++++++++*/
int
check_full_dc_name(char *dc_filter)
{
   int  changed = NO;
   char fullname[MAX_PATH_LENGTH],
        *work_ptr;

   (void)strcpy(fullname, dc_filter);
   work_ptr = fullname + strlen(fullname);
   while ((*work_ptr != '/') && (work_ptr != fullname))
   {
      work_ptr--;
   }
   if (*work_ptr == '/')
   {
      int           gotcha,
                    i;
      char          filter[MAX_FILENAME_LENGTH + 1];
#ifdef HAVE_STATX
      struct statx  stat_buf;
#else
      struct stat   stat_buf;
#endif
      DIR           *dp;
      struct dirent *p_dir;

      work_ptr++;
      (void)strcpy(filter, work_ptr);
      *work_ptr = '\0';

      if ((dp = opendir(fullname)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to opendir() `%s' : %s"),
                    fullname, strerror(errno));
         return(INCORRECT);
      }

      while ((p_dir = readdir(dp)) != NULL)
      {
#ifdef LINUX
         if ((p_dir->d_type != DT_REG) || (p_dir->d_name[0] == '.'))
#else
         if (p_dir->d_name[0] == '.')
#endif
         {
            errno = 0;
            continue;
         }

         (void)strcpy(work_ptr, p_dir->d_name);
         gotcha = NO;
         for (i = 0; i < no_of_dir_configs; i++)
         {
            if ((dc_dcl[i].is_filter == YES) &&
                (my_strcmp(dc_dcl[i].dir_config_file, fullname) == 0))
            {
               dc_dcl[i].in_list = YES;
               gotcha = YES;
               i = no_of_dir_configs; /* To leave for loop. */
            }
         }

         if (gotcha == NO)
         {
#ifdef HAVE_STATX
            if (statx(0, fullname, AT_STATX_SYNC_AS_STAT,
# ifndef LINUX
                      STATX_MODE |
# endif
                      STATX_SIZE | STATX_MTIME, &stat_buf) == -1)
#else
            if (stat(fullname, &stat_buf) < 0)
#endif
            {
               if (errno != ENOENT)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                             _("Failed to statx() `%s' : %s"),
#else
                             _("Failed to stat() `%s' : %s"),
#endif
                             fullname, strerror(errno));
               }
               errno = 0;
               continue;
            }

#ifndef LINUX
            /* Sure it is a normal file? */
# ifdef HAVE_STATX
            if (S_ISREG(stat_buf.stx_mode))
# else
            if (S_ISREG(stat_buf.st_mode))
# endif
            {
#endif
               if (pmatch(filter, p_dir->d_name, NULL) == 0)
               {
                  int length;

                  /* Check space for dc_dcl structure. */
                  if ((no_of_dir_configs % DIR_CONFIG_NAME_STEP_SIZE) == 0)
                  {
                     size_t new_size;

                     new_size = ((no_of_dir_configs / DIR_CONFIG_NAME_STEP_SIZE) + 1) * DIR_CONFIG_NAME_STEP_SIZE * sizeof(struct dir_config_buf);

                     if ((dc_dcl = realloc(dc_dcl, new_size)) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "Could not realloc() memory : %s",
                                   strerror(errno));
                        exit(INCORRECT);
                     }
                  }

                  length = strlen(fullname) + 1;
                  if ((dc_dcl[no_of_dir_configs].dir_config_file = malloc(length)) == NULL)
                  {
                     system_log(FATAL_SIGN, __FILE__, __LINE__,
                                "Could not malloc() %d bytes : %s",
                                length, strerror(errno));
                     exit(INCORRECT);
                  }
                  system_log(DEBUG_SIGN, NULL, 0L,
                             "Detected new DIR_CONFIG %s", fullname);
                  (void)memcpy(dc_dcl[no_of_dir_configs].dir_config_file,
                               fullname, length);
#ifdef HAVE_STATX
                  dc_dcl[no_of_dir_configs].dc_old_time = stat_buf.stx_mtime.tv_sec;
                  dc_dcl[no_of_dir_configs].size = stat_buf.stx_size;
#else
                  dc_dcl[no_of_dir_configs].dc_old_time = stat_buf.st_mtime;
                  dc_dcl[no_of_dir_configs].size = stat_buf.st_size;
#endif
                  dc_dcl[no_of_dir_configs].is_filter = YES;
                  dc_dcl[no_of_dir_configs].in_list = NEITHER;
                  no_of_dir_configs++;
                  changed = YES;
               }
#ifndef LINUX
            }
#endif
         } /* if (gotcha == NO) */
      }

      /* When using readdir() it can happen that it always returns     */
      /* NULL, due to some error. We want to know if this is the case. */
      if (errno == EBADF)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to readdir() `%s' : %s"), fullname, strerror(errno));
      }

      /* Don't forget to close the directory. */
      if (closedir(dp) == -1)
      {
         *work_ptr = '\0';
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed  to closedir() `%s' : %s"),
                    fullname, strerror(errno));
      }
   }

   return(changed);
}


/*++++++++++++++++++++++++ purge_full_dc_names() ++++++++++++++++++++++++*/
int
purge_full_dc_names(void)
{
   int changed = NO,
       entries_removed = 0,
       i;

   for (i = 0; i < (no_of_dir_configs - entries_removed); i++)
   {
      if ((dc_dcl[i].is_filter == YES) && (dc_dcl[i].in_list == NO))
      {
         int j = i;

         while ((j < (no_of_dir_configs - entries_removed)) &&
                (dc_dcl[j].is_filter == YES) && (dc_dcl[j].in_list == NO))
         {
            system_log(DEBUG_SIGN, NULL, 0L,
                       "DIR_CONFIG %s is removed.", dc_dcl[j].dir_config_file);
            free(dc_dcl[j].dir_config_file);
            j++;
         }
         if (j < no_of_dir_configs)
         {
            size_t move_size;

            move_size = (no_of_dir_configs - j) * sizeof(struct dir_config_buf);
            (void)memmove(&dc_dcl[i], &dc_dcl[j], move_size);
         }
         entries_removed += (j - i);
         changed = YES;
      }
   }
   if (entries_removed > 0)
   {
      int current_no_of_dir_configs = no_of_dir_configs;
      size_t new_size,
             old_size;

      no_of_dir_configs -= entries_removed;
      if (no_of_dir_configs < 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmmm, no_of_dir_configs  = %d", no_of_dir_configs);
         no_of_dir_configs = 0;
      }
      if (no_of_dir_configs == 0)
      {
         new_size = DIR_CONFIG_NAME_STEP_SIZE * sizeof(struct dir_config_buf);
      }
      else
      {
         new_size = ((no_of_dir_configs / DIR_CONFIG_NAME_STEP_SIZE) + 1) * DIR_CONFIG_NAME_STEP_SIZE * sizeof(struct dir_config_buf);
      }
      old_size = ((current_no_of_dir_configs / DIR_CONFIG_NAME_STEP_SIZE) + 1) * DIR_CONFIG_NAME_STEP_SIZE * sizeof(struct dir_config_buf);

      if (old_size != new_size)
      {
         if ((dc_dcl = realloc(dc_dcl, new_size)) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Could not realloc() memory : %s", strerror(errno));
            exit(INCORRECT);
         }
      }
   }

   return(changed);
}
