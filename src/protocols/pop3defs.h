/*
 *  pop3defs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 - 2008 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __pop3defs_h
#define __pop3defs_h

#define DEFAULT_POP3_PORT       110
#ifdef WITH_SSL
# define SSL_POP3_PORT          995
#endif

#define POP3_OK                 0
#define POP3_ERROR              -2

#ifndef MAX_RET_MSG_LENGTH
# define MAX_RET_MSG_LENGTH     4096
#endif

/* Function prototypes. */
extern int pop3_connect(char *, int),
           pop3_dele(unsigned int),
           pop3_pass(char *),
           pop3_quit(void),
           pop3_read(char *, int),
           pop3_retrieve(unsigned int, off_t *),
           pop3_stat(int *, off_t *),
           pop3_user(char *);

#endif /* __pop3defs_h */
