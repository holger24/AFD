/*
 *  resend_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   resend_files - resends files from the AFD archive
 **
 ** SYNOPSIS
 **   void resend_files(int no_selected, int *select_list)
 **
 ** DESCRIPTION
 **   resend_files() will resend all files selected in the show_olog
 **   dialog. Only files that have been archived will be resend.
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
 **   09.05.1997 H.Kiehl Created
 **   10.02.1998 H.Kiehl Adapted to new message form.
 **   24.09.2004 H.Kiehl Added split job counter.
 **   16.01.2005 H.Kiehl Adapted to new message names.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>         /* strerror(), strcmp(), strcpy(), strcat()  */
#include <stdlib.h>         /* calloc(), free(), malloc()                */
#include <unistd.h>         /* rmdir(), W_OK, F_OK                       */
#include <time.h>           /* time()                                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>          /* utime()                                   */
#include <dirent.h>         /* opendir(), readdir(), closedir()          */
#include <fcntl.h>
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/List.h>
#include "mafd_ctrl.h"
#include "show_olog.h"
#include "fddefs.h"

/* #define WITH_RESEND_DEBUG 1 */

/* External global variables. */
extern Display          *display;
extern Widget           appshell, /* CHECK_INTERRUPT() */
                        close_button_w,
                        listbox_w,
                        special_button_w,
                        scrollbar_w,
                        statusbox_w;
extern int              items_selected,
                        log_date_length,
                        max_hostname_length,
                        no_of_log_files,
                        special_button_flag;
extern char             *p_work_dir;
extern struct item_list *il;
extern struct info_data id;
extern struct sol_perm  perm;

/* Global variables. */
int                     counter_fd,
                        fsa_fd = -1,
                        fsa_id,
                        no_of_hosts;
#ifdef HAVE_MMAP
off_t                   fsa_size;
#endif
struct filetransfer_status *fsa;

/* Local global variables. */
static int              max_copied_files,
                        overwrite;
static char             archive_dir[MAX_PATH_LENGTH],
                        *p_file_name,
                        *p_archive_name,
                        *p_dest_dir_end = NULL,
#ifdef MULTI_FS_SUPPORT
                        *p_orig_msg_name,
#endif
                        *p_msg_name,
                        dest_dir[MAX_PATH_LENGTH];

/* Local function prototypes. */
static int              get_archive_data(int, int),
                        send_new_message(char *, time_t, unsigned int,
                                         unsigned int, unsigned int, char,
                                         int, off_t),
                        get_file(char *, char *, off_t *);
static void             get_afd_config_value(void),
                        write_fsa(int, int, off_t);


/*############################# resend_files() ##########################*/
void
resend_files(int no_selected, int *select_list)
{
#ifdef MULTI_FS_SUPPORT
   int                added_fs_id;
#endif
   int                i,
                      k,
                      total_no_of_items,
                      length = 0,
                      to_do = 0,    /* Number still to be done. */
                      no_done = 0,  /* Number done.             */
                      not_found = 0,
                      not_archived = 0,
                      not_in_archive = 0,
                      select_done = 0,
                      *select_done_list,
                      *unique_number;
   unsigned int       current_job_id = 0,
                      last_job_id = 0,
                      split_job_counter;
   time_t             creation_time = 0;
   off_t              file_size = 0,
                      total_file_size;
   static int         user_limit = 0;
   char               user_message[256];
   XmString           xstr;
   struct resend_list *rl;

   if ((perm.resend_limit > 0) && (user_limit >= perm.resend_limit))
   {
      (void)sprintf(user_message, "User limit (%d) for resending reached!",
                    perm.resend_limit);
      show_message(statusbox_w, user_message);
      return;
   }
   overwrite = 0;
   dest_dir[0] = '\0';
   if (((rl = calloc(no_selected, sizeof(struct resend_list))) == NULL) ||
       ((select_done_list = calloc(no_selected, sizeof(int))) == NULL))
   {
      (void)xrec(FATAL_DIALOG, "calloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   /* Open counter file, so we can create new unique name. */
   if ((counter_fd = open_counter_file(COUNTER_FILE, &unique_number)) < 0)
   {
      (void)xrec(FATAL_DIALOG, "Failed to open counter file. (%s %d)",
                 __FILE__, __LINE__);
      free((void *)rl);
      return;
   }

   /* See how many files we may copy in one go. */
   get_afd_config_value();

   /* Prepare the archive directory name. */
   p_archive_name = archive_dir + sprintf(archive_dir, "%s%s/",
                                          p_work_dir, AFD_ARCHIVE_DIR);
   p_msg_name = dest_dir + sprintf(dest_dir, "%s%s%s/",
                                   p_work_dir, AFD_FILE_DIR, OUTGOING_DIR);
#ifdef MULTI_FS_SUPPORT
   p_orig_msg_name = p_msg_name;
#endif

   /* Get the fsa_id and no of host of the FSA. */
   if (fsa_attach(SHOW_OLOG) != SUCCESS)
   {
      (void)xrec(FATAL_DIALOG, "Failed to attach to FSA. (%s %d)",
                 __FILE__, __LINE__);
      return;
   }

   /* Block all input and change button name. */
   special_button_flag = STOP_BUTTON;
   xstr = XmStringCreateLtoR("Stop", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);
   CHECK_INTERRUPT();

   /*
    * Get the job ID, file number and position in that file for all
    * selected items. If the file was not archived mark it as done
    * immediately.
    */
   for (i = 0; i < no_selected; i++)
   {
      /* Determine log file and position in this log file. */
      total_no_of_items = 0;
      for (rl[i].file_no = 0; rl[i].file_no < no_of_log_files; rl[i].file_no++)
      {
         total_no_of_items += il[rl[i].file_no].no_of_items;

         if (select_list[i] <= total_no_of_items)
         {
            rl[i].pos = select_list[i] - (total_no_of_items - il[rl[i].file_no].no_of_items) - 1;
            break;
         }
      }

      /* Get the job ID and file name. */
      if (rl[i].pos > -1)
      {
#ifdef WITH_RESEND_DEBUG
         (void)fprintf(stdout, "select=%d no_selected=%d archived=%d job_id=%x file_no=%d pos=%d status=%d (%s %d)\n",
                       i, no_selected, il[rl[i].file_no].archived[rl[i].pos],
                       rl[i].job_id, rl[i].file_no, rl[i].pos, (int)rl[i].status,
                       __FILE__, __LINE__);
#endif
         if (il[rl[i].file_no].archived[rl[i].pos] == 1)
         {
            /*
             * Read the job ID from the output log file.
             */
            int  j;
            char buffer[15];

            if (fseek(il[rl[i].file_no].fp, (long)il[rl[i].file_no].offset[rl[i].pos], SEEK_SET) == -1)
            {
               (void)xrec(FATAL_DIALOG, "fseek() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
               free((void *)rl);
               free((void *)select_done_list);
               close_counter_file(counter_fd, &unique_number);
               return;
            }

            j = 0;
            while (((buffer[j] = fgetc(il[rl[i].file_no].fp)) != '\n') &&
                   (buffer[j] != SEPARATOR_CHAR) && (j < 14))
            {
               j++;
            }
            buffer[j] = '\0';

            rl[i].job_id = (unsigned int)strtoul(buffer, NULL, 16);
            rl[i].status = FILE_PENDING;
            to_do++;
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
    * Now we have the job ID of every file that is to be resend.
    * Plus we have removed those that have not been archived or
    * could not be found.
    * Lets lookup the archive directory for each job ID and then
    * collect all files that are to be resend for this ID. When
    * all files have been collected we send a message for this
    * job ID and then deselect all selected items that have just
    * been resend.
    */
   while (to_do > 0)
   {
      total_file_size = 0;
      for (i = 0; i < no_selected; i++)
      {
         if (rl[i].status == FILE_PENDING)
         {
            id.job_no = current_job_id = rl[i].job_id;
            get_info(GOT_JOB_ID_PRIORITY_ONLY);
            get_info_free();
            break;
         }
      }

#ifdef MULTI_FS_SUPPORT
      added_fs_id = NO;
      p_msg_name = p_orig_msg_name;
#endif

      for (k = i; k < no_selected; k++)
      {
         if ((rl[k].status == FILE_PENDING) &&
             (current_job_id == rl[k].job_id))
         {
            if (get_archive_data(rl[k].pos, rl[k].file_no) < 0)
            {
               rl[k].status = NOT_IN_ARCHIVE;
               not_in_archive++;
            }
            else
            {
#ifdef MULTI_FS_SUPPORT
               if (added_fs_id == NO)
               {
                  int m = 0;

                  /* Copy the filesystem ID to dest_dir. */
                  while ((*(p_archive_name + m) != '/') &&
                         (*(p_archive_name + m) != '\0') &&
                         (m < MAX_INT_HEX_LENGTH))
                  {
                     *(p_orig_msg_name + m) = *(p_archive_name + m);
                     m++;
                  }
                  if ((m == MAX_INT_HEX_LENGTH) ||
                      (*(p_archive_name + m) == '\0'))
                  {
                     (void)xrec(FATAL_DIALOG,
                                "Failed to locate filesystem ID in `%s' : (%s %d)",
                                p_archive_name, __FILE__, __LINE__);
                     return;
                  }
                  *(p_orig_msg_name + m) = '/';
                  p_msg_name = p_dest_dir_end = p_orig_msg_name + m + 1;
                  added_fs_id = YES;
               }
#endif
               if ((select_done % max_copied_files) == 0)
               {
                  /* Copy a message so FD can pick up the job. */
                  if (select_done != 0)
                  {
                     int m;

                     if (send_new_message(p_msg_name, creation_time,
                                          (unsigned int)*unique_number,
                                          split_job_counter,
                                          current_job_id, id.priority,
                                          max_copied_files,
                                          total_file_size) < 0)
                     {
                        (void)xrec(FATAL_DIALOG,
                                   "Failed to create message : (%s %d)",
                                   __FILE__, __LINE__);
                        write_fsa(NO, max_copied_files, total_file_size);
                        free((void *)rl);
                        free((void *)select_done_list);
                        close_counter_file(counter_fd, &unique_number);
                        return;
                     }

                     /* Deselect all those that where resend. */
                     for (m = 0; m < select_done; m++)
                     {
                        XmListDeselectPos(listbox_w, select_done_list[m]);
                     }
                     if (select_done == no_selected)
                     {
                        items_selected = NO;
                     }
                     select_done = 0;
                     total_file_size = 0;
                  }

                  /* Create a new directory. */
                  creation_time = time(NULL);
                  *p_msg_name = '\0';
                  split_job_counter = 0;
                  if (create_name(dest_dir, strlen(dest_dir), id.priority,
                                  creation_time, current_job_id,
                                  &split_job_counter, unique_number,
                                  p_msg_name,
                                  MAX_PATH_LENGTH - (p_msg_name - dest_dir),
                                  counter_fd) < 0)
                  {
                     (void)xrec(FATAL_DIALOG,
                                "Failed to create a unique directory : (%s %d)",
                                __FILE__, __LINE__);
                     free((void *)rl);
                     free((void *)select_done_list);
                     close_counter_file(counter_fd, &unique_number);
                     return;
                  }
                  p_dest_dir_end = p_msg_name;
                  while (*p_dest_dir_end != '\0')
                  {
                     p_dest_dir_end++;
                  }
                  *(p_dest_dir_end++) = '/';
                  *p_dest_dir_end = '\0';
               }
               if (get_file(dest_dir, p_dest_dir_end, &file_size) < 0)
               {
                  rl[k].status = NOT_IN_ARCHIVE;
                  not_in_archive++;
               }
               else
               {
                  rl[k].status = DONE;
                  no_done++;
                  select_done_list[select_done] = select_list[k];
                  select_done++;
                  total_file_size += file_size;
                  last_job_id = current_job_id;

                  if (perm.resend_limit >= 0)
                  {
                     user_limit++;
                     if ((user_limit - overwrite) >= perm.resend_limit)
                     {
                        break;
                     }
                  }
               }
            }
            to_do--;
         }
      } /* for (k = i; k < no_selected; k++) */

      /* Copy a message so FD can pick up the job. */
      if ((select_done > 0) && ((select_done % max_copied_files) != 0))
      {
         int m;

         if (send_new_message(p_msg_name, creation_time,
                              (unsigned int)*unique_number,
                              split_job_counter, last_job_id, id.priority,
                              select_done, total_file_size) < 0)
         {
            (void)xrec(FATAL_DIALOG, "Failed to create message : (%s %d)",
                       __FILE__, __LINE__);
            write_fsa(NO, select_done, total_file_size);
            free((void *)rl);
            free((void *)select_done_list);
            close_counter_file(counter_fd, &unique_number);
            return;
         }

         /* Deselect all those that where resend. */
         for (m = 0; m < select_done; m++)
         {
            XmListDeselectPos(listbox_w, select_done_list[m]);
         }
         if (select_done == no_selected)
         {
            items_selected = NO;
         }
         select_done = 0;
         total_file_size = 0;
      }

      CHECK_INTERRUPT();

      if ((special_button_flag == STOP_BUTTON_PRESSED) ||
          ((perm.resend_limit >= 0) &&
           ((user_limit - overwrite) >= perm.resend_limit)))
      {
         break;
      }
   } /* while (to_do > 0) */

   if ((no_done == 0) && (dest_dir[0] != '\0') && (p_dest_dir_end != NULL))
   {
      /* Remove the directory we created in the files dir. */
      *p_dest_dir_end = '\0';
   }

   /* Show user a summary of what was done. */
   length = 0;
   if (no_done > 0)
   {
      if ((no_done - overwrite) == 1)
      {
         length = sprintf(user_message, "1 file resend");
      }
      else
      {
         length = sprintf(user_message, "%d files resend",
                          no_done - overwrite);
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
   if (not_in_archive > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d not in archive",
                           not_in_archive);
      }
      else
      {
         length = sprintf(user_message, "%d not in archive", not_in_archive);
      }
   }
   if (overwrite > 0)
   {
      if (length > 0)
      {
         length += sprintf(&user_message[length], ", %d overwrites",
                           overwrite);
      }
      else
      {
         length = sprintf(user_message, "%d overwrites", overwrite);
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
   if ((perm.resend_limit >= 0) && ((user_limit - overwrite) >= perm.resend_limit))
   {
      (void)sprintf(&user_message[length], " USER LIMIT (%d) REACHED",
                    perm.resend_limit);
   }
   show_message(statusbox_w, user_message);

   free((void *)rl);
   free((void *)select_done_list);

   close_counter_file(counter_fd, &unique_number);

   if (fsa_detach(NO) < 0)
   {
      (void)xrec(WARN_DIALOG, "Failed to detach from FSA. (%s %d)",
                 __FILE__, __LINE__);
   }

   /* Button back to normal. */
   special_button_flag = SEARCH_BUTTON;
   xstr = XmStringCreateLtoR("Search", XmFONTLIST_DEFAULT_TAG);
   XtVaSetValues(special_button_w, XmNlabelString, xstr, NULL);
   XmStringFree(xstr);

   return;
}


/*+++++++++++++++++++++++++++ get_archive_data() ++++++++++++++++++++++++*/
/*                            ------------------                         */
/* Description: From the output log file, this function gets the file    */
/*              name and the name of the archive directory.              */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int
get_archive_data(int pos, int file_no)
{
   int  i,
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
   while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
   {
      ptr++;
   }
   if (*ptr == SEPARATOR_CHAR)
   {
      ptr++;
   }

   /* Away with the unique string. */
   p_unique_string = ptr;
   while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
   {
      ptr++;
   }
   if (*ptr == SEPARATOR_CHAR)
   {
      ptr++;
   }

   /* Ahhh. Now here is the archive directory we are looking for. */
   i = 0;
   while (*ptr != '\n')
   {
      *(p_archive_name + i) = *ptr;
      i++; ptr++;
   }
   if (i == 0)
   {
      /* Could not find any archive data. */
      return(INCORRECT);
   }
   *(p_archive_name + i++) = '/';
   while ((*p_unique_string != SEPARATOR_CHAR) && (*p_unique_string != ' ') &&
          (*p_unique_string != '\n'))
   {
      *(p_archive_name + i) = *p_unique_string;
      i++; p_unique_string++;
   }
   *(p_archive_name + i++) = '_';

   /* Copy the file name to the archive directory. */
   (void)strcpy((p_archive_name + i),
                &buffer[log_date_length + 1 + max_hostname_length + type_offset + 2]);
   p_file_name = p_archive_name + i;

   return(SUCCESS);
}


/*++++++++++++++++++++++++++ send_new_message() +++++++++++++++++++++++++*/
/*                           ------------------                          */
/* Description: Sends a message via fifo to the FD.                      */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int
send_new_message(char         *p_msg_name,
                 time_t       creation_time,
                 unsigned int unique_number,
                 unsigned int split_job_counter,
                 unsigned int job_id,
                 char         priority,
                 int          files_to_send,
                 off_t        file_size_to_send)
{
   unsigned short dir_no;
   int            fd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  readfd,
#endif
                  ret;
   char           msg_fifo[MAX_PATH_LENGTH],
                  *ptr;

   ptr = p_msg_name;
   while ((*ptr != '/') && (*ptr != '\0'))
   {
      ptr++;
   }
   if (*ptr != '/')
   {
      (void)xrec(ERROR_DIALOG,
                 "Unable to find directory number in `%s' (%s %d)",
                 p_msg_name, __FILE__, __LINE__);
      return(INCORRECT);
   }
   dir_no = (unsigned short)strtoul(ptr + 1, NULL, 16);

   /* Write data to FSA so it can be seen in 'afd_ctrl'. */
   write_fsa(YES, files_to_send, file_size_to_send);

   (void)strcpy(msg_fifo, p_work_dir);
   (void)strcat(msg_fifo, FIFO_DIR);
   (void)strcat(msg_fifo, MSG_FIFO);

   /* Open and create message file. */
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(msg_fifo, &readfd, &fd) == -1)
#else
   if ((fd = open(msg_fifo, O_RDWR)) == -1)
#endif
   {
      (void)xrec(ERROR_DIALOG, "Could not open %s : %s (%s %d)",
                 msg_fifo, strerror(errno), __FILE__, __LINE__);
      ret = INCORRECT;
   }
   else
   {
      char fifo_buffer[MAX_BIN_MSG_LENGTH];

      /* Fill fifo buffer with data. */
      *(time_t *)(fifo_buffer) = creation_time;
#ifdef MULTI_FS_SUPPORT
# if SIZEOF_TIME_T == 4
      *(unsigned int *)(fifo_buffer + sizeof(time_t)) = (dev_t)strtoul(p_archive_name, NULL, 16);;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t)) = job_id;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                        sizeof(unsigned int)) = split_job_counter;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                        sizeof(unsigned int) +
                        sizeof(unsigned int)) = files_to_send;
      *(off_t *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                 sizeof(unsigned int) + sizeof(unsigned int) +
                 sizeof(unsigned int)) = file_size_to_send;
# else
      *(off_t *)(fifo_buffer + sizeof(time_t)) = file_size_to_send;
      *(off_t *)(fifo_buffer + sizeof(time_t) + sizeof(off_t)) = (dev_t)strtoul(p_archive_name, NULL, 16);;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(off_t) +
                        sizeof(dev_t)) = job_id;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(off_t) +
                        sizeof(dev_t) +
                        sizeof(unsigned int)) = split_job_counter;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(off_t) +
                        sizeof(dev_t) + sizeof(unsigned int) +
                        sizeof(unsigned int)) = files_to_send;
# endif
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                        sizeof(unsigned int) + sizeof(unsigned int) +
                        sizeof(unsigned int) + sizeof(off_t)) = unique_number;
      *(unsigned short *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                          sizeof(unsigned int) + sizeof(unsigned int) +
                          sizeof(unsigned int) + sizeof(off_t) +
                          sizeof(unsigned int)) = dir_no;
      *(char *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                sizeof(unsigned int) + sizeof(unsigned int) +
                sizeof(unsigned int) + sizeof(off_t) + sizeof(unsigned int) +
                sizeof(unsigned short)) = priority;
      *(char *)(fifo_buffer + sizeof(time_t) + sizeof(dev_t) +
                sizeof(unsigned int) + sizeof(unsigned int) +
                sizeof(unsigned int) + sizeof(off_t) + sizeof(unsigned int) +
                sizeof(unsigned short) + sizeof(char)) = SHOW_OLOG_NO;
#else
# if SIZEOF_TIME_T == 4
      *(unsigned int *)(fifo_buffer + sizeof(time_t)) = job_id;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) +
                        sizeof(unsigned int)) = split_job_counter;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                        sizeof(unsigned int)) = files_to_send;
      *(off_t *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                 sizeof(unsigned int) +
                 sizeof(unsigned int)) = file_size_to_send;
# else
      *(off_t *)(fifo_buffer + sizeof(time_t)) = file_size_to_send;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) +
                        sizeof(off_t)) = job_id;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(off_t) +
                        sizeof(unsigned int)) = split_job_counter;
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(off_t) +
                        sizeof(unsigned int) +
                        sizeof(unsigned int)) = files_to_send;
# endif
      *(unsigned int *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                        sizeof(unsigned int) + sizeof(unsigned int) +
                        sizeof(off_t)) = unique_number;
      *(unsigned short *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                          sizeof(unsigned int) + sizeof(unsigned int) +
                          sizeof(off_t) + sizeof(unsigned int)) = dir_no;
      *(char *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                sizeof(unsigned int) + sizeof(unsigned int) + sizeof(off_t) +
                sizeof(unsigned int) + sizeof(unsigned short)) = priority;
      *(char *)(fifo_buffer + sizeof(time_t) + sizeof(unsigned int) +
                sizeof(unsigned int) + sizeof(unsigned int) + sizeof(off_t) +
                sizeof(unsigned int) + sizeof(unsigned short) +
                sizeof(char)) = SHOW_OLOG_NO;
#endif

      /* Send the message. */
      if (write(fd, fifo_buffer, MAX_BIN_MSG_LENGTH) != MAX_BIN_MSG_LENGTH)
      {
         (void)xrec(ERROR_DIALOG, "Could not write to %s : %s (%s %d)",
                    msg_fifo, strerror(errno), __FILE__, __LINE__);
         ret = INCORRECT;
      }
      else
      {
         ret = SUCCESS;
      }

#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (close(readfd) == -1)
      {
         (void)xrec(WARN_DIALOG, "Failed to close() %s : %s (%s %d)",
                    msg_fifo, strerror(errno), __FILE__, __LINE__);
      }
#endif
      if (close(fd) == -1)
      {
         (void)xrec(WARN_DIALOG, "Failed to close() %s : %s (%s %d)",
                    msg_fifo, strerror(errno), __FILE__, __LINE__);
      }
   }

   return(ret);
}


/*++++++++++++++++++++++++++++++ get_file() +++++++++++++++++++++++++++++*/
/*                               ----------                              */
/* Description: Will try to link a file from the archive directory to    */
/*              new file directory. If it fails to link them and errno   */
/*              is EXDEV (file systems diver) or EEXIST (file exists),   */
/*              it will try to copy the file (ie overwrite it in case    */
/*              errno is EEXIST).                                        */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int
get_file(char *dest_dir, char *p_dest_dir_end, off_t *file_size)
{
   (void)strcpy(p_dest_dir_end, p_file_name);
   if (eaccess(archive_dir, W_OK) == 0)
   {
      if (link(archive_dir, dest_dir) < 0)
      {
         switch (errno)
         {
            case EEXIST : /* File already exists. Overwrite it. */
                          overwrite++;
                          /* NOTE: Falling through! */
            case EXDEV  : /* File systems differ. */
                          if (copy_file(archive_dir, dest_dir, NULL) < 0)
                          {
                             (void)fprintf(stdout,
                                           "Failed to copy %s to %s (%s %d)\n",
                                           archive_dir, dest_dir,
                                           __FILE__, __LINE__);
                             return(INCORRECT);
                          }
                          break;
            default:      /* All other errors go here. */
                          (void)fprintf(stdout,
                                        "Failed to link() %s to %s : %s (%s %d)\n",
                                        archive_dir, dest_dir, strerror(errno),
                                        __FILE__, __LINE__);
                          return(INCORRECT);
         }
      }
      else /* We must update the file time or else when age limit is */
           /* set the files will be deleted by process sf_xxx before */
           /* being send.                                            */
      {
#ifdef HAVE_STATX
         struct statx stat_buf;
#else
         struct stat stat_buf;
#endif

         if (utime(dest_dir, NULL) == -1)
         {
            /*
             * Do NOT use xrec() here to report any errors. If you try
             * to do this with al lot of files your screen will be filled
             * with lots of error messages.
             */
            (void)fprintf(stdout, "Failed to set utime() of %s : %s (%s %d)\n",
                          dest_dir, strerror(errno), __FILE__, __LINE__);
         }

#ifdef HAVE_STATX
         if (statx(0, dest_dir, AT_STATX_SYNC_AS_STAT,
                   STATX_SIZE, &stat_buf) == -1)
#else
         if (stat(dest_dir, &stat_buf) == -1)
#endif
         {
            (void)fprintf(stdout, "Failed to access `%s' : %s (%s %d)\n",
                          dest_dir, strerror(errno), __FILE__, __LINE__);
            return(INCORRECT);
         }
         else
         {
#ifdef HAVE_STATX
            *file_size = stat_buf.stx_size;
#else
            *file_size = stat_buf.st_size;
#endif
         }
      }
   }
   else
   {
      if (eaccess(archive_dir, R_OK) == 0)
      {
         /*
          * If we do not have write permission to the file we must copy
          * the file so the date of the file is that time when we copied it.
          */
         int from_fd;

         if ((from_fd = open(archive_dir, O_RDONLY)) == -1)
         {
            /* File is not in archive dir. */
            (void)fprintf(stdout, "Failed to open() `%s' : %s (%s %d)\n",
                          archive_dir, strerror(errno), __FILE__, __LINE__);
            return(INCORRECT);
         }
         else
         {
#ifdef HAVE_STATX
            struct statx stat_buf;
#else
            struct stat stat_buf;
#endif

#ifdef HAVE_STATX
            if (statx(from_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                      STATX_SIZE | STATX_MODE, &stat_buf) == -1)
#else
            if (fstat(from_fd, &stat_buf) == -1)
#endif
            {
               (void)fprintf(stderr, "Failed to access %s : %s (%s %d)\n",
                             archive_dir, strerror(errno), __FILE__, __LINE__);
               (void)close(from_fd);
               return(INCORRECT);
            }
            else
            {
               int to_fd;

               (void)unlink(dest_dir);
#ifdef HAVE_STATX
               if ((to_fd = open(dest_dir, O_WRONLY | O_CREAT | O_TRUNC,
                                 stat_buf.stx_mode)) == -1)
#else
               if ((to_fd = open(dest_dir, O_WRONLY | O_CREAT | O_TRUNC,
                                 stat_buf.st_mode)) == -1)
#endif
               {
                  (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                                dest_dir, strerror(errno), __FILE__, __LINE__);
                  (void)close(from_fd);
                  return(INCORRECT);
               }
               else
               {
#ifdef HAVE_STATX
                  if (stat_buf.stx_size > 0)
#else
                  if (stat_buf.st_size > 0)
#endif
                  {
                     int  bytes_buffered;
                     char *buffer;

#ifdef HAVE_STATX
                     if ((buffer = malloc(stat_buf.stx_blksize)) == NULL)
#else
                     if ((buffer = malloc(stat_buf.st_blksize)) == NULL)
#endif
                     {
                        (void)fprintf(stderr,
                                      "malloc() error : %s (%s %d)\n",
                                      strerror(errno), __FILE__, __LINE__);
                        (void)close(from_fd); (void)close(to_fd);
                        return(INCORRECT);
                     }

                     do
                     {
#ifdef HAVE_STATX
                        if ((bytes_buffered = read(from_fd, buffer,
                                                   stat_buf.stx_blksize)) == -1)
#else
                        if ((bytes_buffered = read(from_fd, buffer,
                                                   stat_buf.st_blksize)) == -1)
#endif
                        {
                           (void)fprintf(stderr,
                                         "Failed to read() from %s : %s (%s %d)\n",
                                         archive_dir, strerror(errno),
                                         __FILE__, __LINE__);
                           free(buffer);
                           (void)close(from_fd); (void)close(to_fd);
                           return(INCORRECT);
                        }
                        if (bytes_buffered > 0)
                        {
                           if (write(to_fd, buffer, bytes_buffered) != bytes_buffered)
                           {
                              (void)fprintf(stderr,
                                            "Failed to write() to %s : %s (%s %d)\n",
                                            dest_dir, strerror(errno),
                                            __FILE__, __LINE__);
                              free(buffer);
                              (void)close(from_fd); (void)close(to_fd);
                              return(INCORRECT);
                           }
                        }
#ifdef HAVE_STATX
                     } while (bytes_buffered == stat_buf.stx_blksize);
#else
                     } while (bytes_buffered == stat_buf.st_blksize);
#endif
                     free(buffer);
                  }
                  (void)close(to_fd);
#ifdef HAVE_STATX
                  *file_size = stat_buf.stx_size;
#else
                  *file_size = stat_buf.st_size;
#endif
               }
            }
            (void)close(from_fd);
         }
      }
      else
      {
         if (link(archive_dir, dest_dir) < 0)
         {
            (void)fprintf(stdout, "Failed to link() %s to %s : %s (%s %d)\n",
                          archive_dir, dest_dir, strerror(errno),
                          __FILE__, __LINE__);
            return(INCORRECT);
         }
         else
         {
#ifdef HAVE_STATX
            struct statx stat_buf;
#else
            struct stat stat_buf;
#endif

            /*
             * Since we do not have write permission we cannot update
             * the access and modification time. So if age limit is set,
             * it can happen that the files are deleted immediatly by
             * sf_xxx.
             */
#ifdef HAVE_STATX
            if (statx(0, dest_dir, AT_STATX_SYNC_AS_STAT,
                      STATX_SIZE, &stat_buf) == -1)
#else
            if (stat(dest_dir, &stat_buf) == -1)
#endif
            {
               (void)fprintf(stdout, "Failed to access `%s' : %s (%s %d)\n",
                             dest_dir, strerror(errno), __FILE__, __LINE__);
               return(INCORRECT);
            }
            else
            {
#ifdef HAVE_STATX
               *file_size = stat_buf.stx_size;
#else
               *file_size = stat_buf.st_size;
#endif
            }
         }
      }
   }

   return(SUCCESS);
}


/*++++++++++++++++++++++++++++++ write_fsa() ++++++++++++++++++++++++++++*/
/*                               -----------                             */
/* Description: Writes the number of files and the sum of their size to  */
/*              the FSA. When 'add' is YES these values will be added    */
/*              to the current values in the FSA. If it is NO, they will */
/*              be subtracted.                                           */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
write_fsa(int add, int files_to_send, off_t file_size_to_send)
{
   if (files_to_send > 0)
   {
      unsigned int error_mask,
                   scheme;
      char         real_hostname[MAX_REAL_HOSTNAME_LENGTH + 1],
                   smtp_server[MAX_REAL_HOSTNAME_LENGTH + 1],
                   truncated_hostname[MAX_HOSTNAME_LENGTH + 1],
                   user[MAX_USER_NAME_LENGTH + 1];

      get_info(GOT_JOB_ID);
      get_info_free();
      if ((error_mask = url_evaluate(id.recipient, &scheme, user, NULL, NULL,
#ifdef WITH_SSH_FINGERPRINT
                                     NULL, NULL,
#endif
                                     NULL, NO, real_hostname, NULL, NULL, NULL,
                                     NULL, NULL, NULL, NULL, NULL, NULL,
                                     smtp_server)) < 4)
      {
         int position;

         if (user[0] == '\0')
         {
            if (real_hostname[0] == MAIL_GROUP_IDENTIFIER)
            {
               position = 0;
               while (real_hostname[position + 1] != '\0')
               {
                  real_hostname[position] = real_hostname[position + 1];
                  position++;
               }
               real_hostname[position] = '\0';
            }
         }
#ifdef _WITH_DE_MAIL_SUPPORT
         if (((scheme & SMTP_FLAG) || (scheme & DE_MAIL_FLAG)) &&
             (smtp_server[0] != '\0'))
#else
         if ((scheme & SMTP_FLAG) && (smtp_server[0] != '\0'))
#endif
         {
            position = 0;
            while (smtp_server[position] != '\0')
            {
               real_hostname[position] = smtp_server[position];
               position++;
            }
            real_hostname[position] = '\0';
         }
         t_hostname(real_hostname, truncated_hostname);
         if ((position = get_host_position(fsa, truncated_hostname,
                                           no_of_hosts)) != INCORRECT)
         {
            off_t lock_offset;

            lock_offset = AFD_WORD_OFFSET +
                          (position * sizeof(struct filetransfer_status));
            (void)check_fsa(NO, SHOW_OLOG);
#ifdef LOCK_DEBUG
            lock_region_w(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
            lock_region_w(fsa_fd, lock_offset + LOCK_TFC);
#endif
            if (add == YES)
            {
                /* Total file counter. */
                fsa[position].total_file_counter += files_to_send;

                /* Total file size. */
                fsa[position].total_file_size += file_size_to_send;
            }
            else
            {
                /* Total file counter. */
                fsa[position].total_file_counter -= files_to_send;

                /* Total file size. */
                fsa[position].total_file_size -= file_size_to_send;
            }
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, lock_offset + LOCK_TFC);
#endif
         }

         /*
          * NOTE: When we fail to get the host name or the host is
          *       no longer in the FSA, lets quietly ignore the writing
          *       into the FSA.
          */
      }
      else
      {
         char error_msg[MAX_URL_ERROR_MSG];

         url_get_error(error_mask, error_msg, MAX_URL_ERROR_MSG);
         (void)fprintf(stderr,
                       "Unable to update FSA due to incorrect url `%s' : %s.\n",
                       id.recipient, error_msg);
      }

      /* Free all memory that was allocated in get_info(). */
      free(id.files);
      id.files = NULL;
#ifdef _WITH_DYNAMIC_MEMORY
      if (id.loptions != NULL)
      {
         FREE_RT_ARRAY(id.loptions);
      }
#endif
      free((void *)id.soptions);
   }

   return;
}


/*++++++++++++++++++++++++ get_afd_config_value() +++++++++++++++++++++++*/
static void
get_afd_config_value(void)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH];

   (void)sprintf(config_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_file, F_OK) == 0) &&
       (read_file_no_cr(config_file, &buffer, YES, __FILE__, __LINE__) != INCORRECT))
   {
      char value[MAX_INT_LENGTH];

      if (get_definition(buffer, MAX_COPIED_FILES_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         max_copied_files = atoi(value);
         if ((max_copied_files < 1) || (max_copied_files > 10240))
         {
            max_copied_files = MAX_COPIED_FILES;
         }
      }
      else
      {
         max_copied_files = MAX_COPIED_FILES;
      }
      free(buffer);
   }
   else
   {
      max_copied_files = MAX_COPIED_FILES;
   }

   return;
}
