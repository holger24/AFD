/*
 *  show_dir_list.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_dir_list - Tells which directories are being monitored
 **
 ** SYNOPSIS
 **   void show_dir_list(SSL *ssl)
 **
 ** DESCRIPTION
 **   This function prints a list of all directories currently being
 **   monitored by this AFD in the following format:
 **     DL <dir_number> <dir ID> <dir alias> <dir name> [<original dir name>]
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.10.2006 H.Kiehl Created
 **   19.04.2007 H.Kiehl Added handling for directories not using the
 **                      absolute path name.
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>    /* strerror()                                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>       /* getpwent(), setpwent(), endpwent(), getpwname()*/
#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif
#include <unistd.h>
#include <errno.h>
#include "afddsdefs.h"

/* External global variables. */
extern int                        no_of_dirs;
extern char                       *p_work_dir;
extern struct fileretrieve_status *fra;

/* Local function prototypes. */
static int                        get_home_dir_length(char *);
static void                       get_home_dir_user(char *, char *, int *);


/*########################### show_dir_list() ###########################*/
void
show_dir_list(SSL *ssl)
{
   int  fd;
   char fullname[MAX_PATH_LENGTH];

   (void)command(ssl, "211- AFD directory list:");
   (void)command(ssl, "ND %d", no_of_dirs);

   (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, FIFO_DIR, DIR_NAME_FILE);
   if ((fd = open(fullname, O_RDONLY)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to open() `%s' : %s"), fullname, strerror(errno));
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
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to access `%s' : %s"), fullname, strerror(errno));
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size > AFD_WORD_OFFSET)
#else
         if (stat_buf.st_size > AFD_WORD_OFFSET)
#endif
         {
            char *ptr;

#ifdef HAVE_MMAP
            if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                            stat_buf.stx_size, PROT_READ,
# else
                            stat_buf.st_size, PROT_READ,
# endif
                            MAP_SHARED, fd, 0)) == (caddr_t) -1)
#else
            if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                stat_buf.stx_size, PROT_READ,
# else
                                stat_buf.st_size, PROT_READ,
# endif
                                MAP_SHARED, fullname, 0)) == (caddr_t) -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to mmap() to `%s' : %s"),
                          fullname, strerror(errno));
            }
            else
            {
               int                 *no_of_dir_names;
               struct dir_name_buf *dnb;

               no_of_dir_names = (int *)ptr;
               ptr += AFD_WORD_OFFSET;
               dnb = (struct dir_name_buf *)ptr;

               if (*no_of_dir_names > 0)
               {
                  int  home_dir_length,
                       i, k, m;
                  char home_dir_user[MAX_USER_NAME_LENGTH];

                  for (i = 0; i < no_of_dirs; i++)
                  {
                     for (k = 0; k < *no_of_dir_names; k++)
                     {
                        if (fra[i].dir_id == dnb[k].dir_id)
                        {
                           if (dnb[k].orig_dir_name[0] == '~')
                           {
                              m = 1;
                              while ((dnb[k].orig_dir_name[m] != '/') &&
                                     (dnb[k].orig_dir_name[m] != '\0'))
                              {
                                 home_dir_user[m - 1] = dnb[k].orig_dir_name[m];
                                 m++;
                              }
                              home_dir_user[m - 1] = '\0';
                              home_dir_length = get_home_dir_length(home_dir_user);
                           }
                           else
                           {
                              get_home_dir_user(dnb[k].dir_name, home_dir_user,
                                                &home_dir_length);
                              m = 0;
                           }
                           if (home_dir_user[0] == '\0')
                           {
                              (void)command(ssl, "DL %d %x %s %s %s",
                                            i, fra[i].dir_id, fra[i].dir_alias,
                                            dnb[k].dir_name,
                                            dnb[k].orig_dir_name);
                           }
                           else
                           {
                              (void)command(ssl, "DL %d %x %s %s %s %s %x",
                                            i, fra[i].dir_id, fra[i].dir_alias,
                                            dnb[k].dir_name,
                                            dnb[k].orig_dir_name,
                                            home_dir_user, m);
                           }
                           break;
                        }
                     }
                  }
               }

               ptr -= AFD_WORD_OFFSET;
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
                             _("Failed to munmap() `%s' : %s"),
                             fullname, strerror(errno));
               }
            }
         }
         else
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("Hmmm, `%s' is less then %d bytes long."),
                       fullname, AFD_WORD_OFFSET);
         }
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("Failed to close() `%s' : %s"),
                    fullname, strerror(errno));
      }
   }

   return;
}


/*+++++++++++++++++++++++++ get_home_dir_user() +++++++++++++++++++++++++*/
static void
get_home_dir_user(char *dir_name, char *home_dir_user, int *home_dir_length)
{
   struct passwd *pw;

   home_dir_user[0] = '\0';
   *home_dir_length = 0;
   setpwent();
   while ((pw = getpwent()) != NULL)
   {
      *home_dir_length = strlen(pw->pw_dir);
      if (strncmp(pw->pw_dir, dir_name, *home_dir_length) == 0)
      {
         (void)strcpy(home_dir_user, pw->pw_name);
         break;
      }
   }
   endpwent();

   return;
}


/*++++++++++++++++++++++++ get_home_dir_length() ++++++++++++++++++++++++*/
static int
get_home_dir_length(char *home_dir_user)
{
   int           length;
   struct passwd *pw;

   if ((pw = getpwnam(home_dir_user)) == NULL)
   {
      length = 0;
   }
   else
   {
      length = strlen(pw->pw_dir);
   }

   return(length);
}
