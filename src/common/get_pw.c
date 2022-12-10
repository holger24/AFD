/*
 *  get_pw.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_pw - gets the password from password file
 **
 ** SYNOPSIS
 **   int get_pw(char *uh_name, char *password, int url_conform)
 **
 ** DESCRIPTION
 **   Gets the password for the given user hostname combination (uh_name)
 **   and stores it in password. If it does not find the combination
 **   it will just return an empty string.
 **
 ** RETURN VALUES
 **   If found it will return the password in 'password' and SUCCESS.
 **   If no password is found NONE is returned. Otherwise INCORRECT
 **   will be returned. For both cases NONE and INCORRECT 'password'
 **   will return an empty string.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   12.05.2003 H.Kiehl Created
 **   22.10.2014 H.Kiehl Added possibility to have several different
 **                      encryption methods.
 **
 */
DESCR__E_M3

#include <stdio.h>              /* NULL                                  */
#include <string.h>             /* strcat()                              */
#include <sys/types.h>
#include <sys/stat.h>           /* fstat()                               */
#ifdef HAVE_MMAP
# include <sys/mman.h>          /* mmap(), munmap()                      */
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>             /* open()                                */
#endif
#include <unistd.h>             /* close()                               */
#include <errno.h>

/* External global variables. */
extern char *p_work_dir;


/*############################# get_pw() ################################*/
int
get_pw(char *uh_name, char *password, int url_conform)
{
   int  pwb_fd,
        ret;
   char pwb_file_name[MAX_PATH_LENGTH];

   (void)strcpy(pwb_file_name, p_work_dir);
   (void)strcat(pwb_file_name, FIFO_DIR);
   (void)strcat(pwb_file_name, PWB_DATA_FILE);
   password[0] = '\0';

   if ((pwb_fd = open(pwb_file_name, O_RDONLY)) == -1)
   {
      if (errno == ENOENT)
      {
         /*
          * It can be that there are absolutly no passwords in DIR_CONFIG,
          * so PWB_DATA_FILE is not created.
          */
         ret = SUCCESS;
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to open() `%s' : %s"),
                    pwb_file_name, strerror(errno));
         ret = INCORRECT;
      }
   }
   else
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat  stat_buf;
#endif

#ifdef LOCK_DEBUG
      rlock_region(pwb_fd, 1, __FILE__, __LINE__);
#else
      rlock_region(pwb_fd, 1);
#endif
#ifdef HAVE_STATX
      if (statx(pwb_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(pwb_fd, &stat_buf) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    _("Failed to statx() `%s' : %s"),
#else
                    _("Failed to fstat() `%s' : %s"),
#endif
                    pwb_file_name, strerror(errno));
         ret = INCORRECT;
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size <= AFD_WORD_OFFSET)
#else
         if (stat_buf.st_size <= AFD_WORD_OFFSET)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Password file %s is not long enough to contain any valid data."),
                       pwb_file_name);
            ret = INCORRECT;
         }
         else
         {
            char *ptr;

#ifdef HAVE_MMAP
            if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                            stat_buf.stx_size, PROT_READ, MAP_SHARED,
# else
                            stat_buf.st_size, PROT_READ, MAP_SHARED,
# endif
                            pwb_fd, 0)) == (caddr_t) -1)
#else
            if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                stat_buf.stx_size, PROT_READ, MAP_SHARED,
# else
                                stat_buf.st_size, PROT_READ, MAP_SHARED,
# endif
                                pwb_file_name, 0)) == (caddr_t) -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to mmap() `%s' : %s"),
                          pwb_file_name, strerror(errno));
               ret = INCORRECT;
            }
            else
            {
               if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_PWB_VERSION)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Incorrect password version, unable to get password.");
                  ret = INCORRECT;
               }
               else
               {
                  int               digit,
                                    i, j, k,
                                    no_of_passwd;
                  char              str_hex[2];
                  unsigned char     *tmp_ptr;
                  struct passwd_buf *pwb;

                  no_of_passwd = *(int *)ptr;
                  ptr += AFD_WORD_OFFSET;
                  pwb = (struct passwd_buf *)ptr;
                  ret = NONE; /* In case we do not find anything. */

                  for (i = 0; i < no_of_passwd; i++)
                  {
                     if (CHECK_STRCMP(uh_name, pwb[i].uh_name) == 0)
                     {
                        /* Clear text. */
                        if ((pwb[i].passwd[0] == '$') &&
                            (pwb[i].passwd[1] == '0') &&
                            (pwb[i].passwd[2] == '$'))
                        {
                           (void)strcpy(password, (char *)&pwb[i].passwd[3]);
                           ret = SUCCESS;
                        }
                        else if ((pwb[i].passwd[0] == '$') &&
                                 (pwb[i].passwd[1] == '2') &&
                                 (pwb[i].passwd[2] == '$'))
                             {
                                (void)xor_decrypt(&pwb[i].passwd[3],
                                                  strlen((char *)&pwb[i].passwd[3]),
                                                  (unsigned char *)password);
                             }
                             else
                             {
                                /* AFD internal. */
                                if ((pwb[i].passwd[0] == '$') &&
                                    (pwb[i].passwd[1] == '1') &&
                                    (pwb[i].passwd[2] == '$'))
                                {
                                   tmp_ptr = &pwb[i].passwd[3];
                                }
                                else
                                {
                                   tmp_ptr = pwb[i].passwd;
                                }
                                j = 0; k = 0;
                                digit = -1;
                                while (*tmp_ptr != '\0')
                                {
                                   if ((j % 2) == 0)
                                   {
                                      password[k] = *tmp_ptr + 24 - j;
                                   }
                                   else
                                   {
                                      password[k] = *tmp_ptr + 11 - j;
                                   }
                                   if (url_conform != YES)
                                   {
                                      if (password[k] == '%')
                                      {
                                         digit = 0;
                                         }
                                      else if (digit != -1)
                                           {
                                              str_hex[digit] = password[k];
                                              if (digit == 1)
                                              {
                                                 int value = -1;

                                                 if ((str_hex[0] >= '0') &&
                                                     (str_hex[0] <= '9'))
                                                 {
                                                    value = (str_hex[0] - '0') * 16;
                                                 }
                                                 else if ((str_hex[0] >= 'a') &&
                                                          (str_hex[0] <= 'f'))
                                                      {
                                                         value = ((str_hex[0] - 'a') + 10) * 16;
                                                      }
                                                 else if ((str_hex[0] >= 'A') &&
                                                          (str_hex[0] <= 'F'))
                                                      {
                                                         value = ((str_hex[0] - 'A') + 10) * 16;
                                                      }
                                                 if (value != -1)
                                                 {
                                                    if ((str_hex[1] >= '0') &&
                                                        (str_hex[1] <= '9'))
                                                    {
                                                       value += (str_hex[1] - '0');
                                                       password[k] = value;
                                                       k++;
                                                    }
                                                    else if ((str_hex[1] >= 'a') &&
                                                             (str_hex[1] <= 'f'))
                                                         {
                                                            value += ((str_hex[1] - 'a') + 10);
                                                            password[k] = value;
                                                            k++;
                                                         }
                                                    else if ((str_hex[1] >= 'A') &&
                                                             (str_hex[1] <= 'F'))
                                                         {
                                                            value += ((str_hex[1] - 'A') + 10);
                                                            password[k] = value;
                                                            k++;
                                                         }
                                                         else
                                                         {
                                                            password[k] = '%';
                                                            password[k + 1] = str_hex[0];
                                                            password[k + 2] = str_hex[1];
                                                            k += 3;
                                                         }
                                                    digit = -1;
                                                 }
                                                 else
                                                 {
                                                    password[k] = '%';
                                                    password[k + 1] = str_hex[0];
                                                    k += 2;
                                                    digit = -1;
                                                 }
                                              }
                                              else
                                              {
                                                 digit++;
                                              }
                                           }
                                           else
                                           {
                                              k++;
                                           }
                                   }
                                   else
                                   {
                                      k++;
                                   }
                                   tmp_ptr++; j++;
                                }
                                password[j] = '\0';
                                ret = SUCCESS;
                             }

                        break;
                     }
                  }
                  ptr -= AFD_WORD_OFFSET;
               }
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
                             _("Failed to munmap() from `%s' : %s"),
                             pwb_file_name, strerror(errno));
               }
            }
         }
      }
      if (close(pwb_fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__, _("close() error : %s"),
                    strerror(errno));
      }
   }
   return(ret);
}
