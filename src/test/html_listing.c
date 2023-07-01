#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include "afddefs.h"

#define STORE_HTML_STRING(html_str, str_len, max_str_length, end_char)\
        {                                                          \
           str_len = 0;                                            \
           while ((*ptr != end_char) && (*ptr != '\n') && (*ptr != '\r') &&\
                  (*ptr != '\0') && (str_len < ((max_str_length) - 1)))\
           {                                                       \
              if (*ptr == '&')                                     \
              {                                                    \
                 ptr++;                                            \
                 if ((*(ptr + 1) == 'u') && (*(ptr + 2) == 'm') && \
                     (*(ptr + 3) == 'l') && (*(ptr + 4) == ';'))   \
                 {                                                 \
                    switch (*ptr)                                  \
                    {                                              \
                       case 'a': (html_str)[str_len++] = 228;      \
                                 break;                            \
                       case 'A': (html_str)[str_len++] = 196;      \
                                 break;                            \
                       case 'e': (html_str)[str_len++] = 235;      \
                                 break;                            \
                       case 'E': (html_str)[str_len++] = 203;      \
                                 break;                            \
                       case 'i': (html_str)[str_len++] = 239;      \
                                 break;                            \
                       case 'I': (html_str)[str_len++] = 207;      \
                                 break;                            \
                       case 'o': (html_str)[str_len++] = 246;      \
                                 break;                            \
                       case 'O': (html_str)[str_len++] = 214;      \
                                 break;                            \
                       case 'u': (html_str)[str_len++] = 252;      \
                                 break;                            \
                       case 'U': (html_str)[str_len++] = 220;      \
                                 break;                            \
                       case 's': (html_str)[str_len++] = 223;      \
                                 break;                            \
                       case 'y': (html_str)[str_len++] = 255;      \
                                 break;                            \
                       case 'Y': (html_str)[str_len++] = 195;      \
                                 break;                            \
                       default : /* Just ignore it. */             \
                                 break;                            \
                    }                                              \
                    ptr += 5;                                      \
                    continue;                                      \
                 }                                                 \
                 else if ((*ptr == 's') && (*(ptr + 1) == 'z') &&  \
                          (*(ptr + 2) == 'l') && (*(ptr + 3) == 'i') &&\
                          (*(ptr + 4) == 'g') && (*(ptr + 5) == ';'))\
                      {                                            \
                         (html_str)[str_len++] = 223;              \
                         ptr += 6;                                 \
                         continue;                                 \
                      }                                            \
                 else if ((*ptr == 'a') && (*(ptr + 1) == 'm') &&  \
                          (*(ptr + 2) == 'p') && (*(ptr + 3) == ';'))\
                      {                                            \
                         (html_str)[str_len++] = 38;               \
                         ptr += 4;                                 \
                         continue;                                 \
                      }                                            \
                 else if ((*ptr == 'd') && (*(ptr + 1) == 'e') &&  \
                          (*(ptr + 2) == 'g') && (*(ptr + 3) == ';'))\
                      {                                            \
                         (html_str)[str_len++] = 176;              \
                         ptr += 4;                                 \
                         continue;                                 \
                      }                                            \
                 else if ((*ptr == 'g') && (*(ptr + 1) == 't') &&  \
                          (*(ptr + 2) == ';'))                     \
                      {                                            \
                         (html_str)[str_len++] = '>';              \
                         ptr += 3;                                 \
                         continue;                                 \
                      }                                            \
                 else if ((*ptr == 'l') && (*(ptr + 1) == 't') &&  \
                          (*(ptr + 2) == ';'))                     \
                      {                                            \
                         (html_str)[str_len++] = '<';              \
                         ptr += 3;                                 \
                         continue;                                 \
                      }                                            \
                      else                                         \
                      {                                            \
                         while ((*ptr != ';') && (*ptr != '<') &&  \
                                (*ptr != '\n') && (*ptr != '\r') &&\
                                (*ptr != '\0'))                    \
                         {                                         \
                            ptr++;                                 \
                         }                                         \
                         if (*ptr != ';')                          \
                         {                                         \
                            break;                                 \
                         }                                         \
                      }                                            \
              }                                                    \
              (html_str)[str_len] = *ptr;                          \
              str_len++; ptr++;                                    \
           }                                                       \
           (html_str)[str_len] = '\0';                             \
        }
#define STORE_HTML_DATE()                                          \
        {                                                          \
           int i = 0,                                              \
               space_counter = 0;                                  \
                                                                   \
           while ((*ptr != '<') && (*ptr != '\n') && (*ptr != '\r') &&\
                  (*ptr != '\0') && (i < (MAX_FILENAME_LENGTH - 1)))\
           {                                                       \
              if (*ptr == ' ')                                     \
              {                                                    \
                 if (space_counter == 1)                           \
                 {                                                 \
                    while (*ptr == ' ')                            \
                    {                                              \
                       ptr++;                                      \
                    }                                              \
                    break;                                         \
                 }                                                 \
                 space_counter++;                                  \
              }                                                    \
              if (*ptr == '&')                                     \
              {                                                    \
                 ptr++;                                            \
                 if ((*(ptr + 1) == 'u') && (*(ptr + 2) == 'm') && \
                     (*(ptr + 3) == 'l') && (*(ptr + 4) == ';'))   \
                 {                                                 \
                    switch (*ptr)                                  \
                    {                                              \
                       case 'a': date_str[i++] = 228;              \
                                 break;                            \
                       case 'A': date_str[i++] = 196;              \
                                 break;                            \
                       case 'o': date_str[i++] = 246;              \
                                 break;                            \
                       case 'O': date_str[i++] = 214;              \
                                 break;                            \
                       case 'u': date_str[i++] = 252;              \
                                 break;                            \
                       case 'U': date_str[i++] = 220;              \
                                 break;                            \
                       case 's': date_str[i++] = 223;              \
                                 break;                            \
                       default : /* Just ignore it. */             \
                                 break;                            \
                    }                                              \
                    ptr += 5;                                      \
                    continue;                                      \
                 }                                                 \
                 else                                              \
                 {                                                 \
                    while ((*ptr != ';') && (*ptr != '<') &&       \
                           (*ptr != '\n') && (*ptr != '\r') &&     \
                           (*ptr != '\0'))                         \
                    {                                              \
                       ptr++;                                      \
                    }                                              \
                    if (*ptr != ';')                               \
                    {                                              \
                       break;                                      \
                    }                                              \
                 }                                                 \
              }                                                    \
              date_str[i] = *ptr;                                  \
              i++; ptr++;                                          \
           }                                                       \
           date_str[i] = '\0';                                     \
        }

/* Global variables. */
int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir;
const char *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static int   eval_html_dir_list(char *);
static off_t convert_size(char *, off_t *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ html_listing $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char *buffer = NULL;

   if (argc != 2)
   {
      (void)fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
      exit(-1);
   }
   if (read_file(argv[1], &buffer) == INCORRECT)
   {
      (void)fprintf(stderr, "Failed to read_file() %s\n", argv[1]);
      exit(-1);
   }
   if (eval_html_dir_list(buffer) != SUCCESS)
   {
      (void)fprintf(stderr, "eval_html_dir_list() failed.\n");
      exit(-1);
   }

   exit(SUCCESS);
}


/*++++++++++++++++++++++++ eval_html_dir_list() +++++++++++++++++++++++++*/
static int
eval_html_dir_list(char *html_buffer)
{
   int    listing_complete = YES;
   size_t bytes_buffered = strlen(html_buffer);
   char   *ptr,
          ssh_protocol = '1';

   if ((ptr = llposi(html_buffer, bytes_buffered, "<h1>", 4)) == NULL)
   {
      if ((ptr = llposi(html_buffer, bytes_buffered, "<PRE>", 5)) == NULL)
      {
         if ((ptr = llposi(html_buffer, bytes_buffered,
                           "<?xml version=\"", 15)) == NULL)
         {
            if ((ptr = llposi(html_buffer, bytes_buffered,
                              "<div id=\"downloadLinkArea\">",
                              27)) == NULL)
            {
               if ((ptr = llposi(html_buffer, bytes_buffered,
                                 "<div id=\"contentDiv\">",
                                 21)) == NULL)
               {
                  (void)printf("Unknown HTML directory listing. Please send author a link so that this can be implemented. (%s %d)\n",
                               __FILE__, __LINE__);
                  return(INCORRECT);
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

                  while ((ptr = llposi(ptr, bytes_buffered,
                                       "<a href=\"", 9)) != NULL)
                  {
                     ptr--;
                     file_name_length = 0;
                     if (*ptr == '/')
                     {
                        ptr += 1;
                     }
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
                                    file_size = (off_t)str2offt(date_str, NULL, 10);
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
                     (void)printf("name=%s length=%d mtime=%ld exact_date=%d exact_size=%ld file_size=%ld\n",
                                  file_name, file_name_length, file_mtime,
                                  exact_date, exact_size, file_size);

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

               while ((ptr = llposi(ptr, bytes_buffered,
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

                  (void)printf("name=%s length=%d mtime=-1 exact_date=%d exact_size=-1 file_size=-1\n",
                               file_name, file_name_length, DS2UT_NONE);

                  bytes_buffered = end_ptr - ptr;
               } /* while "<a href=" */
            }
         }
         else
         {
            if ((ptr = llposi(ptr, bytes_buffered,
                              "<IsTruncated>", 13)) == NULL)
            {
               (void)printf("Unknown HTML directory listing. Please send author a link so that this can be implemented. (%s %d)\n",
                            __FILE__, __LINE__);
               return(INCORRECT);
            }
            else
            {
               int    date_str_length,
                      exact_date = DS2UT_NONE,
                      file_name_length = -1;
               size_t bytes_buffered_original = bytes_buffered;
               off_t  exact_size,
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
                  listing_complete = NO;
                  ptr += 2;
               }

               ptr = html_buffer;
               while ((ptr = llposi(ptr, bytes_buffered,
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
                                       (void)printf("name=%s length=%d mtime=%ld exact_date=%d exact_size=%ld file_size=%ld\n",
                                                    file_name,
                                                    file_name_length,
                                                    file_mtime,
                                                    exact_date,
                                                    exact_size, file_size);
                                    }
                                    else
                                    {
                                       (void)printf("Unable to store size (length=%d char=%d). (%s %d)\n",
                                                    file_name_length,
                                                    (int)*ptr,
                                                    __FILE__, __LINE__);
                                       return(INCORRECT);
                                    }
                                 }
                                 else
                                 {
                                    (void)printf("No matching /ETag><Size> found. (%s %d)\n",
                                                 __FILE__, __LINE__);
                                    return(INCORRECT);
                                 }
                              }
                              else
                              {
                                 (void)printf("Unable to store etag (length=%d char=%d). (%s %d)\n",
                                              file_name_length, (int)*ptr,
                                              __FILE__, __LINE__);
                                 return(INCORRECT);
                              }
                           }
                           else
                           {
                              (void)printf("No matching /LastModified><ETag> found. (%s %d)\n",
                                           __FILE__, __LINE__);
                              return(INCORRECT);
                           }
                        }
                        else
                        {
                           (void)printf("Unable to store date (length=%d char=%d). (%s %d)\n",
                                        file_name_length, (int)*ptr,
                                        __FILE__, __LINE__);
                           return(INCORRECT);
                        }
                     }
                     else
                     {
                        (void)printf("No matching /Key><LastModified> found. (%s %d)\n",
                                     __FILE__, __LINE__);
                        return(INCORRECT);
                     }
                  }
                  else
                  {
                     (void)printf("Unable to store file name (length=%d char=%d). (%s %d)\n",
                                  file_name_length, (int)*ptr,
                                  __FILE__, __LINE__);
                     return(INCORRECT);
                  }
                  bytes_buffered = end_ptr - ptr;
               } /* while <Contents><Key> */

               if (file_name_length == -1)
               {
                  listing_complete = YES;

                  /* Bucket is empty or we have some new */
                  /* listing type. So check if KeyCount  */
                  /* is zero.                            */
                  if ((ptr = llposi(html_buffer,
                                    bytes_buffered_original,
                                    "<KeyCount>0</KeyCount>", 22)) == NULL)
                  {
                     /* No <Contents><Key> found! */
                     (void)printf("Unknown HTML directory listing. Please send author a link so that this can be implemented. (%s %d)",
                                  __FILE__, __LINE__);
                     return(INCORRECT);
                  }
               }

               if (listing_complete == NO)
               {
                  int  marker_name_length;
                  char marker_name[24];

                  if (ssh_protocol == '1')
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
                                    bytes_buffered_original,
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
                     if (ssh_protocol != '1')
                     {
                        (void)printf("<IsTruncated> is true, but could not locate a <NextContinuationToken>! (%s %d)\n",
                                     __FILE__, __LINE__);
                        return(INCORRECT);
                     }
                  }
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
               }
               else
               {
                  file_name[0] = '\0';
                  file_mtime = -1;
                  exact_size = -1;
                  file_size = -1;
                  break;
               }

               (void)printf("name=%s length=%d mtime=%ld exact_date=%d exact_size=%ld file_size=%ld\n",
                            file_name, file_name_length, file_mtime,
                            exact_date, exact_size, file_size);

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
            (void)printf("Unknown HTML directory listing. Please send author a link so that this can be implemented. (%s %d)\n",
                         __FILE__, __LINE__);
            return(INCORRECT);
         }
      }
   }
   else
   {
      int    exact_date = DS2UT_NONE,
             file_name_length;
      time_t file_mtime;
      off_t  exact_size,
             file_size = -1;
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
                     }
                     else
                     {
                        file_name[0] = '\0';
                        file_mtime = -1;
                        exact_size = -1;
                        file_size = -1;
                     }

                     (void)printf("name=%s length=%d mtime=%ld exact_date=%d exact_size=%ld file_size=%ld\n",
                                  file_name, file_name_length, file_mtime,
                                  exact_date, exact_size, file_size);
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
                     (void)printf("Directory empty. (%s %d)\n",
                                  __FILE__, __LINE__);
                     return(SUCCESS);
                  }
               }
               (void)printf("Unknown HTML directory listing. Please send author a link so that this can be implemented. (%s %d)\n",
                            __FILE__, __LINE__);
               return(INCORRECT);
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
                    }
                    else
                    {
                       file_name[0] = '\0';
                       file_mtime = -1;
                       exact_size = -1;
                       file_size = -1;
                       break;
                    }

                    (void)printf("name=%s length=%d mtime=%ld exact_date=%d exact_size=%ld file_size=%ld\n",
                                 file_name, file_name_length, file_mtime,
                                 exact_date, exact_size, file_size);

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
                       exact_size = -1;
                       file_mtime = -1;
                    }
                    else
                    {
                       file_name[0] = '\0';
                       file_mtime = -1;
                       exact_size = -1;
                       file_size = -1;
                       break;
                    }

                    (void)printf("name=%s length=%d mtime=%ld exact_date=%d exact_size=%ld file_size=%ld\n",
                                 file_name, file_name_length, file_mtime,
                                 exact_date, exact_size, file_size);

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
                 (void)printf("Unknown HTML directory listing. Please send author a link so that this can be implemented. (%s %d)\n",
                              __FILE__, __LINE__);
                 return(INCORRECT);
              }
      }
      else
      {
         (void)printf("Unknown HTML directory listing. Please send author a link so that this can be implemented. (%s %d)\n",
                      __FILE__, __LINE__);
         return(INCORRECT);
      }
   }

   return(SUCCESS);
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
