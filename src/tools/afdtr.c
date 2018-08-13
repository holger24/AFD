/*
 *  afdtr.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2017, 2018 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#include "afddefs.h"

DESCR__S_M1
/*
 ** NAME
 **   afdtr - show transfer rate
 **
 ** SYNOPSIS
 **   afdtr [-w <working directory>] option
 **           -H <host alias 1>[ <host alias n>]
 **           -I <IP 1>[ <IP n>]
 **
 ** DESCRIPTION
 **   Program afdtr shows the transfer rate of different host or IP
 **   that AFD sends or receives data to/from.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.04.2017 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi()                       */
#include <time.h>                        /* time()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <sys/types.h>
#include <fcntl.h>                       /* open()                       */
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "logdefs.h"
#include "version.h"

#define TRD_STEP_SIZE 100
#define TIME_BUFFER_LENGTH 20   /* YYYY-MM-DD HH:MM:SS */

/* Global variables. */
int         sys_log_fd = STDERR_FILENO,   /* Not used!    */
            verbose = 0;
char        *p_work_dir;
const char  *sys_log_name = SYSTEM_LOG_FIFO;

/* Local global variables. */
static int    tr_counter = 0;
static struct tr_data
              {
                 time_t        timeval;
                 off_t         bytes_done;
                 char          alias_ip[MAX_REAL_HOSTNAME_LENGTH];
                 unsigned char type;
              } *trd;

/* Local function prototypes. */
static int  check_alias_ip(char *, int, char **);
static void eval_line(char *, int, char **, int, char **),
            usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ afdtr() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int     i,
           no_of_search_host_alias = 0,
           no_of_search_ips = 0;
   size_t  line_length;
#ifdef HAVE_GETLINE
   ssize_t n;
#endif
   char    current_tr_file[MAX_PATH_LENGTH],
#ifdef HAVE_GETLINE
           *line = NULL,
#else
           line[MAX_LINE_LENGTH],
#endif
           *p_current_tr_file,
           **search_host_alias = NULL,
           **search_ips = NULL,
           time_buffer[TIME_BUFFER_LENGTH + 1],
           work_dir[MAX_PATH_LENGTH];
   FILE    *current_fp = NULL;

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if ((get_arg(&argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "-h", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
      exit(INCORRECT);
   }

   if (get_arg_array(&argc, argv, "-H", &search_host_alias,
                     &no_of_search_host_alias) != SUCCESS)
   {
      no_of_search_host_alias = 0;
   }
   if (get_arg_array(&argc, argv, "-I", &search_ips,
                     &no_of_search_ips) != SUCCESS)
   {
      no_of_search_ips = 0;
   }
   if (get_arg(&argc, argv, "-v", NULL, 0) == SUCCESS)
   {
      verbose = 1;
   }

   p_current_tr_file = current_tr_file + snprintf(current_tr_file,
                                                 MAX_PATH_LENGTH,
                                                 "%s%s/%s",
                                                 p_work_dir, LOG_DIR,
                                                 TRANSFER_RATE_LOG_NAME);
   *p_current_tr_file = '0';
   *(p_current_tr_file + 1) = '\0';
   if ((current_fp = fopen(current_tr_file, "r")) == NULL)
   {
      (void)fprintf(stderr, "Failed to fopen() `%s' : %s (%s %d)\n",
                    current_tr_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef HAVE_GETLINE
   while ((n = getline(&line, &line_length, current_fp)) != -1)
#else
   while (fgets(line, MAX_LINE_LENGTH, current_fp) != NULL)
#endif
   {
      eval_line(line, no_of_search_host_alias, search_host_alias,
                no_of_search_ips, search_ips);
   }

   if (fclose(current_fp) == EOF)
   {
      (void)fprintf(stderr, "Failed to fclose() `%s' : %s (%s %d)\n",
                    current_tr_file, strerror(errno), __FILE__, __LINE__);
   }

   /* Show what we have. */
   for (i = 0; i < tr_counter; i++)
   {
      (void)strftime(time_buffer, TIME_BUFFER_LENGTH, "%Y-%m-%d %H:%M:%S",
                     localtime(&trd[i].timeval));
      (void)fprintf(stdout, "%s %c %s %ld\n",
                    time_buffer, trd[i].type, trd[i].alias_ip,
                    trd[i].bytes_done);
   }

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++ eval_line() ++++++++++++++++++++++++++++*/
static void
eval_line(char *line,
          int  no_of_search_host_alias,
          char **search_host_alias,
          int  no_of_search_ips,
          char **search_ips)
{
   int    i,
          interval,
          version;
   time_t starttime,
          timeval;
   char   *p_start,
          *ptr = line,
          *tmp_ptr;

   if (*ptr == '*')
   {
      /*
       * Informational line with the following possible formats:
       *      # Start line with internval in seconds and log version number.
       *      *|59d29134|Start|interval=5|version=0
       *      # Stop line, when logging stopped.
       *      *|59d2a1cf|Stop
       *      # Reschuffel line, log has been rotated.
       *      *|59f66b85|Reshuffel|interval=5
       */
      if (*(ptr + 1) == '|')
      {
         if ((*(ptr + 2) == 'x') && (*(ptr + 3) == '|'))
         {
            ptr += 4;

            /* Reshuffel| */
            if ((*ptr == 'R') && (*(ptr + 1) == 'e') && (*(ptr + 2) == 's') &&
                (*(ptr + 3) == 'h') && (*(ptr + 4) == 'u') &&
                (*(ptr + 5) == 'f') && (*(ptr + 6) == 'f') &&
                (*(ptr + 7) == 'e') && (*(ptr + 8) == 'l') &&
                (*(ptr + 9) == '|'))
            {
               ptr += 10;

               /* interval= */
               if ((*ptr == 'i') && (*(ptr + 1) == 'n') &&
                   (*(ptr + 2) == 't') && (*(ptr + 3) == 'e') &&
                   (*(ptr + 4) == 'r') && (*(ptr + 5) == 'v') &&
                   (*(ptr + 6) == 'a') && (*(ptr + 7) == 'l') &&
                   (*(ptr + 8) == '='))
               {
                  ptr += 9;
                  p_start = ptr;
                  while ((*ptr != '\n') && (*ptr != '|'))
                  {
                     ptr++;
                  }
                  if (*ptr == '\n')
                  {
                     interval = (int)strtoul(p_start, NULL, 10);
                  }
                  else if (*ptr == '|')
                       {
                          *ptr = '\0';
                          interval = (int)strtoul(p_start, NULL, 10);
                          ptr++;
                       }
                       else
                       {
#ifndef HAVE_GETLINE
                           /* Ignore rest of line. */
                           if (*ptr != '\n')
                           {
                              while ((*ptr != '\n') && (*ptr != '\0'))
                              {
                                 ptr++;
                              }
                              if (*ptr == '\n')
                              {
                                 ptr++;
                              }
                           }
#endif
                       }
               }
               else
               {
#ifndef HAVE_GETLINE
                  /* Ignore rest of line. */
                  if (*ptr != '\n')
                  {
                     while ((*ptr != '\n') && (*ptr != '\0'))
                     {
                        ptr++;
                     }
                     if (*ptr == '\n')
                     {
                        ptr++;
                     }
                  }
#endif
               }
            }
         }
         else
         {
            ptr += 2;
            while ((*ptr != '|') && (*ptr != '\n'))
            {
               ptr++;
            }
            if (*ptr == '|')
            {
               *ptr = '\0';
               timeval = (time_t)str2timet(line + 2, NULL, 16);
               ptr++;

               /* Start */
               if ((*ptr == 'S') && (*(ptr + 1) == 't') &&
                   (*(ptr + 2) == 'a') && (*(ptr + 3) == 'r') &&
                   (*(ptr + 4) == 't') && (*(ptr + 5) == '|'))
               {
                  starttime = timeval;
                  ptr += 6;

                  /* interval= */
                  if ((*ptr == 'i') && (*(ptr + 1) == 'n') &&
                      (*(ptr + 2) == 't') && (*(ptr + 3) == 'e') &&
                      (*(ptr + 4) == 'r') && (*(ptr + 5) == 'v') &&
                      (*(ptr + 6) == 'a') && (*(ptr + 7) == 'l') &&
                      (*(ptr + 8) == '='))
                  {
                     ptr += 9;
                     p_start = ptr;
                     while ((*ptr != '|') && (*ptr != '\n'))
                     {
                        ptr++;
                     }
                     if (*ptr == '|')
                     {
                        *ptr = '\0';
                        interval = (int)strtoul(p_start, NULL, 10);
                        ptr++;

                        /* Get log version number. */
                        if ((*ptr == 'v') && (*(ptr + 1) == 'e') &&
                            (*(ptr + 2) == 'r') && (*(ptr + 3) == 's') &&
                            (*(ptr + 4) == 'i') && (*(ptr + 5) == 'o') &&
                            (*(ptr + 6) == 'n') && (*(ptr + 7) == '='))
                        {
                           ptr += 8;
                           p_start = ptr;
                           while ((*ptr != '|') && (*ptr != '\n'))
                           {
                              ptr++;
                           }
                           version = (int)strtoul(p_start, NULL, 10);
#ifndef HAVE_GETLINE
                           if (*ptr != '\n')
                           {
                              while ((*ptr != '\n') && (*ptr != '\0'))
                              {
                                 ptr++;
                              }
                              if (*ptr == '\n')
                              {
                                 ptr++;
                              }
                           }
#endif
                        }
                        else
                        {
                           /* We did not find version, assume current version. */
                           version = TRANSFER_RATE_LOG_VERSION;
#ifndef HAVE_GETLINE
                           /* Ignore rest of line. */
                           if (*ptr != '\n')
                           {
                              while ((*ptr != '\n') && (*ptr != '\0'))
                              {
                                 ptr++;
                              }
                              if (*ptr == '\n')
                              {
                                 ptr++;
                              }
                           }
#endif
                        }
                     }
                     else
                     {
#ifndef HAVE_GETLINE
                        /* Ignore rest of line. */
                        if (*ptr != '\n')
                        {
                           while ((*ptr != '\n') && (*ptr != '\0'))
                           {
                              ptr++;
                           }
                           if (*ptr == '\n')
                           {
                              ptr++;
                           }
                        }
#endif
                        interval = TRANSFER_RATE_LOG_INTERVAL; /* Set default. */
                        version = TRANSFER_RATE_LOG_VERSION;
                     }
                  }
                  else
                  {
                     /* If we do not find the interval lets take the default. */
                     interval = TRANSFER_RATE_LOG_INTERVAL;
                     version = TRANSFER_RATE_LOG_VERSION;
                  }

                  if (verbose > 0)
                  {
#if SIZEOF_TIME_T == 4
                     (void)printf("%lx|======> AFD started (interval=%ds version=%d) <======\n",
#else
                     (void)printf("%llx|======> AFD started (interval=%ds version=%d) <======\n",
#endif
                                  (pri_time_t)timeval, interval, version);
                  }
               }
                    /* Stop */
               else if ((*ptr == 'S') && (*(ptr + 1) == 't') &&
                        (*(ptr + 2) == 'o') && (*(ptr + 3) == 'p') &&
                        ((*(ptr + 4) == '\n') || (*(ptr + 4) == '|') ||
                         (*(ptr + 4) == '\0')))
                    {
#ifndef HAVE_GETLINE
                       ptr += 4;
                       if (*ptr == '\n')
                       {
                          ptr++;
                       }
#endif
                       if (verbose > 0)
                       {
#if SIZEOF_TIME_T == 4
                          (void)printf("%lx|======> AFD stopped <======\n",
#else
                          (void)printf("%llx|======> AFD stopped <======\n",
#endif
                                       (pri_time_t)timeval);
                       }
                    }
            }
            else
            {
               (void)fprintf(stderr, "Unable to determine end of time: %s\n",
                             line);
            }
         }
      }
      else
      {
         (void)fprintf(stderr, "Reading garbage: %s\n", line);
      }
   }
   else
   {
      /*
       * Assume normal line with transfer rate values, having the
       * following format:
       *   59d28e27|A|radar-1|10419
       *      |     |    |      |
       *      |     |    |      +-> Decimal number of bytes done in interval.
       *      |     |    +--------> Alias name if type A otherwise IP number.
       *      |     +-------------> Either type A for alias name or type I
       *      |                     for IP number.
       */
      while ((*ptr != '|') && (*ptr != '\n'))
      {
         ptr++;
      }
      if (*ptr == '|')
      {
         if ((tr_counter % TRD_STEP_SIZE) == 0)
         {
            size_t new_size = ((tr_counter / TRD_STEP_SIZE) + 1) *
                              TRD_STEP_SIZE * sizeof(struct tr_data);

            if (tr_counter == 0)
            {
               if ((trd = malloc(new_size)) == NULL)
               {
                  (void)fprintf(stderr,
#if SIZEOF_SIZE_T == 4
                                "Could not malloc() %d bytes : %s (%s %d)\n",
#else
                                "Could not malloc() %lld bytes : %s (%s %d)\n",
#endif
                                (pri_size_t)new_size, strerror(errno),
                                __FILE__, __LINE__);
                  exit(INCORRECT);
               }
            }
            else
            {
               if ((trd = realloc(trd, new_size)) == NULL)
               {
                  (void)fprintf(stderr,
#if SIZEOF_SIZE_T == 4
                                "Could not realloc() %d bytes : %s (%s %d)\n",
#else
                                "Could not realloc() %lld bytes : %s (%s %d)\n",
#endif
                                (pri_size_t)new_size, strerror(errno),
                                __FILE__, __LINE__);
                  exit(INCORRECT);
               }
            }
         }
         *ptr = '\0';
         trd[tr_counter].timeval = (time_t)str2timet(line, NULL, 16);
         trd[tr_counter].type = (unsigned char)*(ptr + 1);
         if (*(ptr + 2) == '|')
         {
            ptr += 3;
            i = 0;
            while ((*ptr != '|') && (*ptr != '\n') &&
                   (i < (MAX_REAL_HOSTNAME_LENGTH - 1)))
            {
               trd[tr_counter].alias_ip[i] = *ptr;
               ptr++; i++;
            }
            if (*ptr == '|')
            {
               trd[tr_counter].alias_ip[i] = '\0';
               if (((no_of_search_host_alias == 0) &&
                    (no_of_search_ips == 0)) ||
                   (((trd[tr_counter].type == 'A') &&
                     ((no_of_search_host_alias > 0) &&
                      (check_alias_ip(trd[tr_counter].alias_ip,
                                      no_of_search_host_alias,
                                      search_host_alias) == 1))) ||
                    ((trd[tr_counter].type == 'I') &&
                     ((no_of_search_ips > 0) &&
                      (check_alias_ip(trd[tr_counter].alias_ip,
                                      no_of_search_ips, search_ips) == 1)))))
               {
                  ptr++;
                  tmp_ptr = ptr;

                  /* Store bytes done. */
                  while (*ptr != '\n')
                  {
                     ptr++;
                  }
                  if (*ptr == '\n')
                  {
                     trd[tr_counter].bytes_done = (off_t)str2offt(tmp_ptr, NULL, 10);
                     tr_counter++;
                  }
               }
            }
         }
      }
      else
      {
         /* Unknown data. Lets just ignore the line. */;
      }
   }

   return;
}


/*++++++++++++++++++++++++++ check_alias_ip() ++++++++++++++++++++++++++*/
static int
check_alias_ip(char *alias_ip, int no_of_search_items, char **items)
{
   int i;

   for (i = 0; i < no_of_search_items; i++)
   {
      if (strcmp(alias_ip, items[i]) == 0)
      {
         return(1);
      }
   }

   return(0);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 _("SYNTAX  : %s [-w working directory] <options>\n"),
                 progname);
   (void)fprintf(stderr,
                 _("          -H <host alias 1>[ <host alias n>]  alias to show\n"));
   (void)fprintf(stderr,
                 _("          -I <IP 1>[ <IP n>]                  IP to show\n"));
   return;
}
