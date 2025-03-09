/*
 *  httpdefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __httpdefs_h
#define __httpdefs_h

#include "commondefs.h"

#define DEFAULT_HTTP_PORT           80
#define DEFAULT_HTTPS_PORT          443
#define MAX_HTTP_HEADER_BUFFER      256
#define MAX_HTTP_DIR_BUFFER         10485760
#define MAX_EXTRA_RETURN_STR_LENGTH 80
#define DEFAULT_AWS4_LIST_VERSION   '2'
#define MAX_AWS4_PARAMETER_LENGTH   63 /* delimiter=%2F&list-type=2&continuation-token=&max-keys=&prefix= */
#define AWS4_MAX_KEYS               ((MAX_HTTP_DIR_BUFFER - (1400 + MAX_INT_LENGTH)) / (1440 + MAX_OFF_T_LENGTH))
/* Note: 1400 is just a rough estimate of the <?xml ...> part and the */
/*       <ListBucketResult> part, which appears once.                 */
/*       1440 is a rough estimate for each list element <Contents>    */
/* So for a MAX_HTTP_DIR_BUFFER of 1 MiB we should be able to hold    */
/* 7176 elements/keys.                                                */

#define PERMANENT_DISCONNECT                 -10
#define CONNECTION_REOPENED                  99
#define WWW_AUTHENTICATE_UNKNOWN             9
#define WWW_AUTHENTICATE_BASIC               10
#define WWW_AUTHENTICATE_DIGEST_MD5          11
#define WWW_AUTHENTICATE_DIGEST_MD5_S        12
#define WWW_AUTHENTICATE_DIGEST_SHA256       13
#define WWW_AUTHENTICATE_DIGEST_SHA256_S     14
#define WWW_AUTHENTICATE_DIGEST_SHA512_256   15
#define WWW_AUTHENTICATE_DIGEST_SHA512_256_S 16
#define WWW_AUTHENTICATE_DIGEST_UNKNOWN      17

#define NOTHING_TO_FETCH            3
#define CHUNKED                     2
#define HTTP_LAST_CHUNK             0

#define HTTP_PROXY_NAME             "(none)"
#define DEFAULT_REGION_STR          "eu-central-1"
#define SHA256_EMPTY_PAYLOAD        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
#define SHA256_EMPTY_PAYLOAD_LENGTH (sizeof(SHA256_EMPTY_PAYLOAD) - 1)

/* Different options that the remote HTTP server overs. */
#define HTTP_OPTION_GET             1
#define HTTP_OPTION_PUT             2
#define HTTP_OPTION_HEAD            4
#define HTTP_OPTION_MOVE            8
#define HTTP_OPTION_POST            16
#define HTTP_OPTION_DELETE          32
#define HTTP_OPTION_OPTIONS         64

/* Digest options. */
#define QOP_AUTH                    1
#define QOP_AUTH_INT                2
#define HASH_USERNAME               4

struct http_message_reply
       {
          char          msg_header[MAX_HTTP_HEADER_BUFFER];
          char          hostname[MAX_REAL_HOSTNAME_LENGTH + 1];
          char          http_proxy[MAX_REAL_HOSTNAME_LENGTH + 1];
          char          user[MAX_USER_NAME_LENGTH + 1];
          char          passwd[MAX_USER_NAME_LENGTH + 1];
          char          region[MAX_REAL_HOSTNAME_LENGTH + 1];
          char          marker[MAX_FILENAME_LENGTH + 1];
          char          service[3];
          char          tls_auth;
          unsigned char service_type;
          unsigned char listobject_version;
#ifdef _WITH_EXTRA_CHECK
          char          http_etag[MAX_EXTRA_LS_DATA_LENGTH + 1];
#endif
          char          *filename;             /* Content-Disposition */
          off_t         content_length;
          time_t        date;
          char          *authorization;
          char          *realm;                /* HTTP digest auth */
          char          *nonce;                /* HTTP digest auth */
          char          *opaque;               /* HTTP digest auth */
          unsigned int  http_options;
          unsigned int  http_options_not_working;
          unsigned int  fra_options;
          unsigned int  digest_options;
          int           port;
          int           header_length;
          int           marker_length;
          int           http_version;
          int           features;
          int           www_authenticate;
          int           bytes_buffered;
          int           bytes_read;
          int           sndbuf_size;
          int           rcvbuf_size;
          int           retries;
          char          debug;
          unsigned char auth_type;
          char          chunked;
          char          close;
          char          free;
#ifdef _WITH_EXTRA_CHECK
          char          http_weak_etag;
#endif
       };

/* Macros for HTML. */
#define STORE_HTML_STRING(html_str, str_len, max_str_length, end_char)\
        {\
           str_len = 0;\
           while ((str_len < ((max_str_length) - 1)) && (*ptr != end_char) &&\
                  (*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))\
           {\
              if (*ptr == '&')\
              {\
                 ptr++;\
                 if ((*(ptr + 1) == 'u') && (*(ptr + 2) == 'm') &&\
                     (*(ptr + 3) == 'l') && (*(ptr + 4) == ';'))\
                 {\
                    switch (*ptr)\
                    {\
                       case 'a': (html_str)[str_len++] = 228;\
                                 break;\
                       case 'A': (html_str)[str_len++] = 196;\
                                 break;\
                       case 'e': (html_str)[str_len++] = 235;\
                                 break;\
                       case 'E': (html_str)[str_len++] = 203;\
                                 break;\
                       case 'i': (html_str)[str_len++] = 239;\
                                 break;\
                       case 'I': (html_str)[str_len++] = 207;\
                                 break;\
                       case 'o': (html_str)[str_len++] = 246;\
                                 break;\
                       case 'O': (html_str)[str_len++] = 214;\
                                 break;\
                       case 'u': (html_str)[str_len++] = 252;\
                                 break;\
                       case 'U': (html_str)[str_len++] = 220;\
                                 break;\
                       case 's': (html_str)[str_len++] = 223;\
                                 break;\
                       case 'y': (html_str)[str_len++] = 255;\
                                 break;\
                       case 'Y': (html_str)[str_len++] = 195;\
                                 break;\
                       default : /* Just ignore it. */\
                                 break;\
                    }\
                    ptr += 5;\
                    continue;\
                 }\
                 else if ((*(ptr + 1) == 'g') && (*(ptr + 2) == 'r') &&\
                          (*(ptr + 3) == 'a') && (*(ptr + 4) == 'v') &&\
                          (*(ptr + 5) == 'e') && (*(ptr + 6) == ';'))\
                      {\
                         switch (*ptr)\
                         {\
                            case 'a': (html_str)[str_len++] = 224;\
                                      break;\
                            case 'A': (html_str)[str_len++] = 192;\
                                      break;\
                            case 'e': (html_str)[str_len++] = 232;\
                                      break;\
                            case 'E': (html_str)[str_len++] = 200;\
                                      break;\
                            case 'o': (html_str)[str_len++] = 242;\
                                      break;\
                            case 'O': (html_str)[str_len++] = 210;\
                                      break;\
                            default : /* Just ignore it. */\
                                      break;\
                         }\
                         ptr += 7;\
                         continue;\
                      }\
                 else if ((*(ptr + 1) == 'a') && (*(ptr + 2) == 'c') &&\
                          (*(ptr + 3) == 'u') && (*(ptr + 4) == 't') &&\
                          (*(ptr + 5) == 'e') && (*(ptr + 6) == ';'))\
                      {\
                         switch (*ptr)\
                         {\
                            case 'a': (html_str)[str_len++] = 225;\
                                      break;\
                            case 'A': (html_str)[str_len++] = 193;\
                                      break;\
                            case 'e': (html_str)[str_len++] = 233;\
                                      break;\
                            case 'E': (html_str)[str_len++] = 201;\
                                      break;\
                            case 'o': (html_str)[str_len++] = 243;\
                                      break;\
                            case 'O': (html_str)[str_len++] = 211;\
                                      break;\
                            default : /* Just ignore it. */\
                                      break;\
                         }\
                         ptr += 7;\
                         continue;\
                      }\
                 else if ((*(ptr + 1) == 'c') && (*(ptr + 2) == 'i') &&\
                          (*(ptr + 3) == 'r') && (*(ptr + 4) == 'c') &&\
                          (*(ptr + 5) == ';'))\
                      {\
                         switch (*ptr)\
                         {\
                            case 'a': (html_str)[str_len++] = 226;\
                                      break;\
                            case 'A': (html_str)[str_len++] = 194;\
                                      break;\
                            case 'e': (html_str)[str_len++] = 234;\
                                      break;\
                            case 'E': (html_str)[str_len++] = 202;\
                                      break;\
                            case 'o': (html_str)[str_len++] = 244;\
                                      break;\
                            case 'O': (html_str)[str_len++] = 212;\
                                      break;\
                            default : /* Just ignore it. */\
                                      break;\
                         }\
                         ptr += 6;\
                         continue;\
                      }\
                 else if ((*(ptr + 1) == 'c') && (*(ptr + 2) == 'e') &&\
                          (*(ptr + 3) == 'd') && (*(ptr + 4) == 'i') &&\
                          (*(ptr + 5) == 'l') && (*(ptr + 6) == ';'))\
                      {\
                         switch (*ptr)\
                         {\
                            case 'c': (html_str)[str_len++] = 231;\
                                      break;\
                            case 'C': (html_str)[str_len++] = 199;\
                                      break;\
                            default : /* Just ignore it. */\
                                      break;\
                         }\
                         ptr += 7;\
                         continue;\
                      }\
                 else if ((*(ptr + 1) == 't') && (*(ptr + 2) == 'i') &&\
                          (*(ptr + 3) == 'l') && (*(ptr + 4) == 'd') &&\
                          (*(ptr + 5) == 'e') && (*(ptr + 6) == ';'))\
                      {\
                         switch (*ptr)\
                         {\
                            case 'n': (html_str)[str_len++] = 241;\
                                      break;\
                            case 'N': (html_str)[str_len++] = 209;\
                                      break;\
                            case 'o': (html_str)[str_len++] = 245;\
                                      break;\
                            default : /* Just ignore it. */\
                                      break;\
                         }\
                         ptr += 7;\
                         continue;\
                      }\
                 else if ((*ptr == 's') && (*(ptr + 1) == 'z') &&\
                          (*(ptr + 2) == 'l') && (*(ptr + 3) == 'i') &&\
                          (*(ptr + 4) == 'g') && (*(ptr + 5) == ';'))\
                      {\
                         (html_str)[str_len++] = 223;\
                         ptr += 6;\
                         continue;\
                      }\
                 else if ((*ptr == 'a') && (*(ptr + 1) == 'm') &&\
                          (*(ptr + 2) == 'p') && (*(ptr + 3) == ';'))\
                      {\
                         (html_str)[str_len++] = 38;\
                         ptr += 4;\
                         continue;\
                      }\
                 else if ((*ptr == 'd') && (*(ptr + 1) == 'e') &&\
                          (*(ptr + 2) == 'g') && (*(ptr + 3) == ';'))\
                      {\
                         (html_str)[str_len++] = 176;\
                         ptr += 4;\
                         continue;\
                      }\
                 else if ((*ptr == 'g') && (*(ptr + 1) == 't') &&\
                          (*(ptr + 2) == ';'))\
                      {\
                         (html_str)[str_len++] = '>';\
                         ptr += 3;\
                         continue;\
                      }\
                 else if ((*ptr == 'l') && (*(ptr + 1) == 't') &&\
                          (*(ptr + 2) == ';'))\
                      {\
                         (html_str)[str_len++] = '<';\
                         ptr += 3;\
                         continue;\
                      }\
                      else\
                      {\
                         while ((*ptr != ';') && (*ptr != '<') &&\
                                (*ptr != '\n') && (*ptr != '\r') &&\
                                (*ptr != '\0'))\
                         {\
                            ptr++;\
                         }\
                         if (*ptr != ';')\
                         {\
                            break;\
                         }\
                      }\
              }\
              (html_str)[str_len] = *ptr;\
              str_len++; ptr++;\
           }\
           (html_str)[str_len] = '\0';\
        }
#define STORE_HTML_DATE()\
        {\
           int i = 0,\
               space_counter = 0;\
\
           while ((i < (MAX_FILENAME_LENGTH - 1)) && (*ptr != '<') &&\
                  (*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))\
           {\
              if (*ptr == ' ')\
              {\
                 if (space_counter == 1)\
                 {\
                    while (*ptr == ' ')\
                    {\
                       ptr++;\
                    }\
                    break;\
                 }\
                 space_counter++;\
              }\
              if (*ptr == '&')\
              {\
                 ptr++;\
                 if ((*(ptr + 1) == 'u') && (*(ptr + 2) == 'm') &&\
                     (*(ptr + 3) == 'l') && (*(ptr + 4) == ';'))\
                 {\
                    switch (*ptr)\
                    {\
                       case 'a': date_str[i++] = 228;\
                                 break;\
                       case 'A': date_str[i++] = 196;\
                                 break;\
                       case 'o': date_str[i++] = 246;\
                                 break;\
                       case 'O': date_str[i++] = 214;\
                                 break;\
                       case 'u': date_str[i++] = 252;\
                                 break;\
                       case 'U': date_str[i++] = 220;\
                                 break;\
                       case 's': date_str[i++] = 223;\
                                 break;\
                       default : /* Just ignore it. */\
                                 break;\
                    }\
                    ptr += 5;\
                    continue;\
                 }\
                 else\
                 {\
                    while ((*ptr != ';') && (*ptr != '<') &&\
                           (*ptr != '\n') && (*ptr != '\r') &&\
                           (*ptr != '\0'))\
                    {\
                       ptr++;\
                    }\
                    if (*ptr != ';')\
                    {\
                       break;\
                    }\
                 }\
              }\
              date_str[i] = *ptr;\
              i++; ptr++;\
           }\
           date_str[i] = '\0';\
        }

/* Function prototypes. */
#ifdef WITH_SSL
extern int  aws_cmd(char *, char *, char *, char *, struct http_message_reply *),
            aws4_put_authentication(char *, char *, off_t, char *, char *,
                                    struct http_message_reply *);
#endif
extern int  basic_authentication(struct http_message_reply *),
            digest_authentication(char *, char *, char *,
                                  struct http_message_reply *),
            http_connect(char *, char *, int, char *, char *, unsigned char,
                         unsigned int, int,
#ifdef WITH_SSL
                         unsigned char, unsigned char, char *, char,
#endif
                         int, int, char, int),
            http_del(char *, char *),
#ifdef _WITH_EXTRA_CHECK
            http_get(char *, char *, char *, char *, off_t *, off_t),
#else
            http_get(char *, char *, char *, off_t *, off_t),
#endif
            http_head(char *, char *, off_t *, time_t *),
            http_noop(char *),
            http_options(char *),
            http_put(char *, char *, char *, off_t, char *, int),
            http_put_response(void),
            http_read(char *, int),
            http_chunk_read(char **, int *),
            http_version(void),
            http_write(char *, char *, int);
extern void http_quit(void),
            http_reset_authentication(int),
            http_set_marker(char *, int);

#endif /* __httpdefs_h */
