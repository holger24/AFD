/*
 *  handle_group_list_mtime.c - Part of AFD, an automatic file
 *                                distribution program.
 *  Copyright (c) 2017 - 2022 Deutscher Wetterdienst (DWD),
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
 **   check_group_list_mtime - check if any group.list file is modified
 **
 ** SYNOPSIS
 **   void init_group_list_mtime(void)
 **   int  check_group_list_mtime(void)
 **   void free_group_list_mtime(void)
 **
 ** DESCRIPTION
 **   Function init_group_list_mtime() initializes the directory names
 **   some initiale buffer we need for the task.
 **
 **   The function check_group_list_mtime() checks if
 **   $AFD_WORK_DIR/etc/group.list has been modified. It also checks
 **   if there any files added, modified and/or removed in directory
 **   $AFD_WORK_DIR/etc/groups/source and $AFD_WORK_DIR/etc/groups/recipient.
 **
 **   Function free_group_list_mtime() frees all alocated resources.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.10.2017 H.Kiehl Created
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

/* Some local definitions. */
#define GROUP_ALIAS_FILES_STEP_SIZE 6
struct group_data
       {
          int    in_list;
          time_t mtime;
          char   name[MAX_FILENAME_LENGTH + 1];
       };


/* External global variables. */
extern char              *p_work_dir;

/* Local global variables. */
static int               group_filter_counter = 0,
                         group_recipient_counter = 0,
                         group_source_counter = 0;
static time_t            group_file_name_mtime = 0;
static char              *filter_dir_name = NULL,
                         *group_file_name = NULL,
                         *p_filter_dir_name = NULL,
                         *p_recipient_dir_name = NULL,
                         *p_source_dir_name = NULL,
                         *recipient_dir_name = NULL,
                         *source_dir_name = NULL;
static struct group_data *gf = NULL,
                         *gr = NULL,
                         *gs = NULL;

/* Local function prototypes. */
static void              purge_gf(int *),
                         purge_gr(int *),
                         purge_gs(int *);


/*###################### init_group_list_mtime() ########################*/
void
init_group_list_mtime(void)
{
   size_t length = strlen(p_work_dir);

   if ((group_file_name = malloc((length + ETC_DIR_LENGTH + GROUP_FILE_LENGTH + 1))) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() memory for storing group file name : %s",
                 strerror(errno));
      return;
   }
   (void)snprintf(group_file_name,
                  length + ETC_DIR_LENGTH + GROUP_FILE_LENGTH + 1,
                  "%s%s%s", p_work_dir, ETC_DIR, GROUP_FILE);

   if ((filter_dir_name = malloc((length + ETC_DIR_LENGTH + GROUP_NAME_DIR_LENGTH + FILE_GROUP_NAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 1))) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() memory for storing file filter directory name : %s",
                 strerror(errno));
      return;
   }
   p_filter_dir_name = filter_dir_name +
                       snprintf(filter_dir_name,
                                length + ETC_DIR_LENGTH + GROUP_NAME_DIR_LENGTH + FILE_GROUP_NAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 1,
                                "%s%s%s%s/",
                                p_work_dir, ETC_DIR, GROUP_NAME_DIR,
                                FILE_GROUP_NAME);

   if ((source_dir_name = malloc((length + ETC_DIR_LENGTH + GROUP_NAME_DIR_LENGTH + SOURCE_GROUP_NAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 1))) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() memory for storing source directory name : %s",
                 strerror(errno));
      return;
   }
   p_source_dir_name = source_dir_name +
                       snprintf(source_dir_name,
                                length + ETC_DIR_LENGTH + GROUP_NAME_DIR_LENGTH + SOURCE_GROUP_NAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 1,
                                "%s%s%s%s/",
                                p_work_dir, ETC_DIR, GROUP_NAME_DIR,
                                SOURCE_GROUP_NAME);

   if ((recipient_dir_name = malloc((length + ETC_DIR_LENGTH + GROUP_NAME_DIR_LENGTH + RECIPIENT_GROUP_NAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 1))) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() memory for storing recipient directory name : %s",
                 strerror(errno));
      return;
   }
   p_recipient_dir_name = recipient_dir_name +
                          snprintf(recipient_dir_name,
                                   length + ETC_DIR_LENGTH + GROUP_NAME_DIR_LENGTH + RECIPIENT_GROUP_NAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 1,
                                   "%s%s%s%s/",
                                   p_work_dir, ETC_DIR, GROUP_NAME_DIR,
                                   RECIPIENT_GROUP_NAME);

   /* Initialize all time values. */
   (void)check_group_list_mtime();

   return;
}


/*##################### check_group_list_mtime() ########################*/
int
check_group_list_mtime(void)
{
   int          changed = NO;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif
   DIR          *dp;

   /* Check if the single group.list file has changed. */
#ifdef HAVE_STATX
   if ((statx(0, group_file_name, AT_STATX_SYNC_AS_STAT,
              STATX_MTIME, &stat_buf) == -1) && (errno != ENOENT))
#else
   if ((stat(group_file_name, &stat_buf) == -1) && (errno != ENOENT))
#endif
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 _("Failed to statx() `%s' : %s"),
#else
                 _("Failed to stat() `%s' : %s"),
#endif
                 group_file_name, strerror(errno));
   }
   else
   {
#ifdef HAVE_STATX
      if (stat_buf.stx_mtime.tv_sec != group_file_name_mtime)
#else
      if (stat_buf.st_mtime != group_file_name_mtime)
#endif
      {
         system_log(DEBUG_SIGN, NULL, 0L,
                    "group list file %s was modified.", group_file_name);
#ifdef HAVE_STATX
         group_file_name_mtime = stat_buf.stx_mtime.tv_sec;
#else
         group_file_name_mtime = stat_buf.st_mtime;
#endif
         changed = YES;
      }
   }

   /* Check if there are any changes in file filter group directory. */
   if ((dp = opendir(filter_dir_name)) == NULL)
   {
      if (errno == ENOENT)
      {
         free(gf);
         gf = NULL;
         group_filter_counter = 0;
      }
      else
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to opendir() `%s' : %s"),
                    filter_dir_name, strerror(errno));
      }
   }
   else
   {
      int           i;
      struct dirent *p_dir;

      for (i = 0; i < group_filter_counter; i++)
      {
         gf[i].in_list = NO;
      }

      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            errno = 0;
            continue;
         }

#ifdef LINUX
         if  (p_dir->d_type == DT_REG)
         {
#endif
         (void)strcpy(p_filter_dir_name, p_dir->d_name);
#ifdef HAVE_STATX
         if (statx(0, filter_dir_name, AT_STATX_SYNC_AS_STAT,
# ifndef LINUX
                   STATX_MODE |
# endif
                   STATX_MTIME, &stat_buf) == -1)
#else
         if (stat(filter_dir_name, &stat_buf) == -1)
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
                         filter_dir_name, strerror(errno));
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
            int gotcha = NO;

            /* First lets check if we already have this file in our list. */
            for (i = 0; i < group_filter_counter; i++)
            {
               if (strcmp(gf[i].name, p_dir->d_name) == 0)
               {
                  gotcha = YES;
#ifdef HAVE_STATX
                  if (stat_buf.stx_mtime.tv_sec != gf[i].mtime)
#else
                  if (stat_buf.st_mtime != gf[i].mtime)
#endif
                  {
#ifdef HAVE_STATX
                     gf[i].mtime = stat_buf.stx_mtime.tv_sec;
#else
                     gf[i].mtime = stat_buf.st_mtime;
#endif
                     changed = YES;
                     break;
                  }
                  gf[i].in_list = YES;
               }
            }

            if (gotcha == NO)
            {
               /* Check space for group data structure. */
               if ((group_filter_counter % GROUP_ALIAS_FILES_STEP_SIZE) == 0)
               {
                  size_t new_size;

                  new_size = ((group_filter_counter / GROUP_ALIAS_FILES_STEP_SIZE) + 1) * GROUP_ALIAS_FILES_STEP_SIZE * sizeof(struct group_data);

                  if ((gf = realloc(gf, new_size)) == NULL)
                  {
                     system_log(FATAL_SIGN, __FILE__, __LINE__,
                                "Could not realloc() memory : %s",
                                strerror(errno));
                     exit(INCORRECT);
                  }
               }

               (void)strncpy(gf[group_filter_counter].name,
                             p_dir->d_name, MAX_FILENAME_LENGTH);
#ifdef HAVE_STATX
               gf[group_filter_counter].mtime = stat_buf.stx_mtime.tv_sec;
#else
               gf[group_filter_counter].mtime = stat_buf.st_mtime;
#endif
               gf[group_filter_counter].in_list = YES;
               group_filter_counter++;
               changed = YES;
            }
#ifdef LINUX
         }
#else
         }
#endif
      }

      /* When using readdir() it can happen that it always returns     */
      /* NULL, due to some error. We want to know if this is the case. */
      *p_filter_dir_name = '\0';
      if (errno == EBADF)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to readdir() `%s' : %s"),
                    filter_dir_name, strerror(errno));
      }

      /* Don't forget to close the directory. */
      if (closedir(dp) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed  to closedir() `%s' : %s"),
                    filter_dir_name, strerror(errno));
      }

      /* Check if we need to remove any old names from buffer. */
      purge_gf(&changed);
   }

   /* Check if there are any changes in source group directory. */
   if ((dp = opendir(source_dir_name)) == NULL)
   {
      if (errno == ENOENT)
      {
         free(gs);
         gs = NULL;
         group_source_counter = 0;
      }
      else
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to opendir() `%s' : %s"),
                    source_dir_name, strerror(errno));
      }
   }
   else
   {
      int           i;
      struct dirent *p_dir;

      for (i = 0; i < group_source_counter; i++)
      {
         gs[i].in_list = NO;
      }

      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            errno = 0;
            continue;
         }

#ifdef LINUX
         if  (p_dir->d_type == DT_REG)
         {
#endif
         (void)strcpy(p_source_dir_name, p_dir->d_name);
#ifdef HAVE_STATX
         if (statx(0, source_dir_name, AT_STATX_SYNC_AS_STAT,
# ifndef LINUX
                   STATX_MODE |
# endif
                   STATX_MTIME, &stat_buf) < 0)
#else
         if (stat(source_dir_name, &stat_buf) < 0)
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
                         source_dir_name, strerror(errno));
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
            int gotcha = NO;

            /* First lets check if we already have this file in our list. */
            for (i = 0; i < group_source_counter; i++)
            {
               if (strcmp(gs[i].name, p_dir->d_name) == 0)
               {
                  gotcha = YES;
#ifdef HAVE_STATX
                  if (stat_buf.stx_mtime.tv_sec != gs[i].mtime)
#else
                  if (stat_buf.st_mtime != gs[i].mtime)
#endif
                  {
#ifdef HAVE_STATX
                     gs[i].mtime = stat_buf.stx_mtime.tv_sec;
#else
                     gs[i].mtime = stat_buf.st_mtime;
#endif
                     changed = YES;
                     break;
                  }
                  gs[i].in_list = YES;
               }
            }

            if (gotcha == NO)
            {
               /* Check space for group data structure. */
               if ((group_source_counter % GROUP_ALIAS_FILES_STEP_SIZE) == 0)
               {
                  size_t new_size;

                  new_size = ((group_source_counter / GROUP_ALIAS_FILES_STEP_SIZE) + 1) * GROUP_ALIAS_FILES_STEP_SIZE * sizeof(struct group_data);

                  if ((gs = realloc(gs, new_size)) == NULL)
                  {
                     system_log(FATAL_SIGN, __FILE__, __LINE__,
                                "Could not realloc() memory : %s",
                                strerror(errno));
                     exit(INCORRECT);
                  }
               }

               (void)strncpy(gs[group_source_counter].name,
                             p_dir->d_name, MAX_FILENAME_LENGTH);
#ifdef HAVE_STATX
               gs[group_source_counter].mtime = stat_buf.stx_mtime.tv_sec;
#else
               gs[group_source_counter].mtime = stat_buf.st_mtime;
#endif
               gs[group_source_counter].in_list = YES;
               group_source_counter++;
               changed = YES;
            }
#ifdef LINUX
         }
#else
         }
#endif
      }

      /* When using readdir() it can happen that it always returns     */
      /* NULL, due to some error. We want to know if this is the case. */
      *p_source_dir_name = '\0';
      if (errno == EBADF)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to readdir() `%s' : %s"),
                    source_dir_name, strerror(errno));
      }

      /* Don't forget to close the directory. */
      if (closedir(dp) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed  to closedir() `%s' : %s"),
                    source_dir_name, strerror(errno));
      }

      /* Check if we need to remove any old names from buffer. */
      purge_gs(&changed);
   }

   /* Check if there are any changes in recipient group directory. */
   if ((dp = opendir(recipient_dir_name)) == NULL)
   {
      if (errno == ENOENT)
      {
         free(gr);
         gr = NULL;
         group_recipient_counter = 0;
      }
      else
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to opendir() `%s' : %s"),
                    recipient_dir_name, strerror(errno));
      }
   }
   else
   {
      int           i;
      struct dirent *p_dir;

      for (i = 0; i < group_recipient_counter; i++)
      {
         gr[i].in_list = NO;
      }

      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            errno = 0;
            continue;
         }

#ifdef LINUX
         if  (p_dir->d_type == DT_REG)
         {
#endif
         (void)strcpy(p_recipient_dir_name, p_dir->d_name);
#ifdef HAVE_STATX
         if (statx(0, recipient_dir_name, AT_STATX_SYNC_AS_STAT,
# ifndef LINUX
                   STATX_MODE |
# endif
                   STATX_MTIME, &stat_buf) < 0)
#else
         if (stat(recipient_dir_name, &stat_buf) < 0)
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
                         recipient_dir_name, strerror(errno));
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
            int gotcha = NO;

            /* First lets check if we already have this file in our list. */
            for (i = 0; i < group_recipient_counter; i++)
            {
               if (strcmp(gr[i].name, p_dir->d_name) == 0)
               {
                  gotcha = YES;
#ifdef HAVE_STATX
                  if (stat_buf.stx_mtime.tv_sec != gr[i].mtime)
#else
                  if (stat_buf.st_mtime != gr[i].mtime)
#endif
                  {
#ifdef HAVE_STATX
                     gr[i].mtime = stat_buf.stx_mtime.tv_sec;
#else
                     gr[i].mtime = stat_buf.st_mtime;
#endif
                     changed = YES;
                     break;
                  }
                  gr[i].in_list = YES;
               }
            }

            if (gotcha == NO)
            {
               /* Check space for group data structure. */
               if ((group_recipient_counter % GROUP_ALIAS_FILES_STEP_SIZE) == 0)
               {
                  size_t new_size;

                  new_size = ((group_recipient_counter / GROUP_ALIAS_FILES_STEP_SIZE) + 1) * GROUP_ALIAS_FILES_STEP_SIZE * sizeof(struct group_data);

                  if ((gr = realloc(gr, new_size)) == NULL)
                  {
                     system_log(FATAL_SIGN, __FILE__, __LINE__,
                                "Could not realloc() memory : %s",
                                strerror(errno));
                     exit(INCORRECT);
                  }
               }

               (void)strncpy(gr[group_recipient_counter].name,
                             p_dir->d_name, MAX_FILENAME_LENGTH);
#ifdef HAVE_STATX
               gr[group_recipient_counter].mtime = stat_buf.stx_mtime.tv_sec;
#else
               gr[group_recipient_counter].mtime = stat_buf.st_mtime;
#endif
               gr[group_recipient_counter].in_list = YES;
               group_recipient_counter++;
               changed = YES;
            }
#ifdef LINUX
         }
#else
         }
#endif
      }

      /* When using readdir() it can happen that it always returns     */
      /* NULL, due to some error. We want to know if this is the case. */
      *p_recipient_dir_name = '\0';
      if (errno == EBADF)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to readdir() `%s' : %s"),
                    recipient_dir_name, strerror(errno));
      }

      /* Don't forget to close the directory. */
      if (closedir(dp) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed  to closedir() `%s' : %s"),
                    recipient_dir_name, strerror(errno));
      }

      /* Check if we need to remove any old names from buffer. */
      purge_gr(&changed);
   }

   return(changed);
}


/*+++++++++++++++++++++++++++++ purge_gf() ++++++++++++++++++++++++++++++*/
static void
purge_gf(int *changed)
{
   int entries_removed = 0,
       i;

   for (i = 0; i < (group_filter_counter - entries_removed); i++)
   {
      if (gf[i].in_list == NO)
      {
         int j = i;

         while ((j < (group_filter_counter - entries_removed)) &&
                (gf[j].in_list == NO))
         {
            system_log(DEBUG_SIGN, NULL, 0L,
                       "file filter group list %s is removed.", gf[j].name);
            j++;
         }
         if (j < group_filter_counter)
         {
            size_t move_size;

            move_size = (group_filter_counter - j) * sizeof(struct group_data);
            (void)memmove(&gf[i], &gf[j], move_size);
         }
         entries_removed += (j - i);
         if (*changed == NO)
         {
            *changed = YES;
         }
      }
   }
   if (entries_removed > 0)
   {
      int    current_group_filter_counter = group_filter_counter;
      size_t new_size,
             old_size;

      group_filter_counter -= entries_removed;
      if (group_filter_counter < 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmmm, group_filter_counter = %d", group_filter_counter);
         group_filter_counter = 0;
      }
      if (group_filter_counter == 0)
      {
         new_size = GROUP_ALIAS_FILES_STEP_SIZE * sizeof(struct group_data);
      }
      else
      {
         new_size = ((group_filter_counter / GROUP_ALIAS_FILES_STEP_SIZE) + 1) * GROUP_ALIAS_FILES_STEP_SIZE * sizeof(struct group_data);
      }
      old_size = ((current_group_filter_counter / GROUP_ALIAS_FILES_STEP_SIZE) + 1) * GROUP_ALIAS_FILES_STEP_SIZE * sizeof(struct group_data);

      if (old_size != new_size)
      {
         if ((gs = realloc(gs, new_size)) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Could not realloc() memory : %s", strerror(errno));
            exit(INCORRECT);
         }
      }
   }

   return;
}


/*+++++++++++++++++++++++++++++ purge_gs() ++++++++++++++++++++++++++++++*/
static void
purge_gs(int *changed)
{
   int entries_removed = 0,
       i;

   for (i = 0; i < (group_source_counter - entries_removed); i++)
   {
      if (gs[i].in_list == NO)
      {
         int j = i;

         while ((j < (group_source_counter - entries_removed)) &&
                (gs[j].in_list == NO))
         {
            system_log(DEBUG_SIGN, NULL, 0L,
                       "source group list %s is removed.", gs[j].name);
            j++;
         }
         if (j < group_source_counter)
         {
            size_t move_size;

            move_size = (group_source_counter - j) * sizeof(struct group_data);
            (void)memmove(&gs[i], &gs[j], move_size);
         }
         entries_removed += (j - i);
         if (*changed == NO)
         {
            *changed = YES;
         }
      }
   }
   if (entries_removed > 0)
   {
      int    current_group_source_counter = group_source_counter;
      size_t new_size,
             old_size;

      group_source_counter -= entries_removed;
      if (group_source_counter < 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmmm, group_source_counter = %d", group_source_counter);
         group_source_counter = 0;
      }
      if (group_source_counter == 0)
      {
         new_size = GROUP_ALIAS_FILES_STEP_SIZE * sizeof(struct group_data);
      }
      else
      {
         new_size = ((group_source_counter / GROUP_ALIAS_FILES_STEP_SIZE) + 1) * GROUP_ALIAS_FILES_STEP_SIZE * sizeof(struct group_data);
      }
      old_size = ((current_group_source_counter / GROUP_ALIAS_FILES_STEP_SIZE) + 1) * GROUP_ALIAS_FILES_STEP_SIZE * sizeof(struct group_data);

      if (old_size != new_size)
      {
         if ((gs = realloc(gs, new_size)) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Could not realloc() memory : %s", strerror(errno));
            exit(INCORRECT);
         }
      }
   }

   return;
}


/*+++++++++++++++++++++++++++++ purge_gr() ++++++++++++++++++++++++++++++*/
static void
purge_gr(int *changed)
{
   int entries_removed = 0,
       i;

   for (i = 0; i < (group_recipient_counter - entries_removed); i++)
   {
      if (gr[i].in_list == NO)
      {
         int j = i;

         while ((j < (group_recipient_counter - entries_removed)) &&
                (gr[j].in_list == NO))
         {
            system_log(DEBUG_SIGN, NULL, 0L,
                       "recipient group list %s is removed.", gr[j].name);
            j++;
         }
         if (j < group_recipient_counter)
         {
            size_t move_size;

            move_size = (group_recipient_counter - j) * sizeof(struct group_data);
            (void)memmove(&gr[i], &gr[j], move_size);
         }
         entries_removed += (j - i);
         if (*changed == NO)
         {
            *changed = YES;
         }
      }
   }
   if (entries_removed > 0)
   {
      int    current_group_recipient_counter = group_recipient_counter;
      size_t new_size,
             old_size;

      group_recipient_counter -= entries_removed;
      if (group_recipient_counter < 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmmm, group_recipient_counter = %d", group_recipient_counter);
         group_recipient_counter = 0;
      }
      if (group_recipient_counter == 0)
      {
         new_size = GROUP_ALIAS_FILES_STEP_SIZE * sizeof(struct group_data);
      }
      else
      {
         new_size = ((group_recipient_counter / GROUP_ALIAS_FILES_STEP_SIZE) + 1) * GROUP_ALIAS_FILES_STEP_SIZE * sizeof(struct group_data);
      }
      old_size = ((current_group_recipient_counter / GROUP_ALIAS_FILES_STEP_SIZE) + 1) * GROUP_ALIAS_FILES_STEP_SIZE * sizeof(struct group_data);

      if (old_size != new_size)
      {
         if ((gr = realloc(gr, new_size)) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Could not realloc() memory : %s", strerror(errno));
            exit(INCORRECT);
         }
      }
   }

   return;
}


/*###################### free_group_list_mtime() ########################*/
void
free_group_list_mtime(void)
{
   free(group_file_name);
   group_file_name = NULL;
   free(gf);
   gf = NULL;
   group_filter_counter = 0;
   free(gs);
   gs = NULL;
   group_source_counter = 0;
   free(gr);
   gr = NULL;
   group_recipient_counter = 0;

   return;
}
