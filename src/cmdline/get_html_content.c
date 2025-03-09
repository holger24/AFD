/*
 *  get_html_content.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2024, 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#include "ahtml_listdefs.h"

DESCR__S_M3 
/*
 ** NAME
 **   get_html_content - gets the content of the given URL
 **
 ** SYNOPSIS
 **   void get_html_content(char *html_content_filename)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.03.2024 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>   /* fopen(), fseeko(), fclose()                      */
#include <stdlib.h>  /* exit(), malloc(), free()                         */
#include <string.h>  /* strerror()                                       */
#include <errno.h>
#include "httpdefs.h"

/* Global variables. */
int          simulation_mode = NO;
unsigned int special_flag = 0;

/* External global variables. */
extern int  sys_log_fd;
extern char msg_str[];


/*########################## get_html_content() #########################*/
void
get_html_content(char *html_content_filename, struct data *p_db)
{
   int   features = 0,
         listing_complete,
         status;
#ifdef _WITH_EXTRA_CHECK
   char  etag[MAX_EXTRA_LS_DATA_LENGTH + 1];
#endif
   char  *listbuffer = NULL;
   off_t bytes_buffered,
         content_length;
   FILE  *fp;

#ifdef WITH_SSL
   if (p_db->strict == YES)
   {
      features |= PROT_OPT_TLS_STRICT_VERIFY;
   }
   if (p_db->legacy_renegotiation == YES)
   {
      features |= PROT_OPT_TLS_LEGACY_RENEGOTIATION;
   }
   if (p_db->no_expect == YES)
#endif
   {
      features |= PROT_OPT_NO_EXPECT;
   }

   status = http_connect(p_db->hostname, p_db->proxy_name, p_db->port,
                         p_db->user, p_db->password, 0, 0, features,
#ifdef WITH_SSL
                         0, SERVICE_NONE, "", p_db->tls_auth,
#endif
                         p_db->sndbuf_size, p_db->rcvbuf_size,
                         p_db->verbose, YES);
   if (status != SUCCESS)
   {
      if (p_db->proxy_name[0] == '\0')
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   "HTTP connection to %s at port %d failed (%d).",
                   p_db->hostname, p_db->port, status);
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, msg_str,
                   "HTTP connection to HTTP proxy %s at port %d failed (%d).",
                   p_db->proxy_name, p_db->port, status);
      }
      exit(CONNECT_ERROR);
   }

   if (p_db->verbose)
   {
      trans_log(INFO_SIGN, NULL, 0, NULL, NULL,
                "Opened HTTP connection to %s:%d.\n",
                p_db->hostname, p_db->port);
   }
   if ((fp = fopen(html_content_filename, "w")) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not fopen() `%s' : %s (%s %d)\n",
                html_content_filename, strerror(errno), __FILE__, __LINE__);
      http_quit();
      exit(INCORRECT);
   }

   do
   {
      bytes_buffered = 0;
      content_length = -1;
#ifdef _WITH_EXTRA_CHECK
      etag[0] = '\0';
#endif
      if (((status = http_get(p_db->remote_dir,
                              (p_db->index_file == NULL) ? "" : p_db->index_file,
                              NULL,
#ifdef _WITH_EXTRA_CHECK
                              etag,
#endif
                              &content_length, 0)) != SUCCESS) &&
                   (status != CHUNKED))
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                   (status == INCORRECT) ? NULL : msg_str,
                   "Failed to open remote directory %s (%d).",
                   p_db->remote_dir, status);
         http_quit();
         exit(eval_timeout(OPEN_REMOTE_ERROR));
      }
      listing_complete = YES;

      if (status == SUCCESS)
      {
         int read_length;

         if (content_length > MAX_HTTP_DIR_BUFFER)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                      "Directory buffer length is only for %d bytes, remote system wants to send %ld bytes. If needed increase MAX_HTTP_DIR_BUFFER.",
#else
                      "Directory buffer length is only for %d bytes, remote system wants to send %lld bytes. If needed increase MAX_HTTP_DIR_BUFFER.",
#endif
                      MAX_HTTP_DIR_BUFFER, (pri_off_t)content_length);
            http_quit();
            exit(ALLOC_ERROR);
         }
         else if (content_length == 0)
              {
                 content_length = MAX_HTTP_DIR_BUFFER;
              }

         if ((listbuffer = malloc(content_length + 1)) == NULL)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
#if SIZEOF_OFF_T == 4
                      "Failed to malloc() %ld bytes : %s (%s %d)\n",
#else
                      "Failed to malloc() %lld bytes : %s (%s %d)\n",
#endif
                      (pri_off_t)(content_length + 1), strerror(errno));
            http_quit();
            exit(ALLOC_ERROR);
         }
         do
         {
            if ((content_length - (bytes_buffered + p_db->blocksize)) >= 0)
            {
               read_length = p_db->blocksize;
            }
            else
            {
               read_length = content_length - bytes_buffered;
            }
            if (read_length > 0)
            {
               if ((status = http_read(&listbuffer[bytes_buffered],
                                       read_length)) == INCORRECT)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                            (status > 0) ? msg_str : NULL,
                            "Failed to read from remote content for %s (%d)",
                            p_db->remote_dir, status);
                  free(listbuffer);
                  http_quit();
                  (void)fclose(fp);
                  exit(eval_timeout(READ_REMOTE_ERROR));
               }
               else if (status > 0)
                    {
                       if (fwrite(&listbuffer[bytes_buffered], 1, status, fp) != status)
                       {
                          (void)rec(sys_log_fd, ERROR_SIGN,
                                    "Failed to fwrite() %d bytes : %s (%s %d)\n",
                                    status, strerror(errno), __FILE__, __LINE__);
                          free(listbuffer);
                          http_quit();
                          (void)fclose(fp);
                          exit(INCORRECT);
                       }
                       bytes_buffered += status;
                       if (bytes_buffered == content_length)
                       {
                          status = 0;
                       }
                       else if (bytes_buffered > content_length)
                            {
                               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                                          "Maximum directory buffer length (%ld bytes) reached.",
#else
                                          "Maximum directory buffer length (%lld bytes) reached.",
#endif
                                          (pri_off_t)content_length);
                               status = 0;
                            }
                    }
            }
            else
            {
               status = 0;
            }
         } while (status != 0);
      }
      else /* status == CHUNKED */
      {
         int  chunksize;
         char *chunkbuffer = NULL;

         chunksize = p_db->blocksize + 4;
         if ((chunkbuffer = malloc(chunksize)) == NULL)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                     "Failed to malloc() %d bytes : %s (%s %d)\n",
                     chunksize, strerror(errno), __FILE__, __LINE__);
            http_quit();
            free(listbuffer);
            (void)fclose(fp);
            exit(ALLOC_ERROR);
         }
         do
         {
            if ((status = http_chunk_read(&chunkbuffer,
                                          &chunksize)) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL,
                         (status == INCORRECT) ? NULL : msg_str,
                         "Failed to read from remote directory listing for %s",
                         p_db->remote_dir);
               http_quit();
               free(chunkbuffer);
               (void)fclose(fp);
               exit(eval_timeout(READ_REMOTE_ERROR));
            }
            else if (status > 0)
                 {
                    if (listbuffer == NULL)
                    {
                       if ((listbuffer = malloc(status)) == NULL)
                       {
                          (void)rec(sys_log_fd, ERROR_SIGN,
#if SIZEOF_OFF_T == 4
                                    "Failed to malloc() %ld bytes : %s (%s %d)\n",
#else
                                    "Failed to malloc() %lld bytes : %s (%s %d)\n",
#endif
                                    (pri_off_t)(content_length + 1), strerror(errno));
                          http_quit();
                          free(chunkbuffer);
                          exit(ALLOC_ERROR);
                       }
                    }
                    else
                    {
                       if (bytes_buffered > MAX_HTTP_DIR_BUFFER)
                       {
                          trans_log(ERROR_SIGN, __FILE__, __LINE__, NULL, NULL,
#if SIZEOF_OFF_T == 4
                                    "Directory length buffer is only for %d bytes, remote system wants to send %ld bytes. If needed increase MAX_HTTP_DIR_BUFFER.",
#else
                                    "Directory length buffer is only for %d bytes, remote system wants to send %lld bytes. If needed increase MAX_HTTP_DIR_BUFFER.",
#endif
                                    MAX_HTTP_DIR_BUFFER,
                                    (pri_off_t)content_length);
                          http_quit();
                          free(listbuffer);
                          free(chunkbuffer);
                          exit(ALLOC_ERROR);
                       }
                       if ((listbuffer = realloc(listbuffer,
                                                 bytes_buffered + status)) == NULL)
                       {
                          (void)rec(sys_log_fd, ERROR_SIGN,
#if SIZEOF_OFF_T == 4
                                    "Failed to realloc() %ld bytes : %s (%s %d)\n",
#else
                                    "Failed to realloc() %lld bytes : %s (%s %d)\n",
#endif
                                    (pri_off_t)(bytes_buffered + status),
                                    strerror(errno), __FILE__, __LINE__);
                           free(chunkbuffer);
                           http_quit();
                           exit(ALLOC_ERROR);
                       }
                    }
                    (void)memcpy(&listbuffer[bytes_buffered],
                                 chunkbuffer, status);
                    if (fwrite(chunkbuffer, 1, status, fp) != status)
                    {
                       (void)rec(sys_log_fd, ERROR_SIGN,
                                 "Failed to fwrite() %d bytes : %s (%s %d)\n",
                                 status, strerror(errno), __FILE__, __LINE__);
                       free(chunkbuffer);
                       http_quit();
                       (void)fclose(fp);
                       exit(INCORRECT);
                    }
                    bytes_buffered += status;
                 }
         } while (status != HTTP_LAST_CHUNK);

         free(chunkbuffer);

         if ((listbuffer = realloc(listbuffer, bytes_buffered + 1)) == NULL)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
#if SIZEOF_OFF_T == 4
                      "Failed to realloc() %ld bytes : %s (%s %d)\n",
#else
                      "Failed to realloc() %lld bytes : %s (%s %d)\n",
#endif
                      (pri_off_t)(bytes_buffered + status),
                      strerror(errno), __FILE__, __LINE__);
            free(chunkbuffer);
            http_quit();
            exit(ALLOC_ERROR);
         }
      }

      if (bytes_buffered > 0)
      {
         listbuffer[bytes_buffered] = '\0';
         if (eval_html_dir_list(listbuffer, bytes_buffered, 0,
                                (p_db->special_flag & HREF_SEARCH_ONLY) ? YES : NO,
                                &listing_complete, p_db) != SUCCESS)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "eval_html_dir_list() error. (%s %d)\n",
                      __FILE__, __LINE__);
         }

      }
      free(listbuffer);
      listbuffer = NULL;
   } while (listing_complete == NO);

   http_quit();
   (void)fclose(fp);

   return;
}
