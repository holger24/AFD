/*
 *  store_passwd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   store_passwd - stores the passwd into an internal database
 **
 ** SYNOPSIS
 **   void store_passwd(char *user, char *hostname, char *passwd)
 **
 ** DESCRIPTION
 **   The function store_passwd() stores the password unreadable in
 **   a database file. If a password for the same user and hostname
 **   already exist, it will be overwritten by the given password.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.05.2003 H.Kiehl Created
 **   12.08.2004 H.Kiehl Don't write password in clear text.
 **   15.04.2008 H.Kiehl Accept url's without @ sign such as http://idefix.
 **   19.04.2008 H.Kiehl No longer necessary to handle the full URL.
 **   22.10.2014 H.Kiehl Added support for different encryption types.
 **
 */
DESCR__E_M3

#include <string.h>          /* strcpy(), strerror()                     */
#include <stdlib.h>          /* realloc()                                */
#include <sys/types.h>
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int               *no_of_passwd,
                         pwb_fd;
extern char              *p_work_dir;
extern struct passwd_buf *pwb;


/*############################ store_passwd() ###########################*/
void
store_passwd(char *user, char *hostname, char *passwd)
{
   int           i;
   char          *ptr,
                 uh_name[MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH + 1]; /* User Host name. */
   unsigned char uh_passwd[MAX_USER_NAME_LENGTH];

   (void)strcpy(uh_name, user);
   if (user[0] != '\0')
   {
      (void)strcpy(uh_name, user);
      (void)strcat(uh_name, hostname);
   }
   else
   {
      (void)strcpy(uh_name, hostname);
   }

   /* Clear text. */
   if ((passwd[0] == '$') && 
       ((passwd[1] == '0') || (passwd[1] == '1') || (passwd[1] == '2')) &&
       (passwd[2] == '$'))
   {
      (void)my_strncpy((char *)uh_passwd, passwd, MAX_USER_NAME_LENGTH);
   }
   else
   {
      /* AFD internal. */
      i = 0;
      while (passwd[i] != '\0')
      {
         if ((i % 2) == 0)
         {
            uh_passwd[i] = passwd[i] - 24 + i;
         }
         else
         {
            uh_passwd[i] = passwd[i] - 11 + i;
         }
         i++;
      }
      uh_passwd[i] = '\0';
   }

   if (pwb == NULL)
   {
      size_t size = (PWB_STEP_SIZE * sizeof(struct passwd_buf)) +
                    AFD_WORD_OFFSET;
      char   pwb_file_name[MAX_PATH_LENGTH];
                  
      (void)strcpy(pwb_file_name, p_work_dir);
      (void)strcat(pwb_file_name, FIFO_DIR);  
      (void)strcat(pwb_file_name, PWB_DATA_FILE);            
      if ((ptr = attach_buf(pwb_file_name, &pwb_fd, &size, DC_PROC_NAME,
#ifdef GROUP_CAN_WRITE
                            (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP),
                            YES)) == (caddr_t) -1)
#else
                            (S_IRUSR | S_IWUSR), YES)) == (caddr_t) -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to mmap() to %s : %s",
                    pwb_file_name, strerror(errno));
         exit(INCORRECT);
      }
      no_of_passwd = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      pwb = (struct passwd_buf *)ptr;

      if (*no_of_passwd > 0)
      {
         for (i = 0; i < *no_of_passwd; i++)
         {
            pwb[i].dup_check = NO;
         }
      }
      else
      {
         ptr -= AFD_WORD_OFFSET;
         *(ptr + SIZEOF_INT + 1) = 0;                         /* Not used. */
         *(ptr + SIZEOF_INT + 1 + 1) = 0;                     /* Not used. */
         *(ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_PWB_VERSION;
         (void)memset((ptr + SIZEOF_INT + 4), 0, SIZEOF_INT); /* Not used. */
         *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;            /* Not used. */
         *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;        /* Not used. */
         *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;        /* Not used. */
         *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;        /* Not used. */
      }

#ifdef LOCK_DEBUG
      lock_region_w(pwb_fd, 1, __FILE__, __LINE__);
#else
      lock_region_w(pwb_fd, 1);
#endif
   }

   /*
    * First check if the password is already stored.
    */
   for (i = 0; i < *no_of_passwd; i++)
   {
      if (CHECK_STRCMP(pwb[i].uh_name, uh_name) == 0)
      {
         if (CHECK_STRCMP((char *)pwb[i].passwd, (char *)uh_passwd) == 0)
         {
            /* Password is already stored. */
            pwb[i].dup_check = YES;
         }
         else
         {
            if (pwb[i].dup_check == NO)
            {
               pwb[i].dup_check = YES;
            }
            else
            {
               (void)system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Different passwords for %s@%s",
                                user, hostname);
            }
            (void)strcpy((char *)pwb[i].passwd, (char *)uh_passwd);
         }

         return;
      }
   }

   /*
    * Password is not in the stored list so this must be a new one.
    * Add it to this list.
    */
   if ((*no_of_passwd != 0) &&
       ((*no_of_passwd % PWB_STEP_SIZE) == 0))
   {
      size_t new_size = (((*no_of_passwd / PWB_STEP_SIZE) + 1) *
                        PWB_STEP_SIZE * sizeof(struct passwd_buf)) +
                        AFD_WORD_OFFSET;

      ptr = (char *)pwb - AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(pwb_fd, ptr, new_size)) == (caddr_t) -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "mmap_resize() error : %s", strerror(errno));
         exit(INCORRECT);
      }
      no_of_passwd = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      pwb = (struct passwd_buf *)ptr;
   }
   (void)strcpy(pwb[*no_of_passwd].uh_name, uh_name);
   (void)strcpy((char *)pwb[*no_of_passwd].passwd, (char *)uh_passwd);
   pwb[*no_of_passwd].dup_check = YES;
   (*no_of_passwd)++;
   
   return;
}
