/*
 *  handle_extra_workdirs.c - Part of AFD, an automatic file distribution
 *                            program.
 *  Copyright (c) 2015 - 2022 Deutscher Wetterdienst (DWD),
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
 **   handle_extra_workdirs - set of functions to handle the extra work
 **                           dirs from AFD_CONFIG
 **
 ** SYNOPSIS
 **   void get_extra_work_dirs(char                   *afd_config_buffer,
 **                            int                    *no_of_extra_work_dirs,
 **                            struct extra_work_dirs **ewl,
 **                            int                    create)
 **   void delete_stale_extra_work_dir_links(int                    no_of_extra_work_dirs
 **                                          struct extra_work_dirs **ewl)
 **   void free_extra_work_dirs(int                    no_of_extra_work_dirs,
 **                             struct extra_work_dirs **ewl)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   04.11.2015 H.Kiehl Created
 **   17.02.2018 H.Kiehl Handle case when there is no AFD_CONFIG.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef HAVE_STATX
# include <sys/sysmacros.h>        /* makedev()                          */
# include <fcntl.h>                /* Definition of AT_* constants       */
#endif
#include <sys/stat.h>
#include <dirent.h>                /* opendir(), closedir(), readdir(),  */
                                   /* DIR, struct dirent                 */
#include <unistd.h>                /* unlink()                           */
#include <time.h>
#include <errno.h>

/* External global definitions. */
extern char *p_work_dir;

/* Local function prototypes. */
static void scan_old_links(int, struct extra_work_dirs *, char *, int);


/*####################### get_extra_work_dirs() #########################*/
void
get_extra_work_dirs(char                   *afd_config_buffer,
                    int                    *no_of_extra_work_dirs,
                    struct extra_work_dirs **ewl,
                    int                    create)
{
   char         afd_file_dir[MAX_PATH_LENGTH],
                *buffer = NULL,
                linkpath[MAX_PATH_LENGTH],
                *ptr,
                value[MAX_ADD_LOCKED_FILES_LENGTH];
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   if (afd_config_buffer == NULL)
   {
      char config_file[MAX_PATH_LENGTH];

      (void)snprintf(config_file, MAX_PATH_LENGTH, "%s%s%s",
                     p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
      if (eaccess(config_file, F_OK) == 0)
      {
         off_t bytes_read;

         if ((bytes_read = read_file_no_cr(config_file, &buffer, YES,
                                           __FILE__, __LINE__)) != INCORRECT)
         {
            if (bytes_read < 6)
            {
               /* Forget it, there is nothing meaningful in it. */
               free(buffer);
               buffer = NULL;
            }
         }
      }
   }
   else
   {
      buffer = afd_config_buffer;
   }

   if (buffer != NULL)
   {
      ptr = buffer;
      *no_of_extra_work_dirs = 1;
      while ((ptr = get_definition(ptr, EXTRA_WORK_DIR_DEF,
                                   value, MAX_PATH_LENGTH)) != NULL)
      {
         (*no_of_extra_work_dirs)++;
      }
   }

   if ((*ewl = malloc(*no_of_extra_work_dirs * sizeof(struct extra_work_dirs))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Failed to malloc() %d bytes : %s"),
                 *no_of_extra_work_dirs * sizeof(struct extra_work_dirs),
                 strerror(errno));
      exit(INCORRECT);
   }

   /* First insert the default from AFD_WORK_DIR environment variable. */
   (void)strcpy(afd_file_dir, p_work_dir);
   (void)strcat(afd_file_dir, AFD_FILE_DIR);
#ifdef HAVE_STATX
   if (statx(0, afd_file_dir, AT_STATX_SYNC_AS_STAT, 0, &stat_buf) == -1)
#else
   if (stat(afd_file_dir, &stat_buf) == -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                 _("Failed to statx() `%s' : %s"),
#else
                 _("Failed to stat() `%s' : %s"),
#endif
                 afd_file_dir, strerror(errno));
      exit(INCORRECT);
   }
   (*ewl)[0].dir_name_length = strlen(p_work_dir);
   if (((*ewl)[0].dir_name = malloc((*ewl)[0].dir_name_length + 1)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Failed to malloc() %d bytes : %s"),
                 (*ewl)[0].dir_name_length, strerror(errno));
      exit(INCORRECT);
   }
   (void)memcpy((*ewl)[0].dir_name, p_work_dir, (*ewl)[0].dir_name_length + 1);
#ifdef HAVE_STATX
   (*ewl)[0].dev = makedev(stat_buf.stx_dev_major, stat_buf.stx_dev_minor);
#else
   (*ewl)[0].dev = stat_buf.st_dev;
#endif
   if (((*ewl)[0].afd_file_dir = malloc(MAX_PATH_LENGTH)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Failed to malloc() %d bytes : %s"),
                 MAX_PATH_LENGTH, strerror(errno));
      exit(INCORRECT);
   }
   (void)snprintf(linkpath, MAX_PATH_LENGTH, "%s%s/%x",
                  p_work_dir, AFD_ARCHIVE_DIR, (unsigned int)(*ewl)[0].dev);
   (void)snprintf((*ewl)[0].afd_file_dir, MAX_PATH_LENGTH, "%s%s",
                  p_work_dir, AFD_ARCHIVE_DIR);
   if (symlink((*ewl)[0].afd_file_dir, linkpath) == -1)
   {
      if (errno == EEXIST)
      {
         char *resolved_path;

         if ((resolved_path = realpath(linkpath, NULL)) == NULL)
         {
            if (errno != ENOENT)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to get realpath() of `%s' : %s"),
                          linkpath, strerror(errno));
               exit(INCORRECT);
            }
         }
         if ((resolved_path == NULL) ||
             (strcmp((*ewl)[0].afd_file_dir, resolved_path) != 0))
         {
            if (unlink(linkpath) != 0)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to unlink() `%s' : %s"),
                          linkpath, strerror(errno));
               exit(INCORRECT);
            }
            if (resolved_path == NULL)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Deleted wrong link `%s'", linkpath);
            }
            else
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Deleted wrong link `%s' [%s != %s]",
                          linkpath, (*ewl)[0].afd_file_dir, resolved_path);
            }
            if (symlink((*ewl)[0].afd_file_dir, linkpath) == -1)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to symlink() `%s' to `%s' : %s"),
                          linkpath, (*ewl)[0].afd_file_dir, strerror(errno));
               exit(INCORRECT);
            }
         }
         free(resolved_path);
      }
      else
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Failed to symlink() `%s' to `%s' : %s"),
                    linkpath, (*ewl)[0].afd_file_dir, strerror(errno));
         exit(INCORRECT);
      }
   }
   (*ewl)[0].afd_file_dir_length = (*ewl)[0].dir_name_length + AFD_FILE_DIR_LENGTH;
   (void)memcpy((*ewl)[0].afd_file_dir, afd_file_dir,
                (*ewl)[0].afd_file_dir_length + 1);
   if (((*ewl)[0].outgoing_file_dir = malloc(MAX_PATH_LENGTH)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Failed to malloc() %d bytes : %s"),
                 MAX_PATH_LENGTH, strerror(errno));
      exit(INCORRECT);
   }
   (*ewl)[0].outgoing_file_dir_length = (*ewl)[0].afd_file_dir_length + OUTGOING_DIR_LENGTH;
   (void)strcpy((*ewl)[0].outgoing_file_dir, (*ewl)[0].afd_file_dir);
   (void)strcpy((*ewl)[0].outgoing_file_dir + (*ewl)[0].afd_file_dir_length, OUTGOING_DIR);
   if (((*ewl)[0].time_dir = malloc(MAX_PATH_LENGTH)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Failed to malloc() %d bytes : %s"),
                 MAX_PATH_LENGTH, strerror(errno));
      exit(INCORRECT);
   }
   (*ewl)[0].time_dir_length = (*ewl)[0].afd_file_dir_length + AFD_TIME_DIR_LENGTH;
   (void)strcpy((*ewl)[0].time_dir, (*ewl)[0].afd_file_dir);
   (void)strcpy((*ewl)[0].time_dir + (*ewl)[0].afd_file_dir_length, AFD_TIME_DIR);
   (*ewl)[0].p_time_dir_id = (*ewl)[0].time_dir + (*ewl)[0].time_dir_length + 1;

   (void)snprintf(linkpath, MAX_PATH_LENGTH, "%s%s%s/%x",
                  p_work_dir, AFD_FILE_DIR, AFD_TMP_DIR,
                  (unsigned int)(*ewl)[0].dev);
   (void)snprintf((*ewl)[0].afd_file_dir + (*ewl)[0].afd_file_dir_length,
                  MAX_INT_HEX_LENGTH, "%s", AFD_TMP_DIR);
   if (symlink((*ewl)[0].afd_file_dir, linkpath) == -1)
   {
      if (errno == EEXIST)
      {
         char *resolved_path;

         if ((resolved_path = realpath(linkpath, NULL)) == NULL)
         {
            if (errno != ENOENT)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to get realpath() of `%s' : %s"),
                          linkpath, strerror(errno));
               exit(INCORRECT);
            }
         }
         if ((resolved_path == NULL) ||
             (strcmp((*ewl)[0].afd_file_dir, resolved_path) != 0))
         {
            if (unlink(linkpath) != 0)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to unlink() `%s' : %s"),
                          linkpath, strerror(errno));
               exit(INCORRECT);
            }
            if (resolved_path == NULL)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Deleted wrong link `%s'", linkpath);
            }
            else
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Deleted wrong link `%s' [%s != %s]",
                          linkpath, (*ewl)[0].afd_file_dir, resolved_path);
            }
            if (symlink((*ewl)[0].afd_file_dir, linkpath) == -1)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to symlink() `%s' to `%s' : %s"),
                          linkpath, (*ewl)[0].afd_file_dir, strerror(errno));
               exit(INCORRECT);
            }
         }
         free(resolved_path);
      }
      else
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Failed to symlink() `%s' to `%s' : %s"),
                    linkpath, (*ewl)[0].afd_file_dir, strerror(errno));
         exit(INCORRECT);
      }
   }
   (*ewl)[0].afd_file_dir[(*ewl)[0].afd_file_dir_length] = '\0';
   (void)snprintf(linkpath, MAX_PATH_LENGTH, "%s%s%s/%x",
                  p_work_dir, AFD_FILE_DIR, OUTGOING_DIR,
                  (unsigned int)(*ewl)[0].dev);
   if (symlink((*ewl)[0].outgoing_file_dir, linkpath) == -1)
   {
      if (errno == EEXIST)
      {
         char *resolved_path;

         if ((resolved_path = realpath(linkpath, NULL)) == NULL)
         {
            if (errno != ENOENT)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to get realpath() of `%s' : %s"),
                          linkpath, strerror(errno));
               exit(INCORRECT);
            }
         }
         if ((resolved_path == NULL) ||
             (strcmp((*ewl)[0].outgoing_file_dir, resolved_path) != 0))
         {
            if (unlink(linkpath) != 0)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to unlink() `%s' : %s"),
                          linkpath, strerror(errno));
               exit(INCORRECT);
            }
            if (resolved_path == NULL)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Deleted wrong link `%s'", linkpath);
            }
            else
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Deleted wrong link `%s' [%s != %s]",
                          linkpath, (*ewl)[0].outgoing_file_dir,
                          resolved_path);
            }
            if (symlink((*ewl)[0].outgoing_file_dir, linkpath) == -1)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to symlink() `%s' to `%s' : %s"),
                          linkpath, (*ewl)[0].outgoing_file_dir,
                          strerror(errno));
               exit(INCORRECT);
            }
         }
         free(resolved_path);
      }
      else
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Failed to symlink() `%s' to `%s' : %s"),
                    linkpath, (*ewl)[0].outgoing_file_dir, strerror(errno));
         exit(INCORRECT);
      }
   }
   (void)snprintf(linkpath, MAX_PATH_LENGTH, "%s%s%s/%x",
                  p_work_dir, AFD_FILE_DIR, AFD_TIME_DIR,
                  (unsigned int)(*ewl)[0].dev);
   if (symlink((*ewl)[0].time_dir, linkpath) == -1)
   {
      if (errno == EEXIST)
      {
         char *resolved_path;

         if ((resolved_path = realpath(linkpath, NULL)) == NULL)
         {
            if (errno != ENOENT)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to get realpath() of `%s' : %s"),
                          linkpath, strerror(errno));
               exit(INCORRECT);
            }
         }
         if ((resolved_path == NULL) ||
             (strcmp((*ewl)[0].time_dir, resolved_path) != 0))
         {
            if (unlink(linkpath) != 0)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to unlink() `%s' : %s"),
                          linkpath, strerror(errno));
               exit(INCORRECT);
            }
            if (resolved_path == NULL)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Deleted wrong link `%s'", linkpath);
            }
            else
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Deleted wrong link `%s' [%s != %s]",
                          linkpath, (*ewl)[0].time_dir, resolved_path);
            }
            if (symlink((*ewl)[0].time_dir, linkpath) == -1)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to symlink() `%s' to `%s' : %s"),
                          linkpath, (*ewl)[0].time_dir, strerror(errno));
               exit(INCORRECT);
            }
         }
         free(resolved_path);
      }
      else
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Failed to symlink() `%s' to `%s' : %s"),
                    linkpath, (*ewl)[0].time_dir, strerror(errno));
         exit(INCORRECT);
      }
   }
   *((*ewl)[0].time_dir + (*ewl)[0].time_dir_length) = '/';
   *((*ewl)[0].time_dir + (*ewl)[0].time_dir_length + 1) = '\0';

   if (*no_of_extra_work_dirs > 1)
   {
      int  i,
           length,
           ret;
      char created_path[MAX_PATH_LENGTH],
           *error_ptr,
           *ewdp,
           new_path[MAX_PATH_LENGTH];


      ewdp = buffer;
      for (i = 1; i < *no_of_extra_work_dirs; i++)
      {
         ewdp = get_definition(ewdp, EXTRA_WORK_DIR_DEF, value,
                               MAX_PATH_LENGTH);
         if (value[0] != '/')
         {
            char *p_path,
                 user[MAX_USER_NAME_LENGTH + 1];

            if (value[0] == '~')
            {
               if (value[1] == '/')
               {
                  p_path = &value[2];
                  user[0] = '\0';
               }
               else
               {
                  int j = 0;

                  p_path = &value[1];
                  while ((*(p_path + j) != '/') && (*(p_path + j) != '\0') &&
                         (j < MAX_USER_NAME_LENGTH))
                  {
                     user[j] = *(p_path + j);
                     j++;
                  }
                  if (j >= MAX_USER_NAME_LENGTH)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                _("User name to long for %s definition %s. User name may be %d bytes long."),
                                EXTRA_WORK_DIR_DEF, value,
                                MAX_USER_NAME_LENGTH);
                  }
                  user[j] = '\0';
               }
               (void)expand_path(user, p_path);
               length = strlen(p_path);
               (void)memmove(value, p_path, length + 1);
            }
            else
            {
               char tmp_config_file[MAX_PATH_LENGTH];

               (void)strcpy(tmp_config_file, value);
               length = snprintf(value, MAX_PATH_LENGTH, "%s%s/%s",
                                 p_work_dir, AFD_FILE_DIR, tmp_config_file);
            }
         }
         else
         {
            length = strlen(value);
         }

         /* Check create file directory. */
         (void)memcpy(new_path, value, length + 1);
         if ((ret = check_create_path(new_path, DIR_MODE, &error_ptr,
                                      create, YES,
                                      created_path)) == CREATED_DIR)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Created extra directory `%s' [%s]",
                       new_path, created_path);
         }
         else if (ret == NO_ACCESS)
              {
                 if (error_ptr != NULL)
                 {
                    *error_ptr = '\0';
                 }
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Cannot access directory `%s' or create a subdirectory in it.",
                            new_path);
              }
         else if (ret == MKDIR_ERROR)
              {
                 if (error_ptr != NULL)
                 {
                    *error_ptr = '\0';
                 }
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Cannot create directory `%s'.", new_path);
              }
         else if (ret == STAT_ERROR)
              {
                 if (error_ptr != NULL)
                 {
                    *error_ptr = '\0';
                 }
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to stat() `%s'.", new_path);
              }
         else if (ret == ALLOC_ERROR)
              {
                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                            "Could not realloc() memory : %s", strerror(errno));
                 exit(INCORRECT);
              }

         if ((ret == SUCCESS) || (ret == CREATED_DIR))
         {
#ifdef HAVE_STATX
            if (statx(0, new_path, AT_STATX_SYNC_AS_STAT, 0, &stat_buf) == -1)
#else
            if (stat(new_path, &stat_buf) == -1)
#endif
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                          "Unable to statx() `%s' : %s. Will ignore this directory.",
#else
                          "Unable to stat() `%s' : %s. Will ignore this directory.",
#endif
                          new_path, strerror(errno));
               (*ewl)[i].dev = 0;
               (*ewl)[i].dir_name = NULL;
               (*ewl)[i].dir_name_length = 0;
               (*ewl)[i].afd_file_dir = NULL;
               (*ewl)[i].afd_file_dir_length = 0;
               (*ewl)[i].outgoing_file_dir = NULL;
               (*ewl)[i].outgoing_file_dir_length = 0;
               (*ewl)[i].time_dir = NULL;
               (*ewl)[i].time_dir_length = 0;
            }
            else
            {
               if (((*ewl)[i].dir_name = malloc(length + 1)) == NULL)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             _("Failed to malloc() %d bytes : %s"),
                             length + 1, strerror(errno));
                  exit(INCORRECT);
               }
               (void)memcpy((*ewl)[i].dir_name, new_path, length + 1);
               (*ewl)[i].dir_name_length = length;
#ifdef HAVE_STATX
               (*ewl)[i].dev = makedev(stat_buf.stx_dev_major,
                                       stat_buf.stx_dev_minor);
#else
               (*ewl)[i].dev = stat_buf.st_dev;
#endif

               ptr = new_path + length;
               (void)strcpy(ptr, AFD_ARCHIVE_DIR);
               if ((ret = check_create_path(new_path, DIR_MODE, &error_ptr,
                                            create, YES,
                                            created_path)) == CREATED_DIR)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Created extra directory `%s' [%s]",
                             new_path, created_path);
               }
               else if (ret == NO_ACCESS)
                    {
                       if (error_ptr != NULL)
                       {
                          *error_ptr = '\0';
                       }
                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                  "Cannot access directory `%s' or create a subdirectory in it.",
                                  new_path);
                    }
               else if (ret == MKDIR_ERROR)
                    {
                       if (error_ptr != NULL)
                       {
                          *error_ptr = '\0';
                       }
                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                  "Cannot create directory `%s'.", new_path);
                    }
               else if (ret == STAT_ERROR)
                    {
                       if (error_ptr != NULL)
                       {
                          *error_ptr = '\0';
                       }
                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                  "Failed to stat() `%s'.", new_path);
                    }
               else if (ret == ALLOC_ERROR)
                    {
                       system_log(FATAL_SIGN, __FILE__, __LINE__,
                                  "Could not realloc() memory : %s",
                                  strerror(errno));
                       exit(INCORRECT);
                    }

               if ((ret == SUCCESS) || (ret == CREATED_DIR))
               {
                  (void)snprintf(linkpath, MAX_PATH_LENGTH, "%s%s/%x",
                                 p_work_dir, AFD_ARCHIVE_DIR,
                                 (unsigned int)(*ewl)[i].dev);
                  if (symlink(new_path, linkpath) == -1)
                  {
                     if (errno == EEXIST)
                     {
                        char *resolved_path;

                        if ((resolved_path = realpath(linkpath, NULL)) == NULL)
                        {
                           if (errno != ENOENT)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         _("Failed to get realpath() of `%s' : %s"),
                                         linkpath, strerror(errno));
                              exit(INCORRECT);
                           }
                        }
                        if ((resolved_path == NULL) ||
                            (strcmp(new_path, resolved_path) != 0))
                        {
                           if (unlink(linkpath) != 0)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         _("Failed to unlink() `%s' : %s"),
                                         linkpath, strerror(errno));
                              exit(INCORRECT);
                           }
                           if (resolved_path == NULL)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         "Deleted wrong link `%s'", linkpath);
                           }
                           else
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         "Deleted wrong link `%s' [%s != %s]",
                                         linkpath, new_path, resolved_path);
                           }
                           if (symlink(new_path, linkpath) == -1)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         _("Failed to symlink() `%s' to `%s' : %s"),
                                         linkpath, new_path, strerror(errno));
                              exit(INCORRECT);
                           }
                        }
                        free(resolved_path);
                     }
                     else
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   _("Failed to symlink() `%s' to `%s' : %s"),
                                   linkpath, new_path, strerror(errno));
                        exit(INCORRECT);
                     }
                  }

                  (void)strcpy(ptr, AFD_FILE_DIR);
                  if ((ret = check_create_path(new_path, DIR_MODE, &error_ptr,
                                               create, YES,
                                               created_path)) == CREATED_DIR)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "Created extra directory `%s' [%s]",
                                new_path, created_path);
                  }
                  else if (ret == NO_ACCESS)
                       {
                          if (error_ptr != NULL)
                          {
                             *error_ptr = '\0';
                          }
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Cannot access directory `%s' or create a subdirectory in it.",
                                     new_path);
                       }
                  else if (ret == MKDIR_ERROR)
                       {
                          if (error_ptr != NULL)
                          {
                             *error_ptr = '\0';
                          }
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Cannot create directory `%s'.", new_path);
                       }
                  else if (ret == STAT_ERROR)
                       {
                          if (error_ptr != NULL)
                          {
                             *error_ptr = '\0';
                          }
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Failed to stat() `%s'.", new_path);
                       }
                  else if (ret == ALLOC_ERROR)
                       {
                          system_log(FATAL_SIGN, __FILE__, __LINE__,
                                     "Could not realloc() memory : %s",
                                     strerror(errno));
                          exit(INCORRECT);
                       }

                  if ((ret == SUCCESS) || (ret == CREATED_DIR))
                  {
                     if (((*ewl)[i].afd_file_dir = malloc(MAX_PATH_LENGTH)) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   _("Failed to malloc() %d bytes : %s"),
                                   MAX_PATH_LENGTH, strerror(errno));
                        exit(INCORRECT);
                     }
                     (*ewl)[i].afd_file_dir_length = length + AFD_FILE_DIR_LENGTH;
                     (void)memcpy((*ewl)[i].afd_file_dir, new_path,
                                  (*ewl)[i].afd_file_dir_length + 1);

                     /* Check create pool directory. */
                     ptr += AFD_FILE_DIR_LENGTH;
                     (void)strcpy(ptr, AFD_TMP_DIR);
                     if ((ret = check_create_path(new_path, DIR_MODE, &error_ptr,
                                                  create, YES,
                                                  created_path)) == CREATED_DIR)
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                   "Created extra directory `%s' [%s]",
                                   new_path, created_path);
                     }
                     else if (ret == NO_ACCESS)
                          {
                             if (error_ptr != NULL)
                             {
                                *error_ptr = '\0';
                             }
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Cannot access directory `%s' or create a subdirectory in it.",
                                        new_path);
                          }
                     else if (ret == MKDIR_ERROR)
                          {
                             if (error_ptr != NULL)
                             {
                                *error_ptr = '\0';
                             }
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Cannot create directory `%s'.", new_path);
                          }
                     else if (ret == STAT_ERROR)
                          {
                             if (error_ptr != NULL)
                             {
                                *error_ptr = '\0';
                             }
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Failed to stat() `%s'.", new_path);
                          }
                     else if (ret == ALLOC_ERROR)
                          {
                             system_log(FATAL_SIGN, __FILE__, __LINE__,
                                        "Could not realloc() memory : %s",
                                        strerror(errno));
                             exit(INCORRECT);
                          }

                     if ((ret == SUCCESS) || (ret == CREATED_DIR))
                     {
                        (void)snprintf(linkpath, MAX_PATH_LENGTH, "%s%s%s/%x",
                                       p_work_dir, AFD_FILE_DIR, AFD_TMP_DIR,
                                       (unsigned int)(*ewl)[i].dev);
                        if (symlink(new_path, linkpath) == -1)
                        {
                           if (errno == EEXIST)
                           {
                              char *resolved_path;

                              if ((resolved_path = realpath(linkpath, NULL)) == NULL)
                              {
                                 if (errno != ENOENT)
                                 {
                                    system_log(FATAL_SIGN, __FILE__, __LINE__,
                                               _("Failed to get realpath() of `%s' : %s"),
                                               linkpath, strerror(errno));
                                    exit(INCORRECT);
                                 }
                              }
                              if ((resolved_path == NULL) ||
                                  (strcmp(new_path, resolved_path) != 0))
                              {
                                 if (unlink(linkpath) != 0)
                                 {
                                    system_log(FATAL_SIGN, __FILE__, __LINE__,
                                               _("Failed to unlink() `%s' : %s"),
                                               linkpath, strerror(errno));
                                    exit(INCORRECT);
                                 }
                                 if (resolved_path == NULL)
                                 {
                                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                               "Deleted wrong link `%s'",
                                               linkpath);
                                 }
                                 else
                                 {
                                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                               "Deleted wrong link `%s' [%s != %s]",
                                               linkpath, new_path,
                                               resolved_path);
                                 }
                                 if (symlink(new_path, linkpath) == -1)
                                 {
                                    system_log(FATAL_SIGN, __FILE__, __LINE__,
                                               _("Failed to symlink() `%s' to `%s' : %s"),
                                               linkpath, new_path,
                                               strerror(errno));
                                    exit(INCORRECT);
                                 }
                              }
                              free(resolved_path);
                           }
                           else
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         _("Failed to symlink() `%s' to `%s' : %s"),
                                         linkpath, new_path, strerror(errno));
                              exit(INCORRECT);
                           }
                        }

                        /* Check create outgoing_file_dir. */
                        (void)strcpy(ptr, OUTGOING_DIR);
                        if ((ret = check_create_path(new_path, DIR_MODE, &error_ptr,
                                                     create, YES,
                                                     created_path)) == CREATED_DIR)
                        {
                           system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      "Created extra directory `%s' [%s]",
                                      new_path, created_path);
                        }
                        else if (ret == NO_ACCESS)
                             {
                                if (error_ptr != NULL)
                                {
                                   *error_ptr = '\0';
                                }
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           "Cannot access directory `%s' or create a subdirectory in it.",
                                           new_path);
                             }
                        else if (ret == MKDIR_ERROR)
                             {
                                if (error_ptr != NULL)
                                {
                                   *error_ptr = '\0';
                                }
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           "Cannot create directory `%s'.",
                                           new_path);
                             }
                        else if (ret == STAT_ERROR)
                             {
                                if (error_ptr != NULL)
                                {
                                   *error_ptr = '\0';
                                }
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           "Failed to stat() `%s'.", new_path);
                             }
                        else if (ret == ALLOC_ERROR)
                             {
                                system_log(FATAL_SIGN, __FILE__, __LINE__,
                                           "Could not realloc() memory : %s",
                                           strerror(errno));
                                exit(INCORRECT);
                             }

                        if ((ret == SUCCESS) || (ret == CREATED_DIR))
                        {
                           if (((*ewl)[i].outgoing_file_dir = malloc(MAX_PATH_LENGTH)) == NULL)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         _("Failed to malloc() %d bytes : %s"),
                                         MAX_PATH_LENGTH, strerror(errno));
                              exit(INCORRECT);
                           }
                           (*ewl)[i].outgoing_file_dir_length = length + AFD_FILE_DIR_LENGTH + OUTGOING_DIR_LENGTH;
                           (void)memcpy((*ewl)[i].outgoing_file_dir, new_path,
                                        (*ewl)[i].outgoing_file_dir_length + 1);

                           (void)snprintf(linkpath, MAX_PATH_LENGTH, "%s%s%s/%x",
                                          p_work_dir, AFD_FILE_DIR, OUTGOING_DIR,
                                          (unsigned int)(*ewl)[i].dev);
                           if (symlink((*ewl)[i].outgoing_file_dir, linkpath) == -1)
                           {
                              if (errno == EEXIST)
                              {
                                 char *resolved_path;

                                 if ((resolved_path = realpath(linkpath,
                                                               NULL)) == NULL)
                                 {
                                    if (errno != ENOENT)
                                    {
                                       system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                  _("Failed to get realpath() of `%s' : %s"),
                                                  linkpath, strerror(errno));
                                       exit(INCORRECT);
                                    }
                                 }
                                 if ((resolved_path == NULL) ||
                                     (strcmp((*ewl)[i].outgoing_file_dir,
                                             resolved_path) != 0))
                                 {
                                    if (unlink(linkpath) != 0)
                                    {
                                       system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                  _("Failed to unlink() `%s' : %s"),
                                                  linkpath, strerror(errno));
                                       exit(INCORRECT);
                                    }
                                    if (resolved_path == NULL)
                                    {
                                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                  "Deleted wrong link `%s'",
                                                  linkpath);
                                    }
                                    else
                                    {
                                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                  "Deleted wrong link `%s' [%s != %s]",
                                                  linkpath,
                                                  (*ewl)[i].outgoing_file_dir,
                                                  resolved_path);
                                    }
                                    if (symlink((*ewl)[i].outgoing_file_dir, linkpath) == -1)
                                    {
                                       system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                  _("Failed to symlink() `%s' to `%s' : %s"),
                                                  linkpath,
                                                  (*ewl)[i].outgoing_file_dir,
                                                  strerror(errno));
                                       exit(INCORRECT);
                                    }
                                 }
                                 free(resolved_path);
                              }
                              else
                              {
                                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                                            _("Failed to symlink() `%s' to `%s' : %s"),
                                            linkpath,
                                            (*ewl)[i].outgoing_file_dir,
                                            strerror(errno));
                                 exit(INCORRECT);
                              }
                           }

                           /* Check create time_dir. */
                           (void)strcpy(ptr, AFD_TIME_DIR);
                           if ((ret = check_create_path(new_path, DIR_MODE, &error_ptr,
                                                        create, YES,
                                                        created_path)) == CREATED_DIR)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         "Created extra directory `%s' [%s]",
                                         new_path, created_path);
                           }
                           else if (ret == NO_ACCESS)
                                {
                                   if (error_ptr != NULL)
                                   {
                                      *error_ptr = '\0';
                                   }
                                   system_log(WARN_SIGN, __FILE__, __LINE__,
                                              "Cannot access directory `%s' or create a subdirectory in it.",
                                              new_path);
                                }
                           else if (ret == MKDIR_ERROR)
                                {
                                   if (error_ptr != NULL)
                                   {
                                      *error_ptr = '\0';
                                   }
                                   system_log(WARN_SIGN, __FILE__, __LINE__,
                                              "Cannot create directory `%s'.",
                                              new_path);
                                }
                           else if (ret == STAT_ERROR)
                                {
                                   if (error_ptr != NULL)
                                   {
                                      *error_ptr = '\0';
                                   }
                                   system_log(WARN_SIGN, __FILE__, __LINE__,
                                              "Failed to stat() `%s'.",
                                              new_path);
                                }
                           else if (ret == ALLOC_ERROR)
                                {
                                   system_log(FATAL_SIGN, __FILE__, __LINE__,
                                              "Could not realloc() memory : %s",
                                              strerror(errno));
                                   exit(INCORRECT);
                                }

                           if ((ret == SUCCESS) || (ret == CREATED_DIR))
                           {
                              if (((*ewl)[i].time_dir = malloc(MAX_PATH_LENGTH)) == NULL)
                              {
                                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                                            _("Failed to malloc() %d bytes : %s"),
                                            MAX_PATH_LENGTH, strerror(errno));
                                 exit(INCORRECT);
                              }
                              (*ewl)[i].time_dir_length = length + AFD_FILE_DIR_LENGTH + AFD_TIME_DIR_LENGTH;
                              (void)memcpy((*ewl)[i].time_dir, new_path,
                                           (*ewl)[i].time_dir_length + 1);
                              (*ewl)[i].p_time_dir_id = (*ewl)[i].time_dir + (*ewl)[i].time_dir_length + 1;

                              (void)snprintf(linkpath, MAX_PATH_LENGTH,
                                             "%s%s%s/%x", p_work_dir,
                                             AFD_FILE_DIR, AFD_TIME_DIR,
                                             (unsigned int)(*ewl)[i].dev);
                              if (symlink((*ewl)[i].time_dir, linkpath) == -1)
                              {
                                 if (errno == EEXIST)
                                 {
                                    char *resolved_path;

                                    if ((resolved_path = realpath(linkpath, NULL)) == NULL)
                                    {
                                       if (errno != ENOENT)
                                       {
                                          system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                     _("Failed to get realpath() of `%s' : %s"),
                                                     linkpath, strerror(errno));
                                          exit(INCORRECT);
                                       }
                                    }
                                    if ((resolved_path == NULL) ||
                                        (strcmp((*ewl)[i].time_dir,
                                                resolved_path) != 0))
                                    {
                                       if (unlink(linkpath) != 0)
                                       {
                                          system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                     _("Failed to unlink() `%s' : %s"),
                                                     linkpath, strerror(errno));
                                          exit(INCORRECT);
                                       }
                                       if (resolved_path == NULL)
                                       {
                                          system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                     "Deleted wrong link `%s'",
                                                     linkpath);
                                       }
                                       else
                                       {
                                          system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                     "Deleted wrong link `%s' [%s != %s]",
                                                     linkpath,
                                                     (*ewl)[i].time_dir,
                                                     resolved_path);
                                       }
                                       if (symlink((*ewl)[i].time_dir,
                                                   linkpath) == -1)
                                       {
                                          system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                     _("Failed to symlink() `%s' to `%s' : %s"),
                                                     linkpath,
                                                     (*ewl)[i].time_dir,
                                                     strerror(errno));
                                          exit(INCORRECT);
                                       }
                                    }
                                    free(resolved_path);
                                 }
                                 else
                                 {
                                    system_log(FATAL_SIGN, __FILE__, __LINE__,
                                               _("Failed to symlink() `%s' to `%s' : %s"),
                                               linkpath, (*ewl)[i].time_dir,
                                               strerror(errno));
                                    exit(INCORRECT);
                                 }
                              }
                              *((*ewl)[i].time_dir + (*ewl)[i].time_dir_length) = '/';
                              *((*ewl)[i].time_dir + (*ewl)[i].time_dir_length + 1) = '\0';
                           }
                           else
                           {
                              (*ewl)[i].time_dir = NULL;
                              (*ewl)[i].time_dir_length = 0;
                           }
                        }
                        else
                        {
                           (*ewl)[i].outgoing_file_dir = NULL;
                           (*ewl)[i].outgoing_file_dir_length = 0;
                           (*ewl)[i].time_dir = NULL;
                           (*ewl)[i].time_dir_length = 0;
                        }
                     }
                     else
                     {
                        (*ewl)[i].afd_file_dir = NULL;
                        (*ewl)[i].afd_file_dir_length = 0;
                        (*ewl)[i].outgoing_file_dir = NULL;
                        (*ewl)[i].outgoing_file_dir_length = 0;
                        (*ewl)[i].time_dir = NULL;
                        (*ewl)[i].time_dir_length = 0;
                     }
                  }
                  else
                  {
                     (*ewl)[i].afd_file_dir = NULL;
                     (*ewl)[i].afd_file_dir_length = 0;
                     (*ewl)[i].outgoing_file_dir = NULL;
                     (*ewl)[i].outgoing_file_dir_length = 0;
                     (*ewl)[i].time_dir = NULL;
                     (*ewl)[i].time_dir_length = 0;
                  }
               }
               else
               {
                  free((*ewl)[i].dir_name);
                  (*ewl)[i].dir_name = NULL;
                  (*ewl)[i].dir_name_length = 0;
                  (*ewl)[i].afd_file_dir = NULL;
                  (*ewl)[i].afd_file_dir_length = 0;
                  (*ewl)[i].outgoing_file_dir = NULL;
                  (*ewl)[i].outgoing_file_dir_length = 0;
                  (*ewl)[i].time_dir = NULL;
                  (*ewl)[i].time_dir_length = 0;
               }
            }
         }
         else
         {
            (*ewl)[i].dir_name = NULL;
            (*ewl)[i].dir_name_length = 0;
            (*ewl)[i].dev = 0;
            (*ewl)[i].afd_file_dir = NULL;
            (*ewl)[i].afd_file_dir_length = 0;
            (*ewl)[i].outgoing_file_dir = NULL;
            (*ewl)[i].outgoing_file_dir_length = 0;
            (*ewl)[i].time_dir = NULL;
            (*ewl)[i].time_dir_length = 0;
         }
      }
   }

   if (afd_config_buffer == NULL)
   {
      free(buffer);
   }

   return;
}


/*############### delete_stale_extra_work_dir_links() ###################*/
void
delete_stale_extra_work_dir_links(int                    no_of_extra_work_dirs,
                                  struct extra_work_dirs *ewl)
{
   char search_dir[MAX_PATH_LENGTH];

   /* Handle archive dir. */
   (void)memcpy(search_dir, ewl[0].dir_name, ewl[0].dir_name_length);
   (void)memcpy(search_dir + ewl[0].dir_name_length, AFD_ARCHIVE_DIR,
                AFD_ARCHIVE_DIR_LENGTH);
   *(search_dir + ewl[0].dir_name_length + AFD_ARCHIVE_DIR_LENGTH) = '\0';
   scan_old_links(no_of_extra_work_dirs, ewl, search_dir,
                  ewl[0].dir_name_length + AFD_ARCHIVE_DIR_LENGTH);

   /* Handle files/pool dir. */
   (void)memcpy(search_dir + ewl[0].dir_name_length, AFD_FILE_DIR,
                AFD_FILE_DIR_LENGTH);
   (void)memcpy(search_dir + ewl[0].dir_name_length + AFD_FILE_DIR_LENGTH,
                AFD_TMP_DIR, AFD_TMP_DIR_LENGTH);
   *(search_dir + ewl[0].dir_name_length + AFD_FILE_DIR_LENGTH + AFD_TMP_DIR_LENGTH) = '\0';
   scan_old_links(no_of_extra_work_dirs, ewl, search_dir,
                  ewl[0].dir_name_length + AFD_FILE_DIR_LENGTH + AFD_TMP_DIR_LENGTH);

   /* Handle files/outgoing dir. */
   (void)memcpy(search_dir + ewl[0].dir_name_length + AFD_FILE_DIR_LENGTH,
                OUTGOING_DIR, OUTGOING_DIR_LENGTH);
   *(search_dir + ewl[0].dir_name_length + AFD_FILE_DIR_LENGTH + OUTGOING_DIR_LENGTH) = '\0';
   scan_old_links(no_of_extra_work_dirs, ewl, search_dir,
                  ewl[0].dir_name_length + AFD_FILE_DIR_LENGTH + OUTGOING_DIR_LENGTH);

   /* Handle files/time dir. */
   (void)memcpy(search_dir + ewl[0].dir_name_length + AFD_FILE_DIR_LENGTH,
                AFD_TIME_DIR, AFD_TIME_DIR_LENGTH);
   *(search_dir + ewl[0].dir_name_length + AFD_FILE_DIR_LENGTH + AFD_TIME_DIR_LENGTH) = '\0';
   scan_old_links(no_of_extra_work_dirs, ewl, search_dir,
                  ewl[0].dir_name_length + AFD_FILE_DIR_LENGTH + AFD_TIME_DIR_LENGTH);

   return;
}


/*++++++++++++++++++++++++++ scan_old_links() +++++++++++++++++++++++++++*/
static void
scan_old_links(int                    no_of_extra_work_dirs,
               struct extra_work_dirs *ewl,
               char                   *search_dir,
               int                    search_dir_length)
{
   DIR           *dp;
   struct dirent *p_dir;
#ifdef HAVE_STATX
   struct statx  stat_buf;
#else
   struct stat   stat_buf;
#endif

   if ((dp = opendir(search_dir)) == NULL)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Cannot opendir() `%s' : %s",
                 search_dir, strerror(errno));
      return;
   }

   *(search_dir + search_dir_length) = '/';
   while ((p_dir = readdir(dp)) != NULL)
   {
      if (p_dir->d_name[0] == '.')
      {
         continue;
      }
      (void)strcpy(search_dir + search_dir_length + 1, p_dir->d_name);

#ifdef HAVE_STATX
      if (statx(0, search_dir, AT_STATX_SYNC_AS_STAT | AT_SYMLINK_NOFOLLOW,
                STATX_MODE, &stat_buf) != -1)
#else
      if (lstat(search_dir, &stat_buf) != -1)
#endif
      {
         /* Only check symbolic links! */
#ifdef HAVE_STATX
         if (S_ISLNK(stat_buf.stx_mode))
#else
         if (S_ISLNK(stat_buf.st_mode))
#endif
         {
            int          gotcha = NO,
                         i;
            unsigned int job_id;

            job_id = (unsigned int)strtoul(p_dir->d_name, (char **)NULL, 16);
            for (i = 0; i < no_of_extra_work_dirs; i++)
            {
               if (job_id == (unsigned int)ewl[i].dev)
               {
                  gotcha = YES;
                  break;
               }
            }
            if (gotcha == NO)
            {
               if (unlink(search_dir) == -1)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to remove stale link `%s'.", search_dir);
               }
               else
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Removed stale link `%s'.", search_dir);
               }
            }
         }
      }
   }
   *(search_dir + search_dir_length) = '\0';

   if (closedir(dp) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Cannot closedir() `%s' : %s",
                 search_dir, strerror(errno));
   }

   return;
}


/*###################### free_extra_work_dirs() #########################*/
void
free_extra_work_dirs(int no_of_extra_work_dirs, struct extra_work_dirs **ewl)
{
   int i;

   for (i = 0; i < no_of_extra_work_dirs; i++)
   {
      free((*ewl)[i].dir_name);
      free((*ewl)[i].time_dir);
      free((*ewl)[i].afd_file_dir);
      free((*ewl)[i].outgoing_file_dir);
   }
   free(*ewl);
   *ewl = NULL;

   return;
}
