/*
 *  reread_host_config.c - Part of AFD, an automatic file distribution
 *                         program.
 *  Copyright (c) 1998 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   reread_host_config - reads the HOST_CONFIG file
 **
 ** SYNOPSIS
 **   int reread_host_config(time_t           *hc_old_time,
 **                          int              *old_no_of_hosts,
 **                          int              *rewrite_host_config,
 **                          size_t           *old_size,
 **                          struct host_list **old_hl,
 **                          unsigned int     *warn_counter,
 **                          FILE             *debug_fp,
 **                          int              inform_fd)
 **
 ** DESCRIPTION
 **   This function reads the HOST_CONFIG file and sets the values
 **   in the FSA.
 **
 ** RETURN VALUES
 **   Depending on the error, the function calls exit() or returns
 **   INCORRECT. On success one of the following values are returned:
 **     NO_CHANGE_IN_HOST_CONFIG
 **     HOST_CONFIG_RECREATED
 **     HOST_CONFIG_DATA_CHANGED
 **     HOST_CONFIG_ORDER_CHANGED
 **     HOST_CONFIG_DATA_ORDER_CHANGED
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.01.1998 H.Kiehl Created
 **   03.08.2001 H.Kiehl Remember if we stopped the queue or transfer
 **                      and some protocol specific information.
 **   19.02.2002 H.Kiehl Stop process dir_check when we reorder the
 **                      FSA.
 **   03.05.2007 H.Kiehl Changed function that it returns what was done.
 **   19.07.2019 H.Kiehl Simulate mode is stored in HOST_CONFIG.
 **   07.02.2023 H.Kiehl Add check if host has been removed but is
 **                      still in DIR_CONFIG. If that is the case
 **                      put it back into the HOST_CONFIG.
 **
 */
DESCR__E_M3

#define CHECK_HOST_REMOVED_BUT_STILL_IN_DIR_CONFIG 1

#include <string.h>              /* strerror(), memset(), strcmp(),      */
                                 /* memcpy(), memmove()                  */
#include <stdlib.h>              /* malloc(), free()                     */
#ifdef HAVE_STATX
# include <fcntl.h>              /* Definition of AT_* constants         */
#endif
#include <sys/stat.h>
#include <sys/wait.h>            /* WNOHANG                              */
#include <unistd.h>
#include <errno.h>
#include "amgdefs.h"

/* External Global Variables. */
extern int                        no_of_hosts;
extern pid_t                      dc_pid;
extern char                       *host_config_file,
                                  *pid_list;
extern struct host_list           *hl;
extern struct filetransfer_status *fsa;
extern struct afd_status          *p_afd_status;


/*########################## reread_host_config() #######################*/
int
reread_host_config(time_t           *hc_old_time,
                   int              *old_no_of_hosts,
                   int              *rewrite_host_config,
                   size_t           *old_size,
                   struct host_list **old_hl,
                   unsigned int     *warn_counter,
                   FILE             *debug_fp,
                   int              inform_fd)
{
   int         ret = NO_CHANGE_IN_HOST_CONFIG;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat stat_buf;
#endif

   /* Get the size of the database file. */
#ifdef HAVE_STATX
   if (statx(0, host_config_file, AT_STATX_SYNC_AS_STAT,
             STATX_MTIME, &stat_buf) == -1)
#else
   if (stat(host_config_file, &stat_buf) == -1)
#endif
   {
      if (errno == ENOENT)
      {
         update_db_log(INFO_SIGN, NULL, 0, debug_fp, warn_counter,
                       "Recreating HOST_CONFIG file with %d hosts.",
                       no_of_hosts);
         *hc_old_time = write_host_config(no_of_hosts, host_config_file, hl);
         return(HOST_CONFIG_RECREATED);
      }
      else
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Could not stat() HOST_CONFIG file %s : %s",
                    host_config_file, strerror(errno));
         return(INCORRECT);
      }
   }

   /* Check if HOST_CONFIG has changed. */
#ifdef HAVE_STATX
   if (*hc_old_time < stat_buf.stx_mtime.tv_sec)
#else
   if (*hc_old_time < stat_buf.st_mtime)
#endif
   {
      int              dir_check_stopped = NO,
                       dummy_1,
                       dummy_2,
                       host_order_changed = NO,
                       host_pos,
                       i,
                       j,
                       new_no_of_hosts,
#ifdef CHECK_HOST_REMOVED_BUT_STILL_IN_DIR_CONFIG
                       no_of_hosts_put_back = 0,
#endif
                       no_of_host_changed = 0;
      size_t           dummy_3;
      char             *mark_list = NULL;
#ifdef CHECK_HOST_REMOVED_BUT_STILL_IN_DIR_CONFIG
      char             *host_list_put_back = NULL,
                       *p_host_list_put_back;
#endif
      struct host_list *dummy_hl;

      if (old_no_of_hosts == NULL)
      {
         old_no_of_hosts = &dummy_1;
      }
      if (rewrite_host_config == NULL)
      {
         rewrite_host_config = &dummy_2;
      }
      if (old_size == NULL)
      {
         old_size = &dummy_3;
      }
      if (old_hl == NULL)
      {
         old_hl = &dummy_hl;
      }

      /* Tell user we have to reread the new HOST_CONFIG file. */
      system_log(INFO_SIGN, NULL, 0, "Rereading HOST_CONFIG...");

      /* Now store the new time. */
#ifdef HAVE_STATX
      *hc_old_time = stat_buf.stx_mtime.tv_sec;
#else
      *hc_old_time = stat_buf.st_mtime;
#endif

      /* Reread HOST_CONFIG file. */
      if (hl != NULL)
      {
         *old_size = no_of_hosts * sizeof(struct host_list);

         if ((*old_hl = malloc(*old_size)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "malloc() error : %s", strerror(errno));
            return(INCORRECT);
         }
         (void)memcpy(*old_hl, hl, *old_size);
         *old_no_of_hosts = no_of_hosts;
         free(hl);
         hl = NULL;
      }
      else
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmm, no old HOST_CONFIG data!");
         *old_no_of_hosts = 0;
      }

      /*
       * Careful! The function eval_host_config() and fsa_attach()
       * will overwrite no_of_hosts! Store it somewhere save.
       */
      *rewrite_host_config = eval_host_config(&no_of_hosts, host_config_file,
                                              &hl, warn_counter, debug_fp, NO);
      new_no_of_hosts = no_of_hosts;
      if (fsa_attach(AMG) != SUCCESS)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__, "Could not attach to FSA!");
         exit(INCORRECT);
      }

      /*
       * In the first step lets just update small changes.
       * (Changes where we do not need to rewrite the FSA.
       * That is when the order of hosts has changed.)
       */
      if ((mark_list = malloc(*old_no_of_hosts)) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "malloc() error : %s", strerror(errno));
         exit(INCORRECT);
      }

#ifdef CHECK_HOST_REMOVED_BUT_STILL_IN_DIR_CONFIG
      /* First lets search if any host_alias has been removed */
      /* that is still in DIR_CONFIG. If that is the case     */
      /* put it back into the HOST_CONFIG.                    */
      (void)memset(mark_list, NO, (unsigned int)(*old_no_of_hosts));
      for (i = 0; i < new_no_of_hosts; i++)
      {
         for (j = 0; j < *old_no_of_hosts; j++)
         {
            if ((mark_list[j] == NO) &&
                (CHECK_STRCMP(hl[i].host_alias, (*old_hl)[j].host_alias) == 0))
            {
               mark_list[j] = YES;
               break;
            }
         }
      }
      for (i = 0; i < *old_no_of_hosts; i++)
      {
         if (mark_list[i] == NO)
         {
            for (j = 0; j < no_of_hosts; j++)
            {
               if (CHECK_STRCMP(fsa[j].host_alias,
                                (*old_hl)[i].host_alias) == 0)
               {
                  if (fsa[j].special_flag & HOST_IN_DIR_CONFIG)
                  {
                     if (((new_no_of_hosts + 1) % HOST_BUF_SIZE) == 0)
                     {
                        size_t new_size;

                        /* Host still in DIR_CONFIG, put it back. */
                        new_size = (((new_no_of_hosts + 1) / HOST_BUF_SIZE) + 1) *
                                   HOST_BUF_SIZE * sizeof(struct host_list);
                        if ((hl = realloc(hl, new_size)) == NULL)
                        {
                           system_log(FATAL_SIGN, __FILE__, __LINE__,
                                      _("Could not reallocate memory for host list : %s"),
                                      strerror(errno));
                           exit(INCORRECT);
                        }
                     }

                     /* Try put it back at same position where it was. */
                     if (i < new_no_of_hosts)
                     {
                        memmove(&hl[i + 1], &hl[i],
                                (new_no_of_hosts - i) * sizeof(struct host_list));
                        memcpy(&hl[i], (&(*old_hl)[i]), sizeof(struct host_list));
                     }
                     else
                     {
                        /* Put it to the end. */
                        memcpy(&hl[new_no_of_hosts], (&(*old_hl)[i]),
                               sizeof(struct host_list));
                        host_order_changed = YES;
                     }
                     mark_list[i] = YES;
                     new_no_of_hosts++;

                     no_of_hosts_put_back++;
                     if (host_list_put_back == NULL)
                     {
                        if ((host_list_put_back = malloc(MAX_HOSTNAME_LENGTH + 4)) == NULL)
                        {
                           system_log(FATAL_SIGN, __FILE__, __LINE__,
                                      _("Could not allocate memory for host list put back : %s"),
                                      strerror(errno));
                           exit(INCORRECT);
                        }
                        p_host_list_put_back = host_list_put_back +
                                               snprintf(host_list_put_back,
                                                        MAX_HOSTNAME_LENGTH + 1,
                                                        "%s",
                                                        (*old_hl)[i].host_alias);
                     }
                     else
                     {
                        int offset = p_host_list_put_back - host_list_put_back;

                        if ((host_list_put_back = realloc(host_list_put_back,
                                                          (MAX_HOSTNAME_LENGTH + 4) * no_of_hosts_put_back)) == NULL)
                        {
                           system_log(FATAL_SIGN, __FILE__, __LINE__,
                                      _("Could not reallocate memory for host list put back : %s"),
                                      strerror(errno));
                           exit(INCORRECT);
                        }
                        p_host_list_put_back = host_list_put_back + offset;
                        p_host_list_put_back += snprintf(p_host_list_put_back,
                                                         2 + MAX_HOSTNAME_LENGTH + 1,
                                                         ", %s",
                                                         (*old_hl)[i].host_alias);
                     }
                  }
                  break;
               }
            } /* for (j = 0; j < no_of_hosts; j++) */
         }
      }
#endif /* CHECK_HOST_REMOVED_BUT_STILL_IN_DIR_CONFIG */

      (void)memset(mark_list, NO, (unsigned int)(*old_no_of_hosts));
      for (i = 0; i < new_no_of_hosts; i++)
      {
         host_pos = INCORRECT;
         for (j = 0; j < *old_no_of_hosts; j++)
         {
            if ((mark_list[j] == NO) &&
                (CHECK_STRCMP(hl[i].host_alias, (*old_hl)[j].host_alias) == 0))
            {
               host_pos = j;
               mark_list[j] = YES;

               /*
                * At this stage we cannot know what protocols are used
                * or if the host is in the DIR_CONFIG.
                * So lets copy them from the old host list. This value
                * can only change when the DIR_CONFIG changes. The
                * function eval_dir_config() will take care if this
                * is the case.
                */
               hl[i].protocol = (*old_hl)[j].protocol;
               hl[i].in_dir_config = (*old_hl)[j].in_dir_config;
               break;
            }
         }
         if (host_pos != INCORRECT)
         {
            if (host_pos != i)
            {
               host_order_changed = YES;
               if ((dc_pid > 0) && (dir_check_stopped == NO))
               {
                  int options;

                  if (com(STOP, __FILE__, __LINE__) == INCORRECT)
                  {
                     options = WNOHANG;
                  }
                  else
                  {
                     options = 0;
                  }
                  (void)amg_zombie_check(&dc_pid, options);
                  dc_pid = NOT_RUNNING;
                  if (pid_list != NULL)
                  {
                     *(pid_t *)(pid_list + ((DC_NO + 1) * sizeof(pid_t))) = 0;
                  }
                  dir_check_stopped = YES;
               }
            }
            if (memcmp(&hl[i], (&(*old_hl)[host_pos]), sizeof(struct host_list)) != 0)
            {
               no_of_host_changed++;

               /*
                * Some parameters for this host have changed. Instead of
                * finding the place where the change took place, overwrite
                * all parameters.
                */
               (void)memcpy(fsa[host_pos].real_hostname[0],
                            hl[i].real_hostname[0], MAX_REAL_HOSTNAME_LENGTH);
               (void)memcpy(fsa[host_pos].real_hostname[1],
                            hl[i].real_hostname[1], MAX_REAL_HOSTNAME_LENGTH);
               if (CHECK_STRCMP(hl[i].host_toggle_str, (*old_hl)[host_pos].host_toggle_str) != 0)
               {
                  if (hl[i].host_toggle_str[0] == '\0')
                  {
                     fsa[host_pos].host_toggle_str[0] = '\0';
                     fsa[host_pos].host_dsp_name[(int)fsa[host_pos].toggle_pos] = ' ';
                     fsa[host_pos].original_toggle_pos = NONE;
                  }
                  else
                  {
                     if ((*old_hl)[host_pos].host_toggle_str[0] == '\0')
                     {
                        fsa[host_pos].toggle_pos = strlen(fsa[host_pos].host_alias);
                     }
                     (void)memcpy(fsa[host_pos].host_toggle_str, hl[i].host_toggle_str, 5);
                     if ((hl[i].host_toggle_str[HOST_ONE] != (*old_hl)[host_pos].host_toggle_str[HOST_ONE]) ||
                         (hl[i].host_toggle_str[HOST_TWO] != (*old_hl)[host_pos].host_toggle_str[HOST_TWO]))
                     {
                        fsa[host_pos].host_toggle_str[HOST_ONE] = hl[i].host_toggle_str[HOST_ONE];
                        fsa[host_pos].host_toggle_str[HOST_TWO] = hl[i].host_toggle_str[HOST_TWO];
                        fsa[host_pos].host_dsp_name[(int)fsa[host_pos].toggle_pos] = fsa[host_pos].host_toggle_str[(int)fsa[host_pos].host_toggle];
                     }
                     if (hl[i].host_toggle_str[0] == AUTO_TOGGLE_OPEN)
                     {
                        fsa[host_pos].auto_toggle = ON;
                     }
                     else
                     {
                        fsa[host_pos].auto_toggle = OFF;
                     }
                  }
               }
               (void)memcpy(fsa[host_pos].proxy_name, hl[i].proxy_name,
                            MAX_PROXY_NAME_LENGTH + 1);
               fsa[host_pos].allowed_transfers = hl[i].allowed_transfers;
               if ((*old_hl)[host_pos].allowed_transfers != hl[i].allowed_transfers)
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
               fsa[host_pos].transfer_rate_limit = hl[i].transfer_rate_limit;
               fsa[host_pos].transfer_timeout = hl[i].transfer_timeout;
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
               if (hl[i].in_dir_config == YES)
               {
                  fsa[host_pos].special_flag |= HOST_IN_DIR_CONFIG;
                  hl[i].host_status &= ~HOST_NOT_IN_DIR_CONFIG;
               }
               else
               {
                  fsa[host_pos].special_flag &= ~HOST_IN_DIR_CONFIG;
                  hl[i].host_status |= HOST_NOT_IN_DIR_CONFIG;
               }
               if (hl[i].host_status & HOST_CONFIG_HOST_DISABLED)
               {
                  fsa[host_pos].special_flag |= HOST_DISABLED;
               }
               else
               {
                  fsa[host_pos].special_flag &= ~HOST_DISABLED;
               }
               if (hl[i].protocol_options & KEEP_CON_NO_FETCH_2)
               {
                  fsa[host_pos].special_flag |= KEEP_CON_NO_FETCH;
               }
               else
               {
                  fsa[host_pos].special_flag &= ~KEEP_CON_NO_FETCH;
               }
               if (hl[i].protocol_options & KEEP_CON_NO_SEND_2)
               {
                  fsa[host_pos].special_flag |= KEEP_CON_NO_SEND;
               }
               else
               {
                  fsa[host_pos].special_flag &= ~KEEP_CON_NO_SEND;
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
         else /* host_pos == INCORRECT */
         {
            /*
             * Since we cannot find this host in the old host list
             * this must be a brand new host. The current FSA is to
             * small to add the new host here so we have to do it
             * in the function change_alias_order().
             */
            host_order_changed = YES;
         }
      } /* for (i = 0; i < new_no_of_hosts; i++) */

#ifdef CHECK_HOST_REMOVED_BUT_STILL_IN_DIR_CONFIG
      if (no_of_hosts_put_back > 0)
      {
         *hc_old_time = write_host_config(new_no_of_hosts, host_config_file, hl);
         if (no_of_hosts_put_back > 1)
         {
            update_db_log(WARN_SIGN, NULL, 0, debug_fp, warn_counter,
                          "%d hosts (%s) had to be put back to HOST_CONFIG because they are still in DIR_CONFIG",
                          no_of_hosts_put_back, host_list_put_back);
         }
         else
         {
            update_db_log(WARN_SIGN, NULL, 0, debug_fp, warn_counter,
                          "Host (%s) had to be put back to HOST_CONFIG because it is still in DIR_CONFIG",
                          host_list_put_back);
         }
         free(host_list_put_back);
      }
#endif

      if (host_order_changed != YES)
      {
         if (new_no_of_hosts != no_of_hosts)
         {
            host_order_changed = YES;
         }
         else
         {
            /*
             * Before freeing the mark_list, check if all hosts are marked.
             * If not a host has been removed from the FSA. Mark host order
             * as changed so the function change_alias_order() can remove
             * the host.
             */
            for (j = 0; j < *old_no_of_hosts; j++)
            {
               if (mark_list[j] == NO)
               {
                  host_order_changed = YES;
                  break;
               }
            }
         }
      }
      free(mark_list);

      if (no_of_host_changed > 0)
      {
         update_db_log(INFO_SIGN, NULL, 0, debug_fp, warn_counter,
                       "%d host changed in HOST_CONFIG.", no_of_host_changed);
         ret = HOST_CONFIG_DATA_CHANGED;
      }

      /*
       * Now lets see if the host order has changed.
       */
      if (host_order_changed == YES)
      {
         size_t new_size;
         char   dummy_name[1],
                **p_host_names = NULL;

         if (new_no_of_hosts > no_of_hosts)
         {
            new_size = new_no_of_hosts * sizeof(char *);
         }
         else
         {
            new_size = no_of_hosts * sizeof(char *);
            dummy_name[0] = '\0';
         }

         if ((p_host_names = malloc(new_size)) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "malloc() error [%d bytes] : %s",
                       new_size, strerror(errno));
            exit(INCORRECT);
         }
         for (i = 0; i < new_no_of_hosts; i++)
         {
            p_host_names[i] = hl[i].host_alias;
         }
         for (i = new_no_of_hosts; i < no_of_hosts; i++)
         {
            p_host_names[i] = dummy_name;
         }

         if ((dc_pid > 0) && (dir_check_stopped == NO))
         {
            int options;

            if (com(STOP, __FILE__, __LINE__) == INCORRECT)
            {
               options = WNOHANG;
            }
            else
            {
               options = 0;
            }
            (void)amg_zombie_check(&dc_pid, options);
            dc_pid = NOT_RUNNING;
            if (pid_list != NULL)
            {
               *(pid_t *)(pid_list + ((DC_NO + 1) * sizeof(pid_t))) = 0;
            }
            dir_check_stopped = YES;
         }
         update_db_log(INFO_SIGN, NULL, 0, debug_fp, warn_counter,
                       "Changing host alias order.");
         if (ret == HOST_CONFIG_DATA_CHANGED)
         {
            ret = HOST_CONFIG_DATA_ORDER_CHANGED;
         }
         else
         {
            ret = HOST_CONFIG_ORDER_CHANGED;
         }
         if (inform_fd == YES)
         {
            p_afd_status->amg_jobs |= REREADING_DIR_CONFIG;
            inform_fd_about_fsa_change();
         }
         change_alias_order(p_host_names, new_no_of_hosts);
         if (inform_fd == YES)
         {
            p_afd_status->amg_jobs &= ~REREADING_DIR_CONFIG;
         }
         free(p_host_names);
      }

      (void)fsa_detach(YES);

      if (old_hl != NULL)
      {
         free(*old_hl);
      }
   }
   else
   {
      update_db_log(INFO_SIGN, NULL, 0, debug_fp, warn_counter,
                    "There is no change in the HOST_CONFIG file.");
   }

   return(ret);
}
