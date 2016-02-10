/*
 *  output_log_ptrs.c - Part of AFD, an automatic file distribution program.
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
 **   demcd_log_ptrs - initialises and sets data pointers for DEMCD
 **
 ** SYNOPSIS
 **   void demcd_log_ptrs(unsigned int   **demcd_job_number,
 **                       char           **demcd_data,
 **                       char           **demcd_file_name,
 **                       unsigned short **demcd_file_name_length,
 **                       off_t          **demcd_file_size,
 **                       unsigned short **demcd_unl,
 **                       size_t         *demcd_size,
 **                       unsigned char  **demcd_confirmation_type,
 **                       char           *tr_hostname)
 **
 ** DESCRIPTION
 **   When sf_xxx() writes data to the output log via a fifo, it
 **   does so by writing 'demcd_data' once. To do this a set of
 **   pointers have to be prepared which point to the right
 **   place in the buffer 'demcd_data'. Once the buffer has been
 **   filled with the necessary data it will look as follows:
 **       <FS><JN><UNL><FNL><CT><HN>\0<LFN>
 **         |   |   |    |    |   |     |
 **         |   |   |    |    |   |     +--> Local file name.
 **         |   |   |    |    |   +--------> \0 terminated string
 **         |   |   |    |    |              of the hostname.
 **         |   |   |    |    +------------> Confirmation type.
 **         |   |   |    +-----------------> Unsigned short holding
 **         |   |   |                        the File Name Length.
 **         |   |   +----------------------> Unsigned short holding
 **         |   |                            the Unique name length.
 **         |   +--------------------------> Unsigned int holding
 **         |                                the Job Number.
 **         +------------------------------> File Size of type
 **                                          off_t.
 **
 ** RETURN VALUES
 **   When successful it assigns memory for the buffer 'demcd_data'. The
 **   following values are being returned:
 **             *demcd_job_number, *demcd_data, *demcd_file_name,
 **             *demcd_file_name_length, *demcd_confirmation_type,
 **             *demcd_file_size, *demcd_unl, demcd_size
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.09.2015 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern struct job db;


/*########################### demcd_log_ptrs() ##########################*/
void
demcd_log_ptrs(unsigned int   **demcd_job_number,
               char           **demcd_data,
               char           **demcd_file_name,
               unsigned short **demcd_file_name_length,
               off_t          **demcd_file_size,
               unsigned short **demcd_unl,
               size_t         *demcd_size,
               unsigned char  **demcd_confirmation_type,
               char           *tr_hostname)
{
   int offset;

   /*
    * Lets determine the largest offset so the 'structure'
    * is aligned correctly.
    */
   offset = sizeof(off_t);
   if (sizeof(unsigned int) > offset)
   {
      offset = sizeof(unsigned int);
   }

   /*
    * Create a buffer which we use can use to send our
    * data to the output log process. The buffer has the
    * following structure:
    *
    * <file size><job number><unique name offset><file name length>
    * <host_name><file name>
    */
   *demcd_size = offset + offset +
                 sizeof(unsigned short) +   /* Unique name offset. */
                 sizeof(unsigned short) +   /* File name length.   */
                 sizeof(unsigned char) +    /* Confirmation type.  */
                 MAX_HOSTNAME_LENGTH + 1 +
                 MAX_FILENAME_LENGTH + 1;   /* Local file name.    */
   if ((*demcd_data = calloc(*demcd_size, sizeof(char))) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "calloc() error : %s", strerror(errno));
      db.demcd_log = NO;
   }
   else
   {
      *demcd_size = offset + offset +
                    sizeof(unsigned short) + sizeof(unsigned short) +
                    sizeof(unsigned char) +
                    MAX_HOSTNAME_LENGTH + 1 + 1; /* NOTE: + 1 + 1 is     */
                                                 /* for '\0' at end      */
                                                 /* of host + file name. */
      *demcd_file_size = &(*(off_t *)(*demcd_data));
      *demcd_job_number = (unsigned int *)(*demcd_data + offset);
      *demcd_unl = (unsigned short *)(*demcd_data + offset + offset);
      *demcd_file_name_length = (unsigned short *)(*demcd_data + offset +
                                                   offset +
                                                   sizeof(unsigned short));
      *demcd_confirmation_type = (unsigned char *)(*demcd_data + offset +
                                                   offset +
                                                   sizeof(unsigned short) +
                                                   sizeof(unsigned short));
      *demcd_file_name = (char *)(*demcd_data + offset + offset +
                                  sizeof(unsigned short) +
                                  sizeof(unsigned short) +
                                  sizeof(unsigned char) +
                                  MAX_HOSTNAME_LENGTH + 1);
      (void)snprintf((char *)(*demcd_data + offset + offset + sizeof(unsigned short) + sizeof(unsigned short) + sizeof(unsigned char)),
                              MAX_HOSTNAME_LENGTH + 1 + MAX_FILENAME_LENGTH + 1,
                     "%-*s",
                     MAX_HOSTNAME_LENGTH, tr_hostname);
   }

   return;
}
