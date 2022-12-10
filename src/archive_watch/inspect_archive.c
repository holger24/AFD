/*
 *  inspect_archive.c - Part of AFD, an automatic file distribution program.
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
 **   inspect_archive - walks through a directory and deletes old
 **                     archives
 **
 ** SYNOPSIS
 **   void inspect_archive(char *archive_dir)
 **
 ** DESCRIPTION
 **   The function inspect_archive() walks through the archive of the
 **   AFD and deletes any old archives. The archive directories have
 **   the following format:
 **    $(AFD_WORK_DIR)/archive/[hostalias]/[user]/[dirnumber]/[archive_name]
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.02.1996 H.Kiehl Created
 **   11.05.1998 H.Kiehl Adapted to new message names.
 **   29.11.2000 H.Kiehl Optimized the removing of files.
 **   28.12.2000 H.Kiehl Remove all empty directories in archive.
 **   23.02.2005 H.Kiehl Remove priority from archive name.
 **   18.08.2015 H.Kiehl Added supoort for multiple filesystems.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                  /* strcpy(), strcmp(), strlen(),    */
                                     /* strerror()                       */
#include <stdlib.h>                  /* atoi()                           */
#include <ctype.h>                   /* isdigit(), isxdigit()            */
#include <unistd.h>                  /* rmdir(), unlink()                */
#include <sys/types.h>
#ifdef HAVE_STATX
# include <fcntl.h>                  /* Definition of AT_* constants     */
#endif
#include <sys/stat.h>
#include <dirent.h>                  /* opendir(), readdir(), closedir() */
#include <errno.h>
#include "awdefs.h"

#define TIME_UP  1

/* External global variables. */
extern unsigned int removed_archives,
                    removed_files;
extern time_t       current_time;

/* Local function prototypes. */
static int          check_time(char *),
                    is_archive_name(char *),
                    remove_archive(char *);


/*########################### inspect_archive() #########################*/
void
inspect_archive(char *archive_dir)
{
   DIR *p_dir_archive;

   if ((p_dir_archive = opendir(archive_dir)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to opendir() `%s' : %s"),
                 archive_dir, strerror(errno));
   }
   else
   {
#ifdef MULTI_FS_SUPPORT
      int           nof;
      char          *ptr_filesystemname;
      struct dirent *dp_filesystemname;
# ifdef HAVE_STATX
      struct statx  stat_buf;
# else
      struct stat   stat_buf;
# endif
      DIR           *p_dir_filesystemname;
#endif
      int           noh = 0,
                    nou, nod;
      char          *ptr_archive,
                    *ptr_hostname,
                    *ptr_username,
                    *ptr_dirnumber;
      struct dirent *dp_archive,
                    *dp_hostname,
                    *dp_username,
                    *dp_dirnumber;
      DIR           *p_dir_hostname,
                    *p_dir_username,
                    *p_dir_dirnumber;

      ptr_archive = archive_dir + strlen(archive_dir);
      *(ptr_archive++) = '/';
      while ((dp_archive = readdir(p_dir_archive)) != NULL)
      {
         if (dp_archive->d_name[0]  != '.')
         {
#ifdef MULTI_FS_SUPPORT
            /* Enter directory with filesystem number. */
            (void)strcpy(ptr_archive, dp_archive->d_name);
# ifdef HAVE_STATX
            if ((statx(0, archive_dir,
                       AT_STATX_SYNC_AS_STAT | AT_SYMLINK_NOFOLLOW,
                       STATX_MODE, &stat_buf) != -1) &&
                (S_ISLNK(stat_buf.stx_mode)))
# else
            if ((lstat(archive_dir, &stat_buf) != -1) &&
                (S_ISLNK(stat_buf.st_mode)))
# endif
            {
               if ((p_dir_filesystemname = opendir(archive_dir)) != NULL)
               {
                  ptr_filesystemname = archive_dir + strlen(archive_dir);
                  *(ptr_filesystemname++) = '/';
                  nof = 0;
                  while ((dp_filesystemname = readdir(p_dir_filesystemname)) != NULL)
                  {
                     nof++;
                     if (dp_filesystemname->d_name[0]  != '.')
                     {
                        /* Enter directory with hostname. */
                        (void)strcpy(ptr_filesystemname,
                                     dp_filesystemname->d_name);
# ifdef HAVE_STATX
                        if ((statx(0, archive_dir,
                                   AT_STATX_SYNC_AS_STAT | AT_SYMLINK_NOFOLLOW,
                                   STATX_MODE, &stat_buf) != -1) &&
                            (S_ISLNK(stat_buf.stx_mode) == 0))
# else
                        if ((lstat(archive_dir, &stat_buf) != -1) &&
                            (S_ISLNK(stat_buf.st_mode) == 0))
# endif
                        {
#else
                        (void)strcpy(ptr_archive, dp_archive->d_name);
#endif
                        if ((p_dir_hostname = opendir(archive_dir)) != NULL)
                        {
                           ptr_hostname = archive_dir + strlen(archive_dir);
                           *(ptr_hostname++) = '/';
                           noh = 0;
                           while ((dp_hostname = readdir(p_dir_hostname)) != NULL)
                           {
                              noh++;
                              if (dp_hostname->d_name[0]  != '.')
                              {
                                 /* Enter directory with username. */
                                 (void)strcpy(ptr_hostname, dp_hostname->d_name);
                                 if ((p_dir_username = opendir(archive_dir)) != NULL)
                                 {
                                    ptr_username = archive_dir + strlen(archive_dir);
                                    *(ptr_username++) = '/';
                                    nou = 0;
                                    while ((dp_username = readdir(p_dir_username)) != NULL)
                                    {
                                       nou++;
                                       if (dp_username->d_name[0]  != '.')
                                       {
                                          /* Enter directory with dirnumber. */
                                          (void)strcpy(ptr_username, dp_username->d_name);
                                          if ((p_dir_dirnumber = opendir(archive_dir)) != NULL)
                                          {
                                             ptr_dirnumber = archive_dir + strlen(archive_dir);
                                             *(ptr_dirnumber++) = '/';
                                             nod = 0;
                                             while ((dp_dirnumber = readdir(p_dir_dirnumber)) != NULL)
                                             {
                                                nod++;
                                                if (dp_dirnumber->d_name[0]  != '.')
                                                {
                                                   if (is_archive_name(dp_dirnumber->d_name) != INCORRECT)
                                                   {
                                                      if (check_time(dp_dirnumber->d_name) == TIME_UP)
                                                      {
                                                         (void)strcpy(ptr_dirnumber, dp_dirnumber->d_name);
                                                         if (remove_archive(archive_dir) != INCORRECT)
                                                         {
                                                            removed_archives++;
                                                            nod--;
#ifdef _LOG_REMOVE_INFO
                                                            system_log(INFO_SIGN, __FILE__, __LINE__,
                                                                       _("Removed archive `%s'."),
                                                                       archive_dir);
#endif
                                                         }
                                                      }
                                                   }
                                                }
                                             } /* while (readdir(dirnumber)) */
                                             if (closedir(p_dir_dirnumber) == -1)
                                             {
                                                ptr_dirnumber[-1] = '\0';
                                                system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                           _("Failed to closedir() `%s' : %s"),
                                                           archive_dir, strerror(errno));
                                             }
                                             else if (nod == 2)
                                                  {
                                                     ptr_dirnumber[-1] = '\0';
                                                     if ((rmdir(archive_dir) == -1) &&
                                                         (errno != EEXIST))
                                                     {
                                                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                                                   _("Failed to rmdir() `%s' : %s"),
                                                                   archive_dir,
                                                                   strerror(errno));
                                                     }
                                                     else
                                                     {
                                                        nou--;
                                                     }
                                                  }
                                          }
                                       }
                                    }
                                    if (closedir(p_dir_username) == -1)
                                    {
                                       ptr_username[-1] = '\0';
                                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                  _("Failed to closedir() `%s' : %s"),
                                                  archive_dir, strerror(errno));
                                    }
                                    else if (nou == 2)
                                         {
                                            ptr_username[-1] = '\0';
                                            if ((rmdir(archive_dir) == -1) &&
                                                (errno != EEXIST))
                                            {
                                               system_log(WARN_SIGN, __FILE__, __LINE__,
                                                          _("Failed to rmdir() `%s' : %s"),
                                                          archive_dir, strerror(errno));
                                            }
                                            else
                                            {
                                               noh--;
                                            }
                                         }
                                 }
                              }
                           }
                           if (closedir(p_dir_hostname) == -1)
                           {
                              ptr_hostname[-1] = '\0';
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         _("Failed to closedir() `%s' : %s"),
                                         archive_dir, strerror(errno));
                           }
                           else if (noh == 2)
                                {
                                   ptr_hostname[-1] = '\0';
                                   if ((rmdir(archive_dir) == -1) &&
                                       (errno != EEXIST))
                                   {
                                      system_log(WARN_SIGN, __FILE__, __LINE__,
                                                 _("Failed to rmdir() `%s' : %s"),
                                                 archive_dir, strerror(errno));
                                   }
                                }
                        }
#ifdef MULTI_FS_SUPPORT
                        }
                     }
                  }
                  if (closedir(p_dir_filesystemname) == -1)
                  {
                     ptr_filesystemname[-1] = '\0';
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                _("Failed to closedir() `%s' : %s"),
                                archive_dir, strerror(errno));
                  }
                  else if (noh == 2)
                       {
                          ptr_filesystemname[-1] = '\0';
# ifdef HAVE_STATX
                          if ((statx(0, archive_dir,
                                     AT_STATX_SYNC_AS_STAT | AT_SYMLINK_NOFOLLOW,
                                     STATX_MODE, &stat_buf) != -1) &&
                              (S_ISLNK(stat_buf.stx_mode) == 0))
# else
                          if ((lstat(archive_dir, &stat_buf) != -1) &&
                              (S_ISLNK(stat_buf.st_mode) == 0))
# endif
                          {
                             if ((rmdir(archive_dir) == -1) &&
                                 (errno != EEXIST))
                             {
                                system_log(WARN_SIGN, __FILE__, __LINE__,
                                           _("Failed to rmdir() `%s' : %s"),
                                           archive_dir, strerror(errno));
                             }
                          }
                       }
               }
            }
#endif
         }
      }
      ptr_archive[-1] = '\0';
      if (closedir(p_dir_archive) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to closedir() `%s' : %s"),
                    archive_dir, strerror(errno));
      }
   }

   return;
}


/*++++++++++++++++++++++++++++ check_time() +++++++++++++++++++++++++++++*/
static int
check_time(char *name)
{
   time_t archive_time;

   if (name[1] == '_')
   {
      archive_time = (time_t)str2timet(&name[2], (char **)NULL, 10);
   }
   else
   {
      archive_time = (time_t)str2timet(name, (char **)NULL, 16);
   }

   if ((errno != ERANGE) && (current_time < archive_time))
   {
      return(0);
   }

   return(TIME_UP);
}


/*++++++++++++++++++++++++++ is_archive_name() ++++++++++++++++++++++++++*/
static int
is_archive_name(char *name)
{
   int i;

   /* First check for old style archive directory names. */
   if (name[1] == '_')
   {
      if (isdigit((int)(name[0])) != 0)
      {
         i = 2;
      }
      else
      {
         return(INCORRECT);
      }
   }
   else
   {
      i = 0;
   }

   while (name[i] != '\0')
   {
      if (isxdigit((int)(name[i])) == 0)
      {
         if (name[i] != '_')
         {
            return(INCORRECT);
         }
      }
      i++;
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++ remove_archive() ++++++++++++++++++++++++++*/
static int
remove_archive(char *dirname)
{
   char          *ptr;
   struct dirent *dirp;
   DIR           *dp;

   if ((dp = opendir(dirname)) == NULL)
   {
      if (errno != ENOTDIR)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to opendir() `%s' : %s"),
                    dirname, strerror(errno));
      }
      return(INCORRECT);
   }

   ptr = dirname + strlen(dirname);
   *ptr++ = '/';

   while ((dirp = readdir(dp)) != NULL)
   {
      if ((dirp->d_name[0] == '.') && ((dirp->d_name[1] == '\0') ||
          ((dirp->d_name[1] == '.') && (dirp->d_name[2] == '\0'))))
      {
         continue;
      }
      (void)strcpy(ptr, dirp->d_name);
      if (unlink(dirname) == -1)
      {
         if (errno == ENOENT)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Failed to unlink() `%s' : %s",
                       dirname, strerror(errno));
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to delete `%s' : %s"),
                       dirname, strerror(errno));
            (void)closedir(dp);
            return(INCORRECT);
         }
      }
      else
      {
         removed_files++;
      }
   }
   ptr[-1] = 0;
   if ((rmdir(dirname) == -1) && (errno != EEXIST))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to rmdir() `%s' : %s"), dirname, strerror(errno));
      (void)closedir(dp);
      return(INCORRECT);
   }
   if (closedir(dp) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to closedir() `%s' : %s"),
                 dirname, strerror(errno));
      return(INCORRECT);
   }

   return(SUCCESS);
}
