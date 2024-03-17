/*
 *  eval_html_dir_list.c - Part of AFD, an automatic file distribution
 *                         program.
 *  Copyright (c) 2006 - 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   eval_html_dir_list - retrieves filename, size and date
 **
 ** SYNOPSIS
 **   int eval_html_dir_list(char          *html_buffer,
 **                          off_t         bytes_buffered,
 **                          unsigned char list_version,
 **                          int           href_search_only,
 **                          int           *listing_complete,
 **                          struct data   *p_db)
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
 **   08.03.2024 H.Kiehl Copied from get_remote_file_names_http.c.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                /* strerror(), memmove()              */
#include <stdlib.h>                /* malloc(), realloc(), free()        */
#include <ctype.h>                 /* isdigit()                          */
#include <errno.h>
#include "httpdefs.h"


/* Local function prototypes. */
static off_t convert_size(char *, off_t *);
static int   href_list(char *, off_t, struct data *);


/*######################## eval_html_dir_list() #########################*/
int
eval_html_dir_list(char          *html_buffer,
                   off_t         bytes_buffered,
                   unsigned char list_version,
                   int           href_search_only,
                   int           *listing_complete,
                   struct data   *p_db)
{
   int  status = SUCCESS;
   char *ptr;

   if (listing_complete != NULL)
   {
      *listing_complete = YES;
   }
   if (href_search_only == YES)
   {
      status = href_list(html_buffer, bytes_buffered, p_db);
      if ((ptr = llposi(html_buffer, bytes_buffered,
                        "<IsTruncated>", 13)) != NULL)
      {
         /* true */
         if ((*(ptr - 1) == 't') && (*ptr == 'r') &&
             (*(ptr + 1) == 'u') && (*(ptr + 2) == 'e') &&
             (*(ptr + 3) == '<'))
         {
            if (listing_complete != NULL)
            {
               *listing_complete = NO;
            }
         }
      }
      return(status);
   }
   if ((ptr = llposi(html_buffer, (size_t)bytes_buffered, "<h1>", 4)) == NULL)
   {
      if ((ptr = llposi(html_buffer, (size_t)bytes_buffered, "<PRE>", 5)) == NULL)
      {
         if ((ptr = llposi(html_buffer, (size_t)bytes_buffered,
                           "<?xml version=\"", 15)) == NULL)
         {
            if ((ptr = llposi(html_buffer, (size_t)bytes_buffered,
                              "<div id=\"downloadLinkArea\">",
                              27)) == NULL)
            {
               if ((ptr = llposi(html_buffer, (size_t)bytes_buffered,
                                 "<div id=\"contentDiv\">",
                                 21)) == NULL)
               {
                  status = href_list(html_buffer, bytes_buffered, p_db);
               }
               else
               {
                  int    exact_date = -1,
                         file_name_length = -1;
                  off_t  exact_size,
                         file_size = -1;
                  time_t file_mtime = -1;
                  char   date_str[MAX_FILENAME_LENGTH],
                         *end_ptr = html_buffer + bytes_buffered,
                         file_name[MAX_FILENAME_LENGTH];

                  while ((*ptr == '\n') || (*ptr == '\r'))
                  {
                     ptr++;
                  }

                  while ((ptr = llposi(ptr, (end_ptr - ptr),
                                       "<a href=\"", 9)) != NULL)
                  {
                     ptr--;
                     file_name_length = 0;

                     /*
                      * If '<a href="' starts with a / lets take the
                      * complete path as file name. Reason is that
                      * some websites specify a different path. In
                      * httpcmd.c and gf_http.c we must make sure
                      * that we take out the path again.
                      * For now lets just do it for this type of
                      * listing and see how it works out, before
                      * using this method on the other listing
                      * types below.
                      */
                     if (*ptr == '/')
                     {
                        char end_char,
                             *tmp_ptr = ptr;

                        /* First determine end character. It can */
                        /* either be < or ".                     */
                        while ((*ptr != '<') && (*ptr != '"') &&
                               (*ptr != '\n') && (*ptr != '\r') &&
                               (*ptr != '\0'))
                        {
                           ptr++;
                        }
                        if ((*ptr == '<') || (*ptr == '"'))
                        {
                           end_char = *ptr;
                           ptr = tmp_ptr;
                           STORE_HTML_STRING(file_name, file_name_length,
                                             MAX_FILENAME_LENGTH, end_char);
                            if (*ptr == '<')
                            {
                               if ((file_name_length > 1) &&
                                   (*(ptr - 1) == ' '))
                               {
                                  file_name_length--;
                                  file_name[file_name_length] = '\0';
                               }
                               while ((*ptr != '"') && (*ptr != '\n') &&
                                      (*ptr != '\r') && (*ptr != '\0'))
                               {
                                  ptr++;
                               }
                            }
                            if (*ptr == '"')
                            {
                               ptr++;
                               if (*ptr == '>')
                               {
                                  ptr++;
                                  while ((*ptr != '<') && (*ptr != '\n') &&
                                         (*ptr != '\r') && (*ptr != '\0'))
                                  {
                                     ptr++;
                                  }
                                  if (*ptr == '<')
                                  {
                                     while (*ptr == '<')
                                     {
                                        ptr++;
                                        while ((*ptr != '>') &&
                                               (*ptr != '\n') &&
                                               (*ptr != '\r') &&
                                                  (*ptr != '\0'))
                                     {
                                           ptr++;
                                        }
                                        if (*ptr == '>')
                                        {
                                           ptr++;
                                           while (*ptr == ' ')
                                           {
                                              ptr++;
                                           }
                                        }
                                     }
                                  }
                                  if ((*ptr != '\n') && (*ptr != '\r') &&
                                      (*ptr != '\0'))
                                  {
                                     while (*ptr == ' ')
                                     {
                                        ptr++;
                                     }

                                     /* Store date string. */
                                     if ((isdigit((int)(*ptr)) != 0) &&
                                         (isdigit((int)(*(ptr + 15))) != 0) &&
                                         (*(ptr + 16) == ' '))
                                     {
                                        int i = 0;

                                        memcpy(date_str, ptr, 16);
                                        date_str[16] = '\0';
                                        file_mtime = datestr2unixtime(date_str,
                                                                      &exact_date);
                                        ptr += 16;
                                        while (*ptr == ' ')
                                        {
                                           ptr++;
                                        }
                                        while ((i < MAX_FILENAME_LENGTH) &&
                                               (isdigit((int)(*ptr)) != 0))
                                        {
                                           date_str[i] = *ptr;
                                           ptr++; i++;
                                        }
                                        if (i > 0)
                                        {
                                           date_str[i] = '\0';
                                           file_size = (off_t)str2offt(date_str,
                                                                       NULL, 10);
                                           exact_size = 1;
                                        }
                                     }
                                  }
                                  else
                                  {
                                     break;
                                  }
                               }
                               else
                               {
                                  break;
                               }
                            }
                            else
                            {
                               break;
                            }
                        }
                        else
                        {
                           break;
                        }
                     }
                     else
                     {
                        while ((*ptr != '"') && (*ptr != '\n') &&
                               (*ptr != '\r') && (*ptr != '\0'))
                        {
                           ptr++;
                        }
                        if (*ptr == '"')
                        {
                           ptr++;
                           if (*ptr == '>')
                           {
                              ptr++;
                              STORE_HTML_STRING(file_name, file_name_length,
                                                MAX_FILENAME_LENGTH, '<');
                              if (*ptr == '<')
                              {
                                 while (*ptr == '<')
                                 {
                                    ptr++;
                                    while ((*ptr != '>') && (*ptr != '\n') &&
                                           (*ptr != '\r') && (*ptr != '\0'))
                                    {
                                       ptr++;
                                    }
                                    if (*ptr == '>')
                                    {
                                       ptr++;
                                       while (*ptr == ' ')
                                       {
                                          ptr++;
                                       }
                                    }
                                 }
                              }
                              if ((*ptr != '\n') && (*ptr != '\r') &&
                                  (*ptr != '\0'))
                              {
                                 while (*ptr == ' ')
                                 {
                                    ptr++;
                                 }

                                 /* Store date string. */
                                 if ((isdigit((int)(*ptr)) != 0) &&
                                     (isdigit((int)(*(ptr + 15))) != 0) &&
                                     (*(ptr + 16) == ' '))
                                 {
                                    int i = 0;

                                    memcpy(date_str, ptr, 16);
                                    date_str[16] = '\0';
                                    file_mtime = datestr2unixtime(date_str,
                                                                  &exact_date);
                                    ptr += 16;
                                    while (*ptr == ' ')
                                    {
                                       ptr++;
                                    }
                                    while ((i < MAX_FILENAME_LENGTH) &&
                                           (isdigit((int)(*ptr)) != 0))
                                    {
                                       date_str[i] = *ptr;
                                       ptr++; i++;
                                    }
                                    if (i > 0)
                                    {
                                       date_str[i] = '\0';
                                       file_size = (off_t)str2offt(date_str,
                                                                   NULL, 10);
                                       exact_size = 1;
                                    }
                                 }
                              }
                              else
                              {
                                 break;
                              }
                           }
                           else
                           {
                              break;
                           }
                        }
                        else
                        {
                           break;
                        }
                     }
                     if (p_db->verbose > 0)
                     {
                        (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                      "%s mtime=%ld exact=%d size=%ld exact=%ld (%s %d)\n",
# else
                                      "%s mtime=%lld exact=%d size=%ld exact=%ld (%s %d)\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                      "%s mtime=%ld exact=%d size=%lld exact=%lld (%s %d)\n",
# else
                                      "%s mtime=%lld exact=%d size=%lld exact=%lld (%s %d)\n",
# endif
#endif
                                      file_name, (pri_time_t)file_mtime,
                                      exact_date, (pri_off_t)file_size,
                                      (pri_off_t)exact_size, __FILE__,
                                      __LINE__);
                     }
                     else
                     {
                        (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                      "%s mtime=%ld exact=%d size=%ld exact=%ld\n",
# else
                                      "%s mtime=%lld exact=%d size=%ld exact=%ld\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                      "%s mtime=%ld exact=%d size=%lld exact=%lld\n",
# else
                                      "%s mtime=%lld exact=%d size=%lld exact=%lld\n",
# endif
#endif
                                      file_name, (pri_time_t)file_mtime,
                                      exact_date, (pri_off_t)file_size,
                                      (pri_off_t)exact_size);
                     }
                     bytes_buffered = end_ptr - ptr;
                  } /* while "<a href=" */
               }
            }
            else
            {
               int  file_name_length;
               char *end_ptr = html_buffer + bytes_buffered,
                    file_name[MAX_FILENAME_LENGTH];

               while ((*ptr == '\n') || (*ptr == '\r'))
               {
                  ptr++;
               }

               /* Ignore next line. */
               while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
               {
                  ptr++;
               }
               while ((*ptr == '\n') || (*ptr == '\r'))
               {
                  ptr++;
               }

               while ((ptr = llposi(ptr, (end_ptr - ptr),
                                    "<a href=\"", 9)) != NULL)
               {
                  ptr--;
                  file_name_length = 0;
                  if ((*ptr == '.') && (*(ptr + 1) == '/'))
                  {
                     ptr += 2;
                  }

                  /* Store file name. */
                  STORE_HTML_STRING(file_name, file_name_length,
                                    MAX_FILENAME_LENGTH, '"');

                  if (p_db->verbose > 0)
                  {
                     (void)fprintf(stdout,
                                   "%s mtime=-1 exact=%d size=-1 exact=-1 (%s %d)\n",
                                   file_name, DS2UT_NONE, __FILE__, __LINE__);
                  }
                  else
                  {
                     (void)fprintf(stdout,
                                   "%s mtime=-1 exact=%d size=-1 exact=-1\n",
                                   file_name, DS2UT_NONE);
                  }
                  bytes_buffered = end_ptr - ptr;
               } /* while "<a href=" */
            }
         }
         else
         {
            if ((ptr = llposi(ptr, (bytes_buffered - (ptr - html_buffer)),
                              "<IsTruncated>", 13)) == NULL)
            {
               status = href_list(html_buffer, bytes_buffered, p_db);
            }
            else
            {
               int    date_str_length,
                      exact_date = DS2UT_NONE,
                      file_name_length = -1;
               off_t  bytes_buffered_original = bytes_buffered,
                      exact_size,
                      file_size;
               time_t file_mtime;
               char   date_str[MAX_FILENAME_LENGTH],
                      *end_ptr = html_buffer + bytes_buffered,
#ifdef _WITH_EXTRA_CHECK
                      etag[MAX_EXTRA_LS_DATA_LENGTH],
#endif
                      file_name[MAX_FILENAME_LENGTH],
                      size_str[MAX_FILENAME_LENGTH];

               /* true */
               if ((*(ptr - 1) == 't') && (*ptr == 'r') &&
                   (*(ptr + 1) == 'u') && (*(ptr + 2) == 'e') &&
                   (*(ptr + 3) == '<'))
               {
                  if (listing_complete != NULL)
                  {
                     *listing_complete = NO;
                  }
                  ptr += 2;
               }

               ptr = html_buffer;
               while ((ptr = llposi(ptr, (end_ptr - ptr),
                                    "<Contents><Key>", 15)) != NULL)
               {
                  ptr--;
                  file_name_length = 0;
                  while ((file_name_length < MAX_FILENAME_LENGTH) &&
                         (*ptr != '<') && (*ptr != '\r') && (*ptr != '\0'))
                  {
                     file_name[file_name_length] = *ptr;
                     file_name_length++; ptr++;
                  }
                  if (*ptr == '<')
                  {
                     file_name[file_name_length] = '\0';
                     ptr++;
                     /* /Key><LastModified> */
                     if ((*ptr == '/') && (*(ptr + 1) == 'K') &&
                         (*(ptr + 2) == 'e') && (*(ptr + 3) == 'y') &&
                         (*(ptr + 4) == '>') && (*(ptr + 5) == '<') &&
                         (*(ptr + 6) == 'L') && (*(ptr + 7) == 'a') &&
                         (*(ptr + 8) == 's') && (*(ptr + 9) == 't') &&
                         (*(ptr + 10) == 'M') && (*(ptr + 11) == 'o') &&
                         (*(ptr + 12) == 'd') && (*(ptr + 13) == 'i') &&
                         (*(ptr + 14) == 'f') && (*(ptr + 15) == 'i') &&
                         (*(ptr + 16) == 'e') && (*(ptr + 17) == 'd') &&
                         (*(ptr + 18) == '>'))
                     {
                        ptr += 19;
                        date_str_length = 0;
                        while ((date_str_length < MAX_FILENAME_LENGTH) &&
                               (*ptr != '<') && (*ptr != '\r') &&
                               (*ptr != '\0'))
                        {
                           date_str[date_str_length] = *ptr;
                           date_str_length++; ptr++;
                        }
                        if (*ptr == '<')
                        {
                           date_str[date_str_length] = '\0';
                           file_mtime = datestr2unixtime(date_str,
                                                         &exact_date);
                           ptr++;
                           /* /LastModified><ETag> */
                           if ((*ptr == '/') && (*(ptr + 1) == 'L') &&
                               (*(ptr + 2) == 'a') && (*(ptr + 3) == 's') &&
                               (*(ptr + 4) == 't') && (*(ptr + 5) == 'M') &&
                               (*(ptr + 6) == 'o') && (*(ptr + 7) == 'd') &&
                               (*(ptr + 8) == 'i') && (*(ptr + 9) == 'f') &&
                               (*(ptr + 10) == 'i') &&
                               (*(ptr + 11) == 'e') &&
                               (*(ptr + 12) == 'd') &&
                               (*(ptr + 13) == '>') &&
                               (*(ptr + 14) == '<') &&
                               (*(ptr + 15) == 'E') &&
                               (*(ptr + 16) == 'T') &&
                               (*(ptr + 17) == 'a') &&
                               (*(ptr + 18) == 'g') && (*(ptr + 19) == '>'))
                           {
                              ptr += 20;
                              date_str_length = 0;
#ifdef _WITH_EXTRA_CHECK
                              while ((date_str_length < MAX_EXTRA_LS_DATA_LENGTH) &&
                                     (*ptr != '<') && (*ptr != '\r') &&
                                     (*ptr != '\0'))
                              {
                                 etag[date_str_length] = *ptr;
                                 date_str_length++; ptr++;
                              }
#else
                              while ((*ptr != '<') && (*ptr != '\r') &&
                                     (*ptr != '\0'))
                              {
                                 ptr++;
                              }
#endif
                              if (*ptr == '<')
                              {
#ifdef _WITH_EXTRA_CHECK
                                 etag[date_str_length] = '\0';
#endif
                                 ptr++;
                                 /* /ETag><Size> */
                                 if ((*ptr == '/') && (*(ptr + 1) == 'E') &&
                                     (*(ptr + 2) == 'T') &&
                                     (*(ptr + 3) == 'a') &&
                                     (*(ptr + 4) == 'g') &&
                                     (*(ptr + 5) == '>') &&
                                     (*(ptr + 6) == '<') &&
                                     (*(ptr + 7) == 'S') &&
                                     (*(ptr + 8) == 'i') &&
                                     (*(ptr + 9) == 'z') &&
                                     (*(ptr + 10) == 'e') &&
                                     (*(ptr + 11) == '>'))
                                 {
                                    ptr += 12;
                                    date_str_length = 0;

                                    while ((date_str_length < MAX_FILENAME_LENGTH) &&
                                           (*ptr != '<') && (*ptr != '\r') &&
                                           (*ptr != '\0'))
                                    {
                                       size_str[date_str_length] = *ptr;
                                       date_str_length++; ptr++;
                                    }
                                    if (*ptr == '<')
                                    {
                                       size_str[date_str_length] = '\0';
                                       exact_size = convert_size(size_str,
                                                                 &file_size);
                                       if (p_db->verbose > 0)
                                       {
                                          (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                                        "%s mtime=%ld exact=%d size=%ld exact=%ld (%s %d)\n",
# else
                                                        "%s mtime=%lld exact=%d size=%ld exact=%ld (%s %d)\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                                        "%s mtime=%ld exact=%d size=%lld exact=%lld (%s %d)\n",
# else
                                                        "%s mtime=%lld exact=%d size=%lld exact=%lld (%s %d)\n",
# endif
#endif
                                                        file_name,
                                                        (pri_time_t)file_mtime,
                                                        exact_date,
                                                        (pri_off_t)file_size,
                                                        (pri_off_t)exact_size,
                                                        __FILE__, __LINE__);
                                       }
                                       else
                                       {
                                          (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                                        "%s mtime=%ld exact=%d size=%ld exact=%ld\n",
# else
                                                        "%s mtime=%lld exact=%d size=%ld exact=%ld\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                                        "%s mtime=%ld exact=%d size=%lld exact=%lld\n",
# else
                                                        "%s mtime=%lld exact=%d size=%lld exact=%lld\n",
# endif
#endif
                                                        file_name,
                                                        (pri_time_t)file_mtime,
                                                        exact_date,
                                                        (pri_off_t)file_size,
                                                        (pri_off_t)exact_size);
                                       }
                                    }
                                    else
                                    {
                                       trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                                 "eval_html_dir_list", NULL,
                                                 "Unable to store size (length=%d char=%d).",
                                                 file_name_length, (int)*ptr);
                                       if (listing_complete != NULL)
                                       {
                                          *listing_complete = YES;
                                       }
                                       return(INCORRECT);
                                    }
                                 }
                                 else
                                 {
                                    trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                              "eval_html_dir_list", NULL,
                                              "No matching /ETag><Size> found.");
                                    if (listing_complete != NULL)
                                    {
                                       *listing_complete = YES;
                                    }
                                    return(INCORRECT);
                                 }
                              }
                              else
                              {
                                 trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                           "eval_html_dir_list", NULL,
                                           "Unable to store etag (length=%d char=%d).",
                                           file_name_length, (int)*ptr);
                                 if (listing_complete != NULL)
                                 {
                                    *listing_complete = YES;
                                 }
                                 return(INCORRECT);
                              }
                           }
                           else
                           {
                              trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "eval_html_dir_list", NULL,
                                        "No matching /LastModified><ETag> found.");
                              if (listing_complete != NULL)
                              {
                                 *listing_complete = YES;
                              }
                              return(INCORRECT);
                           }
                        }
                        else
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                     "eval_html_dir_list", NULL,
                                     "Unable to store date (length=%d char=%d).",
                                     file_name_length, (int)*ptr);
                           if (listing_complete != NULL)
                           {
                              *listing_complete = YES;
                           }
                           return(INCORRECT);
                        }
                     }
                     else
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "eval_html_dir_list", NULL,
                                  "No matching /Key><LastModified> found.");
                        if (listing_complete != NULL)
                        {
                           *listing_complete = YES;
                        }
                        return(INCORRECT);
                     }
                  }
                  else
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__,
                               "eval_html_dir_list", NULL,
                               "Unable to store file name (length=%d char=%d).",
                               file_name_length, (int)*ptr);
                     if (listing_complete != NULL)
                     {
                        *listing_complete = YES;
                     }
                     return(INCORRECT);
                  }
                  bytes_buffered = end_ptr - ptr;
               } /* while <Contents><Key> */

               if (file_name_length == -1)
               {
                  if (listing_complete != NULL)
                  {
                     *listing_complete = YES;
                  }

                  /* Bucket is empty or we have some new */
                  /* listing type. So check if KeyCount  */
                  /* is zero.                            */
                  if ((ptr = llposi(html_buffer,
                                    (size_t)bytes_buffered_original,
                                    "<KeyCount>0</KeyCount>", 22)) == NULL)
                  {
                     /* No <Contents><Key> found! */
                     return(href_list(html_buffer, bytes_buffered, p_db));
                  }
               }

               if ((listing_complete != NULL) && (*listing_complete == NO))
               {
                  int  marker_name_length;
                  char marker_name[24];

                  if (list_version == '1')
                  {
                     (void)strcpy(marker_name, "<NextMarker>");
                     marker_name_length = 12;
                  }
                  else
                  {
                     (void)strcpy(marker_name, "<NextContinuationToken>");
                     marker_name_length = 23;
                  }
                  if ((ptr = llposi(html_buffer,
                                    (size_t)bytes_buffered_original,
                                    marker_name,
                                    marker_name_length)) != NULL)
                  {
                     ptr--;
                     file_name_length = 0;
                     while ((file_name_length < MAX_FILENAME_LENGTH) &&
                            (*ptr != '<'))
                     {
                        file_name[file_name_length] = *ptr;
                        ptr++; file_name_length++;
                     }
                     file_name[file_name_length] = '\0';
                  }
                  else
                  {
                     if (list_version != '1')
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                  NULL, html_buffer,
                                  "<IsTruncated> is true, but could not locate a <NextContinuationToken>!");
                        *listing_complete = YES;
                        return(INCORRECT);
                     }
                  }
                  http_set_marker(file_name, file_name_length);
               }
            }
         }
      }
      else
      {
         while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
         {
            ptr++;
         }
         while ((*ptr == '\n') || (*ptr == '\r'))
         {
            ptr++;
         }
         if ((*ptr == '<') && (*(ptr + 1) == 'H') && (*(ptr + 2) == 'R'))
         {
            int    exact_date = DS2UT_NONE,
                   file_name_length;
            time_t file_mtime;
            off_t  exact_size,
                   file_size;
            char   date_str[MAX_FILENAME_LENGTH],
                   file_name[MAX_FILENAME_LENGTH],
                   size_str[MAX_FILENAME_LENGTH];

            /* Ignore HR line. */
            while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
            {
               ptr++;
            }
            while ((*ptr == '\n') || (*ptr == '\r'))
            {
               ptr++;
            }

            /* Ignore the two directory lines. */
            while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
            {
               ptr++;
            }
            while ((*ptr == '\n') || (*ptr == '\r'))
            {
               ptr++;
            }
            while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
            {
               ptr++;
            }
            while ((*ptr == '\n') || (*ptr == '\r'))
            {
               ptr++;
            }

            while (*ptr == '<')
            {
               while (*ptr == '<')
               {
                  ptr++;
                  while ((*ptr != '>') && (*ptr != '\n') &&
                         (*ptr != '\r') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  if (*ptr == '>')
                  {
                     ptr++;
                     while (*ptr == ' ')
                     {
                        ptr++;
                     }
                  }
               }

               if ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
               {
                  /* Store file name. */
                  STORE_HTML_STRING(file_name, file_name_length,
                                    MAX_FILENAME_LENGTH, '<');

                  if (*ptr == '<')
                  {
                     while (*ptr == '<')
                     {
                        ptr++;
                        while ((*ptr != '>') && (*ptr != '\n') &&
                               (*ptr != '\r') && (*ptr != '\0'))
                        {
                           ptr++;
                        }
                        if (*ptr == '>')
                        {
                           ptr++;
                           while (*ptr == ' ')
                           {
                              ptr++;
                           }
                        }
                     }
                  }
                  if ((*ptr != '\n') && (*ptr != '\r') &&
                      (*ptr != '\0'))
                  {
                     while (*ptr == ' ')
                     {
                        ptr++;
                     }

                     /* Store date string. */
                     STORE_HTML_DATE();
                     file_mtime = datestr2unixtime(date_str, &exact_date);

                     if (*ptr == '<')
                     {
                        while (*ptr == '<')
                        {
                           ptr++;
                           while ((*ptr != '>') && (*ptr != '\n') &&
                                  (*ptr != '\r') && (*ptr != '\0'))
                           {
                              ptr++;
                           }
                           if (*ptr == '>')
                           {
                              ptr++;
                              while (*ptr == ' ')
                              {
                                 ptr++;
                              }
                           }
                        }
                     }
                     if ((*ptr != '\n') && (*ptr != '\r') &&
                         (*ptr != '\0'))
                     {
                        int str_len;

                        /* Store size string. */
                        STORE_HTML_STRING(size_str, str_len,
                                          MAX_FILENAME_LENGTH, '<');
                        exact_size = convert_size(size_str, &file_size);
                     }
                     else
                     {
                        exact_size = -1;
                        file_size = -1;
                     }
                  }
                  else
                  {
                     file_mtime = -1;
                     exact_size = -1;
                     file_size = -1;
                  }
                  if (p_db->verbose > 0)
                  {
                     (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                   "%s mtime=%ld exact=%d size=%ld exact=%ld (%s %d)\n",
# else
                                   "%s mtime=%lld exact=%d size=%ld exact=%ld (%s %d)\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                   "%s mtime=%ld exact=%d size=%lld exact=%lld (%s %d)\n",
# else
                                   "%s mtime=%lld exact=%d size=%lld exact=%lld (%s %d)\n",
# endif
#endif
                                   file_name, (pri_time_t)file_mtime,
                                   exact_date, (pri_off_t)file_size,
                                   (pri_off_t)exact_size, __FILE__, __LINE__);
                  }
                  else
                  {
                     (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                   "%s mtime=%ld exact=%d size=%ld exact=%ld\n",
# else
                                   "%s mtime=%lld exact=%d size=%ld exact=%ld\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                   "%s mtime=%ld exact=%d size=%lld exact=%lld\n",
# else
                                   "%s mtime=%lld exact=%d size=%lld exact=%lld\n",
# endif
#endif
                                   file_name, (pri_time_t)file_mtime,
                                   exact_date, (pri_off_t)file_size,
                                   (pri_off_t)exact_size);
                  }
               }
               else
               {
                  file_name[0] = '\0';
                  file_mtime = -1;
                  exact_size = -1;
                  file_size = -1;
                  break;
               }

               /* Go to end of line. */
               while ((*ptr != '\n') && (*ptr != '\r') &&
                      (*ptr != '\0'))
               {
                  ptr++;
               }
               while ((*ptr == '\n') || (*ptr == '\r'))
               {
                  ptr++;
               }
            }
         }
         else
         {
            status = href_list(html_buffer, bytes_buffered, p_db);
         }
      }
   }
   else
   {
      int    exact_date = DS2UT_NONE,
             file_name_length;
      time_t file_mtime;
      off_t  exact_size,
             file_size;
      char   date_str[MAX_FILENAME_LENGTH],
             file_name[MAX_FILENAME_LENGTH],
             size_str[MAX_FILENAME_LENGTH];

      while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
      {
         ptr++;
      }
      while ((*ptr == '\n') || (*ptr == '\r'))
      {
         ptr++;
      }
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      if (*ptr == '<')
      {
         /* Table type listing. */
         if ((*(ptr + 1) == 't') && (*(ptr + 2) == 'a') &&
             (*(ptr + 3) == 'b') && (*(ptr + 4) == 'l') &&
             (*(ptr + 5) == 'e') && (*(ptr + 6) == '>'))
         {
            ptr += 7;

            /* Ignore the two heading lines. */
            while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
            {
               ptr++;
            }
            while ((*ptr == '\n') || (*ptr == '\r'))
            {
               ptr++;
            }
            while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
            {
               ptr++;
            }
            while ((*ptr == '\n') || (*ptr == '\r'))
            {
               ptr++;
            }
            if ((*ptr == ' ') && (*(ptr + 1) == ' ') &&
                (*(ptr + 2) == ' ') && (*(ptr + 3) == '<') &&
                (*(ptr + 4) == 't') && (*(ptr + 5) == 'r') &&
                (*(ptr + 6) == '>'))
            {
               ptr += 7;

               /* Ignore the two heading lines. */
               while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
               {
                  ptr++;
               }
               while ((*ptr == '\n') || (*ptr == '\r'))
               {
                  ptr++;
               }
               while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
               {
                  ptr++;
               }
               while ((*ptr == '\n') || (*ptr == '\r'))
               {
                  ptr++;
               }
            }

            /* <tr><td */
            if ((*ptr == '<') && (*(ptr + 1) == 't') &&
                (*(ptr + 2) == 'r') && (*(ptr + 3) == '>') &&
                (*(ptr + 4) == '<') && (*(ptr + 5) == 't') &&
                (*(ptr + 6) == 'd'))
            {
               /* Read line by line. */
               do
               {
                  file_name_length = 0;
                  ptr += 6;
                  while ((*ptr != '>') &&
                         (*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  if (*ptr == '>')
                  {
                     ptr++;
                     while (*ptr == '<')
                     {
                        ptr++;
                        if ((*ptr == 'a') && (*(ptr + 1) == ' ') &&
                            (*(ptr + 2) == 'h') && (*(ptr + 3) == 'r') &&
                            (*(ptr + 4) == 'e') && (*(ptr + 5) == 'f') &&
                            (*(ptr + 6) == '=') && (*(ptr + 7) == '"'))
                        {
                           char *p_start;

                           ptr += 8;
                           p_start = ptr;

                           /* Go to end of href statement and cut out */
                           /* the file name.                          */
                           while ((*ptr != '"') && (*ptr != '\n') &&
                                  (*ptr != '\r') && (*ptr != '\0'))
                           {
                              ptr++;
                           }
                           if (*ptr == '"')
                           {
                              char *tmp_ptr = ptr;

                              ptr--;
                              while ((*ptr != '/') && (ptr != p_start))
                              {
                                 ptr--;
                              }
                              while (*ptr == '/')
                              {
                                 ptr++;
                              }

                              /* Store file name. */
                              STORE_HTML_STRING(file_name, file_name_length,
                                                MAX_FILENAME_LENGTH, '"');

                              ptr = tmp_ptr + 1;
                           }

                           while ((*ptr != '>') && (*ptr != '\n') &&
                                  (*ptr != '\r') && (*ptr != '\0'))
                           {
                              ptr++;
                           }
                           if (*ptr == '>')
                           {
                              ptr++;
                           }
                        }
                        else
                        {
                           while ((*ptr != '>') && (*ptr != '\n') &&
                                  (*ptr != '\r') && (*ptr != '\0'))
                           {
                              ptr++;
                           }
                           if (*ptr == '>')
                           {
                              ptr++;
                           }
                        }
                     }
                     if ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                     {
                        /* Ensure we do not already have the file name from */
                        /* the href statement before.                       */
                        if (file_name_length == 0)
                        {
                           /* Store file name. */
                           STORE_HTML_STRING(file_name, file_name_length,
                                             MAX_FILENAME_LENGTH, '<');
                        }
                        else
                        {
                           while ((*ptr != '<') && (*ptr != '\n') &&
                                  (*ptr != '\r') && (*ptr != '\0'))
                           {
                              ptr++;
                           }
                        }

                        while (*ptr == '<')
                        {
                           ptr++;
                           while ((*ptr != '>') && (*ptr != '\n') &&
                                  (*ptr != '\r') && (*ptr != '\0'))
                           {
                              ptr++;
                           }
                           if (*ptr == '>')
                           {
                              ptr++;
                           }
                        }
                        if ((*ptr != '\n') && (*ptr != '\r') &&
                            (*ptr != '\0'))
                        {
                           int str_len;

                           while (*ptr == ' ')
                           {
                              ptr++;
                           }

                           /* Store date string. */
                           STORE_HTML_STRING(date_str, str_len,
                                             MAX_FILENAME_LENGTH, '<');
                           file_mtime = datestr2unixtime(date_str,
                                                          &exact_date);

                           while (*ptr == '<')
                           {
                              ptr++;
                              while ((*ptr != '>') && (*ptr != '\n') &&
                                     (*ptr != '\r') && (*ptr != '\0'))
                              {
                                 ptr++;
                              }
                              if (*ptr == '>')
                              {
                                 ptr++;
                              }
                           }
                           if ((*ptr != '\n') && (*ptr != '\r') &&
                               (*ptr != '\0'))
                           {
                              /* Store size string. */
                              STORE_HTML_STRING(size_str, str_len,
                                                MAX_FILENAME_LENGTH, '<');
                              exact_size = convert_size(size_str,
                                                        &file_size);
                           }
                           else
                           {
                              exact_size = -1;
                              file_size = -1;
                           }
                        }
                        else
                        {
                           file_mtime = -1;
                           exact_size = -1;
                           file_size = -1;
                        }
                        if (p_db->verbose > 0)
                        {
                           (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                         "%s mtime=%ld exact=%d size=%ld exact=%ld (%s %d)\n",
# else
                                         "%s mtime=%lld exact=%d size=%ld exact=%ld (%s %d)\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                         "%s mtime=%ld exact=%d size=%lld exact=%lld (%s %d)\n",
# else
                                         "%s mtime=%lld exact=%d size=%lld exact=%lld (%s %d)\n",
# endif
#endif
                                         file_name, (pri_time_t)file_mtime,
                                         exact_date, (pri_off_t)file_size,
                                         (pri_off_t)exact_size, __FILE__,
                                         __LINE__);
                        }
                        else
                        {
                           (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                         "%s mtime=%ld exact=%d size=%ld exact=%ld\n",
# else
                                         "%s mtime=%lld exact=%d size=%ld exact=%ld\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                         "%s mtime=%ld exact=%d size=%lld exact=%lld\n",
# else
                                         "%s mtime=%lld exact=%d size=%lld exact=%lld\n",
# endif
#endif
                                         file_name, (pri_time_t)file_mtime,
                                         exact_date, (pri_off_t)file_size,
                                         (pri_off_t)exact_size);
                        }
                     }
                     else
                     {
                        file_name[0] = '\0';
                        file_mtime = -1;
                        exact_size = -1;
                        file_size = -1;
                     }
                  }

                  /* Go to end of line. */
                  while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  while ((*ptr == '\n') || (*ptr == '\r'))
                  {
                     ptr++;
                  }
               } while ((*ptr == '<') && (*(ptr + 1) == 't') &&
                        (*(ptr + 2) == 'r') && (*(ptr + 3) == '>') &&
                        (*(ptr + 4) == '<') && (*(ptr + 5) == 't') &&
                        (*(ptr + 6) == 'd'));
            }
            else
            {
               if ((*ptr == ' ') && (*(ptr + 1) == ' ') &&
                   (*(ptr + 2) == ' ') && (*(ptr + 3) == '<') &&
                   (*(ptr + 4) == 't') && (*(ptr + 5) == 'r') &&
                   (*(ptr + 6) == '>'))
               {
                  ptr += 7;
                  while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                  {
                     ptr++;
                  }
                  while ((*ptr == '\n') || (*ptr == '\r'))
                  {
                     ptr++;
                  }
                  while ((*ptr == ' ') || (*ptr == '\t'))
                  {
                     ptr++;
                  }
                  if ((*ptr == '<') && (*(ptr + 1) == '/') &&
                      (*(ptr + 2) == 't') && (*(ptr + 3) == 'a') &&
                      (*(ptr + 4) == 'b') && (*(ptr + 5) == 'l') &&
                      (*(ptr + 6) == 'e') && (*(ptr + 7) == '>'))
                  {
                     trans_log(DEBUG_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "Directory empty.");
                     return(SUCCESS);
                  }
               }
               status = href_list(html_buffer, bytes_buffered, p_db);
            }
         }
              /* Pre type listing. */
         else if (((*(ptr + 1) == 'p') && (*(ptr + 4) == '>')) || /* <pre> */
                  ((*(ptr + 1) == 'a') && (*(ptr + 2) == ' ') &&  /* <a href= */
                   (*(ptr + 3) == 'h') && (*(ptr + 7) == '=')))
              {
                 if ((*(ptr + 1) == 'p') && (*(ptr + 4) == '>'))
                 {
                    /* Ignore heading line. */
                    while ((*ptr != '\n') && (*ptr != '\r') &&
                           (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while ((*ptr == '\n') || (*ptr == '\r'))
                    {
                       ptr++;
                    }
                 }

                 while (*ptr == '<')
                 {
                    file_name[0] = '\0';
                    file_name_length = 0;
                    while (*ptr == '<')
                    {
                       ptr++;
                       if ((*ptr == 'a') && (*(ptr + 1) == ' ') &&
                        (*(ptr + 2) == 'h') && (*(ptr + 3) == 'r') &&
                        (*(ptr + 4) == 'e') && (*(ptr + 5) == 'f') &&
                        (*(ptr + 6) == '=') && (*(ptr + 7) == '"'))
                       {
                          ptr += 8;
                          STORE_HTML_STRING(file_name, file_name_length,
                                            MAX_FILENAME_LENGTH, '"');
                       }
                       else
                       {
                          while ((*ptr != '>') && (*ptr != '\n') &&
                                 (*ptr != '\r') && (*ptr != '\0'))
                          {
                             ptr++;
                          }
                       }
                       if (*ptr == '>')
                       {
                          ptr++;
                          while (*ptr == ' ')
                          {
                             ptr++;
                          }
                       }
                    }

                    if ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                    {
                       if (file_name[0] == '\0')
                       {
                          /* Store file name. */
                          STORE_HTML_STRING(file_name, file_name_length,
                                            MAX_FILENAME_LENGTH, '<');
                       }
                       else
                       {
                          /* Away with the shown, maybe cut off filename. */
                          while ((*ptr != '<') && (*ptr != '\n') &&
                                 (*ptr != '\r') && (*ptr != '\0'))
                          {
                             ptr++;
                          }
                       }

                       if (*ptr == '<')
                       {
                          while (*ptr == '<')
                          {
                             ptr++;
                             while ((*ptr != '>') && (*ptr != '\n') &&
                                    (*ptr != '\r') && (*ptr != '\0'))
                             {
                                ptr++;
                             }
                             if (*ptr == '>')
                             {
                                ptr++;
                                while (*ptr == ' ')
                                {
                                   ptr++;
                                }
                             }
                          }
                       }
                       if ((*ptr != '\n') && (*ptr != '\r') &&
                           (*ptr != '\0'))
                       {
                          while (*ptr == ' ')
                          {
                             ptr++;
                          }

                          /* Store date string. */
                          STORE_HTML_DATE();
                          file_mtime = datestr2unixtime(date_str,
                                                        &exact_date);

                          if (*ptr == '<')
                          {
                             while (*ptr == '<')
                             {
                                ptr++;
                                while ((*ptr != '>') && (*ptr != '\n') &&
                                       (*ptr != '\r') && (*ptr != '\0'))
                                {
                                   ptr++;
                                }
                                if (*ptr == '>')
                                {
                                   ptr++;
                                   while (*ptr == ' ')
                                   {
                                      ptr++;
                                   }
                                }
                             }
                          }
                          if ((*ptr != '\n') && (*ptr != '\r') &&
                              (*ptr != '\0'))
                          {
                             int str_len;

                             /* Store size string. */
                             STORE_HTML_STRING(size_str, str_len,
                                               MAX_FILENAME_LENGTH, '<');
                             exact_size = convert_size(size_str,
                                                       &file_size);
                          }
                          else
                          {
                             exact_size = -1;
                             file_size = -1;
                          }
                       }
                       else
                       {
                          file_mtime = -1;
                          exact_size = -1;
                          file_size = -1;
                       }
                       if (p_db->verbose > 0)
                       {
                          (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                        "%s mtime=%ld exact=%d size=%ld exact=%ld (%s %d)\n",
# else
                                        "%s mtime=%lld exact=%d size=%ld exact=%ld (%s %d)\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                        "%s mtime=%ld exact=%d size=%lld exact=%lld (%s %d)\n",
# else
                                        "%s mtime=%lld exact=%d size=%lld exact=%lld (%s %d)\n",
# endif
#endif
                                        file_name, (pri_time_t)file_mtime,
                                        exact_date, (pri_off_t)file_size,
                                        (pri_off_t)exact_size, __FILE__,
                                        __LINE__);
                       }
                       else
                       {
                          (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                                        "%s mtime=%ld exact=%d size=%ld exact=%ld\n",
# else
                                        "%s mtime=%lld exact=%d size=%ld exact=%ld\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                                        "%s mtime=%ld exact=%d size=%lld exact=%lld\n",
# else
                                        "%s mtime=%lld exact=%d size=%lld exact=%lld\n",
# endif
#endif
                                        file_name, (pri_time_t)file_mtime,
                                        exact_date, (pri_off_t)file_size,
                                        (pri_off_t)exact_size);
                       }
                    }
                    else
                    {
                       file_name[0] = '\0';
                       file_mtime = -1;
                       exact_size = -1;
                       file_size = -1;
                       break;
                    }

                    /* Go to end of line. */
                    while ((*ptr != '\n') && (*ptr != '\r') &&
                           (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while ((*ptr == '\n') || (*ptr == '\r'))
                    {
                       ptr++;
                    }
                 }
              }
              /* List type listing. */
         else if ((*(ptr + 1) == 'u') && (*(ptr + 3) == '>'))
              {
                 /* Ignore first line. */
                 while ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                 {
                    ptr++;
                 }
                 while ((*ptr == '\n') || (*ptr == '\r'))
                 {
                    ptr++;
                 }

                 while (*ptr == '<')
                 {
                    while (*ptr == '<')
                    {
                       ptr++;
                       while ((*ptr != '>') && (*ptr != '\n') &&
                              (*ptr != '\r') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                       if (*ptr == '>')
                       {
                          ptr++;
                          while (*ptr == ' ')
                          {
                             ptr++;
                          }
                       }
                    }

                    if ((*ptr != '\n') && (*ptr != '\r') && (*ptr != '\0'))
                    {
                       /* Store file name. */
                       STORE_HTML_STRING(file_name, file_name_length,
                                         MAX_FILENAME_LENGTH, '<');

                       if (p_db->verbose > 0)
                       {
                          (void)fprintf(stdout,
                                        "%s mtime=-1 exact=%d size=-1 exact=-1 (%s %d)\n",
                                        file_name, exact_date, __FILE__,
                                        __LINE__);
                       }
                       else
                       {
                          (void)fprintf(stdout,
                                        "%s mtime=-1 exact=%d size=-1 exact=-1\n",
                                        file_name, exact_date);
                       }
                       file_mtime = -1;
                       exact_size = -1;
                       file_size = -1;
                    }
                    else
                    {
                       file_name[0] = '\0';
                       file_mtime = -1;
                       exact_size = -1;
                       file_size = -1;
                       break;
                    }

                    /* Go to end of line. */
                    while ((*ptr != '\n') && (*ptr != '\r') &&
                           (*ptr != '\0'))
                    {
                       ptr++;
                    }
                    while ((*ptr == '\n') || (*ptr == '\r'))
                    {
                       ptr++;
                    }
                 }
              }
              else
              {
                 status = href_list(html_buffer, bytes_buffered, p_db);
              }
      }
      else
      {
         status = href_list(html_buffer, bytes_buffered, p_db);
      }
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++ href_list() +++++++++++++++++++++++++++++*/
static int
href_list(char *html_buffer, off_t bytes_buffered, struct data *p_db)
{
   int    exact_date,
          file_name_length = -1,
          status = SUCCESS;
   off_t  exact_size,
          file_size;
   time_t file_mtime;
   char   date_str[MAX_FILENAME_LENGTH],
          *end_ptr = html_buffer + bytes_buffered,
          file_name[MAX_FILENAME_LENGTH],
          *ptr = html_buffer;

   while ((ptr = llposi(ptr, (end_ptr - ptr), "<a href=\"", 9)) != NULL)
   {
      ptr--;
      file_name_length = 0;
      exact_size = -1;
      file_size = -1;
      exact_date = -1;
      file_mtime = -1;

      STORE_HTML_STRING(file_name, file_name_length, MAX_FILENAME_LENGTH, '"');
      if (file_name_length > 0)
      {
         /* Remove html tag (eg <view-source>). */
         if (*(ptr - 1) == '>')
         {
            char *tmp_ptr = &file_name[file_name_length - 1];

            while ((tmp_ptr > file_name) && (*tmp_ptr != '<'))
            {
               tmp_ptr--;
            }
            if (*tmp_ptr == '<')
            {
               tmp_ptr--;
               if (*tmp_ptr == ' ')
               {
                  while (*tmp_ptr == ' ')
                  {
                     tmp_ptr--;
                  }
                  tmp_ptr++;
               }
               *tmp_ptr = '\0';
               file_name_length = tmp_ptr - file_name;
            }
         }

         /* If filename has a / at end, assume it is a directory. */
         if (file_name[file_name_length - 1] == '/')
         {
            continue;
         }
         ptr++; /* Away with " */
         if (*ptr == '>')
         {
            ptr++;

            while (*ptr == ' ')
            {
               ptr++;
            }

            /* Remove partial name. */
            while ((*ptr != '<') && (*ptr != '\n') &&
                   (*ptr != '\r') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
         while (*ptr == '<')
         {
            ptr++;
            while ((*ptr != '>') && (*ptr != '\n') &&
                   (*ptr != '\r') && (*ptr != '\0'))
            {
               ptr++;
            }
            if (*ptr == '>')
            {
               ptr++;
               while (*ptr == ' ')
               {
                  ptr++;
               }
            }
         }
         if ((*ptr != '\n') && (*ptr != '\r') &&
             (*ptr != '\0'))
         {
            /* Store date string. */
            STORE_HTML_DATE();
            file_mtime = datestr2unixtime(date_str, &exact_date);
            while (*ptr == '<')
            {
               ptr++;
               while ((*ptr != '>') && (*ptr != '\n') &&
                      (*ptr != '\r') && (*ptr != '\0'))
               {
                  ptr++;
               }
               if (*ptr == '>')
               {
                  ptr++;
                  while (*ptr == ' ')
                  {
                     ptr++;
                  }
               }
            }

            if ((*ptr != '\n') && (*ptr != '\r') &&
                (*ptr != '\0'))
            {
               int str_len;

               /* Store size string. */
               STORE_HTML_STRING(date_str, str_len,
                                 MAX_FILENAME_LENGTH, '<');
               exact_size = convert_size(date_str, &file_size);
            }
         }
      }
      else
      {
         continue;
      }

      if (p_db->hostname[0] != '\0')
      {
         /* Check if file name is a URL. */
         if ((file_name[4] == ':') && (file_name[5] == '/') &&
             (file_name[6] == '/'))
         {
            /* http */
            if ((file_name[0] == 'h') && (file_name[1] == 't') &&
                (file_name[2] == 't') && (file_name[3] == 'p'))
            {
               int          port = DEFAULT_HTTP_PORT;
               unsigned int error_mask;
               time_t       now = time(NULL);
               char         hostname[MAX_REAL_HOSTNAME_LENGTH + 1],
                            password[MAX_USER_NAME_LENGTH + 1],
                            remote_dir[MAX_RECIPIENT_LENGTH + 1],
                            user[MAX_USER_NAME_LENGTH + 1];

               if ((error_mask = url_evaluate(file_name, NULL, user, NULL, NULL,
#ifdef WITH_SSH_FINGERPRINT
                                              NULL, NULL,
#endif
                                              password, NO, hostname, &port,
                                              remote_dir, NULL, &now,
                                              NULL, NULL, NULL, NULL,
                                              NULL, NULL)) > 3)
               {
                  continue;
               }
               else
               {
                  if ((port != p_db->port) ||
                      (strcmp(hostname, p_db->hostname) != 0) ||
                      (strcmp(user, p_db->user) != 0) ||
                      (strcmp(password, p_db->password) != 0))
                  {
                     continue;
                  }
                  if (strncmp(p_db->remote_dir, remote_dir,
                              strlen(p_db->remote_dir)) != 0)
                  {
                     char *tmp_ptr = &file_name[7];

                     while ((*tmp_ptr != '/') && (*tmp_ptr != '\0'))
                     {
                        tmp_ptr++;
                     }
                     if (*tmp_ptr == '/')
                     {
                        (void)memmove(file_name, tmp_ptr, strlen(tmp_ptr) + 1);
                     }
                     else
                     {
                        continue;
                     }
                  }
                  else
                  {
                     size_t length = 0;
                     char   *tmp_ptr = file_name + strlen(file_name);

                     /* Just show file name. */
                     while ((tmp_ptr > file_name) && (*tmp_ptr != '/'))
                     {
                        tmp_ptr--;
                        length++;
                     }
                     if (*tmp_ptr == '/')
                     {
                        tmp_ptr++;
                        (void)memmove(file_name, tmp_ptr, length);
                     }
                     else
                     {
                        continue;
                     }
                  }
               }
            }
                 /* sftp */
            else if ((file_name[0] == 's') && (file_name[1] == 'f') &&
                     (file_name[2] == 't') && (file_name[3] == 'p'))
                 {
                    continue;
                 }
#ifdef WITH_SSL
                 /* ftps */
            else if ((file_name[0] == 'f') && (file_name[1] == 't') &&
                     (file_name[2] == 'p') && (file_name[3] == 's'))
                 {
                    continue;
                 }
#endif
         }
#ifdef WITH_SSL
         else if ((file_name[5] == ':') && (file_name[6] == '/') &&
                  (file_name[7] == '/'))
              {
                 /* https */
                 if ((file_name[0] == 'h') && (file_name[1] == 't') &&
                     (file_name[2] == 't') && (file_name[3] == 'p') &&
                     (file_name[4] == 's'))
                 {
                    int          port = DEFAULT_HTTPS_PORT;
                    unsigned int error_mask;
                    time_t       now = time(NULL);
                    char         hostname[MAX_REAL_HOSTNAME_LENGTH + 1],
                                 password[MAX_USER_NAME_LENGTH + 1],
                                 remote_dir[MAX_RECIPIENT_LENGTH + 1],
                                 user[MAX_USER_NAME_LENGTH + 1];

                    if ((error_mask = url_evaluate(file_name, NULL, user, NULL, NULL,
#ifdef WITH_SSH_FINGERPRINT
                                                   NULL, NULL,
#endif
                                                   password, NO, hostname, &port,
                                                   remote_dir, NULL, &now,
                                                   NULL, NULL, NULL, NULL,
                                                   NULL, NULL)) > 3)
                    {
                       continue;
                    }
                    else
                    {
                       if ((port != p_db->port) ||
                           (strcmp(hostname, p_db->hostname) != 0) ||
                           (strcmp(user, p_db->user) != 0) ||
                           (strcmp(password, p_db->password) != 0))
                       {
                          continue;
                       }
                       if (strncmp(p_db->remote_dir, remote_dir, strlen(p_db->remote_dir)) != 0)
                       {
                          char *tmp_ptr = &file_name[8];

                          while ((*tmp_ptr != '/') && (*tmp_ptr != '\0'))
                          {
                             tmp_ptr++;
                          }
                          if (*tmp_ptr == '/')
                          {
                             (void)memmove(file_name, tmp_ptr, strlen(tmp_ptr) + 1);
                          }
                          else
                          {
                             continue;
                          }
                       }
                       else
                       {
                          size_t length = 0;
                          char   *tmp_ptr = file_name + strlen(file_name);

                          /* Just show file name. */
                          while ((tmp_ptr > file_name) && (*tmp_ptr != '/'))
                          {
                             tmp_ptr--;
                             length++;
                          }
                          if (*tmp_ptr == '/')
                          {
                             tmp_ptr++;
                             (void)memmove(file_name, tmp_ptr, length);
                          }
                          else
                          {
                             continue;
                          }
                       }
                    }
                 }
              }
#endif
         else if ((file_name[2] == ':') && (file_name[3] == '/') &&
                  (file_name[4] == '/'))
              {
                 /* ftp */
                 if ((file_name[0] == 'f') && (file_name[1] == 't') &&
                     (file_name[2] == 'p'))
                 {
                    continue;
                 }
              }
              /* mailto:// */
         else if ((file_name[0] == 'm') && (file_name[1] == 'a') &&
                  (file_name[2] == 'i') && (file_name[3] == 'l') &&
                  (file_name[4] == 't') && (file_name[5] == 'o') &&
                  (file_name[6] == ':') && (file_name[7] == '/') &&
                  (file_name[8] == '/'))
              {
                 continue;
              }
      }

      if ((file_name[0] == '?') && (file_name[1] == 'C') &&
          (file_name[2] == '=') && (file_name[4] == ';'))
      {
         continue;
      }

      if (p_db->verbose > 0)
      {
         (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                       "%s mtime=%ld exact=%d size=%ld exact=%ld (%s %d)\n",
# else
                       "%s mtime=%lld exact=%d size=%ld exact=%ld (%s %d)\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                       "%s mtime=%ld exact=%d size=%lld exact=%lld (%s %d)\n",
# else
                       "%s mtime=%lld exact=%d size=%lld exact=%lld (%s %d)\n",
# endif
#endif
                       file_name, (pri_time_t)file_mtime,
                       exact_date, (pri_off_t)file_size,
                       (pri_off_t)exact_size, __FILE__, __LINE__);
      }
      else
      {
         (void)fprintf(stdout,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                       "%s mtime=%ld exact=%d size=%ld exact=%ld\n",
# else
                       "%s mtime=%lld exact=%d size=%ld exact=%ld\n",
# endif
#else
# if SIZEOF_TIME_T == 4
                       "%s mtime=%ld exact=%d size=%lld exact=%lld\n",
# else
                       "%s mtime=%lld exact=%d size=%lld exact=%lld\n",
# endif
#endif
                       file_name, (pri_time_t)file_mtime,
                       exact_date, (pri_off_t)file_size,
                       (pri_off_t)exact_size);
      }
      bytes_buffered = end_ptr - ptr;
   } /* while "<a href=" */

   return(status);
}


/*------------------------- convert_size() ------------------------------*/
static off_t
convert_size(char *size_str, off_t *size)
{
   off_t exact_size;
   char  *ptr,
         *ptr_start;

   ptr = size_str;
   while (*ptr == ' ')
   {
      ptr++;
   }
   ptr_start = ptr;

   while (isdigit((int)*ptr))
   {
      ptr++;
   }
   if (*ptr == '.')
   {
      ptr++;
      while (isdigit((int)*ptr))
      {
         ptr++;
      }
   }
   if (ptr != ptr_start)
   {
      switch (*ptr)
      {
         case 'K': /* Kilobytes. */
            exact_size = KILOBYTE;
            break;
         case 'M': /* Megabytes. */
            exact_size = MEGABYTE;
            break;
         case 'G': /* Gigabytes. */
            exact_size = GIGABYTE;
            break;
         case 'T': /* Terabytes. */
            exact_size = TERABYTE;
            break;
         case 'P': /* Petabytes. */
            exact_size = PETABYTE;
            break;
         case 'E': /* Exabytes. */
            exact_size = EXABYTE;
            break;
         default :
            exact_size = 1;
            break;
      }
      *size = (off_t)(strtod(ptr_start, (char **)NULL) * exact_size);
   }
   else
   {
      *size = -1;
      exact_size = -1;
   }

   return(exact_size);
}
