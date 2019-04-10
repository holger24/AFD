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
   char *ptr;

   if ((ptr = lposi(html_buffer, "<h1>", 4)) == NULL)
   {
      if ((ptr = lposi(html_buffer, "<PRE>", 5)) == NULL)
      {
         (void)printf("Unknown HTML directory listing. Please send author a link so that this can be implemented.\n");
         return(INCORRECT);
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
            int    file_name_length;
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
                     file_mtime = datestr2unixtime(date_str);

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

               (void)printf("name=%s length=%d mtime=%ld exact_size=%ld file_size=%ld\n",
                            file_name, file_name_length, file_mtime, exact_size,
                            file_size);

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
            (void)printf("Unknown HTML directory listing. Please send author a link so that this can be implemented.\n");
            return(INCORRECT);
         }
      }
   }
   else
   {
      int    file_name_length;
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
                           file_mtime = datestr2unixtime(date_str);

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

                     (void)printf("name=%s length=%d mtime=%ld exact_size=%ld file_size=%ld\n",
                                  file_name, file_name_length, file_mtime,
                                  exact_size, file_size);
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
              (void)printf("Unknown HTML directory listing. Please send author a link so that this can be implemented.\n");
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
                          file_mtime = datestr2unixtime(date_str);

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

                    (void)printf("name=%s length=%d mtime=%ld exact_size=%ld file_size=%ld\n",
                                 file_name, file_name_length, file_mtime,
                                 exact_size, file_size);

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

                    (void)printf("name=%s length=%d mtime=%ld exact_size=%ld file_size=%ld\n",
                                 file_name, file_name_length, file_mtime,
                                 exact_size, file_size);

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
                 printf("Unknown HTML directory listing. Please send author a link so that this can be implemented.\n");
                 return(INCORRECT);
              }
      }
      else
      {
         (void)printf("Unknown HTML directory listing. Please send author a link so that this can be implemented.\n");
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
