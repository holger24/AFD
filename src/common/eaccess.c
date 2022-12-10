/*
 *  eaccess.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 - 2022 Deutscher Wetterdienst (DWD),
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
 **   eaccess - check user's permissions for a file based on the
 **             effective user ID
 **
 ** SYNOPSIS
 **   int eaccess(char *pathname, int mode)
 **
 ** DESCRIPTION
 **   The function eaccess() checks whether the process would be allowd
 **   to read, write or test for existence of the file <pathname> based
 **   on the effective user ID of the process.
 **
 ** RETURN VALUES
 **   On success, zero is returned. On error -1 is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   23.01.2002 H.Kiehl Created
 **   05.09.2005 H.Kiehl We did not check the case when a group has
 **                      multiple users.
 **
 */
DESCR__E_M3

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <pwd.h>
#include <grp.h>
#include <unistd.h>            /* geteuid(), getegid()                   */
#include <errno.h>


/* Local function prototypes. */
static int check_group(uid_t, gid_t, struct group **, struct passwd **);


#ifndef HAVE_EACCESS
/*############################# eaccess() ###############################*/
int
eaccess(char *pathname, int access_mode)
{
   if ((access_mode & R_OK) || (access_mode & W_OK) || (access_mode & X_OK))
   {
      int           access_ctrl = 0;
#ifdef HAVE_STATX
      struct statx  stat_buf;
#else
      struct stat   stat_buf;
#endif
      struct group  *group = NULL;
      struct passwd *passwd = NULL;

#ifdef HAVE_STATX
      if (statx(0, pathname, AT_STATX_SYNC_AS_STAT,
                STATX_MODE | STATX_UID | STATX_GUID, &stat_buf) == -1)
#else
      if (stat(pathname, &stat_buf) == -1)
#endif
      {
         access_ctrl = -1;
      }
      else
      {
         uid_t euid;
         gid_t egid;

         euid = geteuid();
         egid = getegid();
         if (access_mode & R_OK)
         {
#ifdef HAVE_STATX
            if ((stat_buf.stx_mode & S_IROTH) ||
                ((euid == stat_buf.stx_uid) && (stat_buf.stx_mode & S_IRUSR)) ||
                ((egid == stat_buf.stx_gid) && (stat_buf.stx_mode & S_IRGRP)) ||
                (check_group(euid, stat_buf.stx_gid, &group, &passwd) == YES))
#else
            if ((stat_buf.st_mode & S_IROTH) ||
                ((euid == stat_buf.st_uid) && (stat_buf.st_mode & S_IRUSR)) ||
                ((egid == stat_buf.st_gid) && (stat_buf.st_mode & S_IRGRP)) ||
                (check_group(euid, stat_buf.st_gid, &group, &passwd) == YES))
#endif
            {
               access_ctrl = 0;
            }
            else
            {
               access_ctrl = -1;
            }
         }
         if ((access_ctrl == 0) && (access_mode & W_OK))
         {
#ifdef HAVE_STATX
            if ((stat_buf.stx_mode & S_IWOTH) ||
                ((euid == stat_buf.stx_uid) && (stat_buf.stx_mode & S_IWUSR)) ||
                ((egid == stat_buf.stx_gid) && (stat_buf.stx_mode & S_IWGRP)) ||
                (check_group(euid, stat_buf.stx_gid, &group, &passwd) == YES))
#else
            if ((stat_buf.st_mode & S_IWOTH) ||
                ((euid == stat_buf.st_uid) && (stat_buf.st_mode & S_IWUSR)) ||
                ((egid == stat_buf.st_gid) && (stat_buf.st_mode & S_IWGRP)) ||
                (check_group(euid, stat_buf.st_gid, &group, &passwd) == YES))
#endif
            {
               if (access_ctrl != -1)
               {
                  access_ctrl = 0;
               }
            }
            else
            {
               access_ctrl = -1;
            }
         }
         if ((access_ctrl == 0) && (access_mode & X_OK))
         {
#ifdef HAVE_STATX
            if ((stat_buf.stx_mode & S_IXOTH) ||
                ((euid == stat_buf.stx_uid) && (stat_buf.stx_mode & S_IXUSR)) ||
                ((egid == stat_buf.stx_gid) && (stat_buf.stx_mode & S_IXGRP)) ||
                (check_group(euid, stat_buf.stx_gid, &group, &passwd) == YES))
#else
            if ((stat_buf.st_mode & S_IXOTH) ||
                ((euid == stat_buf.st_uid) && (stat_buf.st_mode & S_IXUSR)) ||
                ((egid == stat_buf.st_gid) && (stat_buf.st_mode & S_IXGRP)) ||
                (check_group(euid, stat_buf.st_gid, &group, &passwd) == YES))
#endif
            {
               if (access_ctrl != -1)
               {
                  access_ctrl = 0;
               }
            }
            else
            {
               access_ctrl = -1;
            }
         }
      }
      return(access_ctrl);
   }
   else if (access_mode == F_OK)
        {
           int fd;

           if ((fd = open(pathname, O_RDONLY)) == -1)
           {
              return(-1);
           }
           else
           {
              (void)close(fd);
           }
           return(0);
        }
        else
        {
           errno = EINVAL;
           return(-1);
        }
}


/*############################# check_group() ###########################*/
static int
check_group(uid_t euid, gid_t guid, struct group **p_group, struct passwd **passwd)
{
   int i;

   if (*p_group == NULL)
   {
      if ((*p_group = getgrgid(guid)) == NULL)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to getgrgid() : %s"), strerror(errno));
         return(NO);
      }
   }
   if (*passwd == NULL)
   {
      if ((*passwd = getpwuid(euid)) == NULL)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to getpwuid() : %s"), strerror(errno));
         return(NO);
      }
   }
   i = 0;
   while ((*p_group)->gr_mem[i] != NULL)
   {
      if (my_strcmp((*p_group)->gr_mem[i], (*passwd)->pw_name) == 0)
      {
         return(YES);
      }
      i++;
      
   }

   return(NO);
}
#endif /* !HAVE_EACCESS */
