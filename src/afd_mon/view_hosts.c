/*
 *  view_hosts.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   view_hosts - lists all hosts served by all AFD's in MSA
 **
 ** SYNOPSIS
 **   view_hosts [working directory] <option> <host name>
 **                                   -a  search alias names
 **                                   -r  search real names
 **                                   -A  search AFD alias names
 **                                   -C  same as -A, just show first found
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.06.1999 H.Kiehl Created
 **   31.03.2017 H.Kiehl Show better help text.
 **                      Handle group names.
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* malloc()                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                       /* O_RDWR, O_CREAT              */
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include "mondefs.h"
#include "version.h"

/* Global variables. */
int                    sys_log_fd = STDERR_FILENO,   /* Not used!    */
                       msa_id,
                       msa_fd = -1,
                       no_of_afds = 0;
off_t                  msa_size;
char                   *p_work_dir;
struct mon_status_area *msa;
struct afd_host_list   **ahl;
const char             *sys_log_name = MON_SYS_LOG_FIFO;

/* Local function prototypes. */
static void            usage(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ view_hosts() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  get_afd_alias = NO,
        get_afd_alias_cut = NO,
        check_host_alias,
        i,
        j,
        no_of_filters;
   char work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);

   if (get_mon_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   if ((argc > 2) && (argv[1][0] == '-') &&
       ((argv[1][1] == 'A') || (argv[1][1] == 'C') ||
        (argv[1][1] == 'a') || (argv[1][1] == 'r')) &&
       (argv[1][2] == '\0'))
   {
      if (argv[1][1] == 'a')
      {
         check_host_alias = YES;
      }
      else if (argv[1][1] == 'A')
           {
              get_afd_alias = YES;
           }
      else if (argv[1][1] == 'C')
           {
              get_afd_alias_cut = YES;
           }
           else
           {
              check_host_alias = NO;
           }
      no_of_filters = argc - 2;
   }
   else
   {
      usage();
      exit(INCORRECT);
   }

   if ((i = msa_attach_passive()) < 0)
   {
      if (i == INCORRECT_VERSION)
      {
         (void)fprintf(stderr, "ERROR   : This program is not able to attach to the MSA due to incorrect version. (%s %d)\n",
                       __FILE__, __LINE__);
      }
      else
      {
         (void)fprintf(stderr, "ERROR   : Failed to attach to MSA. (%s %d)\n",
                       __FILE__, __LINE__);
      }
      exit(INCORRECT);
   }
   if ((get_afd_alias == YES) || (get_afd_alias_cut == YES))
   {
      for (i = 0; i < no_of_afds; i++)
      {
         for (j = 0; j < no_of_filters; j++)
         {
            if ((pmatch(argv[j + 2], msa[i].hostname[0], NULL) == 0) ||
                ((msa[i].hostname[1][0] != '\0') &&
                 (pmatch(argv[j + 2], msa[i].hostname[1], NULL) == 0)))
            {
               if (get_afd_alias_cut == YES)
               {
                  (void)fprintf(stdout, "%s", msa[i].afd_alias);
                  j = no_of_filters;
                  i = no_of_afds;
               }
               else
               {
                  (void)fprintf(stdout, "%s ", msa[i].afd_alias);
               }
            }
         }
      }
      (void)fprintf(stdout, "\n");
   }
   else
   {
      int          ahl_fd,
                   k,
                   show_afd_name,
                   show_header = YES;
      char         ahl_file[MAX_PATH_LENGTH],
                   *p_ahl_file,
                   *ptr;
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat  stat_buf;
#endif

      p_ahl_file = ahl_file +
                   sprintf(ahl_file, "%s%s%s",
                           p_work_dir, FIFO_DIR, AHL_FILE_NAME);
      if ((ahl = malloc(no_of_afds * sizeof(struct afd_host_list *))) == NULL)
      {
         (void)fprintf(stderr,
                       "ERROR   : Failed to malloc() memory : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      for (i = 0; i < no_of_afds; i++)
      {
         (void)sprintf(p_ahl_file, "%s", msa[i].afd_alias);
#ifdef HAVE_STATX
         if ((statx(0, ahl_file, AT_STATX_SYNC_AS_STAT,
                    STATX_SIZE, &stat_buf) == 0) && (stat_buf.stx_size > 0))
#else
         if ((stat(ahl_file, &stat_buf) == 0) && (stat_buf.st_size > 0))
#endif
         {
            if ((ptr = map_file(ahl_file, &ahl_fd, NULL, &stat_buf,
                                O_RDWR | O_CREAT, FILE_MODE)) == (caddr_t) -1)
            {
               (void)fprintf(stderr,
                             "ERROR : Failed to mmap() to %s : %s (%s %d)\n",
                             ahl_file, strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            ahl[i] = (struct afd_host_list *)ptr;
            if (close(ahl_fd) == -1)
            {
               (void)fprintf(stderr, "DEBUG : close() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
            }
         }
         else
         {
            ahl[i] = NULL;
         }
      }

      if (check_host_alias == YES)
      {
         for (i = 0; i < no_of_afds; i++)
         {
            if (ahl[i] != NULL)
            {
               show_afd_name = YES;
               for (j = 0; j < msa[i].no_of_hosts; j++)
               {
                  for (k = 0; k < no_of_filters; k++)
                  {
                     if (pmatch(argv[k + 2], ahl[i][j].host_alias, NULL) == 0)
                     {
                        if (show_header == YES)
                        {
                           (void)fprintf(stdout, "%-*s Pos   %-*s %-*s %-*s\n",
                                                 MAX_AFDNAME_LENGTH,
                                                 "AFD Alias",
                                                 MAX_HOSTNAME_LENGTH,
                                                 "alias",
                                                 25,
                                                 "real hostname 1",
                                                 25,
                                                 "real hostname 2");
                           show_header = NO;
                        }
                        if (show_afd_name == YES)
                        {
                           show_afd_name = NO;
                           if (ahl[i][j].real_hostname[0][0] == GROUP_IDENTIFIER)
                           {
                              (void)fprintf(stdout, "%-*s %-4d: %-*s\n",
                                            MAX_AFDNAME_LENGTH,
                                            msa[i].afd_alias, j,
                                            MAX_HOSTNAME_LENGTH,
                                            ahl[i][j].host_alias);
                           }
                           else
                           {
                              if (ahl[i][j].real_hostname[1][0] == '\0')
                              {
                                 (void)fprintf(stdout, "%-*s %-4d: %-*s %-*s\n",
                                               MAX_AFDNAME_LENGTH,
                                               msa[i].afd_alias, j,
                                               MAX_HOSTNAME_LENGTH,
                                               ahl[i][j].host_alias,
                                               25,
                                               ahl[i][j].real_hostname[0]);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "%-*s %-4d: %-*s %-*s %-*s\n",
                                               MAX_AFDNAME_LENGTH,
                                               msa[i].afd_alias, j,
                                               MAX_HOSTNAME_LENGTH,
                                               ahl[i][j].host_alias,
                                               25,
                                               ahl[i][j].real_hostname[0],
                                               25,
                                               ahl[i][j].real_hostname[1]);
                              }
                           }
                        }
                        else
                        {
                           if (ahl[i][j].real_hostname[0][0] == GROUP_IDENTIFIER)
                           {
                              (void)fprintf(stdout, "%-*s %-4d: %-*s\n",
                                            MAX_AFDNAME_LENGTH, " ", j,
                                            MAX_HOSTNAME_LENGTH,
                                            ahl[i][j].host_alias);
                           }
                           else
                           {
                              if (ahl[i][j].real_hostname[1][0] == '\0')
                              {
                                 (void)fprintf(stdout, "%-*s %-4d: %-*s %-*s\n",
                                               MAX_AFDNAME_LENGTH, " ", j,
                                               MAX_HOSTNAME_LENGTH,
                                               ahl[i][j].host_alias,
                                               25,
                                               ahl[i][j].real_hostname[0]);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "%-*s %-4d: %-*s %-*s %-*s\n",
                                               MAX_AFDNAME_LENGTH, " ", j,
                                               MAX_HOSTNAME_LENGTH,
                                               ahl[i][j].host_alias,
                                               25,
                                               ahl[i][j].real_hostname[0],
                                               25,
                                               ahl[i][j].real_hostname[1]);
                              }
                           }
                        }

                        /* No need to check the other filters. */
                        break;
                     }
                  }
               }
            }
         }
      }
      else
      {
         for (i = 0; i < no_of_afds; i++)
         {
            if (ahl[i] != NULL)
            {
               show_afd_name = YES;
               for (j = 0; j < msa[i].no_of_hosts; j++)
               {
                  for (k = 0; k < no_of_filters; k++)
                  {
                     if (ahl[i][j].real_hostname[0][0] != GROUP_IDENTIFIER)
                     {
                        if ((pmatch(argv[k + 2], ahl[i][j].real_hostname[0], NULL) == 0) ||
                            ((ahl[i][j].real_hostname[1][0] != '\0') &&
                             (pmatch(argv[k + 2], ahl[i][j].real_hostname[1], NULL) == 0)))
                        {
                           if (show_header == YES)
                           {
                              (void)fprintf(stdout, "%-*s Pos   %-*s %-*s %-*s\n",
                                                    MAX_AFDNAME_LENGTH,
                                                    "AFD Alias",
                                                    MAX_HOSTNAME_LENGTH,
                                                    "alias",
                                                    25,
                                                    "real hostname 1",
                                                    25,
                                                    "real hostname 2");
                              show_header = NO;
                           }
                           if (show_afd_name == YES)
                           {
                              show_afd_name = NO;
                              if (ahl[i][j].real_hostname[1][0] == '\0')
                              {
                                 (void)fprintf(stdout, "%-*s %-4d: %-*s %-*s\n",
                                               MAX_AFDNAME_LENGTH,
                                               msa[i].afd_alias, j,
                                               MAX_HOSTNAME_LENGTH,
                                               ahl[i][j].host_alias,
                                               25,
                                               ahl[i][j].real_hostname[0]);
                           }
                              else
                              {
                                 (void)fprintf(stdout, "%-*s %-4d: %-*s %-*s %-*s\n",
                                               MAX_AFDNAME_LENGTH,
                                               msa[i].afd_alias, j,
                                               MAX_HOSTNAME_LENGTH,
                                               ahl[i][j].host_alias,
                                               25,
                                               ahl[i][j].real_hostname[0],
                                               25,
                                               ahl[i][j].real_hostname[1]);
                              }
                           }
                           else
                           {
                              if (ahl[i][j].real_hostname[1][0] == '\0')
                              {
                                 (void)fprintf(stdout, "%-*s %-4d: %-*s %-*s\n",
                                               MAX_AFDNAME_LENGTH, " ", j,
                                               MAX_HOSTNAME_LENGTH,
                                               ahl[i][j].host_alias,
                                               25,
                                               ahl[i][j].real_hostname[0]);
                              }
                              else
                              {
                                 (void)fprintf(stdout, "%-*s %-4d: %-*s %-*s %-*s\n",
                                               MAX_AFDNAME_LENGTH, " ", j,
                                               MAX_HOSTNAME_LENGTH,
                                               ahl[i][j].host_alias,
                                               25,
                                               ahl[i][j].real_hostname[0],
                                               25,
                                               ahl[i][j].real_hostname[1]);
                              }
                           }

                           /* No need to check the other filters. */
                           break;
                        }
                     }
                  }
               }
            }
         }
      }
   }

   /* Just to lazy to free and unmap everything! */
   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(void)
{
   (void)fprintf(stderr,
                 "SYNTAX : view_hosts [-w working directory] <option> <host name 1> [.. <host name n>]\n");
   (void)fprintf(stderr,
                 "                                            -a  search alias names\n");
   (void)fprintf(stderr,
                 "                                            -r  search real names\n");
   (void)fprintf(stderr,
                 "                                            -A  search AFD alias names\n");
   (void)fprintf(stderr,
                 "                                            -C  same as -A, just show first found\n");
   return;
}
