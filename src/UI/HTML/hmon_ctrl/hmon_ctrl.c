/*
 *  hmon_ctrl.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   hmon_ctrl - shows all information in the MSA about a specific
 **               AFD in HTML
 **
 ** SYNOPSIS
 **   hmon_ctrl [-w <working directory>] afdname|position
 **
 ** DESCRIPTION
 **   This program shows all information about a specific AFD in the
 **   MSA in HTML.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   28.02.2015 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr            */
#include <string.h>                      /* strerror()                   */
#include <stdlib.h>                      /* atoi()                       */
#include <ctype.h>                       /* isdigit()                    */
#include <time.h>                        /* ctime()                      */
#include <sys/types.h>
#include <unistd.h>                      /* STDERR_FILENO, sleep()       */
#include <errno.h>
#include "ui_common_defs.h"
#include "mondefs.h"
#include "sumdefs.h"
#include "version.h"

/* Global variables. */
int                    sys_log_fd = STDERR_FILENO,   /* Not used!    */
                       msa_fd = -1,
                       msa_id,
                       no_of_afds = 0;
off_t                  msa_size;
char                   *p_work_dir;
struct mon_status_area *msa;
const char             *sys_log_name = MON_SYS_LOG_FIFO;

/* Local function prototype. */
static void usage(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ msa_view() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          flush_output,
                i, j,
                last = 0,
                position = -1;
   unsigned int interval;
   FILE         *output;
   char         afdname[MAX_AFDNAME_LENGTH + 1],
                *ptr,
                str_fc[5],
                str_fs[5],
                str_tr[5],
                str_fr[4],
                str_jq[4],
                str_at[4],
                str_ec[3],
                str_hec[3],
                val[MAX_PATH_LENGTH + 1],
                work_dir[MAX_PATH_LENGTH];
   const char   *color_pool[COLOR_POOL_SIZE] =
                {
                   HTML_COLOR_0,
                   HTML_COLOR_1,
                   HTML_COLOR_2,
                   HTML_COLOR_3,
                   HTML_COLOR_4,
                   HTML_COLOR_5,
                   HTML_COLOR_6,
                   HTML_COLOR_7,
                   HTML_COLOR_8,
                   HTML_COLOR_9,
                   HTML_COLOR_10,
                   HTML_COLOR_11,
                   HTML_COLOR_12,
                   HTML_COLOR_13,
                   HTML_COLOR_14,
                   HTML_COLOR_15,
                   HTML_COLOR_16,
                   HTML_COLOR_17,
                   HTML_COLOR_18,
#ifdef _WITH_WMO_SUPPORT
                   HTML_COLOR_19,
                   HTML_COLOR_20
#else
                   HTML_COLOR_19
#endif
                };

   CHECK_FOR_VERSION(argc, argv);

   if (get_mon_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;
#ifdef WITH_SETUID_PROGS
   set_afd_euid(work_dir);
#endif

   if (get_arg(&argc, argv, "-d", val, MAX_INT_LENGTH) == SUCCESS)
   {
      interval = atoi(val);
   }
   else
   {
      interval = 0;
   }

   if (get_arg(&argc, argv, "-o", val, MAX_PATH_LENGTH) == SUCCESS)
   {
      if ((output = fopen(val, "w")) == NULL)
      {
         (void)fprintf(stderr, "Failed to fopen() %s : %s\n",
                       val, strerror(errno));
         exit(INCORRECT);
      }
      flush_output = YES;
   }
   else
   {
      output = stdout;
      flush_output = NO;
   }

   if (argc == 2)
   {
      if (isdigit((int)(argv[1][0])) != 0)
      {
         position = atoi(argv[1]);
         last = position + 1;
      }
      else
      {
         (void)my_strncpy(afdname, argv[1], MAX_AFDNAME_LENGTH + 1);
      }
   }
   else if (argc == 1)
        {
           position = -2;
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
         (void)fprintf(stderr,
                       "ERROR   : This program is not able to attach to the MSA due to incorrect version. (%s %d)\n",
                       __FILE__, __LINE__);
      }
      else
      {
         (void)fprintf(stderr, "ERROR   : Failed to attach to MSA. (%s %d)\n",
                       __FILE__, __LINE__);
      }
      exit(INCORRECT);
   }

   if (position == -1)
   {
      for (i = 0; i < no_of_afds; i++)
      {
         if (my_strcmp(msa[i].afd_alias, afdname) == 0)
         {
            position = i;
            break;
         }
      }
      if (position < 0)
      {
         (void)fprintf(stderr,
                       "WARNING : Could not find AFD `%s' in MSA. (%s %d)\n",
                       afdname, __FILE__, __LINE__);
         exit(INCORRECT);
      }
      last = position + 1;
   }
   else if (position == -2)
        {
           last = no_of_afds;
           position = 0;
        }
   else if (position >= no_of_afds)
        {
           (void)fprintf(stderr,
                         "WARNING : There are only %d AFD's in the MSA. (%s %d)\n",
                         no_of_afds, __FILE__, __LINE__);
           exit(INCORRECT);
        }

   ptr = (char *)msa;
   ptr -= AFD_WORD_OFFSET;

   for (;;)
   {
      (void)fprintf(output,
                    "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n");
      (void)fprintf(output,
                    "<html>\n<head>\n   <meta charset=\"utf-8\"/>\n");
      (void)fprintf(output,
                    "   <meta http-equiv=\"refresh\" content=\"5\"/>\n");
      (void)fprintf(output,
                    "   <title>AFD Monitor</title>\n</head>\n");
      (void)fprintf(output, "<body bgcolor=\"#F0ECD6\">\n");
      (void)fprintf(output, "<table align=center bgcolor=\"%s\">\n",
                    color_pool[(int)DEFAULT_BG]);
      (void)fprintf(output, "<tr>\n");
      (void)fprintf(output, "<th style=\"width:%dem;\" align=\"center\" valign=\"middle\" bgcolor=\"%s\">%s</th>\n",
                    MAX_AFDNAME_LENGTH, color_pool[(int)LABEL_BG], "AFD");
      (void)fprintf(output, "<th style=\"width:4em;\" align=\"center\" valign=\"middle\" bgcolor=\"%s\">fc</th>\n",
                    color_pool[(int)LABEL_BG]);
      (void)fprintf(output, "<th style=\"width:4em;\" align=\"center\" valign=\"middle\" bgcolor=\"%s\">fs</th>\n",
                    color_pool[(int)LABEL_BG]);
      (void)fprintf(output, "<th style=\"width:4em;\" align=\"center\" valign=\"middle\" bgcolor=\"%s\">tr</th>\n",
                    color_pool[(int)LABEL_BG]);
      (void)fprintf(output, "<th style=\"width:3em;\" align=\"center\" valign=\"middle\" bgcolor=\"%s\">fr</th>\n",
                    color_pool[(int)LABEL_BG]);
      (void)fprintf(output, "<th style=\"width:3em;\" align=\"center\" valign=\"middle\" bgcolor=\"%s\">jq</th>\n",
                    color_pool[(int)LABEL_BG]);
      (void)fprintf(output, "<th style=\"width:3em;\" align=\"center\" valign=\"middle\" bgcolor=\"%s\">at</th>\n",
                    color_pool[(int)LABEL_BG]);
      (void)fprintf(output, "<th style=\"width:2em;\" align=\"center\" valign=\"middle\" bgcolor=\"%s\">ec</th>\n",
                    color_pool[(int)LABEL_BG]);
      (void)fprintf(output, "<th style=\"width:2em;\" align=\"center\" valign=\"middle\" bgcolor=\"%s\">eh</th>\n",
                    color_pool[(int)LABEL_BG]);
      (void)fprintf(output, "</tr>\n");
      for (j = position; j < last; j++)
      {
         (void)fprintf(output, "<tr>\n");
         if (msa[j].connect_status == NOT_WORKING2)
         {
            (void)fprintf(output,
                          "<td align=\"left\" valign=\"middle\" style=\"background-color:%s; color:%s\">%-*s</td>\n",
                          color_pool[(int)NOT_WORKING2], color_pool[(int)WHITE],
                          MAX_AFDNAME_LENGTH, msa[j].afd_alias);
         }
         else
         {
            (void)fprintf(output,
                          "<td align=\"left\" valign=\"middle\" bgcolor=\"%s\">%-*s</td>\n",
                          color_pool[(int)msa[j].connect_status],
                          MAX_AFDNAME_LENGTH, msa[j].afd_alias);
         }
         CREATE_FC_STRING(str_fc, msa[j].fc);
         (void)fprintf(output, "<td align=\"right\" valign=\"middle\" bgcolor=\"%s\">%s</td>\n", color_pool[(int)CHAR_BACKGROUND], str_fc);
         CREATE_FS_STRING(str_fs,  msa[j].fs);
         (void)fprintf(output, "<td align=\"right\" valign=\"middle\" bgcolor=\"%s\">%s</td>\n", color_pool[(int)CHAR_BACKGROUND], str_fs);
         CREATE_FS_STRING(str_tr,  msa[j].tr);
         (void)fprintf(output, "<td align=\"right\" valign=\"middle\" bgcolor=\"%s\">%s</td>\n", color_pool[(int)CHAR_BACKGROUND], str_tr);
         CREATE_JQ_STRING(str_fr, msa[j].fr);
         (void)fprintf(output, "<td align=\"right\" valign=\"middle\" bgcolor=\"%s\">%s</td>\n", color_pool[(int)CHAR_BACKGROUND], str_fr);
         CREATE_JQ_STRING(str_jq, msa[j].jobs_in_queue);
         if ((msa[j].danger_no_of_jobs != 0) &&
             (msa[j].jobs_in_queue > msa[j].danger_no_of_jobs) &&
             (msa[j].jobs_in_queue <= ((msa[j].danger_no_of_jobs * 2) - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR)))
         {
            (void)fprintf(output,
                          "<td align=\"right\" valign=\"middle\" bgcolor=\"%s\">%s</td>\n",
                          color_pool[(int)WARNING_ID], str_jq);
         }
         else if ((msa[j].danger_no_of_jobs != 0) &&
                  (msa[j].jobs_in_queue > ((msa[j].danger_no_of_jobs * 2) - STOP_AMG_THRESHOLD - DIRS_IN_FILE_DIR)))
              {
                 (void)fprintf(output,
                               "<td align=\"right\" valign=\"middle\" style=\"background-color:%s; color:%s\">%s</td>\n",
                               color_pool[(int)NOT_WORKING2], color_pool[(int)WHITE],
                               str_jq);
              }
              else
              {
                 (void)fprintf(output,
                               "<td align=\"right\" valign=\"middle\" bgcolor=\"%s\">%s</td>\n",
                               color_pool[(int)CHAR_BACKGROUND], str_jq);
              }
         CREATE_JQ_STRING(str_at, msa[j].no_of_transfers);
         (void)fprintf(output, "<td align=\"right\" valign=\"middle\" bgcolor=\"%s\">%s</td>\n", color_pool[(int)CHAR_BACKGROUND], str_at);
         CREATE_EC_STRING(str_ec, msa[j].ec);
         if (msa[j].ec > 0)
         {
            (void)fprintf(output, "<td align=\"right\" valign=\"middle\" style=\"background-color:%s; color:%s\">%s</td>\n",
                          color_pool[(int)CHAR_BACKGROUND],
                          color_pool[(int)NOT_WORKING2], str_ec);
         }
         else
         {
            (void)fprintf(output,
                          "<td align=\"right\" valign=\"middle\" bgcolor=\"%s\">%s</td>\n",
                          color_pool[(int)CHAR_BACKGROUND], str_ec);
         }
         CREATE_EC_STRING(str_hec, msa[j].host_error_counter);
         if (msa[j].host_error_counter > 0)
         {
            (void)fprintf(output,
                          "<td align=\"right\" valign=\"middle\" style=\"background-color:%s; color:%s\">%s</td>\n",
                          color_pool[(int)NOT_WORKING2], color_pool[(int)WHITE],
                          str_hec);
         }
         else
         {
            (void)fprintf(output, "<td align=\"right\" valign=\"middle\" bgcolor=\"%s\">%s</td>\n",
                          color_pool[(int)CHAR_BACKGROUND], str_hec);
         }
         (void)fprintf(output, "</tr>\n");
      }
      (void)fprintf(output, "</table>\n</body>\n</html>\n");

      if (interval > 0)
      {
         if (flush_output == YES)
         {
            (void)fflush(output);
         }
         (void)sleep(interval);
         if (flush_output == YES)
         {
            (void)ftruncate(fileno(output), 0);
            (void)fseek(output, 0L, SEEK_SET);
         }
      }
      else
      {
         break;
      }
   }

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(void)
{
   (void)fprintf(stderr,
                 "SYNTAX  : hmon_ctrl [--version][-w <working directory>] afdname|position\n");
   return;
}
