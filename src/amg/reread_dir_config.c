/*
 *  reread_dir_config.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   reread_dir_config - reads the DIR_CONFIG file
 **
 ** SYNOPSIS
 **   int reread_dir_config(int              dc_changed,
 **                         off_t            db_size,
 **                         time_t           *hc_old_time,
 **                         int              old_no_of_hosts,
 **                         int              rewrite_host_config,
 **                         size_t           old_size,
 **                         int              rescan_time,
 **                         int              max_no_proc,
 **                         int              *using_groups,
 **                         unsigned int     *warn_counter,
 **                         FILE             *debug_fp,
 **                         pid_t            udc_pid,
 **                         struct host_list *old_hl)
 **                                                         
 ** DESCRIPTION
 **   This function reads the DIR_CONFIG file and sets the values
 **   in the FSA.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when an error is encountered. Otherwise the
 **   following status value can be returned:
 **        DIR_CONFIG_EMPTY
 **        DIR_CONFIG_NO_VALID_DATA
 **        DIR_CONFIG_UPDATED_DC_PROBLEMS
 **        DIR_CONFIG_UPDATED
 **        NO_CHANGE_IN_DIR_CONFIG
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.01.1998 H.Kiehl Created
 **   04.08.2001 H.Kiehl Added changes for host_status and special flag
 **                      field fromt the HOST_CONFIG file.
 **   16.05.2002 H.Kiehl Removed shared memory stuff.
 **   03.05.2007 H.Kiehl Changed function that it returns what was done.
 **   10.04.2017 H.Kiehl Added debug_fp parameter to print warnings and
 **                      errors.
 **
 */
DESCR__E_M3

#include <stdio.h>             /* fclose(), fseek()                      */
#include <string.h>            /* memcmp(), memset(), strerror()         */
#include <stdlib.h>            /* malloc(), free()                       */
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>            /* kill()                                 */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                        create_source_dir,
                                  data_length,
                                  dnb_fd,
                                  no_of_hosts,
                                  no_of_dir_configs;
extern mode_t                     create_source_dir_mode;
extern pid_t                      dc_pid;
extern char                       **dir_config_file,
                                  *host_config_file,
                                  *pid_list,
                                  *p_work_dir;
extern struct host_list           *hl;
extern struct dir_name_buf        *dnb;
extern struct filetransfer_status *fsa;


/*########################## reread_dir_config() ########################*/
int
reread_dir_config(int              dc_changed,
                  off_t            db_size,
                  time_t           *hc_old_time,
                  int              old_no_of_hosts,
                  int              rewrite_host_config,
                  size_t           old_size,
                  int              rescan_time,
                  int              max_no_proc,
                  int              *using_groups,
                  unsigned int     *warn_counter,
                  FILE             *debug_fp,
                  pid_t            udc_pid,
                  struct host_list *old_hl)
{
   int new_fsa = NO,
       ret = NO_CHANGE_IN_DIR_CONFIG;

   if (db_size > 0)
   {
      if ((dc_changed == NO) && (old_hl != NULL))
      {
         /* First check if there was any change at all. */
         if ((old_no_of_hosts == no_of_hosts) &&
             (memcmp(hl, old_hl, old_size) == 0))
         {
            new_fsa = NONE;
            if (rewrite_host_config == NO)
            {
               system_log(INFO_SIGN, NULL, 0,
                          "There is no change in the HOST_CONFIG file.");
            }
         }
         else /* Yes, something did change. */
         {
            int  host_pos,
                 i,
                 j,
                 no_of_gotchas = 0;
            char *gotcha;

            /*
             * The gotcha array is used to find hosts that are in the
             * old FSA but not in the HOST_CONFIG file.
             */
            if ((gotcha = malloc(old_no_of_hosts)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "malloc() error : %s", strerror(errno));
               exit(INCORRECT);
            }
            (void)memset(gotcha, NO, old_no_of_hosts);

            for (i = 0; i < no_of_hosts; i++)
            {
               host_pos = INCORRECT;
               for (j = 0; j < old_no_of_hosts; j++)
               {
                  if (gotcha[j] != YES)
                  {
                     if (CHECK_STRCMP(hl[i].host_alias, old_hl[j].host_alias) == 0)
                     {
                        host_pos = j;
                        break;
                     }
                  }
               }
               if (host_pos == INCORRECT)
               {
                  /*
                   * A new host. Now we have to create a new FSA.
                   */
                  new_fsa = YES;
                  break;
               }
               else
               {
                  gotcha[j] = YES;
                  no_of_gotchas++;
               }
            } /* for (i = 0; i < no_of_hosts; i++) */

            free(gotcha);

            if ((no_of_gotchas != old_no_of_hosts) || (new_fsa == YES))
            {
               new_fsa = YES;
               create_fsa();
               system_log(INFO_SIGN, NULL, 0,
                          "Found %d hosts in HOST_CONFIG.", no_of_hosts);
            }
         }
      } /* if ((dc_changed == NO) && (old_hl != NULL)) */

      /* Check if DIR_CONFIG has changed. */
      if (dc_changed == YES)
      {
         int   i;
         pid_t tmp_dc_pid = dc_pid;

         /* Tell user we have to reread the new DIR_CONFIG file(s). */
         system_log(INFO_SIGN, NULL, 0, "Rereading DIR_CONFIG(s)...");

         /* Stop running jobs. */
         if ((data_length > 0) && (dc_pid > 0))
         {
            if (com(STOP, __FILE__, __LINE__) == INCORRECT)
            {
               /* If the process does not answer, lets assume */
               /* something is really wrong here and lets see */
               /* if the process has died.                    */
               if (amg_zombie_check(&dc_pid, WNOHANG) != YES)
               {
                  /*
                   * It is still alive but does not respond
                   * so mark it as in unknown state. The pid
                   * is already stored in tmp_dc_pid.
                   */
                  dc_pid = UNKNOWN_STATE;
               }
            }
            else
            {
               dc_pid = NOT_RUNNING;

               if (pid_list != NULL)
               {
                  *(pid_t *)(pid_list + ((DC_NO + 1) * sizeof(pid_t))) = 0;
               }

               /* Collect zombie of stopped job. */
               (void)amg_zombie_check(&dc_pid, 0);
            }
         }

         /* Reread database file. */
         for (i = 0; i < no_of_hosts; i++)
         {
            hl[i].in_dir_config = NO;
            hl[i].protocol = 0;
         }
#ifdef WITH_ONETIME
         if (eval_dir_config(db_size, warn_counter, debug_fp, NO, using_groups) < 0)
#else
         if (eval_dir_config(db_size, warn_counter, debug_fp, using_groups) < 0)
#endif
         {
            update_db_log(ERROR_SIGN, __FILE__, __LINE__, debug_fp, NULL,
                          "Could not find any valid entries in database %s",
                          (no_of_dir_configs > 1) ? "files" : "file");
         }

         /* Free dir name buffer which is no longer needed. */
         if (dnb != NULL)
         {
            unmap_data(dnb_fd, (void *)&dnb);
         }

         /* Start, restart or stop jobs. */
         if (data_length > 0)
         {
            /*
             * When dir_check is started it too will write to debug_fp!
             * So lets flush the data now.
             */
            if (debug_fp != NULL)
            {
               (void)fflush(debug_fp);
            }

            /*
             * Since there might have been an old FSA which has more
             * information then the HOST_CONFIG lets rewrite this
             * file using the information from both HOST_CONFIG and
             * old FSA. That what is found in the HOST_CONFIG will
             * always have a higher priority.
             */
            *hc_old_time = write_host_config(no_of_hosts, host_config_file, hl);
            system_log(INFO_SIGN, NULL, 0,
                       "Found %d hosts in HOST_CONFIG.", no_of_hosts);

            switch (dc_pid)
            {
               case NOT_RUNNING :
               case DIED :
                  dc_pid = make_process_amg(p_work_dir, DC_PROC_NAME,
                                            rescan_time, max_no_proc,
                                            (create_source_dir == YES) ? create_source_dir_mode : 0,
                                            udc_pid);
                  if (pid_list != NULL)
                  {
                     *(pid_t *)(pid_list + ((DC_NO + 1) * sizeof(pid_t))) = dc_pid;
                  }
                  break;

               case UNKNOWN_STATE :
                  /* Since we do not know the state, lets just kill it. */
                  if (tmp_dc_pid > 0)
                  {
                     if (kill(tmp_dc_pid, SIGINT) < 0)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Failed to send kill signal to process %s : %s",
                                   DC_PROC_NAME, strerror(errno));

                        /* Don't exit here, since the process might */
                        /* have died in the meantime.               */
                     }
                     else
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                   "Have killed %s (%d) because it was in unknown state.",
#else
                                   "Have killed %s (%lld) because it was in unknown state.",
#endif
                                   DC_PROC_NAME, (pri_pid_t)tmp_dc_pid);
                     }

                     /* Eliminate zombie of killed job. */
                     (void)amg_zombie_check(&tmp_dc_pid, 0);
                  }
                  else
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                "Hmmm, pid is %d!!!", (pri_pid_t)tmp_dc_pid);
#else
                                "Hmmm, pid is %lld!!!", (pri_pid_t)tmp_dc_pid);
#endif
                  }

                  dc_pid = make_process_amg(p_work_dir, DC_PROC_NAME,
                                            rescan_time, max_no_proc,
                                            (create_source_dir == YES) ? create_source_dir_mode : 0,
                                            udc_pid);
                  if (pid_list != NULL)
                  {
                     *(pid_t *)(pid_list + ((DC_NO + 1) * sizeof(pid_t))) = dc_pid;
                  }
                  break;

               default :
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Hmmm..., whats going on? I should not be here.");
                  break;
            }
            if (dc_pid > 0)
            {
               /*
                * Wait for dir_check to come up again and ready so in
                * case there are warnings or errors they can be shown
                * to the user that has used udc.
                */
               if (com(DATA_READY, __FILE__, __LINE__) != SUCCESS)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Process %s did not reply on DATA_READY!",
                             DIR_CHECK);
               }

               if (debug_fp != NULL)
               {
                  /*
                   * dir_check() could have written something, so we need
                   * to go to the end of the file, in case we want to write
                   * some more information.
                   */
                  if (fseek(debug_fp, 0L, SEEK_END) == -1)
                  {
                     system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                "fseek() to end of file failed : %s",
                                strerror(errno));
                  }
               }

               ret = DIR_CONFIG_UPDATED;
            }
            else
            {
               ret = DIR_CONFIG_UPDATED_DC_PROBLEMS;
            }
         }
         else
         {
            if (dc_pid > 0)
            {
               (void)com(STOP, __FILE__, __LINE__);
               dc_pid = NOT_RUNNING;
            }
            ret = DIR_CONFIG_NO_VALID_DATA;
         }

         /* Tell user we have reread new DIR_CONFIG file. */
         system_log(INFO_SIGN, NULL, 0,
                    "Done with rereading DIR_CONFIG %s.",
                    (no_of_dir_configs > 1) ? "files" : "file");
      }
      else if ((old_hl != NULL) && (new_fsa == NO))
           {
              int  host_order_changed = NO,
                   no_of_host_changed = 0,
                   host_pos,
                   i,
                   j;
              char *mark_list;

              if (fsa_attach(AMG) != SUCCESS)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Could not attach to FSA!");
                 return(INCORRECT);
              }

              /*
               * In the first step lets just update small changes.
               * (Changes where we do no need to rewrite the FSA.
               * That is when the order of hosts has changed.)
               */
              if ((mark_list = malloc(old_no_of_hosts)) == NULL)
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "malloc() error : %s", strerror(errno));
                 (void)fsa_detach(NO);
                 return(INCORRECT);
              }
              for (i = 0; i < no_of_hosts; i++)
              {
                 host_pos = INCORRECT;
                 (void)memset(mark_list, NO, old_no_of_hosts);
                 for (j = 0; j < old_no_of_hosts; j++)
                 {
                    if ((mark_list[j] == NO) &&
                        (CHECK_STRCMP(hl[i].host_alias, old_hl[j].host_alias) == 0))
                    {
                       host_pos = j;
                       mark_list[j] = YES;
                       break;
                    }
                 }
                 if (host_pos != INCORRECT)
                 {
                    if (host_pos != i)
                    {
                       host_order_changed = YES;
                    }
                    if (memcmp(&hl[i], &old_hl[host_pos], sizeof(struct host_list)) != 0)
                    {
                       no_of_host_changed++;

                       /*
                        * Some parameters for this host have
                        * changed. Instead of finding the
                        * place where the change took place,
                        * overwrite all parameters.
                        */
                       (void)memcpy(fsa[host_pos].real_hostname[0],
                                    hl[i].real_hostname[0],
                                    MAX_REAL_HOSTNAME_LENGTH);
                       (void)memcpy(fsa[host_pos].real_hostname[1],
                                    hl[i].real_hostname[1],
                                    MAX_REAL_HOSTNAME_LENGTH);
                       (void)memcpy(fsa[host_pos].proxy_name, hl[i].proxy_name,
                                    MAX_PROXY_NAME_LENGTH + 1);
                       fsa[host_pos].allowed_transfers = hl[i].allowed_transfers;
                       if (old_hl[i].allowed_transfers != hl[i].allowed_transfers)
                       {
                          int k;

                          for (k = 0; k < fsa[host_pos].allowed_transfers; k++)
                          {
                             fsa[host_pos].job_status[k].no_of_files = 0;
                             fsa[host_pos].job_status[k].connect_status = DISCONNECT;
#ifdef _WITH_BURST_2
                             fsa[host_pos].job_status[k].job_id = NO_ID;
#endif
                          }
                          for (k = fsa[host_pos].allowed_transfers; k < MAX_NO_PARALLEL_JOBS; k++)
                          {
                             fsa[host_pos].job_status[k].no_of_files = -1;
                          }
                       }
                       fsa[host_pos].max_errors = hl[i].max_errors;
                       fsa[host_pos].retry_interval = hl[i].retry_interval;
                       fsa[host_pos].block_size = hl[i].transfer_blksize;
                       fsa[host_pos].max_successful_retries = hl[i].successful_retries;
                       fsa[host_pos].file_size_offset = hl[i].file_size_offset;
                       fsa[host_pos].transfer_timeout = hl[i].transfer_timeout;
                       fsa[host_pos].transfer_rate_limit = hl[i].transfer_rate_limit;
                       fsa[host_pos].protocol = hl[i].protocol;
                       fsa[host_pos].protocol_options = hl[i].protocol_options;
                       fsa[host_pos].ttl = hl[i].ttl;
                       fsa[host_pos].socksnd_bufsize = hl[i].socksnd_bufsize;
                       fsa[host_pos].sockrcv_bufsize = hl[i].sockrcv_bufsize;
                       fsa[host_pos].keep_connected = hl[i].keep_connected;
                       fsa[host_pos].warn_time = hl[i].warn_time;
#ifdef WITH_DUP_CHECK
                       fsa[host_pos].dup_check_flag = hl[i].dup_check_flag;
                       fsa[host_pos].dup_check_timeout = hl[i].dup_check_timeout;
#endif
                       fsa[host_pos].special_flag = 0;
                       if (hl[i].host_status & HOST_CONFIG_HOST_DISABLED)
                       {
                          fsa[host_pos].special_flag |= HOST_DISABLED;
                       }
                       else
                       {
                          fsa[host_pos].special_flag &= ~HOST_DISABLED;
                       }
                       fsa[host_pos].host_status = 0;
                       if (hl[i].host_status & STOP_TRANSFER_STAT)
                       {
                          fsa[host_pos].host_status |= STOP_TRANSFER_STAT;
                       }
                       else
                       {
                          fsa[host_pos].host_status &= ~STOP_TRANSFER_STAT;
                       }
                       if (hl[i].host_status & PAUSE_QUEUE_STAT)
                       {
                          fsa[host_pos].host_status |= PAUSE_QUEUE_STAT;
                       }
                       else
                       {
                          fsa[host_pos].host_status &= ~PAUSE_QUEUE_STAT;
                       }
                       if (hl[i].host_status & HOST_ERROR_OFFLINE_STATIC)
                       {
                          fsa[host_pos].host_status |= HOST_ERROR_OFFLINE_STATIC;
                       }
                       else
                       {
                          fsa[host_pos].host_status &= ~HOST_ERROR_OFFLINE_STATIC;
                       }
                       if (hl[i].host_status & DO_NOT_DELETE_DATA)
                       {
                          fsa[host_pos].host_status |= DO_NOT_DELETE_DATA;
                       }
                       else
                       {
                          fsa[host_pos].host_status &= ~DO_NOT_DELETE_DATA;
                       }
                       if (hl[i].host_status & SIMULATE_SEND_MODE)
                       {
                          fsa[host_pos].host_status |= SIMULATE_SEND_MODE;
                       }
                       else
                       {
                          fsa[host_pos].host_status &= ~SIMULATE_SEND_MODE;
                       }
                    }
                 }
              } /* for (i = 0; i < no_of_hosts; i++) */

              free(mark_list);

              if (no_of_host_changed > 0)
              {
                 system_log(INFO_SIGN, NULL, 0,
                            "%d host changed in HOST_CONFIG.",
                            no_of_host_changed);
              }

              /*
               * Now lets see if the host order has changed.
               */
              if (host_order_changed == YES)
              {
                 size_t new_size = no_of_hosts * sizeof(char *);
                 char   **p_host_names;

                 if ((p_host_names = malloc(new_size)) == NULL)
                 {
                    system_log(FATAL_SIGN, __FILE__, __LINE__,
                               "malloc() error [%d bytes] : %s",
                               new_size, strerror(errno));
                    (void)fsa_detach(NO);
                    return(INCORRECT);
                 }
                 for (i = 0; i < no_of_hosts; i++)
                 {
                    p_host_names[i] = hl[i].host_alias;
                 }
                 system_log(INFO_SIGN, NULL, 0, "Changing host alias order.");
                 change_alias_order(p_host_names, -1);
                 free(p_host_names);
              }

              (void)fsa_detach(YES);
           } /* if (old_hl != NULL) */
   }
   else
   {
      if (no_of_dir_configs > 1)
      {
         update_db_log(WARN_SIGN, NULL, 0, debug_fp, NULL,
                       "All DIR_CONFIG files are empty.");
      }
      else
      {
         update_db_log(WARN_SIGN, NULL, 0, debug_fp, NULL,
                       "DIR_CONFIG file is empty.");
      }
      ret = DIR_CONFIG_EMPTY;
   }

   if (rewrite_host_config == YES)
   {
      *hc_old_time = write_host_config(no_of_hosts, host_config_file, hl);
      system_log(INFO_SIGN, NULL, 0,
                "Found %d hosts in HOST_CONFIG.", no_of_hosts);
   }

   return(ret);
}
