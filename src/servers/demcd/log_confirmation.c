/*
 *  log_confirmation.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   log_confirmation - checks if line contains one of the mail ID's
 **
 ** SYNOPSIS
 **   void log_confirmation(int pos, int type)
 **
 ** DESCRIPTION
 **   The function log_confirmation() writes the confirmation to
 **   the log file.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.09.2015 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>             /* strcpy(), strlen()                    */
#include <unistd.h>             /* write()                               */
#include <errno.h>
#include "demcddefs.h"



/* External global variables. */
extern int                    *no_demcd_queued;
extern struct demcd_queue_buf *dqb;
#ifdef _OUTPUT_LOG
extern int                    ol_fd;
# ifdef WITHOUT_FIFO_RW_SUPPORT
extern int                    ol_readfd;
# endif
extern unsigned int           *ol_job_number,
                              *ol_retries;
extern char                   *ol_data,
                              *ol_file_name,
                              *ol_output_type;
extern unsigned short         *ol_archive_name_length,
                              *ol_file_name_length,
                              *ol_unl;
extern off_t                  *ol_file_size;
extern size_t                 ol_size,
                              ol_real_size;
extern clock_t                *ol_transfer_time;
#endif /* _OUTPUT_LOG */


/*########################## log_confirmation() #########################*/
void
log_confirmation(int pos, int type)
{
#ifdef _CONFIRMATION_LOG
#else
# ifdef _OUTPUT_LOG
   char *ptr;

   if (ol_fd == -2)
   {
# ifdef WITHOUT_FIFO_RW_SUPPORT
      output_log_fd(&ol_fd, &ol_readfd, NULL);
# else
      output_log_fd(&ol_fd, NULL);
# endif
      output_log_ptrs(&ol_retries, &ol_job_number, &ol_data, &ol_file_name,
                      &ol_file_name_length, &ol_archive_name_length,
                      &ol_file_size, &ol_unl, &ol_size, &ol_transfer_time,
                      &ol_output_type, dqb[pos].alias_name, 0 /* TODO */,
                      DE_MAIL, NULL);
   }
   ptr = dqb[pos].de_mail_privat_id;
   while ((*ptr != '-') && (*ptr != '\0'))
   {
      ptr++; /* Away with CRC32 of work dir. */
   }
   if (*ptr == '-')
   {
      ptr++;
      while ((*ptr != '-') && (*ptr != '\0'))
      {
         ptr++; /* Away with CRC32 of system name. */
      }
      if (*ptr == '-')
      {
         ptr++;
      }
      else
      {
         ptr = dqb[pos].de_mail_privat_id;
      }
   }
   else
   {
      ptr = dqb[pos].de_mail_privat_id;
   }
   (void)strcpy(ol_file_name, ptr);
   *ol_unl = strlen(ptr);
   (void)strcpy(ol_file_name + *ol_unl, dqb[pos].file_name);
   *ol_file_name_length = (unsigned short)strlen(ol_file_name);
   ol_file_name[*ol_file_name_length] = SEPARATOR_CHAR;
   ol_file_name[*ol_file_name_length + 1] = '\0';
   (*ol_file_name_length)++;
   *ol_file_size = dqb[pos].file_size;
   *ol_job_number = dqb[pos].jid;
   *ol_retries = 0;
   *ol_transfer_time = 0;
   *ol_archive_name_length = 0;
   if (type == CL_TIMEUP)
   {
      *ol_output_type = OT_CONF_TIMEUP + '0';
   }
   else if (type == CL_DISPATCH)
        {
           *ol_output_type = OT_CONF_OF_DISPATCH + '0';
        }
   else if (type == CL_RECEIPT)
        {
           *ol_output_type = OT_CONF_OF_RECEIPT + '0';
        }
   else if (type == CL_RETRIEVE)
        {
           *ol_output_type = OT_CONF_OF_RETRIEVE + '0';
        }
        else
        {
           *ol_output_type = OT_UNKNOWN + '0';
        }
   ol_real_size = *ol_file_name_length + ol_size;
   if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "write() error : %s", strerror(errno));
   }
# endif
#endif

   return;
}
