/*
 *  demcddefs - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __demcddefs_h
#define __demcddefs_h

#define DE_MAIL_PRIVAT_ID_LENGTH (MAX_INT_HEX_LENGTH + 1 + MAX_INT_HEX_LENGTH + 1 + 1 + MAX_MSG_NAME_LENGTH + 1)

#define CL_DISPATCH 1
#define CL_RECEIPT  2
#define CL_RETRIEVE 3
#define CL_TIMEUP   4

/* Structure to hold a message of process demcd. */
#define DEMCD_QUE_BUF_SIZE 150
struct demcd_queue_buf
       {
          char          de_mail_privat_id[DE_MAIL_PRIVAT_ID_LENGTH];
          char          file_name[MAX_FILENAME_LENGTH];
          char          alias_name[MAX_HOSTNAME_LENGTH];
          time_t        log_time;
          off_t         file_size;
          unsigned int  jid;
          unsigned char confirmation_type;
       };

/* Function prototypes. */
void check_demcd_queue_space(void),
     check_line(char *),
     log_confirmation(int, int);

#endif /* __demcddefs_h */
