/*
 *  ftpdefs.h - Part of AFD, an automatic file distribution program.
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

#ifndef __ftpdefs_h
#define __ftpdefs_h

#ifndef MAX_FILENAME_LENGTH
# define MAX_FILENAME_LENGTH             256
#endif
#ifndef SUCCESS
# define SUCCESS                         0
#endif
#ifndef INCORRECT
# define INCORRECT                       -1
#endif

#ifndef MAX_RET_MSG_LENGTH
# define MAX_RET_MSG_LENGTH              4096
#endif
#define MAX_DATA_CONNECT_RETRIES         3
#define DEFAULT_FTP_PORT                 21
#define MAX_FTP_DATE_LENGTH              15

#ifdef _BLOCK_MODE
# define DATA_BLOCK                      128
# define END_BLOCK                       64
#endif

/* Definitions for the different FTP modes (PORT, PASV, EPRT, EPSV). */
#define PASSIVE_MODE                     1
#define ACTIVE_MODE                      2
#define EXTENDED_MODE                    4
#define ALLOW_DATA_REDIRECT              8

/* Definitions for the ftp_data() function. */
#define DATA_WRITE                       1
#define DATA_READ                        2

/* Definitions for the ftp_list() function. */
#define LIST_CMD                         1
#define NLIST_CMD                        2
#define ONE_FILENAME                     4
#define BUFFERED_LIST                    8
#ifdef WITH_SSL
# define ENCRYPT_DATA                    16
#endif
#define MLSD_CMD                         32
#define FLIST_CMD                        64
#define FNLIST_CMD                       128
#define SLIST_CMD                        256

/* FTP options available. */
#define FTP_OPTION_FEAT                  1
#define FTP_OPTION_MDTM                  2
#define FTP_OPTION_SIZE                  4
#define FTP_OPTION_MLST                  8
#define FTP_OPTION_MLST_MODIFY           16
#define FTP_OPTION_MLST_SIZE             32
#define FTP_OPTION_MLST_TYPE             64
#define FTP_OPTION_MLST_PERM             128
#define FTP_OPTION_UTF8                  256

struct ftp_connect_data
       {
          unsigned int   ftp_options;
          unsigned short data_port;
       };

/* Function prototypes. */
extern int  ftp_account(char *),
#ifdef _BLOCK_MODE
            ftp_block_write(char *, unsigned short, char),
            ftp_mode(char),
            ftp_open(char *, int),
#endif
            ftp_cd(char *, int, char *, char *),
            ftp_chmod(char *, char *),
            ftp_close_data(void),
#ifdef WITH_SSL
            ftp_connect(char *, int, int, int, int),
            ftp_auth_data(void),
            ftp_ssl_auth(int, int),
            ftp_ssl_init(char),
            ftp_ssl_disable_ctrl_encrytion(void),
#else
            ftp_connect(char *, int),
#endif
            ftp_data(char *, off_t, int, int, int, int, char *, char *),
            ftp_data_port(void),
            ftp_date(char *, time_t *),
            ftp_dele(char *),
            ftp_exec(char *, char *),
            ftp_feat(unsigned int *),
            ftp_get_reply(void),
            ftp_idle(int),
            ftp_keepalive(void),
            ftp_list(int, int, ...),
            ftp_mlst(char *, time_t *),
            ftp_move(char *, char *, int, int, char *, char *),
            ftp_noop(void),
            ftp_pass(char *),
            ftp_pwd(void),
            ftp_quit(void),
            ftp_read(char *, int),
#ifdef WITH_SENDFILE
            ftp_sendfile(int, off_t *, int),
#endif
            ftp_set_date(char *, time_t),
            ftp_set_utf8_on(void),
            ftp_size(char *, off_t *),
            ftp_type(char),
            ftp_user(char *),
            ftp_write(char *, char *, int);
#endif /* __ftpdefs_h */
