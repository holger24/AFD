/*
 *  create_url_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   create_url_file - creates file which contains a url
 **
 ** SYNOPSIS
 **   void create_url_file(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.01.2005 H.Kiehl Created
 **   11.07.2024 H.Kiehl Check if user and/or password have special
 **                      URL characters. If that is the case mask them.
 **
 */
DESCR__E_M3

#include <stdio.h>               /* sprintf()                            */
#include <string.h>              /* strerror()                           */
#include <stdlib.h>              /* exit()                               */
#include <unistd.h>              /* getpid(), write(), close()           */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "xsend_file.h"

/* External global variables. */
extern char             url_file_name[];
extern struct send_data *db;


/*########################## create_url_file() ##########################*/
void
create_url_file(void)
{
   int   fd;
   uid_t euid, /* Effective user ID. */
         ruid; /* Real user ID. */

   euid = geteuid();
   ruid = getuid();
   if (euid != ruid)
   {
      if (seteuid(ruid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                       ruid, strerror(errno), __FILE__, __LINE__);
      }
   }
#if SIZEOF_PID_T == 4
   (void)sprintf(url_file_name, ".xsend_file_url.%d", (pri_pid_t)getpid());
#else
   (void)sprintf(url_file_name, ".xsend_file_url.%lld", (pri_pid_t)getpid());
#endif

   if ((fd = open(url_file_name, (O_WRONLY | O_CREAT | O_TRUNC),
                  (S_IRUSR | S_IWUSR))) == -1)
   {
      (void)fprintf(stderr, "Failed to open() `%s' : %s (%s %d)\n",
                    url_file_name, strerror(errno), __FILE__, __LINE__);
      url_file_name[0] = '\0';
      exit(INCORRECT);
   }
   else
   {
      char   buffer[MAX_PATH_LENGTH + 1],
             *rptr,
             *wptr;
      size_t length;

      if (euid != ruid)
      {
         if (seteuid(euid) == -1)
         {
            (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                          euid, strerror(errno), __FILE__, __LINE__);
         }
      }

      if (db->protocol == FTP)
      {
         length = sprintf(buffer, "%s://", FTP_SHEME);
      }
      else if (db->protocol == SMTP)
           {
              length = sprintf(buffer, "%s://", SMTP_SHEME);
           }
      else if (db->protocol == LOC)
           {
              length = sprintf(buffer, "%s://", LOC_SHEME);
           }
      else if (db->protocol == SFTP)
           {
              length = sprintf(buffer, "%s://", SFTP_SHEME);
           }
#ifdef _WITH_SCP_SUPPORT
      else if (db->protocol == SCP)
           {
              length = sprintf(buffer, "%s://", SCP_SHEME);
           }
#endif
#ifdef _WITH_WMO_SUPPORT
      else if (db->protocol == WMO)
           {
              length = sprintf(buffer, "%s://", WMO_SHEME);
           }
#endif
           else
           {
#if SIZEOF_LONG == 4
              (void)fprintf(stderr, "Unknown protocol %d. (%s %d)\n",
#else
              (void)fprintf(stderr, "Unknown protocol %ld. (%s %d)\n",
#endif
                            db->protocol, __FILE__, __LINE__);
              exit(INCORRECT);
           }

      rptr = db->user;
      wptr = &buffer[length];
      while (*rptr != '\0')
      {
         if ((*rptr == '@') || (*rptr == ':') || (*rptr == '/') ||
             (*rptr == ';'))
         {
            *wptr = '\\';
            *(wptr + 1) = *rptr;
            wptr += 2; length += 2;
         }
         else
         {
            *wptr = *rptr;
            wptr++; length++;
         }
         rptr++;
      }
      *wptr = '\0';
      if (db->protocol != SMTP)
      {
         if ((db->password != NULL) && (db->password[0] != '\0'))
         {
            rptr = db->password;

            *wptr = ':';
            wptr++; length++;
            while (*rptr != '\0')
            {
               if ((*rptr == '@') || (*rptr == ':') || (*rptr == '/') ||
                   (*rptr == ';'))
               {
                  *wptr = '\\';
                  *(wptr + 1) = *rptr;
                  wptr += 2; length += 2;
               }
               else
               {
                  *wptr = *rptr;
                  wptr++; length++;
               }
               rptr++;
            }
            *wptr = '\0';
         }
         length += sprintf(&buffer[length], "@%s", db->hostname);
         if (db->target_dir[0] != '\0')
         {
            if ((db->target_dir[0] != '/') ||
                ((db->target_dir[0] == '/') && (db->target_dir[1] != '/')))
            {
               length += sprintf(&buffer[length], "/%s", db->target_dir);
            }
            else
            {
               length += sprintf(&buffer[length], "%s", db->target_dir);
            }
         }
      }
      if (db->smtp_server[0] != '\0')
      {
         length += sprintf(&buffer[length], ";server=%s", db->smtp_server);
      }
      if (write(fd, buffer, length) != length)
      {
         (void)fprintf(stderr, "Failed to write() to `%s' : %s (%s %d)\n",
                       url_file_name, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (close(fd) == -1)
      {
         (void)fprintf(stderr, "Failed to close() `%s' : %s (%s %d)\n",
                       url_file_name, strerror(errno), __FILE__, __LINE__);
      }
   }
   return;
}
