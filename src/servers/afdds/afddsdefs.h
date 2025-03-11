/*
 *  afddsdefs - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2025 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __afddsdefs_h
#define __afddsdefs_h

#include <openssl/ssl.h>
#include "afdd_common_defs.h"

#define AFDDS_CERT_FILENAME      "cert.pem"
#define AFDDS_KEY_FILENAME       "key.pem"

#define DEFAULT_AFD_TLS_PORT_NO  "3994"
#define AFDDS_SHUTDOWN_MESSAGE   "500 AFDDS shutdown."

/* Function prototypes. */
extern void    check_changes(SSL *),
               display_file(SSL *),
               handle_request(SSL *, int, int, int, char *),
               show_dir_list(SSL *),
               show_host_list(SSL *),
               show_host_stat(SSL *),
               show_job_list(SSL *),
               show_summary_stat(SSL *);
extern int     command(SSL *ssl, char *fmt, ...),
               get_display_data(SSL *, char *, int, char *, int, int, int, int);
extern ssize_t ssl_write(SSL *, const char *, size_t);

#endif /* __afddsdefs_h */
