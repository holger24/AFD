/*
 *  smtpdefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __smtpdefs_h
#define __smtpdefs_h

#include "commondefs.h"

#define SMTP_HOST_NAME          "localhost"
#define DEFAULT_SMTP_PORT       25

#ifndef MAX_RET_MSG_LENGTH
# define MAX_RET_MSG_LENGTH     4096
#endif
#define MAX_SMTP_SUBJECT_LENGTH 512

/* Most newer SMTP servers report there capabilities when issung EHLO. */
struct smtp_server_capabilities
       {
          char auth_login;
          char auth_plain;
          char starttls;
          char ssl_enabled;
       };

/* Function prototypes. */
extern int encode_base64(unsigned char *, int, unsigned char *, int),
           smtp_auth(unsigned char, char *, char *),
           smtp_connect(char *, int, int),
           smtp_helo(char *),
           smtp_ehlo(char *),
           smtp_noop(void),
           smtp_user(char *),
           smtp_rcpt(char *),
#ifdef WITH_SSL
           smtp_smarttls(int, int),
#endif
           smtp_open(void),
           smtp_write(char *, char *, size_t),
           smtp_write_iso8859(char *, char *, int),
           smtp_write_subject(char *, size_t *, char *),
           smtp_close(void),
           smtp_quit(void);

#endif /* __smtpdefs_h */
