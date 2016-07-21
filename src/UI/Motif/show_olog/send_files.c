/*
 *  send_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2016 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   send_files - sends files from the AFD archive to a host not
 **                in the FSA
 **
 ** SYNOPSIS
 **   void send_files(int no_selected, int *select_list)
 **
 ** DESCRIPTION
 **   send_files() will send all files selected in the show_olog
 **   dialog to a host not specified in the FSA. Only files that
 **   have been archived will be resend.
 **   Since the selected list can be rather long, this function
 **   will try to optimise the resending of files by collecting
 **   all jobs in a list with the same ID and generate a single
 **   message for all of them. If this is not done, far to many
 **   messages will be generated.
 **
 **   Every time a complete list with the same job ID has been
 **   resend, will cause this function to deselect those items.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.03.1999 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>         /* strerror(), strcmp(), strcpy(), strcat()  */
#include <stdlib.h>         /* calloc(), free()                          */
#include <unistd.h>         /* getpid()                                  */
#include <time.h>           /* time()                                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>         /* opendir(), readdir(), closedir()          */
#include <fcntl.h>
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/List.h>
#include "mafd_ctrl.h"
#include "show_olog.h"
#include "fddefs.h"

/* External global variables. */
extern Display          *display;
extern Widget           listbox_w,
                        special_button_w,
                        scrollbar_w,
                        statusbox_w;
extern int              log_date_length,
                        max_hostname_length,
                        no_of_log_files,
                        special_button_flag;
extern char             font_name[],
                        *p_work_dir;
extern struct item_list *il;
extern struct sol_perm  perm;

/* Local global variables. */
static char             archive_dir[MAX_PATH_LENGTH],
                        *p_archive_name,
                        *p_file_name;

/* Local function prototypes. */
static int              get_archive_data(int, int);


/*############################## send_files() ###########################*/
void
send_files(int no_selected, int *select_list)
{
   int                 i,
                       length = 0,
                       limit_reached = 0,
                       to_do = 0,    /* Number still to be done. */
                       not_found = 0,
                       not_archived = 0,
                       total_no_of_items;
   static int          user_limit = 0;
   static unsigned int counter = 0;
   char                user_message[256];
   struct resend_list *rl;

   if ((perm.send_limit > 0) && (user_limit >= perm.send_limit))
   {
      (void)sprintf(user_message, "User limit (%d) for resending reached!",
                    perm.send_limit);
      show_message(statusbox_w, user_message);
      return;
   }
   if ((rl = calloc(no_selected, sizeof(struct resend_list))) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "calloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   /* Prepare the archive directory name. */
   p_archive_name = archive_dir;
   p_archive_name += sprintf(archive_dir, "%s%s/",
                             p_work_dir, AFD_ARCHIVE_DIR);

   /*
    * Get the job ID, file number and position in that file for all
    * selected items. If the file was not archived mark it as done
    * immediately.
    */
   for (i = 0; i < no_selected; i++)
   {
      /* Determine log file and position in this log file. */
      total_no_of_items = 0;
      rl[i].pos = -1;
      for (rl[i].file_no = 0; rl[i].file_no < no_of_log_files; rl[i].file_no++)
      {
         total_no_of_items += il[rl[i].file_no].no_of_items;

         if (select_list[i] <= total_no_of_items)
         {
            rl[i].pos = select_list[i] - (total_no_of_items - il[rl[i].file_no].no_of_items) - 1;
            if (rl[i].pos > il[rl[i].file_no].no_of_items)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "pos (%d) is greater then no_of_items (%d), ignoring this.",
                          rl[i].pos, il[rl[i].file_no].no_of_items);
               rl[i].pos = -1;
            }
            break;
         }
      }

      /* Check if the file is in fact archived. */
      if (rl[i].pos > -1)
      {
         if (il[rl[i].file_no].archived[rl[i].pos] == 1)
         {
            if ((perm.send_limit > 0) &&
                ((user_limit + to_do) >= perm.send_limit))
            {
               rl[i].status = SEND_LIMIT_REACHED;
               limit_reached++;
            }
            else
            {
               to_do++;
            }
         }
         else
         {
            rl[i].status = NOT_ARCHIVED;
            not_archived++;
         }
      }
      else
      {
         rl[i].status = NOT_FOUND;
         not_found++;
      }
   }

   /*
    * Now we have checked all files are in archive that are to be send.
    * Lets lookup the archive directory for each job ID and then
    * collect all files that are to be resend for this ID. When
    * all files have been collected we start sending all of them
    * to the given destination.
    */
   if (to_do > 0)
   {
      char  *args[7],
            file_name_file[MAX_PATH_LENGTH],
            progname[XSEND_FILE_LENGTH + 1];
      uid_t euid, /* Effective user ID. */
            ruid; /* Real user ID. */
      FILE  *fp;

      user_limit += to_do;
#if SIZEOF_PID_T == 4
      (void)sprintf(file_name_file, ".file_name_file.%d.%d",
#else
      (void)sprintf(file_name_file, ".file_name_file.%lld.%d",
#endif
                    (pri_pid_t)getpid(), counter);
      counter++;

      /*
       * Temporaly disable setuid flag so the file gets opened
       * as real user.
       */
      euid = geteuid();
      ruid = getuid();
      if (euid != ruid)
      {
         if (seteuid(ruid) == -1)
         {
            (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                          ruid, strerror(errno), __FILE__, __LINE__);
         }
      }
      if ((fp = fopen(file_name_file, "w")) == NULL)
      {
         (void)fprintf(stderr, "Failed to fopen() %s : %s (%s %d)\n",
                       file_name_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      for (i = 0; i < no_selected; i++)
      {
         if ((rl[i].pos > -1) && (rl[i].status != NOT_ARCHIVED))
         {
            if (get_archive_data(rl[i].pos, rl[i].file_no) == SUCCESS)
            {
               (void)fprintf(fp, "%s|%s\n", archive_dir, p_file_name);
            }
         }
      }
      if (fclose(fp) == EOF)
      {
         (void)fprintf(stderr, "Failed to fclose() %s : %s (%s %d)\n",
                       file_name_file, strerror(errno), __FILE__, __LINE__);
      }

      (void)strcpy(progname, XSEND_FILE);
      args[0] = progname;
      args[1] = WORK_DIR_ID;
      args[2] = p_work_dir;
      args[3] = "-f";
      args[4] = font_name;
      args[5] = file_name_file;
      args[6] = NULL;
      make_xprocess(progname, progname, args, -1);
      if (euid != ruid)
      {
         if (seteuid(euid) == -1)
         {
            (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                          euid, strerror(errno), __FILE__, __LINE__);
         }
      }
   } /* if (to_do > 0) */

   /* Show user a summary of what was done. */
   user_message[0] = ' ';
   user_message[1] = '\0';
   if (to_do > 0)
   {
      if (to_do == 1)
      {
         length = sprintf(user_message, "1 file to be send");
      }
      else
      {
         length = sprintf(user_message, "%d files to be send", to_do);
      }
   }
   if (not_archived > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d not archived",
                           not_archived);
      }
      else
      {
         length = sprintf(user_message, "%d not archived", not_archived);
      }
   }
   if (not_found > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d not found", not_found);
      }
      else
      {
         length = sprintf(user_message, "%d not found", not_found);
      }
   }
   if (limit_reached > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d not send due to limit",
                           limit_reached);
      }
      else
      {
         length = sprintf(user_message, "%d not send due to limit",
                          limit_reached);
      }
   }
   show_message(statusbox_w, user_message);

   free((void *)rl);

   return;
}


/*+++++++++++++++++++++++++++ get_archive_data() ++++++++++++++++++++++++*/
/*                            ------------------                         */
/* Description: From the output log file, this function gets the file    */
/*              name and the name of the archive directory and stores    */
/*              these in archive_dir[].                                  */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int
get_archive_data(int pos, int file_no)
{
   int  i, j,
        type_offset;
   char buffer[MAX_FILENAME_LENGTH + MAX_PATH_LENGTH],
        *ptr,
        *p_unique_string;

   if (fseek(il[file_no].fp, (long)il[file_no].line_offset[pos], SEEK_SET) == -1)
   {
      (void)xrec(FATAL_DIALOG, "fseek() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }
   if (fgets(buffer, MAX_FILENAME_LENGTH + MAX_PATH_LENGTH, il[file_no].fp) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "fgets() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   if (buffer[log_date_length + 1 + max_hostname_length + 2] == ' ')
   {
#ifdef ACTIVATE_THIS_AFTER_VERSION_14
      type_offset = 5;
#else
      if (buffer[log_date_length + 1 + max_hostname_length + 4] == ' ')
      {
         type_offset = 5;
      }
      else
      {
         type_offset = 3;
      }
#endif
   }
   else
   {
      type_offset = 1;
   }
   ptr = &buffer[log_date_length + 1 + max_hostname_length + type_offset + 2];

   /* Mark end of file name. */
   while (*ptr != SEPARATOR_CHAR)
   {
      ptr++;
   }
   *(ptr++) = '\0';
   if (*ptr != SEPARATOR_CHAR)
   {
      /* Ignore the remote file name. */
      while (*ptr != SEPARATOR_CHAR)
      {
         ptr++;
      }
   }
   ptr++;

   /* Away with the size. */
   while (*ptr != SEPARATOR_CHAR)
   {
      ptr++;
   }
   ptr++;

   /* Away with transfer duration. */
   while (*ptr != SEPARATOR_CHAR)
   {
      ptr++;
   }
   ptr++;

   /* Away with number of retries. */
   if (type_offset > 1)
   {
      while (*ptr != SEPARATOR_CHAR)
      {
         ptr++;
      }
      ptr++;
   }

   /* Away with the job ID. */
   while (*ptr != SEPARATOR_CHAR)
   {
      ptr++;
   }
   ptr++;

   /* Away with the unique string. */
   p_unique_string = ptr;
   while (*ptr != SEPARATOR_CHAR)
   {
      ptr++;
   }
   ptr++;

   /* Ahhh. Now here is the archive directory we are looking for. */
   i = 0;
   while (*ptr != '\n')
   {
      *(p_archive_name + i) = *ptr;
      i++; ptr++;
   }
   *(p_archive_name + i++) = '/';
   while ((*p_unique_string != SEPARATOR_CHAR) && (*p_unique_string != ' '))
   {
      *(p_archive_name + i) = *p_unique_string;
      i++; p_unique_string++;
   }
   *(p_archive_name + i++) = '_';

   /* Copy the file name to the archive directory. */
   j = 0;
   ptr = &buffer[log_date_length + 1 + max_hostname_length + type_offset + 2];
   while (*ptr != '\0')
   {
      if (*ptr == ' ')
      {
         *(p_archive_name + i + j) = '\\';
         *(p_archive_name + i + j + 1) = ' ';
         j += 2;
      }
      else
      {
         *(p_archive_name + i + j) = *ptr;
         j++;
      }
      ptr++;
   }
   *(p_archive_name + i + j) = '\0';
   p_file_name = p_archive_name + i;

   return(SUCCESS);
}
