/*
 *  commondefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __commondefs_h
#define __commondefs_h

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef WITH_SSL
# include <openssl/ssl.h>
#endif

#define MAX_CONTENT_TYPE_LENGTH 30

/* Function prototypes. */
extern void    get_content_type(char *, char *, int);
extern int     command(int, char *, ...),
               connect_with_timeout(int, const struct sockaddr *, socklen_t),
               test_command(int, char *, ...);
#ifdef WITH_SSL
extern int     ssl_connect(int, char *, char *, int, int);
extern ssize_t ssl_write(SSL *, const char *, size_t);
extern char    *rfc2253_formatted(X509_NAME *name),
               *ssl_error_msg(char *, SSL *, int *, int, char *);
#endif

#endif /* __commondefs_h */
