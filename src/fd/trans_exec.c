/*
 *  trans_exec.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   trans_exec - execute a command for a file that has just been send.
 **
 ** SYNOPSIS
 **   void trans_exec(char    *file_path,
 **                   char    *fullname,
 **                   char    *p_file_name_buffer,
 **                   clock_t clktck)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None. If we fail to log the fail name, all that happens the next
 **   time is that the complete file gets send again.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.05.2001 H.Kiehl Created
 **   11.02.2007 H.Kiehl Added locking option.
 **   03.08.2017 H.Kiehl Allow user to specify with the %n parameter
 **                      the full new name.
 **
 */
DESCR__E_M3

#include <stdio.h>                /* snprintf()                          */
#include <stdlib.h>               /* free()                              */
#include <string.h>               /* strerror()                          */
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>               /* geteuid()                           */
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern int                        fsa_fd,
                                  simulation_mode,
                                  transfer_log_fd;
extern struct filetransfer_status *fsa;
extern struct job                 db;


/*############################ trans_exec() #############################*/
void
trans_exec(char    *file_path,
           char    *fullname,
           char    *p_file_name_buffer,
           clock_t clktck)
{
   char *p_command,
        tmp_connect_status;

   if (simulation_mode == YES)
   {
      return;
   }
   tmp_connect_status = fsa->job_status[(int)db.job_no].connect_status;
   fsa->job_status[(int)db.job_no].connect_status = POST_EXEC;
   p_command = db.trans_exec_cmd;
   while ((*p_command == ' ') || (*p_command == '\t'))
   {
      p_command++;
   }
   if ((*p_command == '\n') || (*p_command == '\0'))
   {
      trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                "No command specified for executing. Ignoring this option.");
   }
   else
   {
      register int ii = 0,
                   k = 0;
      int          doit;
      char         *p_end = p_command,
                   *p_tmp_dir,
                   *insert_list[MAX_EXEC_FILE_SUBSTITUTION],
                   insert_type[MAX_EXEC_FILE_SUBSTITUTION],
                   tmp_option[1024],
                   command_str[1024],
                   *return_str = NULL;

      while ((*p_end != '\n') && (*p_end != '\0') &&
             (ii < MAX_EXEC_FILE_SUBSTITUTION))
      {
         if ((*p_end == '%') &&
             ((*(p_end + 1) == 's') || (*(p_end + 1) == 'n')))
         {
            tmp_option[k] = *p_end;
            tmp_option[k + 1] = *(p_end + 1);
            insert_list[ii] = &tmp_option[k];
            insert_type[ii] = *(p_end + 1);
            ii++;
            p_end += 2;
            k += 2;
         }
         else
         {
            tmp_option[k] = *p_end;
            p_end++; k++;
         }
      }
      if (ii >= MAX_EXEC_FILE_SUBSTITUTION)
      {
         trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                   "To many %%s in pexec option. Can oly handle %d.",
                   MAX_EXEC_FILE_SUBSTITUTION);
      }
      tmp_option[k] = '\0';
      p_command = tmp_option;

      if ((db.special_flag & EXECUTE_IN_TARGET_DIR) &&
          (db.protocol & LOC_FLAG))
      {
         doit = YES;
         p_tmp_dir = NULL;
      }
      else
      {
         /*
          * Create a temporary directory in which the user can execute
          * the commands. We do not want the manipulated files in the
          * archive. After the user commands are executed the files
          * in the temporary directory and the directory will be
          * removed. This is not efficient, but is the simplest way
          * to implement this.
          */
         p_tmp_dir = file_path + strlen(file_path);
         (void)strcpy(p_tmp_dir, "/.tmp");
         if ((mkdir(file_path, DIR_MODE) == -1) && (errno != EEXIST))
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                      "Failed to mkdir() %s : %s", file_path, strerror(errno));
            doit = NO;
         }
         else
         {
            *(p_tmp_dir + 5) = '/';
            (void)strcpy(p_tmp_dir + 6, p_file_name_buffer);
            if (copy_file(fullname, file_path, NULL) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Failed to copy_file() `%s' to `%s'.",
                         fullname, file_path);
               *(p_tmp_dir + 5) = '\0';
               doit = NEITHER;
            }
            else
            {
               doit = YES;
            }
         }
      }

      if (doit == YES)
      {
#ifdef HAVE_SETPRIORITY
         int  sched_priority;
#endif
         char job_str[4];

         job_str[0] = '[';
         job_str[1] = db.job_no + '0';
         job_str[2] = ']';
         job_str[3] = '\0';
         if (p_tmp_dir != NULL)
         {
            *(p_tmp_dir + 5) = '\0';
         }

#ifdef HAVE_SETPRIORITY
         if (db.exec_base_priority != NO_PRIORITY)
         {
            sched_priority = db.exec_base_priority;
            if (db.add_afd_priority == YES)
            {                           
               sched_priority += (int)(fsa->job_status[(int)db.job_no].unique_name[MAX_MSG_NAME_LENGTH - 1]);
               if (sched_priority > db.min_sched_priority)        
               {
                  sched_priority = db.min_sched_priority;
               }
               else if (sched_priority < db.max_sched_priority)
                    {
                       sched_priority = db.max_sched_priority;
                    }
            }
            if ((sched_priority == db.current_priority) ||
                ((db.current_priority > sched_priority) && (geteuid() != 0)))
            {
               sched_priority = NO_PRIORITY;
            }
         }
         else
         {
            sched_priority = NO_PRIORITY;
         }
#endif

         if (db.set_trans_exec_lock == YES)
         {
#ifdef LOCK_DEBUG
            lock_region_w(fsa_fd, db.lock_offset + LOCK_EXEC, __FILE__, __LINE__);
#else
            lock_region_w(fsa_fd, db.lock_offset + LOCK_EXEC);
#endif
         }

         /* Setup command by inserting the filenames for all %s */
         if (ii > 0)
         {
            int  fullname_checked = NO,
                 length,
                 length_start,
                 mask_file_name,
                 p_file_name_buffer_checked = NO,
                 ret;
            char *fnp,
                 *p_name,
                 tmp_char;

            insert_list[ii] = &tmp_option[k];
            tmp_char = *insert_list[0];
            *insert_list[0] = '\0';
            length_start = snprintf(command_str, 1024,
                                    "cd %s && %s", file_path, p_command);
            *insert_list[0] = tmp_char;

            /* Generate command string with file name(s). */
            length = 0;
            for (k = 1; k < (ii + 1); k++)
            {
               if (insert_type[k - 1] == 'n')
               {
                  p_name = fullname;
               }
               else
               {
                  p_name = p_file_name_buffer;
               }
               if (((insert_type[k - 1] == 'n') && (fullname_checked == NO)) ||
                   ((insert_type[k - 1] == 's') &&
                    (p_file_name_buffer_checked == NO)))
               {
                  fnp = p_name;
                  mask_file_name = NO;
                  do
                  {
                     if ((*fnp == ';') || (*fnp == ' '))
                     {
                        mask_file_name = YES;
                        break;
                     }
                     fnp++;
                  } while (*fnp != '\0');
               }

               tmp_char = *insert_list[k];
               *insert_list[k] = '\0';
               if (mask_file_name == YES)
               {
                  length += snprintf(command_str + length_start + length,
                                     1024 - (length_start + length),
                                     "\"%s\"%s", p_name,
                                     insert_list[k - 1] + 2);
               }
               else
               {
                  length += snprintf(command_str + length_start + length,
                                     1024 - (length_start + length),
                                     "%s%s", p_name, insert_list[k - 1] + 2);
               }
               *insert_list[k] = tmp_char;
            }

            if ((ret = exec_cmd(command_str, &return_str, transfer_log_fd,
                                fsa->host_dsp_name, MAX_HOSTNAME_LENGTH,
#ifdef HAVE_SETPRIORITY
                                sched_priority,
#endif
                                job_str, NULL, NULL, clktck,
                                db.trans_exec_timeout,
                                YES, YES)) != 0) /* ie != SUCCESS */
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Failed to execute command %s [Return code = %d]",
                         command_str, ret);
               if ((return_str != NULL) && (return_str[0] != '\0'))
               {
                  char *end_ptr = return_str,
                       *start_ptr;

                  do
                  {
                     start_ptr = end_ptr;
                     while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                     {
                        end_ptr++;
                     }
                     if (*end_ptr == '\n')
                     {
                        *end_ptr = '\0';
                        end_ptr++;
                     }
                     trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                               "%s", start_ptr);
                  } while (*end_ptr != '\0');
               }
            }
            else
            {
               (void)my_strncpy(fsa->job_status[(int)db.job_no].file_name_in_use,
                                command_str, MAX_MSG_NAME_LENGTH + 1);
            }
         }
         else /* Just execute command without file. */
         {
            int ret;

            if (snprintf(command_str, 1024, "cd %s && %s", file_path, p_command) >= 1024)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Failed to copy full command to buffer since it is longer then 1024 bytes.");
            }
            else
            {
               if ((ret = exec_cmd(command_str, &return_str, transfer_log_fd,
                                   fsa->host_dsp_name, MAX_HOSTNAME_LENGTH,
#ifdef HAVE_SETPRIORITY
                                   sched_priority,
#endif
                                   job_str, NULL, NULL, clktck,
                                   db.trans_exec_timeout, YES, YES)) != 0)
               {
                  trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                            "Failed to execute command %s [Return code = %d]",
                            command_str, ret);
                  if ((return_str != NULL) && (return_str[0] != '\0'))
                  {
                     char *end_ptr = return_str,
                          *start_ptr;

                     do
                     {
                        start_ptr = end_ptr;
                        while ((*end_ptr != '\n') && (*end_ptr != '\0'))
                        {
                           end_ptr++;
                        }
                        if (*end_ptr == '\n')
                        {
                           *end_ptr = '\0';
                           end_ptr++;
                        }
                        trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                                  "%s", start_ptr);
                     } while (*end_ptr != '\0');
                  }
               }
               else
               {
                  (void)my_strncpy(fsa->job_status[(int)db.job_no].file_name_in_use,
                                   p_command, MAX_MSG_NAME_LENGTH + 1);
               }
            }
         }
         if (db.set_trans_exec_lock == YES)
         {
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, db.lock_offset + LOCK_EXEC, __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, db.lock_offset + LOCK_EXEC);
#endif
         }
         free(return_str);
      }
      if (p_tmp_dir != NULL)
      {
         if ((doit == YES) || (doit == NEITHER))
         {
            if (rec_rmdir(file_path) < 0)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__, NULL, NULL,
                         "Failed to remove directory %s.", file_path);
            }
         }
         *p_tmp_dir = '\0';
      }
   }
   fsa->job_status[(int)db.job_no].file_name_in_use[0] = '\0';
   fsa->job_status[(int)db.job_no].connect_status = tmp_connect_status;

   return;
}
