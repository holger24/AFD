/*
 *  output_log_ptrs.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   output_log_ptrs - initialises and sets data pointers for output log
 **
 ** SYNOPSIS
 **   void output_log_ptrs(unsigned int   **ol_retries,
 **                        unsigned int   **ol_job_number,
 **                        char           **ol_data,
 **                        char           **ol_file_name,
 **                        unsigned short **ol_file_name_length,
 **                        unsigned short **ol_archive_name_length,
 **                        off_t          **ol_file_size,
 **                        unsigned short **ol_unl,
 **                        size_t         *ol_size,
 **                        clock_t        **ol_transfer_time,
 **                        char           **ol_output_type,
 **                        char           *tr_hostname,
 **                        int            host_toggle,
 **                        int            protocol,
 **                        char           *output_log)
 **
 ** DESCRIPTION
 **   When sf_xxx() writes data to the output log via a fifo, it
 **   does so by writing 'ol_data' once. To do this a set of
 **   pointers have to be prepared which point to the right
 **   place in the buffer 'ol_data'. Once the buffer has been
 **   filled with the necessary data it will look as follows:
 **   <TD><FS><NR><JN><UNL><FNL><ANL><HN>\0<LFN>[ /<RFN>]\0[<AD>\0]
 **     |   |   |   |   |    |    |    |     |       |       |
 **     |   |   |   |   |    |    |    |     |   +---+       |
 **     |   |   |   |   |    |    |    |     |   |           V
 **     |   |   |   |   |    |    |    |     |   |   If ANL >0 this
 **     |   |   |   |   |    |    |    |     |   |   contains a \0
 **     |   |   |   |   |    |    |    |     |   |   terminated string of
 **     |   |   |   |   |    |    |    |     |   |   the Archive Directory.
 **     |   |   |   |   |    |    |    |     |   +-> Remote file name.
 **     |   |   |   |   |    |    |    |     +-----> Local file name.
 **     |   |   |   |   |    |    |    +-----------> \0 terminated string
 **     |   |   |   |   |    |    |                  of the hostname,
 **     |   |   |   |   |    |    |                  output type, host
 **     |   |   |   |   |    |    |                  toggle and protocol.
 **     |   |   |   |   |    |    +----------------> An unsigned short
 **     |   |   |   |   |    |                       holding the Archive
 **     |   |   |   |   |    |                       Name Length if the
 **     |   |   |   |   |    |                       file has been archived.
 **     |   |   |   |   |    |                       If not, ANL = 0.
 **     |   |   |   |   |    +---------------------> Unsigned short holding
 **     |   |   |   |   |                            the File Name Length.
 **     |   |   |   |   +--------------------------> Unsigned short holding
 **     |   |   |   |                                the Unique name length.
 **     |   |   |   +------------------------------> Unsigned int holding
 **     |   |   |                                    the Job Number.
 **     |   |   +----------------------------------> Number of retries.
 **     |   +--------------------------------------> File Size of type
 **     |                                            off_t.
 **     +------------------------------------------> Transfer Duration of
 **                                                  type clock_t.
 **
 ** RETURN VALUES
 **   When successful it assigns memory for the buffer 'ol_data'. The
 **   following values are being returned:
 **             *ol_job_number, *ol_data, *ol_file_name,
 **             *ol_file_name_length, *ol_archive_name_length,
 **             *ol_file_size, *ol_unl, ol_size, *ol_transfer_time,
 **             *ol_output_type
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   08.05.1997 H.Kiehl Created
 **   10.12.1997 H.Kiehl Made ol_file_name_length unsigned short due to
 **                      trans_rename option.
 **   17.06.2001 H.Kiehl Added archive_name_length.
 **   14.10.2004 H.Kiehl Added Unique Identifier to trace a job within AFD.
 **   21.02.2008 H.Kiehl Added host toggle and number of retries.
 **   12.09.2008 H.Kiehl Open fifo in a separate function.
 **   14.02.2009 H.Kiehl Added output type.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>


/*########################### output_log_ptrs() #########################*/
void
output_log_ptrs(unsigned int   **ol_retries,
                unsigned int   **ol_job_number,
                char           **ol_data,
                char           **ol_file_name,
                unsigned short **ol_file_name_length,
                unsigned short **ol_archive_name_length,
                off_t          **ol_file_size,
                unsigned short **ol_unl,
                size_t         *ol_size,
                clock_t        **ol_transfer_time,
                char           **ol_output_type,
                char           *tr_hostname,
                int            host_toggle,
                int            protocol,
                char           *output_log)
{
   int offset;

   /*
    * Lets determine the largest offset so the 'structure'
    * is aligned correctly.
    */
   offset = sizeof(clock_t);
   if (sizeof(off_t) > offset)
   {
      offset = sizeof(off_t);
   }
   if (sizeof(unsigned int) > offset)
   {
      offset = sizeof(unsigned int);
   }

   /*
    * Create a buffer which we use can use to send our
    * data to the output log process. The buffer has the
    * following structure:
    *
    * <transfer time><file size><retries><job number>
    * <unique name offset><file name length><archive name length>
    * <host_name><file name [+ remote file name] + archive dir>
    */
   *ol_size = offset + offset + offset + offset +
              sizeof(unsigned short) +       /* Unique name offset. */
              sizeof(unsigned short) +       /* File name length.   */
              sizeof(unsigned short) +       /* Archive name length.*/
              MAX_HOSTNAME_LENGTH + 2 + 2 + 2 + 1 +
              MAX_FILENAME_LENGTH + 1 +      /* Local file name.    */
              MAX_FILENAME_LENGTH + 2 +      /* Remote file name.   */
              MAX_FILENAME_LENGTH + 1;       /* Archive dir.        */
   if ((*ol_data = calloc(*ol_size, sizeof(char))) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "calloc() error : %s", strerror(errno));
      if (output_log != NULL)
      {  
         *output_log = NO;
      }
   }
   else
   {
      *ol_size = offset + offset + offset + offset +
                 sizeof(unsigned short) + sizeof(unsigned short) +
                 sizeof(unsigned short) + MAX_HOSTNAME_LENGTH + 2 + 2 + 2 +
                 1 + 1;                    /* NOTE: + 1 + 1 is     */
                                           /* for '\0' at end      */
                                           /* of host + file name. */
      *ol_transfer_time = (clock_t *)*ol_data;
      *ol_file_size = &(*(off_t *)(*ol_data + offset));
      *ol_retries = (unsigned int *)(*ol_data + offset + offset);
      *ol_job_number = (unsigned int *)(*ol_data + offset + offset + offset);
      *ol_unl = (unsigned short *)(*ol_data + offset + offset + offset +
                                   offset);
      *ol_file_name_length = (unsigned short *)(*ol_data + offset + offset +
                                                 offset + offset +
                                                sizeof(unsigned short));
      *ol_archive_name_length = (unsigned short *)(*ol_data + offset +
                                                   offset + offset +
                                                   offset +
                                                   sizeof(unsigned short) +
                                                   sizeof(unsigned short));
      *ol_output_type = (char *)(*ol_data + offset + offset + offset +
                                 offset + sizeof(unsigned short) +
                                 sizeof(unsigned short) +
                                 sizeof(unsigned short) +
                                 MAX_HOSTNAME_LENGTH + 1);
      *ol_file_name = (char *)(*ol_data + offset + offset + offset +
                               offset + sizeof(unsigned short) +
                               sizeof(unsigned short) +
                               sizeof(unsigned short) +
                               MAX_HOSTNAME_LENGTH + 2 + 2 + 2 + 1);
      (void)snprintf((char *)(*ol_data + offset + offset + offset + offset + sizeof(unsigned short) + sizeof(unsigned short) + sizeof(unsigned short)),
                               MAX_HOSTNAME_LENGTH + 2 + 2 + 2 + 1 + MAX_FILENAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 2 + MAX_FILENAME_LENGTH + 1,
                     "%-*s 0 %x %x",
                     MAX_HOSTNAME_LENGTH, tr_hostname, host_toggle, protocol);
   }

   return;
}
