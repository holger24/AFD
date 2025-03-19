/*
 *  eval_input_alda.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M3
/*
 ** NAME
 **   eval_input_alda - checks syntax of input for process alda
 **
 ** SYNOPSIS
 **   void eval_input_alda(int *argc, char *argv[])
 **
 ** DESCRIPTION
 **   This module checks whether the syntax is correct. The syntax is as
 **   follows:
 **           alda [options] <file name pattern>
 **
 **   Mode options
 **           -c                           continuous
 **           -C                           continuous daemon
 **           -l                           local log data
 **           -r                           remote log data
 **           -b                           back trace data
 **           -f                           forward trace data
 **   Range parameters
 **           -s <AFD host name/alias/ID>  Starting AFD hostname/alias/ID.
 **           -e <AFD host name/alias/ID>  Ending AFD hostname/alias/ID.
 **           -t <start>[-<end>]           Time frame at starting point.
 **           -T <start>[-<end>]           Time frame at end point.
 **           -L <log type>                Search only in given log type.
 **                                        Log type can be:
 **                                          I - Input Log
 **                                          U - Distribution Log
 **                                          P - Production Log
 **                                          C - Output Log confirmed
 **                                          R - Output Log retrieved
 **                                          O - Output Log delivered
 **                                          D - Delete Log
 **                                        Default: IUPOD
 **           -g <time in seconds>         Maximum time to search for
 **                                        a single file before giving up.
 **           -G <time in minutes>         Maximum time we may search for
 **                                        all files.
 **   Format parameter
 **           -o <format>                  Specifies the output format.
 **                                        Possible format parameters
 **                                        are as listed:
 **              -- Input log data --
 **              %[Z]IT<time char>      - input time
 **              %[Y]IF                 - input file name
 **              %[X]IS<size char>      - input file size
 **              %[Z]II                 - input source ID
 **              %[Y]IN                 - full source name
 **              %[Z]IU                 - unique number
 **              -- Distribution log data --
 **              %[Z]Ut<time char>      - distribution time
 **              %[Z]UT<time char>      - input time
 **              %[Y]UF                 - input file name
 **              %[X]US<size char>      - input file size
 **              %[Z]UI                 - input source ID
 **              %[Z]UU                 - unique number
 **              %[Z]Un                 - number of jobs distributed
 **              %[Z]Uj<separator char> - list of job ID's
 **              %[Z]Uc<separator char> - list of number of pre-processing
 **              %[Z]UY                 - distribution type
 **              -- Production log data --
 **              %[Z]Pt<time char>      - time when production starts
 **              %[Z]PT<time char>      - time when production finished
 **              %[X]PD<duration char>  - production time (duration)
 **              %[X]Pu<duration char>  - CPU usage
 **              %[Z]Pb                 - ratio relationship 1
 **              %[Z]PB                 - ratio relationship 2
 **              %[Z]PJ                 - job ID
 **              %[Z]PZ<time char>      - job creation time
 **              %[Z]PU                 - unique number
 **              %[Z]PL                 - split job number
 **              %[Y]Pf                 - input file name
 **              %[X]Ps<size char>      - input file size
 **              %[Y]PF                 - produced file name
 **              %[X]PS<size char>      - produced file size
 **              %[Y]PC                 - command executed
 **              %[Z]PR                 - return code of command executed
 **              -- Output log data --
 **              %[Z]Ot<time char>      - time when sending starts
 **              %[Z]OT<time char>      - time when file is transmitted
 **              %[X]OD<duration char>  - time taken to transmit file
 **              %[Y]Of                 - local output file name
 **              %[Y]OF                 - remote output file name/directory
 **              %[Y]OE                 - final output file name/directory
 **              %[Z]Op                 - protocol ID used for transmission
 **              %[Y]OP                 - protocol used for transmission
 **              %[X]OS<size char>      - output file size
 **              %[Z]OJ                 - job ID
 **              %[Z]Oe                 - number of retries
 **              %[Y]OA                 - archive directory
 **              %[Z]OZ<time char>      - job creation time
 **              %[Z]OU                 - unique number
 **              %[Z]OL                 - split job number
 **              %[Y]OM                 - mail queue ID
 **              %[Y]Oh                 - target real hostname/IP
 **              %[Y]OH                 - target alias name
 **              %[Y]OR                 - Recipient of job
 **              %[Z]Oo                 - output type ID
 **              %[Y]OO                 - output type string
 **              -- Delete log data --
 **              %[Z]Dt<time char>      - time when job was created
 **              %[Z]DT<time char>      - time when file was deleted
 **              %[Z]Dr                 - delete reason ID
 **              %[Y]DR                 - delete reason string
 **              %[Y]DW                 - user/program causing deletion
 **              %[Y]DA                 - additional reason
 **              %[Z]DZ<time char>      - job creation time
 **              %[Z]DU                 - unique number
 **              %[Z]DL                 - split job number
 **              %[Y]DF                 - file name of deleted file
 **              %[X]DS<size char>      - file size of deleted file
 **              %[Z]DJ                 - job ID of deleted file
 **              %[Z]DI                 - input source ID
 **              %[Y]DN                 - full source name
 **              %[Y]DH                 - target alias name
 **              -- AFD information --
 **              %[Y]Ah                 - AFD real hostname/IP
 **              %[Y]AH                 - AFD alias name
 **              %[Y]AV                 - AFD version
 **
 **               [X] -> [-][0]#[.#]] or [-][0]#[d|o|x]
 **               [Y] -> [-]# or [<individual character positions>]
 **               [Z] -> [-][0]#[d|o|x]
 **
 **            (used second chars: AbBcCDeEfFhHIJjLNnpOoPrRsStTuUWYZ)
 **
 **            Time character (t,T):
 **                 a - Abbreviated weekday name: Tue
 **                 A - Full weekday name: Tuesday
 **                 b - Abbreviated month name: Jan
 **                 B - Full month name: January
 **                 c - Date and time: Tue Jan 19 16:24:50 1999
 **                 d - Day of the month [01 - 31]: 19
 **                 H - Hour of the 24-hour day [00 - 23]: 16
 **                 I - Hour of the 24-hour day [00 - 12]: 04
 **                 j - Day of the year [001 - 366]: 19
 **                 m - Month [01 - 12]: 01
 **                 M - Minute [00 - 59]: 24
 **                 p - AM/PM: PM
 **                 S - Second [00 - 61]: 50
 **             (*) u - Unix time: 916759490
 **                 U - Sunday week number [00 - 53]: 02
 **                 w - Weekday [0 - 6] (0=Sunday): 2
 **                 W - Monday week number [00 - 53]: 02
 **                 X - Time: 16:24:50
 **                 y - Year without centry [00 - 99]: 99
 **                 Y - Year with centry: 1999
 **                 Z - Time zone name: CET
 **            Duration character (D,u):
 **                 A - Automatic shortes format: 4d
 **                             d - days
 **                             h - hours
 **                             m - minutes
 **                             s - seconds
 **             (*) D - Days only : 4
 **             (*) H - Hours only : 102
 **             (*) M - Minutes only: 6144
 **             (*) S - Seconds only: 368652
 **                 X - Time (h:mm:ss): 102:24:12
 **                 Y - Time (d:hh:mm): 4:06:24
 **            Size character (S):
 **             (#) a - Automatic shortes format: 1 GB
 **                             B  - byte
 **                             KB - kilobyte (10^3)
 **                             MB - megabyte (10^6)
 **                             GB - gigabyte (10^9)
 **                             TB - terabyte (10^12)
 **                             PB - petabyte (10^15)
 **                             EB - exabyte  (10^18)
 **             (#) A - Automatic shortes format: 1 GiB
 **                             B   - byte
 **                             KiB - kibibyte (2^10)
 **                             MiB - mebibyte (2^20)
 **                             GiB - gibibyte (2^30)
 **                             TiB - tebibyte (2^40)
 **                             PiB - pebibyte (2^50)
 **                             EiB - exbibyte (2^60)
 **             (#) B - Bytes only: 1884907510
 **             (#) e - Exabyte only : 0
 **             (#) E - Exbibyte only: 0
 **             (#) g - Gigabyte only: 1
 **             (#) G - Gibibyte only: 1
 **             (#) k - Kilobyte only: 1884907
 **             (#) K - Kibibyte only: 1840729
 **             (#) m - Megabyte only: 1884
 **             (#) M - Mebibyte only: 1797
 **             (#) p - Petabyte only: 0
 **             (#) P - Pebibyte only: 0
 **             (#) t - Terabyte only: 0
 **             (#) T - Tebibyte only: 0
 **
 **            (*) Can be printed as decimal (d), octal (o) or hexadecimal (x)
 **            (#) Can be printed as numeric string with decimal point
 **                                      OR
 **                Can be printed as decimal (d), octal (o) or hexadecimal (x)
 **   Search parameters
 **           -d <directory name/alias/ID> Directory name, alias or ID.
 **           -h <host name/alias/ID>      Host name, alias or ID.
 **           -j <job ID>                  Job identifier.
 **           -u <unique number>           Unique number.
 **           -z <size>                    Original file size in byte.
 **                                        (Production log only!)
 **           -S[I|U|P|O|D] <size>         File size in byte.
 **           -D[P|O] <time>               Duration in seconds.
 **           -p <protocol>                Protocol used for transport.
 **   Other parameters
 **           -F <file name>               Footer to add to output.
 **           -R <x>                       Rotate log x times.
 **           -H <file name>               Header to add to output.
 **           -O <file name>               File where to write output.
 **           -v[v[v[v[v[v]]]]]            Verbose mode.
 **           -w <work dir>                Working directory of the AFD.
 **           --header_line=<line>         Add the given header line to
 **                                        output. The following
 **                                        % parameters can be used to
 **                                        insert additional system
 **                                        infomation:
 **                                          %I - inode number of the log file
 **                                          %H - host ID
 **
 **   To be able to differentiate between name, alias and ID:
 **       alias - must always begin with %
 **       ID    - must always begin with #
 **       name  - just the name without extra identifier
 **
 ** RETURN VALUES
 **   If any of the above parameters have been specified it returns
 **   them in the appropriate variable.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.04.2007 H.Kiehl Created
 **   04.05.2024 H.Kiehl Setup umask so we do not create log files wih
 **                      mode 666.
 **   19.03.2025 H.Kiehl Add --header_line=<line> parameter to add
 **                      a header line with additional system information.
 **
 */
DESCR__E_M3

#include <stdio.h>          /* stderr, fprintf(), fopen()                */
#include <string.h>         /* strerror(), strcpy()                      */
#include <stdlib.h>         /* exit()                                    */
#include <time.h>           /* time()                                    */
#include <ctype.h>          /* isdigit()                                 */
#include <sys/stat.h>       /* umask()                                   */
#include <unistd.h>         /* access()                                  */
#include <errno.h>
#include "aldadefs.h"
#ifdef WITH_AFD_MON
# include "mondefs.h"
#endif

#define START_HOST_TYPE  1
#define END_HOST_TYPE    2
#define SEARCH_DIR_TYPE  3
#define SEARCH_HOST_TYPE 4
#define ALIAS_TYPE       5
#define ID_TYPE          6
#define NAME_TYPE        7
#define START_TIME_TYPE  8
#define END_TIME_TYPE    9

#define ADDITIONAL_EXTRA_LENGTH 16


/* External global variables. */
extern unsigned int end_alias_counter,
                    *end_id,
                    end_id_counter,
                    end_name_counter,
                    file_pattern_counter,
                    mode,
                    protocols,
                    search_dir_alias_counter,
                    *search_dir_id,
                    search_dir_id_counter,
                    search_dir_name_counter,
                    search_duration_flag,
                    search_file_size_flag,
                    search_orig_file_size_flag,
                    search_host_alias_counter,
                    *search_host_id,
                    search_host_id_counter,
                    search_host_name_counter,
                    search_job_id,
                    search_unique_number,
                    search_log_type,
#ifdef _OUTPUT_LOG
                    show_output_type,
#endif
                    start_alias_counter,
                    *start_id,
                    start_id_counter,
                    start_name_counter;
extern int          gt_lt_sign,
                    gt_lt_sign_duration,
                    gt_lt_sign_orig,
                    no_of_header_lines,
                    rotate_limit,
                    trace_mode,
                    verbose;
extern time_t       end_time_end,
                    end_time_start,
                    max_diff_time,
                    max_search_time,
                    start_time_end,
                    start_time_start;
extern off_t        search_file_size,
                    search_orig_file_size;
extern double       search_duration;
extern char         **end_alias,
                    **end_name,
                    **file_pattern,
                    footer_filename[],
                    *format_str,
                    header_filename[],
                    **header_line,
                    output_filename[],
                    **search_dir_alias,
                    **search_dir_name,
                    **search_host_alias,
                    **search_host_name,
                    **start_alias,
                    **start_name;
extern FILE         *output_fp;

/* Local function prototypes. */
static int          eval_time(char *, int, time_t),
                    get_time_value(char *, char **, time_t),
                    insert_line(char ***, const char *, size_t),
                    store_protocols(char *);
static void         store_name_alias_id(char *, int),
                    usage(char *);


/*########################## eval_input_alda() ##########################*/
void
eval_input_alda(int *argc, char *argv[])
{
   int    correct = YES;               /* Was input/syntax correct?      */
   time_t now;
   char   *progname;

   progname = argv[0];
   now = time(NULL);
   max_search_time = 0L;
   max_diff_time = DEFAULT_MAX_DIFF_TIME;
   start_time_start = 0;
   start_time_end = 0;
   end_time_start = 0;
   end_time_end = 0;
   protocols = 0;
   mode = 0;
   output_filename[0] = '\0';
   rotate_limit = DEFAULT_ROTATE_LIMIT;
   header_filename[0] = '\0';
   footer_filename[0] = '\0';
   verbose = 0;

   argv++;
   (*argc)--;
   if ((*argc > 0) && (argv[0][0] == '-'))
   {
      while ((*argc > 0) && ((*argv)[0] == '-'))
      {
         switch (*(argv[0] + 1))
         {
            /*
             * Mode parameters
             * ===============
             */
            case 'c' : /* Continuous mode. */
               mode |= ALDA_CONTINUOUS_MODE;
               (*argc)--;
               argv++;
               break;

            case 'C' : /* Continuous daemon mode. */
               mode |= ALDA_CONTINUOUS_DAEMON_MODE;
               (*argc)--;
               argv++;
               break;

            case 'l' : /* Local mode. */
               mode |= ALDA_LOCAL_MODE;
               (*argc)--;
               argv++;
               break;

            case 'r' : /* Remote mode. */
#ifdef WITH_AFD_MON
               mode |= ALDA_REMOTE_MODE;
#else
               (void)fprintf(stderr,
                             "ERROR  : This code has not been compiled for remote mode.\n");
               (void)fprintf(stderr,
                             "         Please configure with --enable-afd_mon or\n");
               (void)fprintf(stderr,
                             "         --enable-compile_afd_mon_only and recompile.\n");
               correct = NO;
#endif
               (*argc)--;
               argv++;
               break;

            case 'b' : /* Back trace data. */
               mode |= ALDA_BACKWARD_MODE;
               (*argc)--;
               argv++;
               break;

            case 'f' : /* Forward trace data. */
               mode |= ALDA_FORWARD_MODE;
               (*argc)--;
               argv++;
               break;

            /*
             * Range parameters
             * ================
             */
            case 's' : /* Starting AFD hostname/alias/ID. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No starting AFD hostname/alias/ID specified for parameter -s.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  store_name_alias_id(*(argv + 1), START_HOST_TYPE);
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 'e' : /* Ending AFD hostname/alias/ID. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No ending AFD hostname/alias/ID specified for parameter -s.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  store_name_alias_id(*(argv + 1), END_HOST_TYPE);
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 't' : /* Time frame at starting point. */
               if ((*argc == 1) ||
                   ((*(argv + 1)[0] == '-') &&
                    ((isdigit((int)(*((argv + 1)[0] + 1))) == 0) ||
                     (*((argv + 1)[0] + 2) == '\0'))))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No time frame specified for parameter -t.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  if (eval_time(*(argv + 1), START_TIME_TYPE, now) != SUCCESS)
                  {
                     correct = NO;
                  }
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 'T' : /* Time frame at ending point. */
               if ((*argc == 1) ||
                   ((*(argv + 1)[0] == '-') &&
                    ((isdigit((int)(*((argv + 1)[0] + 1))) == 0) ||
                     (*((argv + 1)[0] + 2) == '\0'))))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No time frame specified for parameter -T.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  if (eval_time(*(argv + 1), END_TIME_TYPE, now) != SUCCESS)
                  {
                     correct = NO;
                  }
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 'L' : /* Search only in given log type. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No log type specified for parameter -L.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  int i;

                  search_log_type = 0;
#ifdef _OUTPUT_LOG
                  show_output_type = 0;
#endif
                  i = 0;
                  do
                  {
                     switch ((*(argv + 1))[i])
                     {
#ifdef _INPUT_LOG
                        case 'I' : /* Input log. */
                           search_log_type |= SEARCH_INPUT_LOG;
                           break;
#endif
#ifdef _DISTRIBUTION_LOG
                        case 'U' : /* Distribution log. */
                           search_log_type |= SEARCH_DISTRIBUTION_LOG;
                           break;
#endif
#ifdef _PRODUCTION_LOG
                        case 'P' : /* Production log. */
                           search_log_type |= SEARCH_PRODUCTION_LOG;
                           break;
#endif
#ifdef _OUTPUT_LOG
# if defined(_WITH_DE_MAIL_SUPPORT) && !defined(_CONFIRMATION_LOG)
                        case 'C' : /* Output log confirmation. */
                           search_log_type |= SEARCH_OUTPUT_LOG;
                           show_output_type |= SHOW_CONF_OF_DISPATCH;
                           show_output_type |= SHOW_CONF_OF_RECEIPT;
                           show_output_type |= SHOW_CONF_OF_RETRIEVE;
                           show_output_type |= SHOW_CONF_TIMEUP;
                           break;

# endif
                        case 'R' : /* Output log retrieve. */
                           search_log_type |= SEARCH_OUTPUT_LOG;
                           show_output_type |= SHOW_NORMAL_RECEIVED;
                           break;

                        case 'O' : /* Output log delivered. */
                           search_log_type |= SEARCH_OUTPUT_LOG;
                           show_output_type |= SHOW_NORMAL_DELIVERED;
                           break;
#endif
#ifdef _DELETE_LOG
                        case 'D' : /* Delete log. */
                           search_log_type |= SEARCH_DELETE_LOG;
                           break;
#endif
                     }
                     i++;
                  } while ((*(argv + 1))[i] != '\0');
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 'g' : /* Maximum time to search for a single file */
                       /* before giving up.                        */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No seconds specified for parameter -g.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  max_diff_time = (time_t)(str2timet(*(argv + 1), NULL, 10));
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 'G' : /* Maximum time we may search for all files. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No minutes specified for parameter -G.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  max_search_time = (time_t)(str2timet(*(argv + 1), NULL, 10) * 60);
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            /*
             * Format parameters
             * =================
             */
            case 'o' : /* Specifies the output format. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No output format specified for parameter -o.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  int  length = 0;
                  char *ptr = (argv + 1)[0];

                  if (format_str != NULL)
                  {
                     free(format_str);
                     format_str = NULL;
                  }
                  while (*(ptr + length) != '\0')
                  {
                     length++;
                  }
                  if ((format_str = malloc(length + 1)) == NULL)
                  {
                     (void)fprintf(stderr,
                                      "ERROR  : Failed to malloc() %d bytes for format string : %s (%s %d)\n",
                                   length + 1, strerror(errno),
                                   __FILE__, __LINE__);
                     correct = NO;
                  }
                  else
                  {
                     length = 0;
                     ptr = (argv + 1)[0];

                     while (*ptr != '\0')
                     {
                        format_str[length] = *ptr;
                        length++; ptr++;
                     }
                     format_str[length] = '\0';
                  }
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            /*
             * Search parameters
             * =================
             */
            case 'd' : /* Directory name/alias/ID. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No directory name/alias/ID specified for parameter -d.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  store_name_alias_id(*(argv + 1), SEARCH_DIR_TYPE);
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 'h' : /* Host name/alias/ID. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No host name/alias/ID specified for parameter -h.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  store_name_alias_id(*(argv + 1), SEARCH_HOST_TYPE);
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 'j' : /* Job identifier. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No job ID specified for parameter -j.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  search_job_id = (unsigned int)strtoul(*(argv + 1), NULL, 16);
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 'u' : /* Unique number. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No unique number specified for parameter -u.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  search_unique_number = (unsigned int)strtoul(*(argv + 1), NULL, 16);
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 'z' : /* Original file size (production only!) in byte. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No size specified for parameter -s.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  char *ptr;

                  search_orig_file_size_flag = SEARCH_PRODUCTION_LOG;
                  ptr = *(argv + 1);
                  if (*ptr == '<')
                  {
                     gt_lt_sign_orig = LESS_THEN_SIGN;
                     ptr++;
                  }
                  else if (*ptr == '>')
                       {
                          gt_lt_sign_orig = GREATER_THEN_SIGN;
                          ptr++;
                       }
                  else if (*ptr == '!')
                       {
                          gt_lt_sign_orig = NOT_SIGN;
                          ptr++;
                       }
                       else
                       {
                          gt_lt_sign_orig = EQUAL_SIGN;
                          if (*ptr == '=')
                          {
                             ptr++;
                          }
                       }
                  search_orig_file_size = (unsigned int)strtoul(ptr, NULL, 10);
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 'S' : /* File size in byte. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No size specified for parameter -S.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  char *ptr;

                  switch (*(argv[0] + 2))
                  {
#ifdef _INPUT_LOG
                     case 'I' : /* Input */
                        search_file_size_flag = SEARCH_INPUT_LOG;
                        break;
#endif
#ifdef _DISTRIBUTION_LOG
                     case 'U' : /* Distribution */
                        search_file_size_flag = SEARCH_DISTRIBUTION_LOG;
                        break;
#endif
#ifdef _PRODUCTION_LOG
                     case 'P' : /* Production */
                        search_file_size_flag = SEARCH_PRODUCTION_LOG;
                        break;
#endif
#ifdef _OUTPUT_LOG
                     case 'O' : /* Output */
                        search_file_size_flag = SEARCH_OUTPUT_LOG;
                        break;
#endif
#ifdef _DELETE_LOG
                     case 'D' : /* Delete */
                        search_file_size_flag = SEARCH_DELETE_LOG;
                        break;
#endif
                     default : /* All */
                        search_file_size_flag = SEARCH_ALL_LOGS;
                        break;
                  }

                  ptr = *(argv + 1);
                  if (*ptr == '<')
                  {
                     gt_lt_sign = LESS_THEN_SIGN;
                     ptr++;
                  }
                  else if (*ptr == '>')
                       {
                          gt_lt_sign = GREATER_THEN_SIGN;
                          ptr++;
                       }
                       else
                       {
                          gt_lt_sign = EQUAL_SIGN;
                          if (*ptr == '=')
                          {
                             ptr++;
                          }
                       }
                  search_file_size = (unsigned int)strtoul(ptr, NULL, 10);
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 'D' : /* Duration in seconds. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No duration specified for parameter -D.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  char *ptr;

                  switch (*(argv[0] + 2))
                  {
#ifdef _PRODUCTION_LOG
                     case 'P' : /* Production */
                        search_duration_flag = SEARCH_PRODUCTION_LOG;
                        break;
#endif
#ifdef _OUTPUT_LOG
                     case 'O' : /* Output */
                        search_duration_flag = SEARCH_OUTPUT_LOG;
                        break;
#endif
                     default : /* All */
                        search_duration_flag = SEARCH_ALL_LOGS;
                        break;
                  }

                  ptr = *(argv + 1);
                  if (*ptr == '<')
                  {
                     gt_lt_sign_duration = LESS_THEN_SIGN;
                     ptr++;
                  }
                  else if (*ptr == '>')
                       {
                          gt_lt_sign_duration = GREATER_THEN_SIGN;
                          ptr++;
                       }
                  else if (*ptr == '!')
                       {
                          gt_lt_sign_duration = NOT_SIGN;
                          ptr++;
                       }
                       else
                       {
                          gt_lt_sign_duration = EQUAL_SIGN;
                          if (*ptr == '=')
                          {
                             ptr++;
                          }
                       }
                  search_duration = strtod(ptr, NULL);
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 'p' : /* Protocol used for transport. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No protocol specified for parameter -p.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  if (store_protocols(*(argv + 1)) == INCORRECT)
                  {
                     correct = NO;
                  }
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            /*
             * Other parameters
             * ================
             */
            case 'F' : /* Footer to add to output. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No filename specified for parameter -F.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  if (access(*(argv + 1), R_OK) == 0)
                  {
                     (void)my_strncpy(footer_filename, *(argv + 1),
                                      MAX_PATH_LENGTH);
                  }
                  else
                  {
                     (void)fprintf(stderr,
                                   "ERROR  : Failed to access() %s : %s\n",
                                   *(argv + 1), strerror(errno));
                     correct = NO;
                  }
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 'H' : /* Header to add to output. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No filename specified for parameter -H.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  if (access(*(argv + 1), R_OK) == 0)
                  {
                     (void)my_strncpy(header_filename, *(argv + 1), MAX_PATH_LENGTH);
                  }
                  else
                  {
                     (void)fprintf(stderr,
                                   "ERROR  : Failed to access() %s : %s\n",
                                   *(argv + 1), strerror(errno));
                     correct = NO;
                  }
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 'O' : /* File where to write output. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No filename specified for parameter -O.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  (void)my_strncpy(output_filename, *(argv + 1),
                                   MAX_PATH_LENGTH);
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 'R' : /* Number of times the log should be rotated. */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "ERROR  : No rotate limit specified for parameter -R.\n");
                  correct = NO;
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  rotate_limit = atoi(*(argv + 1));
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case 'v' : /* Verbose mode. */
               while (*(argv[0] + 1 + verbose) == 'v')
               {
                  verbose++;
               }
               if (*(argv[0] + 1 + verbose) != '\0')
               {
                   /* Unknown parameter. */
                   (void)fprintf(stderr,
                                 "ERROR  : Unknown parameter %c. (%s %d)\n",
                                 *(argv[0] + 1), __FILE__, __LINE__);
                   correct = NO;
               }
               (*argc)--;
               argv++;
               break;

            /*
             * NOTE: AFD_WORK_DIR is handled by function get_afd_path()
             *       much earlier. That function cuts away the -w argument
             *       so if we hit it here again it is a second -w argument!
             *       Do not bail out, just give a warning, since it is
             *       common error that users using aldad specify the
             *       working directory as an argument.
             */
            case 'w' : /* Second working directory argument is wrong! */
               if ((*argc == 1) || (*(argv + 1)[0] == '-'))
               {
                  (void)fprintf(stderr,
                                "WARNING: Working directory already set and no working directory specified for parameter -w.\n");
                  (*argc) -= 1;
                  argv += 1;
               }
               else
               {
                  (void)fprintf(stderr,
                                "WARNING: Working directory already set. Ignoring.\n");
                  (*argc) -= 2;
                  argv += 2;
               }
               break;

            case '-' : /* Other parameters starting with --. */
               {
                  char *p_name = &argv[0][2];

                  /* --header_line= */
                  if (strcmp(p_name, "header_line=") != 0)
                  {
                     if (insert_line(&header_line, &argv[0][14],
                                     no_of_header_lines))
                     {
                        no_of_header_lines++;
                     }
                  }
                  (*argc)--;
                  argv++;
                  break;
               }

            default : /* Unknown parameter. */
               (void)fprintf(stderr,
                             "ERROR  : Unknown parameter `%s' [argc=%d] (%s %d)\n",
                             argv[0], *argc, __FILE__, __LINE__);
               (*argc)--;
               argv++;
               correct = NO;
               break;
         }
      }
   }

   if ((correct == NO) ||
       (((mode & ALDA_BACKWARD_MODE) == 0) &&
        ((mode & ALDA_FORWARD_MODE) == 0)))
   {
      usage(progname);
      exit(INCORRECT);
   }

   /* Store file name pattern(s). */
   if (*argc > 0)
   {
      int i,
          length,
          max_length;

      max_length = 0;
      for (i = 0; i < *argc; i++)
      {
        length = 0;
        while (argv[i][length] != '\0')
        {
           length++;
        }
        if (length > max_length)
        {
           max_length = length;
        }
      }
      RT_ARRAY(file_pattern, *argc, (max_length + 1), char);
      for (i = 0; i < *argc; i++)
      {
        length = 0;
        while (argv[i][length] != '\0')
        {
           file_pattern[i][length] = argv[i][length];
           length++;
        }
        file_pattern[i][length] = '\0';
      }
      file_pattern_counter = *argc;
   }
   else
   {
      RT_ARRAY(file_pattern, 1, 2, char);
      file_pattern[0][0] = '*';
      file_pattern[0][1] = '\0';
      file_pattern_counter = 1;
   }

   /* Set default output string if not set. */
   if (format_str == NULL)
   {
      if ((format_str = malloc(DEFAULT_OUTPUT_ALL_FORMAT_LENGTH + 3)) == NULL)
      {
         (void)fprintf(stderr,
                       "ERROR  : Failed to malloc() %d bytes for format string : %s (%s %d)\n",
                       (int)(DEFAULT_OUTPUT_ALL_FORMAT_LENGTH + 3),
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (search_log_type == SEARCH_ALL_LOGS)
      {
         (void)strcpy(format_str, DEFAULT_OUTPUT_ALL_FORMAT);
         trace_mode = ON;
      }
#ifdef _INPUT_LOG
      else if (search_log_type == SEARCH_INPUT_LOG)
           {
              (void)strcpy(format_str, DEFAULT_OUTPUT_INPUT_FORMAT);
              trace_mode = OFF;
           }
#endif
#ifdef _DISTRIBUTION_LOG
      else if (search_log_type == SEARCH_DISTRIBUTION_LOG)
           {
              (void)strcpy(format_str, DEFAULT_OUTPUT_DISTRIBUTION_FORMAT);
              trace_mode = OFF;
           }
#endif
#ifdef _PRODUCTION_LOG
      else if (search_log_type == SEARCH_PRODUCTION_LOG)
           {
              (void)strcpy(format_str, DEFAULT_OUTPUT_PRODUCTION_FORMAT);
              trace_mode = OFF;
           }
#endif
#ifdef _OUTPUT_LOG
      else if (search_log_type == SEARCH_OUTPUT_LOG)
           {
              (void)strcpy(format_str, DEFAULT_OUTPUT_OUTPUT_FORMAT);
              trace_mode = OFF;
           }
#endif
#ifdef _DELETE_LOG
      else if (search_log_type == SEARCH_DELETE_LOG)
           {
              (void)strcpy(format_str, DEFAULT_OUTPUT_DELETE_FORMAT);
              trace_mode = OFF;
           }
#endif
           else
           {
              (void)strcpy(format_str, DEFAULT_OUTPUT_ALL_FORMAT);
              trace_mode = ON;
           }
   }
   else
   {
      if (search_log_type == SEARCH_ALL_LOGS)
      {
         trace_mode = ON;
      }
#ifdef _INPUT_LOG
      else if (search_log_type == SEARCH_INPUT_LOG)
           {
              trace_mode = OFF;
           }
#endif
#ifdef _DISTRIBUTION_LOG
      else if (search_log_type == SEARCH_DISTRIBUTION_LOG)
           {
              trace_mode = OFF;
           }
#endif
#ifdef _PRODUCTION_LOG
      else if (search_log_type == SEARCH_PRODUCTION_LOG)
           {
              trace_mode = OFF;
           }
#endif
#ifdef _OUTPUT_LOG
      else if (search_log_type == SEARCH_OUTPUT_LOG)
           {
              trace_mode = OFF;
           }
#endif
#ifdef _DELETE_LOG
      else if (search_log_type == SEARCH_DELETE_LOG)
           {
              trace_mode = OFF;
           }
#endif
           else
           {
              trace_mode = ON;
           }
   }

   /* If no protocol is set, assume we want all protocols. */
   if (protocols == 0)
   {
      protocols = ~0u;
   }

   /* Check if all neccessary mode flags have been set, otherwise set them. */
   if (((mode & ALDA_LOCAL_MODE) == 0) && ((mode & ALDA_REMOTE_MODE) == 0))
   {
      mode |= ALDA_LOCAL_MODE;
   }
   if (((mode & ALDA_BACKWARD_MODE) == 0) && ((mode & ALDA_FORWARD_MODE) == 0))
   {
      mode |= ALDA_FORWARD_MODE;
   }

   /* Check if the time values are initilized. */
   if (end_time_start == 0)
   {
      end_time_start = now;
   }
   if (end_time_end == 0)
   {
      end_time_end = now;
   }

   if (output_filename[0] == '\0')
   {
      output_fp = stdout;
   }
   else
   {
      /*
       * Set umask so that all log files have the permission 644.
       * If we do not set this here fopen() will create files with
       * permission 666 according to POSIX.1.
       */
#ifdef GROUP_CAN_WRITE
      (void)umask(S_IWOTH);
#else
      (void)umask(S_IWGRP | S_IWOTH);
#endif

      if ((output_fp = fopen(output_filename, "a")) == NULL)
      {
         (void)fprintf(stderr, "Failed to fopen() `%s' : %s (%s %d)\n",
                       output_filename, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   return;
}


/*++++++++++++++++++++++++ store_name_alias_id() ++++++++++++++++++++++++*/
static void
store_name_alias_id(char *input, int dir_host_type)
{
   unsigned int *p_alias_counter = NULL,
                **p_id = NULL, /* Silence compiler. */
                *p_id_counter = NULL,
                *p_name_counter = NULL;
   int          length,
                max_length = 0,
                max_alias_length = 0,
                max_id_length = 0,
                max_name_length = 0,
                type = NAME_TYPE;
   char         ***p_alias = NULL, /* Silence compiler. */
                ***p_name = NULL,  /* Silence compiler. */
                *ptr;

   /* Count number of parameters. */
   ptr = input;
   do
   {
      if (*ptr == '%')
      {
         ptr++;
         if (p_alias_counter == NULL)
         {
            if (dir_host_type == START_HOST_TYPE)
            {
               max_length = MAX_AFDNAME_LENGTH + 1;
               p_alias = &start_alias;
               p_alias_counter = &start_alias_counter;
            }
            else if (dir_host_type == END_HOST_TYPE)
                 {
                    max_length = MAX_AFDNAME_LENGTH + 1;
                    p_alias = &end_alias;
                    p_alias_counter = &end_alias_counter;
                 }
            else if (dir_host_type == SEARCH_HOST_TYPE)
                 {
                    max_length = MAX_HOSTNAME_LENGTH + ADDITIONAL_EXTRA_LENGTH + 1;
                    p_alias = &search_host_alias;
                    p_alias_counter = &search_host_alias_counter;
                 }
                 else
                 {
                    max_length = MAX_DIR_ALIAS_LENGTH + ADDITIONAL_EXTRA_LENGTH + 1;
                    p_alias = &search_dir_alias;
                    p_alias_counter = &search_dir_alias_counter;
                 }
            *p_alias_counter = 0;
            max_alias_length = 0;
            type = ALIAS_TYPE;
         }
      }
      else if (*ptr == '#')
           {
              ptr++;
              if (p_id_counter == NULL)
              {
                 if (dir_host_type == START_HOST_TYPE)
                 {
                    p_id = &start_id;
                    p_id_counter = &start_id_counter;
                 }
                 else if (dir_host_type == END_HOST_TYPE)
                      {
                         p_id = &end_id;
                         p_id_counter = &end_id_counter;
                      }
                 else if (dir_host_type == SEARCH_HOST_TYPE)
                      {
                         p_id = &search_host_id;
                         p_id_counter = &search_host_id_counter;
                      }
                      else
                      {
                         p_id = &search_dir_id;
                         p_id_counter = &search_dir_id_counter;
                      }
                 *p_id_counter = 0;
                 max_length = MAX_INT_LENGTH;
                 max_id_length = 0;
                 type = ID_TYPE;
              }
           }
           else
           {
              if (*ptr == '\\')
              {
                 ptr++;
              }
              if (p_name_counter == NULL)
              {
                 if (dir_host_type == START_HOST_TYPE)
                 {
                    max_length = MAX_REAL_HOSTNAME_LENGTH;
                    p_name = &start_name;
                    p_name_counter = &start_name_counter;
                 }
                 else if (dir_host_type == END_HOST_TYPE)
                      {
                         max_length = MAX_REAL_HOSTNAME_LENGTH;
                         p_name = &end_name;
                         p_name_counter = &end_name_counter;
                      }
                 else if (dir_host_type == SEARCH_HOST_TYPE)
                      {
                         max_length = MAX_REAL_HOSTNAME_LENGTH;
                         p_name = &search_host_name;
                         p_name_counter = &search_host_name_counter;
                      }
                      else
                      {
                         max_length = MAX_PATH_LENGTH;
                         p_name = &search_dir_name;
                         p_name_counter = &search_dir_name_counter;
                      }
                 *p_name_counter = 0;
                 max_name_length = 0;
                 type = NAME_TYPE;
              }
           }

      length = 0;
      do
      {
         if (*(ptr + length) == '\\')
         {
            ptr++;
         }
         length++;
      } while ((length < max_length) &&
               (*(ptr + length) != ',') && (*(ptr + length)  != '\0'));

      ptr += length;
      if (length >= max_length)
      {
         char *p_start = ptr - length,
              *tmp_ptr;

         while ((p_start >= input) && (*p_start != ',') &&
                ((p_start - 1) >= input) && (*(p_start - 1) != '\\'))
         {
            if (p_start == input)
            {
               break;
            }
            p_start--;
         }
         while ((*ptr != ',') && (*ptr != '\0'))
         {
            if (*ptr == '\\')
            {
               ptr++;
            }
            ptr++;
         }
         if (*ptr == ',')
         {
            *ptr = '\0';
            tmp_ptr = ptr;
         }
         else
         {
            tmp_ptr = NULL;
         }
         (void)fprintf(stderr,
                       "WARNING: Ignoring %s since it may only be %d bytes long.\n",
                       p_start, max_length);
         if (tmp_ptr != NULL)
         {
            *tmp_ptr = ',';
         }
      }
      else
      {
         if (type == ALIAS_TYPE)
         {
            if (length > max_alias_length)
            {
               max_alias_length = length;
            }
            (*p_alias_counter)++;
         }
         else if (type == ID_TYPE)
              {
                 if (length > max_id_length)
                 {
                    max_id_length = length;
                 }
                 (*p_id_counter)++;
              }
              else
              {
                 if (length > max_name_length)
                 {
                    max_name_length = length;
                 }
                 (*p_name_counter)++;
              }
      }
      if (*ptr == ',')
      {
         ptr++;
         length = 0;
      }
   } while (*ptr != '\0');

   /* ALlocate the necessary memory. */
   if (p_alias_counter != NULL)
   {
      RT_ARRAY(*p_alias, (*p_alias_counter), (max_alias_length + 1), char);
      *p_alias_counter = 0;
   }
   if (p_id_counter != NULL)
   {
      if ((*p_id = malloc((*p_id_counter) * sizeof(unsigned int))) == NULL)
      {
         (void)fprintf(stderr,
                       "ERROR  : Failed to malloc() %lu bytes.\n",
                       (long unsigned int)((*p_id_counter) * sizeof(unsigned int)));
      }
      *p_id_counter = 0;
   }
   if (p_name_counter != NULL)
   {
      RT_ARRAY(*p_name, (*p_name_counter), (max_name_length + 1), char);
      *p_name_counter = 0;
   }

   /* Lets store the input in the allocated area. */
   ptr = input;
   do
   {
       if (*ptr == '%')
       {
          ptr++;
       }
       else if (*ptr == '#')
            {
               ptr++;
            }
            else
            {
               if (*ptr == '\\')
               {
                  ptr++;
               }
            }

      length = 0;
      if (type == ALIAS_TYPE)
      {
         do
         {
            if (*(ptr + length) == '\\')
            {
               ptr++;
            }
            (*p_alias)[(*p_alias_counter)][length] = *(ptr + length);
            length++;
         } while ((*(ptr + length) != ',') && (*(ptr + length)  != '\0'));
         (*p_alias)[(*p_alias_counter)][length] = '\0';
         (*p_alias_counter)++;
      }
      else if (type == ID_TYPE)
           {
              char *tmp_ptr;

              do
              {
                 length++;
              } while ((*(ptr + length) != ',') && (*(ptr + length)  != '\0'));
              if (*(ptr + length) == ',')
              {
                 tmp_ptr = ptr + length;
                 *tmp_ptr = '\0';
              }
              else
              {
                 tmp_ptr = NULL;
              }
              *p_id[(*p_id_counter)] = (unsigned int)strtoul(ptr, NULL, 16);
              if (tmp_ptr != NULL)
              {
                 *tmp_ptr = ',';
              }
              (*p_id_counter)++;
           }
           else
           {
              do
              {
                 if (*(ptr + length) == '\\')
                 {
                    ptr++;
                 }
                 (*p_name)[(*p_name_counter)][length] = *(ptr + length);
                 length++;
              } while ((*(ptr + length) != ',') && (*(ptr + length)  != '\0'));
              (*p_name)[(*p_name_counter)][length] = '\0';
              (*p_name_counter)++;
           }

      ptr += length;
      if (*ptr == ',')
      {
         ptr++;
         length = 0;
      }
   } while (*ptr != '\0');

   return;
}


/*++++++++++++++++++++++++++ store_protocols() ++++++++++++++++++++++++++*/
static int
store_protocols(char *str_protocols)
{
   int  ret;
   char *p_start,
        *ptr;

   p_start = ptr = str_protocols;
   do
   {
      while ((*ptr != ',') && (*ptr != '\0'))
      {
         ptr++;
      }
      if (*ptr == ',')
      {
         *ptr = '\0';
         ptr++;
      }
      if (my_strcmp(p_start, ALDA_FTP_SHEME) == 0)
      {
         protocols |= ALDA_FTP_FLAG;
      }
      else if (my_strcmp(p_start, ALDA_LOC_SHEME) == 0)
           {
              protocols |= ALDA_LOC_FLAG;
           }
      else if (my_strcmp(p_start, ALDA_EXEC_SHEME) == 0)
           {
              protocols |= ALDA_EXEC_FLAG;
           }
      else if (my_strcmp(p_start, ALDA_SMTP_SHEME) == 0)
           {
              protocols |= ALDA_SMTP_FLAG;
           }
      else if (my_strcmp(p_start, ALDA_DEMAIL_SHEME) == 0)
           {
              protocols |= ALDA_DE_MAIL_FLAG;
           }
      else if (my_strcmp(p_start, ALDA_SFTP_SHEME) == 0)
           {
              protocols |= ALDA_SFTP_FLAG;
           }
      else if (my_strcmp(p_start, ALDA_SCP_SHEME) == 0)
           {
              protocols |= ALDA_SCP_FLAG;
           }
      else if (my_strcmp(p_start, ALDA_HTTP_SHEME) == 0)
           {
              protocols |= ALDA_HTTP_FLAG;
           }
      else if (my_strcmp(p_start, ALDA_HTTPS_SHEME) == 0)
           {
              protocols |= ALDA_HTTPS_FLAG;
           }
      else if (my_strcmp(p_start, ALDA_FTPS_SHEME) == 0)
           {
              protocols |= ALDA_FTPS_FLAG;
           }
      else if (my_strcmp(p_start, ALDA_WMO_SHEME) == 0)
           {
              protocols |= ALDA_WMO_FLAG;
           }
      else if (my_strcmp(p_start, ALDA_MAP_SHEME) == 0)
           {
              protocols |= ALDA_MAP_FLAG;
           }
      else if (my_strcmp(p_start, ALDA_DFAX_SHEME) == 0)
           {
              protocols |= ALDA_DFAX_FLAG;
           }
           else
           {
              (void)fprintf(stderr, "Unknown protocol `%s'.\n", p_start);
              return(INCORRECT);
           }
      p_start = ptr;
   } while (*ptr != '\0');

   if (protocols)
   {
      ret = SUCCESS;
   }
   else
   {
      ret = INCORRECT;
      (void)fprintf(stderr, "No protocol specified. (%s %d)\n",
                    __FILE__, __LINE__);
   }

   return(ret);
}


/*++++++++++++++++++++++++++++ insert_line() +++++++++++++++++++++++++++*/
/*                             -------------                            */
/* Taken from a stackoverflow awnser:                                   */
/*    https://stackoverflow.com/questions/61941666                      */
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
int
insert_line(char ***root, const char str[], size_t i)
{
   char *p = malloc(strlen(str) + 1);
   int  success = p != NULL;

   if (success)
   {
      char **tmp = realloc(*root, (i + 1) * sizeof(char *));

      if (success)
      {
         (void)strcpy(p, str);
         tmp[i] = p;
         *root = tmp;
      }
      else
      {
         free(p);
      }
   }

   return(success);
}


/*++++++++++++++++++++++++++++ eval_time() ++++++++++++++++++++++++++++++*/
/*                             -----------                               */
/* Syntax is as follows: <start time>[-<end time>]                       */
/* Accepted time formats for <start time> and <end time> are as follows: */
/*     Absolute: MMDDhhmm                                                */
/*               DDhhmm                                                  */
/*               hhmm                                                    */
/*     Relative: -DDhhmm                                                 */
/*               -hhmm                                                   */
/*               -mm                                                     */
/* Where MMDDhhmmss have the following meaning:                          */
/*        MM - The month as a decimal number (range 01 to 12).           */
/*        DD - The day of the month as a decimal number (range 01 to 31).*/
/*        hh - The hour as a decimal number using a 24-hour clock (range */
/*             00 to 23).                                                */
/*        mm - The minute as a decimal number (range 00 to 59).          */
/*        ss - The second as a decimal number (range 00 to 61).          */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int
eval_time(char *input, int type, time_t now)
{
   time_t value;
   char   *ptr;

   ptr = input;
   value = get_time_value(input, &ptr, now);
   switch (value)
   {
      case INCORRECT : /* Syntax is incorrect. */
         (void)fprintf(stderr, "Time syntax incorrect.\n");
         return(INCORRECT);

      case SUCCESS   : /* No time value, take current time. */
         value = now;
         break;

      default        : /* Some time value is set. */
         break;
   }
   if (type == START_TIME_TYPE)
   {
      start_time_start = value;
   }
   else if (type == END_TIME_TYPE)
        {
           end_time_start = value;
        }

   if (*ptr != '\0')
   {
      value = get_time_value(ptr, NULL, now);
      switch (value)
      {
         case INCORRECT : /* Syntax is incorrect. */
            (void)fprintf(stderr, "Time syntax incorrect.\n");
            return(INCORRECT);

         case SUCCESS   : /* No time value, take current time. */
            value = now;
            break;

         default        : /* Some time value is set. */
            break;
      }
      if (type == START_TIME_TYPE)
      {
         if (value >= start_time_start)
         {
            start_time_end = value;
         }
         else
         {
            (void)fprintf(stderr, "End value must be >= start time.\n");
            return(INCORRECT);
         }
      }
      else if (type == END_TIME_TYPE)
           {
              if (value >= end_time_start)
              {
                 end_time_end = value;
              }
              else
              {
                 (void)fprintf(stderr, "End value must be >= start time.\n");
                 return(INCORRECT);
              }
           }
   }

   return(SUCCESS);
}


/*-------------------------- get_time_value() ---------------------------*/
static int
get_time_value(char *input, char **ret_ptr, time_t now)
{
   int    length,
          max_length;
   time_t value = SUCCESS;
   char   *ptr;

   ptr = input;
   length = 0;
   if (*ptr == '-')
   {
      max_length = 6;
      ptr++;
   }
   else
   {
      max_length = 10;
   }
   while ((*(ptr + length) != '\0') && (*(ptr + length) != '-') &&
          (length < max_length))
   {
      length++;
   }
   if ((length > 0) && (length <= max_length))
   {
      char tmp_char;

      if (*(ptr + length) == '-')
      {
         tmp_char = '-';
         *(ptr + length) = '\0';
      }
      else
      {
         tmp_char = '\0';
      }
      if (max_length == 6)
      {
         if ((!isdigit((int)(*ptr))) || (!isdigit((int)(*(ptr + 1)))))
         {
            *(ptr + length) = tmp_char;
            return(INCORRECT);
         }

         if (length == 2) /* -mm */
         {
            int min;

            min = atoi(ptr);
            if ((min < 0) || (min > 99))
            {
               *(ptr + length) = tmp_char;
               return(INCORRECT);
            }

            value = now - (min * 60);
         }
         else if (length == 4) /* -hhmm */
              {
                 if ((!isdigit((int)(*(ptr + 2)))) ||
                     (!isdigit((int)(*(ptr + 3)))))
                 {
                    *(ptr + length) = tmp_char;
                    return(INCORRECT);
                 }
                 else
                 {
                    int  hour,
                         min;
                    char tmp_char2;

                    tmp_char2 = *(ptr + 2);
                    *(ptr + 2) = '\0';
                    hour = atoi(ptr);
                    *(ptr + 2) = tmp_char2;
                    if ((hour < 0) || (hour > 99))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    min = atoi((ptr + 2));
                    if ((min < 0) || (min > 59))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }

                    value = now - (min * 60) - (hour * 3600);
                 }
              }
         else if (length == 6) /* -DDhhmm */
              {
                 if ((!isdigit((int)(*(ptr + 2)))) ||
                     (!isdigit((int)(*(ptr + 3)))) ||
                     (!isdigit((int)(*(ptr + 4)))) ||
                     (!isdigit((int)(*(ptr + 5)))))
                 {
                    *(ptr + length) = tmp_char;
                    return(INCORRECT);
                 }
                 else
                 {
                    int  day,
                         hour,
                         min;
                    char tmp_char2;

                    tmp_char2 = *(ptr + 2);
                    *(ptr + 2) = '\0';
                    day = atoi(ptr);
                    *(ptr + 2) = tmp_char2;
                    if ((day < 0) || (day > 99))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    tmp_char2 = *(ptr + 4);
                    *(ptr + 4) = '\0';
                    hour = atoi((ptr + 2));
                    *(ptr + 4) = tmp_char2;
                    if ((hour < 0) || (hour > 23))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    min = atoi((ptr + 4));
                    if ((min < 0) || (min > 59))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }

                    value = now - (min * 60) - (hour * 3600) - (day * 86400);
                 }
              }
              else
              {
                 *(ptr + length) = tmp_char;
                 return(INCORRECT);
              }
      }
      else
      {
         if ((!isdigit((int)(*ptr))) || (!isdigit((int)(*(ptr + 1)))) ||
             (!isdigit((int)(*(ptr + 2)))) || (!isdigit((int)(*(ptr + 3)))))
         {
            *(ptr + length) = tmp_char;
            return(INCORRECT);
         }

         if (length == 4) /* hhmm */
         {
            int       hour,
                      min;
            char      tmp_char2;
            struct tm *bd_time;     /* Broken-down time. */

            tmp_char2 = *(ptr + 2);
            *(ptr + 2) = '\0';
            hour = atoi(ptr);
            *(ptr + 2) = tmp_char2;
            if ((hour < 0) || (hour > 23))
            {
               *(ptr + length) = tmp_char;
               return(INCORRECT);
            }
            min = atoi((ptr + 2));
            if ((min < 0) || (min > 59))
            {
               *(ptr + length) = tmp_char;
               return(INCORRECT);
            }
            if ((bd_time = localtime(&now)) == NULL)
            {
               (void)fprintf(stderr, "Failed to determine localtime() : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               *(ptr + length) = tmp_char;
               return(INCORRECT);
            }
            bd_time->tm_sec  = 0;
            bd_time->tm_min  = min;
            bd_time->tm_hour = hour;

            value = mktime(bd_time);
         }
         else if (length == 6) /* DDhhmm */
              {
                 if ((!isdigit((int)(*(ptr + 4)))) ||
                     (!isdigit((int)(*(ptr + 5)))))
                 {
                    *(ptr + length) = tmp_char;
                    return(INCORRECT);
                 }
                 else
                 {
                    int       day,
                              hour,
                              min;
                    char      tmp_char2;
                    struct tm *bd_time;     /* Broken-down time. */

                    tmp_char2 = *(ptr + 2);
                    *(ptr + 2) = '\0';
                    day = atoi(ptr);
                    *(ptr + 2) = tmp_char2;
                    if ((day < 0) || (day > 31))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    tmp_char2 = *(ptr + 4);
                    *(ptr + 4) = '\0';
                    hour = atoi((ptr + 2));
                    *(ptr + 4) = tmp_char2;
                    if ((hour < 0) || (hour > 23))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    min = atoi((ptr + 4));
                    if ((min < 0) || (min > 59))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    if ((bd_time = localtime(&now)) == NULL)
                    {
                       (void)fprintf(stderr, "Failed to determine localtime() : %s (%s %d)\n",
                                     strerror(errno), __FILE__, __LINE__);
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    bd_time->tm_sec  = 0;
                    bd_time->tm_min  = min;
                    bd_time->tm_hour = hour;
                    bd_time->tm_mday = day;

                    value = mktime(bd_time);
                 }
              }
         else if (length == 8) /* MMDDhhmm */
              {
                 if ((!isdigit((int)(*(ptr + 4)))) ||
                     (!isdigit((int)(*(ptr + 5)))) ||
                     (!isdigit((int)(*(ptr + 6)))) ||
                     (!isdigit((int)(*(ptr + 7)))))
                 {
                    *(ptr + length) = tmp_char;
                    return(INCORRECT);
                 }
                 else
                 {
                    int       day,
                              hour,
                              min,
                              month;
                    char      tmp_char2;
                    struct tm *bd_time;     /* Broken-down time. */

                    tmp_char2 = *(ptr + 2);
                    *(ptr + 2) = '\0';
                    month = atoi(ptr);
                    *(ptr + 2) = tmp_char2;
                    if ((month < 0) || (month > 12))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    tmp_char2 = *(ptr + 4);
                    *(ptr + 4) = '\0';
                    day = atoi(ptr + 2);
                    *(ptr + 4) = tmp_char2;
                    if ((day < 0) || (day > 31))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    tmp_char2 = *(ptr + 6);
                    *(ptr + 6) = '\0';
                    hour = atoi((ptr + 4));
                    *(ptr + 6) = tmp_char2;
                    if ((hour < 0) || (hour > 23))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    min = atoi((ptr + 6));
                    if ((min < 0) || (min > 59))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    if ((bd_time = localtime(&now)) == NULL)
                    {
                       (void)fprintf(stderr, "Failed to determine localtime() : %s (%s %d)\n",
                                     strerror(errno), __FILE__, __LINE__);
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    bd_time->tm_sec  = 0;
                    bd_time->tm_min  = min;
                    bd_time->tm_hour = hour;
                    bd_time->tm_mday = day;
                    if ((bd_time->tm_mon == 0) && (month == 12))
                    {
                       bd_time->tm_year -= 1;
                    }
                    bd_time->tm_mon  = month - 1;

                    value = mktime(bd_time);
                 }
              }
         else if (length == 10) /* MMDDhhmmss */
              {
                 if ((!isdigit((int)(*(ptr + 4)))) ||
                     (!isdigit((int)(*(ptr + 5)))) ||
                     (!isdigit((int)(*(ptr + 6)))) ||
                     (!isdigit((int)(*(ptr + 7)))) ||
                     (!isdigit((int)(*(ptr + 8)))) ||
                     (!isdigit((int)(*(ptr + 9)))))
                 {
                    *(ptr + length) = tmp_char;
                    return(INCORRECT);
                 }
                 else
                 {
                    int       day,
                              hour,
                              min,
                              month,
                              second;
                    char      tmp_char2;
                    struct tm *bd_time;     /* Broken-down time. */

                    tmp_char2 = *(ptr + 2);
                    *(ptr + 2) = '\0';
                    month = atoi(ptr);
                    *(ptr + 2) = tmp_char2;
                    if ((month < 0) || (month > 12))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    tmp_char2 = *(ptr + 4);
                    *(ptr + 4) = '\0';
                    day = atoi(ptr + 2);
                    *(ptr + 4) = tmp_char2;
                    if ((day < 0) || (day > 31))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    tmp_char2 = *(ptr + 6);
                    *(ptr + 6) = '\0';
                    hour = atoi((ptr + 4));
                    *(ptr + 6) = tmp_char2;
                    if ((hour < 0) || (hour > 23))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    tmp_char2 = *(ptr + 8);
                    *(ptr + 8) = '\0';
                    min = atoi((ptr + 6));
                    *(ptr + 8) = tmp_char2;
                    if ((min < 0) || (min > 59))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    second = atoi((ptr + 8));
                    if ((second < 0) || (second > 61))
                    {
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    if ((bd_time = localtime(&now)) == NULL)
                    {
                       (void)fprintf(stderr, "Failed to determine localtime() : %s (%s %d)\n",
                                     strerror(errno), __FILE__, __LINE__);
                       *(ptr + length) = tmp_char;
                       return(INCORRECT);
                    }
                    bd_time->tm_sec  = second;
                    bd_time->tm_min  = min;
                    bd_time->tm_hour = hour;
                    bd_time->tm_mday = day;
                    if ((bd_time->tm_mon == 0) && (month == 12))
                    {
                       bd_time->tm_year -= 1;
                    }
                    bd_time->tm_mon  = month - 1;

                    value = mktime(bd_time);
                 }
              }
              else
              {
                 *(ptr + length) = tmp_char;
                 return(INCORRECT);
              }
      }

      ptr += length;
      *ptr = tmp_char;
      if (ret_ptr != NULL)
      {
         if (*ptr == '\0')
         {
            *ret_ptr = ptr;
         }
         else
         {
            *ret_ptr = ptr + 1;
         }
      }
   }
   else
   {
      value = INCORRECT;
   }

   return(value);
}


/*+++++++++++++++++++++++++++++ usage() +++++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "Usage: %s [options] <file name pattern>\n", progname);
   (void)fprintf(stderr, "\n    Mode options\n");
   (void)fprintf(stderr, "           -c                           continuous\n");
   (void)fprintf(stderr, "           -C                           continuous daemon\n");
   (void)fprintf(stderr, "           -l                           local log data (default)\n");
   (void)fprintf(stderr, "           -r                           remote log data\n");
   (void)fprintf(stderr, "           -b                           back trace data\n");
   (void)fprintf(stderr, "           -f                           forward trace data\n");
   (void)fprintf(stderr, "    Range parameters\n");
   (void)fprintf(stderr, "           -s <AFD host name/alias/ID>  Starting AFD hostname/alias/ID.\n");
   (void)fprintf(stderr, "           -e <AFD host name/alias/ID>  Ending AFD hostname/alias/ID.\n");
   (void)fprintf(stderr, "           -t <start>[-<end>]           Time frame at starting point.\n");
   (void)fprintf(stderr, "              Time is specified as follows:\n");
   (void)fprintf(stderr, "                 Absolute: MMDDhhmmss, MMDDhhmm, DDhhmm or hhmm\n");
   (void)fprintf(stderr, "                 Relative: -DDhhmm, -hhmm or -mm\n");
   (void)fprintf(stderr, "              Where MMDDhhmmss have the following meaning:\n");
   (void)fprintf(stderr, "                 MM - The month as a decimal number (range 01 to 12).\n");
   (void)fprintf(stderr, "                 DD - The day of the month as a decimal number (range 01 to 31).\n");
   (void)fprintf(stderr, "                 hh - The hour as a decimal number using a 24-hour clock (range\n");
   (void)fprintf(stderr, "                      00 to 23).\n");
   (void)fprintf(stderr, "                 mm - The minute as a decimal number (range 00 to 59).\n");
   (void)fprintf(stderr, "                 ss - The second as a decimal number (range 00 to 61).\n");
   (void)fprintf(stderr, "           -T <start>[-<end>]           Time frame at end point.\n");
   (void)fprintf(stderr, "           -L <log type>                Search only in given log type.\n");
   (void)fprintf(stderr, "                                        Log type can be:\n");
#ifdef _INPUT_LOG
   (void)fprintf(stderr, "                                          I - Input Log\n");
#endif
#ifdef _DISTRIBUTION_LOG
   (void)fprintf(stderr, "                                          U - Distribution Log\n");
#endif
#ifdef _PRODUCTION_LOG
   (void)fprintf(stderr, "                                          P - Production Log\n");
#endif
#ifdef _OUTPUT_LOG
# if defined(_WITH_DE_MAIL_SUPPORT) && !defined(_CONFIRMATION_LOG)
   (void)fprintf(stderr, "                                          C - Output Log confirmed\n");
# endif
   (void)fprintf(stderr, "                                          R - Output Log retrieved\n");
   (void)fprintf(stderr, "                                          O - Output Log delivered\n");
#endif
#ifdef _DELETE_LOG
   (void)fprintf(stderr, "                                          D - Delete Log\n");
#endif
   (void)fprintf(stderr, "                                        Default: IUPOD\n");
   (void)fprintf(stderr, "           -g <time in seconds>         Maximum time to search for\n");
   (void)fprintf(stderr, "                                        a single file before giving up.\n");
   (void)fprintf(stderr, "           -G <time in minutes>         Maximum time we may search\n");
   (void)fprintf(stderr, "                                        for all files.\n");
   (void)fprintf(stderr, "    Format parameters\n");
   (void)fprintf(stderr, "           -o <format>                  Specifies the output format.\n");
   (void)fprintf(stderr, "                                        Possible format parameters\n");
   (void)fprintf(stderr, "                                        are as listed:\n");
#ifdef _INPUT_LOG
   (void)fprintf(stderr, "              -- Input log data --\n");
   (void)fprintf(stderr, "              %%[Z]IT<time char>      - input time\n");
   (void)fprintf(stderr, "              %%[Y]IF                 - input file name\n");
   (void)fprintf(stderr, "              %%[X]IS<size char>      - input file size\n");
   (void)fprintf(stderr, "              %%[Z]II                 - input source ID\n");
   (void)fprintf(stderr, "              %%[Y]IN                 - full source name\n");
   (void)fprintf(stderr, "              %%[Z]IU                 - unique number\n");
#endif
#ifdef _DISTRIBUTION_LOG
   (void)fprintf(stderr, "              -- Distribution log data --\n");
   (void)fprintf(stderr, "              %%[Z]Ut<time char>      - distribution time\n");
   (void)fprintf(stderr, "              %%[Z]UT<time char>      - input time\n");
   (void)fprintf(stderr, "              %%[Y]UF                 - input file name\n");
   (void)fprintf(stderr, "              %%[X]US<size char>      - input file size\n");
   (void)fprintf(stderr, "              %%[Z]UI                 - input source ID\n");
# ifndef _INPUT_LOG
   (void)fprintf(stderr, "              %%[Y]UN                 - full source name\n");
# endif
   (void)fprintf(stderr, "              %%[Z]UU                 - unique number\n");
   (void)fprintf(stderr, "              %%[Z]Un                 - number of jobs distributed\n");
   (void)fprintf(stderr, "              %%[Z]Uj<separator char> - list of job ID's\n");
   (void)fprintf(stderr, "              %%[Z]Uc<separator char> - list of number of pre-processing\n");
   (void)fprintf(stderr, "              %%[Z]UY                 - distribution type\n");
#endif
#ifdef _PRODUCTION_LOG
   (void)fprintf(stderr, "              -- Production log data --\n");
   (void)fprintf(stderr, "              %%[Z]Pt<time char>      - time when production starts\n");
   (void)fprintf(stderr, "              %%[Z]PT<time char>      - time when production finished\n");
   (void)fprintf(stderr, "              %%[X]PD<duration char>  - production time (duration)\n");
   (void)fprintf(stderr, "              %%[X]Pu<duration char>  - CPU usage\n");
   (void)fprintf(stderr, "              %%[Z]Pb                 - ratio relationship 1\n");
   (void)fprintf(stderr, "              %%[Z]PB                 - ratio relationship 2\n");
   (void)fprintf(stderr, "              %%[Z]PJ                 - job ID\n");
   (void)fprintf(stderr, "              %%[Z]PZ<time char>      - job creation time\n");
   (void)fprintf(stderr, "              %%[Z]PU                 - unique number\n");
   (void)fprintf(stderr, "              %%[Z]PL                 - split job number\n");
   (void)fprintf(stderr, "              %%[Y]Pf                 - input file name\n");
   (void)fprintf(stderr, "              %%[X]Ps<size char>      - input file size\n");
   (void)fprintf(stderr, "              %%[Y]PF                 - produced file name\n");
   (void)fprintf(stderr, "              %%[X]PS<size char>      - produced file size\n");
   (void)fprintf(stderr, "              %%[Y]PC                 - command executed\n");
   (void)fprintf(stderr, "              %%[Z]PR                 - return code of command executed\n");
#endif
#ifdef _OUTPUT_LOG
   (void)fprintf(stderr, "              -- Output log data --\n");
   (void)fprintf(stderr, "              %%[Z]Ot<time char>      - time when sending starts\n");
   (void)fprintf(stderr, "              %%[Z]OT<time char>      - time when file is transmitted\n");
   (void)fprintf(stderr, "              %%[X]OD<duration char>  - time taken to transmit file\n");
   (void)fprintf(stderr, "              %%[Y]Of                 - local output file name\n");
   (void)fprintf(stderr, "              %%[Y]OF                 - remote output file name/directory\n");
   (void)fprintf(stderr, "              %%[Y]OE                 - final output file name/directory\n");
   (void)fprintf(stderr, "              %%[Z]Op                 - protocol ID used for transmission\n");
   (void)fprintf(stderr, "              %%[Y]OP                 - protocol used for transmission\n");
   (void)fprintf(stderr, "              %%[X]OS<size char>      - output file size\n");
   (void)fprintf(stderr, "              %%[Z]OJ                 - job ID\n");
   (void)fprintf(stderr, "              %%[Z]Oe                 - number of retries\n");
   (void)fprintf(stderr, "              %%[Y]OA                 - archive directory\n");
   (void)fprintf(stderr, "              %%[Z]OZ<time char>      - job creation time\n");
   (void)fprintf(stderr, "              %%[Z]OU                 - unique number\n");
   (void)fprintf(stderr, "              %%[Z]OL                 - split job number\n");
   (void)fprintf(stderr, "              %%[Y]OM                 - mail queue ID\n");
   (void)fprintf(stderr, "              %%[Y]Oh                 - target real hostname/IP\n");
   (void)fprintf(stderr, "              %%[Y]OH                 - target alias name\n");
   (void)fprintf(stderr, "              %%[Y]OR                 - Recipient of job\n");
   (void)fprintf(stderr, "              %%[Z]Oo                 - output type ID\n");
   (void)fprintf(stderr, "              %%[Y]OO                 - output type string\n");
#endif
#ifdef _DELETE_LOG
   (void)fprintf(stderr, "              -- Delete log data --\n");
   (void)fprintf(stderr, "              %%[Z]Dt<time char>      - time when job was created\n");
   (void)fprintf(stderr, "              %%[Z]DT<time char>      - time when file was deleted\n");
   (void)fprintf(stderr, "              %%[Z]Dr                 - delete reason ID\n");
   (void)fprintf(stderr, "              %%[Y]DR                 - delete reason string\n");
   (void)fprintf(stderr, "              %%[Y]DW                 - user/program causing deletion\n");
   (void)fprintf(stderr, "              %%[Y]DA                 - additional reason\n");
   (void)fprintf(stderr, "              %%[Z]DZ<time char>      - job creation time\n");
   (void)fprintf(stderr, "              %%[Z]DU                 - unique number\n");
   (void)fprintf(stderr, "              %%[Z]DL                 - split job number\n");
   (void)fprintf(stderr, "              %%[Y]DF                 - file name of deleted file\n");
   (void)fprintf(stderr, "              %%[X]DS<size char>      - file size of deleted file\n");
   (void)fprintf(stderr, "              %%[Z]DJ                 - job ID of deleted file\n");
   (void)fprintf(stderr, "              %%[Z]DI                 - input source ID\n");
   (void)fprintf(stderr, "              %%[Y]DN                 - full source name\n");
   (void)fprintf(stderr, "              %%[Y]DH                 - target alias name\n");
#endif
   (void)fprintf(stderr, "              -- AFD information --\n");
   (void)fprintf(stderr, "              %%[Y]Ah                 - AFD real hostname/IP\n");
   (void)fprintf(stderr, "              %%[Y]AH                 - AFD alias name\n");
   (void)fprintf(stderr, "              %%[Y]AV                 - AFD version\n");
   (void)fprintf(stderr, "\n               [X] -> [-][0]#[.#]] or [-][0]#[d|o|x]\n");
   (void)fprintf(stderr, "               [Y] -> [-]# or [<individual character positions>]\n");
   (void)fprintf(stderr, "               [Z] -> [-][0]#[d|o|x]\n");
   (void)fprintf(stderr, "\n            Time character (t,T):\n");
   (void)fprintf(stderr, "                 a - Abbreviated weekday name: Tue\n");
   (void)fprintf(stderr, "                 A - Full weekday name: Tuesday\n");
   (void)fprintf(stderr, "                 b - Abbreviated month name: Jan\n");
   (void)fprintf(stderr, "                 B - Full month name: January\n");
   (void)fprintf(stderr, "                 c - Date and time: Tue Jan 19 16:24:50 1999\n");
   (void)fprintf(stderr, "                 d - Day of the month [01 - 31]: 19\n");
   (void)fprintf(stderr, "                 H - Hour of the 24-hour day [00 - 23]: 16\n");
   (void)fprintf(stderr, "                 I - Hour of the 24-hour day [00 - 12]: 04\n");
   (void)fprintf(stderr, "                 j - Day of the year [001 - 366]: 19\n");
   (void)fprintf(stderr, "                 m - Month [01 - 12]: 01\n");
   (void)fprintf(stderr, "                 M - Minute [00 - 59]: 24\n");
   (void)fprintf(stderr, "                 p - AM/PM: PM\n");
   (void)fprintf(stderr, "                 S - Second [00 - 61]: 50\n");
   (void)fprintf(stderr, "             (*) u - Unix time: 916759490\n");
   (void)fprintf(stderr, "                 U - Sunday week number [00 - 53]: 02\n");
   (void)fprintf(stderr, "                 w - Weekday [0 - 6] (0=Sunday): 2\n");
   (void)fprintf(stderr, "                 W - Monday week number [00 - 53]: 02\n");
   (void)fprintf(stderr, "                 X - Time: 16:24:50\n");
   (void)fprintf(stderr, "                 y - Year without century [00 - 99]: 99\n");
   (void)fprintf(stderr, "                 Y - Year with century: 1999\n");
   (void)fprintf(stderr, "                 Z - Time zone name: CET\n");
   (void)fprintf(stderr, "            Duration character (D,u):\n");
   (void)fprintf(stderr, "                 A - Automatic shortest format: 4d\n");
   (void)fprintf(stderr, "                             d - days\n");
   (void)fprintf(stderr, "                             h - hours\n");
   (void)fprintf(stderr, "                             m - minutes\n");
   (void)fprintf(stderr, "                             s - seconds\n");
   (void)fprintf(stderr, "             (*) D - Days only : 4\n");
   (void)fprintf(stderr, "             (*) H - Hours only : 102\n");
   (void)fprintf(stderr, "             (*) M - Minutes only: 6144\n");
   (void)fprintf(stderr, "             (*) S - Seconds only: 368652\n");
   (void)fprintf(stderr, "                 X - Time (h:mm:ss): 102:24:12\n");
   (void)fprintf(stderr, "                 Y - Time (d:hh:mm): 4:06:24\n");
   (void)fprintf(stderr, "            Size character (S):\n");
   (void)fprintf(stderr, "             (#) a - Automatic shortest format: 1 GB\n");
   (void)fprintf(stderr, "                             B  - byte\n");
   (void)fprintf(stderr, "                             KB - kilobyte (10^3)\n");
   (void)fprintf(stderr, "                             MB - megabyte (10^6)\n");
   (void)fprintf(stderr, "                             GB - gigabyte (10^9)\n");
   (void)fprintf(stderr, "                             TB - terabyte (10^12)\n");
   (void)fprintf(stderr, "                             PB - petabyte (10^15)\n");
   (void)fprintf(stderr, "                             EB - exabyte  (10^18)\n");
   (void)fprintf(stderr, "             (#) A - Automatic shortest format: 1 GiB\n");
   (void)fprintf(stderr, "                             B   - byte\n");
   (void)fprintf(stderr, "                             KiB - kibibyte (2^10)\n");
   (void)fprintf(stderr, "                             MiB - mebibyte (2^20)\n");
   (void)fprintf(stderr, "                             GiB - gibibyte (2^30)\n");
   (void)fprintf(stderr, "                             TiB - tebibyte (2^40)\n");
   (void)fprintf(stderr, "                             PiB - pebibyte (2^50)\n");
   (void)fprintf(stderr, "                             EiB - exbibyte (2^60)\n");
   (void)fprintf(stderr, "             (#) B - Bytes only: 1884907510\n");
   (void)fprintf(stderr, "             (#) e - Exabyte only : 0\n");
   (void)fprintf(stderr, "             (#) E - Exbibyte only: 0\n");
   (void)fprintf(stderr, "             (#) g - Gigabyte only: 1\n");
   (void)fprintf(stderr, "             (#) G - Gibibyte only: 1\n");
   (void)fprintf(stderr, "             (#) k - Kilobyte only: 1884907\n");
   (void)fprintf(stderr, "             (#) K - Kibibyte only: 1840729\n");
   (void)fprintf(stderr, "             (#) m - Megabyte only: 1884\n");
   (void)fprintf(stderr, "             (#) M - Mebibyte only: 1797\n");
   (void)fprintf(stderr, "             (#) p - Petabyte only: 0\n");
   (void)fprintf(stderr, "             (#) P - Pebibyte only: 0\n");
   (void)fprintf(stderr, "             (#) t - Terabyte only: 0\n");
   (void)fprintf(stderr, "             (#) T - Tebibyte only: 0\n");
   (void)fprintf(stderr, "\n            (*) Can be printed as decimal (d), octal (o) or hexadecimal (x)\n");
   (void)fprintf(stderr, "            (#) Can be printed as numeric string with decimal point\n");
   (void)fprintf(stderr, "                                      OR\n");
   (void)fprintf(stderr, "                Can be printed as decimal (d), octal (o) or hexadecimal (x)\n");
   (void)fprintf(stderr, "    Search parameters\n");
   (void)fprintf(stderr, "            -d <directory name/alias/ID> Directory name, alias or ID.\n");
   (void)fprintf(stderr, "                                            dir name no prefix\n");
   (void)fprintf(stderr, "                                            dir alias use prefix %%\n");
   (void)fprintf(stderr, "                                            dir ID use prefix #\n");
   (void)fprintf(stderr, "            -h <host name/alias/ID>      Host name, alias or ID.\n");
   (void)fprintf(stderr, "                                            host name no prefix\n");
   (void)fprintf(stderr, "                                            host alias use prefix %%\n");
   (void)fprintf(stderr, "                                            host ID use prefix #\n");
   (void)fprintf(stderr, "            -j <job ID>                  Job identifier.\n");
   (void)fprintf(stderr, "            -u <unique number>           Unique number.\n");
   (void)fprintf(stderr, "            -z <size>                    Original file size in byte.\n");
   (void)fprintf(stderr, "                                         (Production log only!)\n");
   (void)fprintf(stderr, "            -S[I|U|P|O|D] <size>         File size in byte.\n");
   (void)fprintf(stderr, "            -D[P|O] <time>               Duration in seconds.\n");
   (void)fprintf(stderr, "            -p <protocol>                Protocol used for transport:\n");
   (void)fprintf(stderr, "                                          %s\n", ALDA_FTP_SHEME);
   (void)fprintf(stderr, "                                          %s\n", ALDA_LOC_SHEME);
   (void)fprintf(stderr, "                                          %s\n", ALDA_EXEC_SHEME);
   (void)fprintf(stderr, "                                          %s\n", ALDA_SMTP_SHEME);
   (void)fprintf(stderr, "                                          %s\n", ALDA_DEMAIL_SHEME);
   (void)fprintf(stderr, "                                          %s\n", ALDA_SFTP_SHEME);
   (void)fprintf(stderr, "                                          %s\n", ALDA_SCP_SHEME);
   (void)fprintf(stderr, "                                          %s\n", ALDA_HTTP_SHEME);
   (void)fprintf(stderr, "                                          %s\n", ALDA_HTTPS_SHEME);
   (void)fprintf(stderr, "                                          %s\n", ALDA_FTPS_SHEME);
   (void)fprintf(stderr, "                                          %s\n", ALDA_WMO_SHEME);
   (void)fprintf(stderr, "                                          %s\n", ALDA_MAP_SHEME);
   (void)fprintf(stderr, "                                          %s\n", ALDA_DFAX_SHEME);
   (void)fprintf(stderr, "    Other parameters\n");
   (void)fprintf(stderr, "            -F <file name>               Footer to add to output.\n");
   (void)fprintf(stderr, "            -R <x>                       Rotate log x times.\n");
   (void)fprintf(stderr, "            -H <file name>               Header to add to output.\n");
   (void)fprintf(stderr, "            -O <file name>               File where to write output.\n");
   (void)fprintf(stderr, "            -v[v[v[v[v[v]]]]]            Verbose mode.\n");
   (void)fprintf(stderr, "            -w <work dir>                Working directory of the AFD.\n");
   (void)fprintf(stderr, "            --header_line=<line>         Add the given header line to\n");
   (void)fprintf(stderr, "                                         output. The following\n");
   (void)fprintf(stderr, "                                         %% parameters can be used to\n");
   (void)fprintf(stderr, "                                         insert additional system\n");
   (void)fprintf(stderr, "                                         infomation:\n");
   (void)fprintf(stderr, "                                           %%I - inode number of the log file\n");
#ifdef HAVE_GETHOSTID
   (void)fprintf(stderr, "                                           %%H - host ID\n");
#endif
   (void)fprintf(stderr, "    To be able to differentiate between name, alias and ID:\n");
   (void)fprintf(stderr, "        alias - must always begin with %%\n");
   (void)fprintf(stderr, "        ID    - must always begin with #\n");
   (void)fprintf(stderr, "        name  - just the name without extra identifier\n");

   return;
}
