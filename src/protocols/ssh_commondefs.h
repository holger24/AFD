/*
 *  ssh_commondefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __ssh_commondefs_h
#define __ssh_commondefs_h

#define DEFAULT_SSH_PORT                     22
#define SSH_COMMAND                          "ssh"

#ifdef WITH_SSH_FINGERPRINT
# ifdef WITH_REMOVE_FROM_KNOWNHOSTS
struct ssh_data
       {
          char hostname[MAX_REAL_HOSTNAME_LENGTH];
          char user[MAX_USER_NAME_LENGTH + 1];
          int  port;
       };
# endif
#endif

/* Function prototypes. */
extern int    get_ssh_reply(int, int),
              my_siginterrupt(int, int),
              ssh_child_up(void),
              ssh_exec(char *, int, unsigned char, int,
#ifndef FORCE_SFTP_NOOP
                       int,
#endif
                       char *, char *, char *, char *, int *),
#ifdef WITH_SSH_FINGERPRINT
              ssh_login(int, char *, char, char *);
#else
              ssh_login(int, char *, char);
#endif
extern size_t pipe_write(int, char *, size_t);

#endif /* __ssh_commondefs_h */
