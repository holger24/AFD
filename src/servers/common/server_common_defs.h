/*
 *  server_common_defs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __server_common_defs_h
#define __server_common_defs_h

#include <openssl/ssl.h>

/* Function Prototypes. */
extern int     command(SSL *ssl, char *fmt, ...),
               get_free_connection(const int);
extern ssize_t ssl_write(SSL *, const char *, size_t);

#endif /* __server_common_defs_h */
