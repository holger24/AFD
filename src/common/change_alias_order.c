/*
 *  change_alias_order.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2025 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   change_alias_order - changes the order of hostnames in the FSA
 **
 ** SYNOPSIS
 **   void change_alias_order(char **p_host_names)
 **
 ** DESCRIPTION
 **   This function creates a new FSA (Filetransfer Status Area) with
 **   the hosnames ordered as they are found in p_host_names.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.08.1997 H.Kiehl Created
 **   08.09.2000 H.Kiehl Update FRA as well.
 **   03.08.2001 H.Kiehl Remember if we stopped the queue or transfer
 **                      and some protocol specific information.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>       /* strcpy(), strcat(), strerror()              */
#include <stdlib.h>       /* exit()                                      */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>    /* mmap()                                      */
#endif
#include <unistd.h>       /* read(), write(), close(), lseek(), unlink() */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                        fsa_id,
                                  fsa_fd,
                                  no_of_dirs,
                                  no_of_hosts;
#ifdef HAVE_MMAP
extern off_t                      fsa_size;
#endif
extern char                       *p_work_dir;
extern struct host_list           *hl;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;


/*######################### change_alias_order() ########################*/
void
change_alias_order(char **p_host_names, int new_no_of_hosts)
{
   int                        i,
                              ignore_first_errors,
                              fd,
                              position,
                              pagesize,
                              old_no_of_hosts = no_of_hosts,
                              loop_no_of_hosts,
                              current_fsa_id,
                              new_fsa_fd;
   off_t                      new_fsa_size;
   char                       *ptr,
                              fsa_id_file[MAX_PATH_LENGTH],
                              new_fsa_stat[MAX_PATH_LENGTH];
   struct flock               wlock;
   struct filetransfer_status *new_fsa;

   if (new_no_of_hosts != -1)
   {
      if (no_of_hosts > new_no_of_hosts)
      {
         loop_no_of_hosts = no_of_hosts;
      }
      else
      {
         loop_no_of_hosts = new_no_of_hosts;
      }
      no_of_hosts = new_no_of_hosts;
   }
   else
   {
      loop_no_of_hosts = no_of_hosts;
   }

   (void)strcpy(fsa_id_file, p_work_dir);
   (void)strcat(fsa_id_file, FIFO_DIR);
   (void)strcat(fsa_id_file, FSA_ID_FILE);

   wlock.l_type   = F_WRLCK;
   wlock.l_whence = SEEK_SET;
   wlock.l_start  = 0;
#ifdef HAVE_MMAP
   wlock.l_len    = fsa_size;
#else
   wlock.l_len    = AFD_WORD_OFFSET + (no_of_hosts * sizeof(struct filetransfer_status));
#endif /* HAVE_MMAP */
   if (fcntl(fsa_fd, F_SETLKW, &wlock) < 0)
   {
      /* Is lock already set or are we setting it again? */
      if ((errno != EACCES) && (errno != EAGAIN) && (errno != EBUSY))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not set write lock for FSA_STAT_FILE : %s"),
                    strerror(errno));
      }
      else
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("Could not set write lock for FSA_STAT_FILE : %s"),
                    strerror(errno));
      }
   }

   /*
    * When changing the order of the hosts, lock the FSA_ID_FILE so no
    * one gets the idea to do the same thing or change the DIR_CONFIG
    * file.
    */
   if ((fd = lock_file(fsa_id_file, ON)) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed to lock `%s' [%d]"), fsa_id_file, fd);
      exit(INCORRECT);
   }

   /* Read the fsa_id. */
   if (read(fd, &current_fsa_id, sizeof(int)) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not read the value of the fsa_id : %s"),
                 strerror(errno));
      (void)close(fd);
      exit(INCORRECT);
   }

   if (current_fsa_id != fsa_id)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 _("AAAaaaarrrrghhhh!!! DON'T CHANGE THE DIR_CONFIG FILE WHILE USING edit_hc!!!!"));
      (void)close(fd);
      exit(INCORRECT);
   }
   current_fsa_id++;

   /* Mark old FSA as stale. */
   *(int *)((char *)fsa - AFD_WORD_OFFSET) = STALE;
   pagesize = *(int *)((char *)fsa - AFD_WORD_OFFSET + SIZEOF_INT + 4);
   ignore_first_errors = *(unsigned char *)((char *)fsa - AFD_WORD_OFFSET + SIZEOF_INT + 1 + 1);

   /*
    * Create a new FSA with the new ordering of the host aliases.
    */
   (void)snprintf(new_fsa_stat, MAX_PATH_LENGTH, "%s%s%s.%d",
                  p_work_dir, FIFO_DIR, FSA_STAT_FILE, current_fsa_id);

   /* Now map the new FSA region to a file. */
   if ((new_fsa_fd = coe_open(new_fsa_stat,
                              (O_RDWR | O_CREAT | O_TRUNC), FILE_MODE)) < 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Failed to open() `%s' : %s"),
                 new_fsa_stat, strerror(errno));
      exit(INCORRECT);
   }
   new_fsa_size = AFD_WORD_OFFSET +
                  (no_of_hosts * sizeof(struct filetransfer_status));
   if (lseek(new_fsa_fd, new_fsa_size - 1, SEEK_SET) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Failed to lseek() in `%s' : %s"),
                 new_fsa_stat, strerror(errno));
      exit(INCORRECT);
   }
   if (write(new_fsa_fd, "", 1) != 1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("write() error : %s"), strerror(errno));
      exit(INCORRECT);
   }
#ifdef HAVE_MMAP
   if ((ptr = mmap(NULL, new_fsa_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                   new_fsa_fd, 0)) == (caddr_t) -1)
#else
   if ((ptr = mmap_emu(NULL, new_fsa_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                       new_fsa_stat, 0)) == (caddr_t) -1)
#endif
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("mmap() error : %s"), strerror(errno));
      exit(INCORRECT);
   }

   /* Write number of hosts to new mmap region. */
   *(int *)ptr = no_of_hosts;
   *(ptr + SIZEOF_INT + 1 + 1) = ignore_first_errors;
   *(ptr + SIZEOF_INT + 1 + 1 + 1) = CURRENT_FSA_VERSION; /* FSA version number. */
   *(int *)(ptr + SIZEOF_INT + 4) = pagesize;
   *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;        /* Not used. */
   *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;    /* Not used. */
   *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;    /* Not used. */
   *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;    /* Not used. */

   /* Copy configuration information from the old FSA. */
   ptr += AFD_WORD_OFFSET;
   *((char *)ptr - AFD_FEATURE_FLAG_OFFSET_END) = *((char *)fsa - AFD_FEATURE_FLAG_OFFSET_END);

   /* Reposition fsa pointer after no_of_host. */
   new_fsa = (struct filetransfer_status *)ptr;

   if (fra_attach() != SUCCESS)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, _("Failed to attach to FRA."));
      exit(INCORRECT);
   }

   /*
    * Now copy each entry from the old FSA to the new FSA in
    * the order they are found in the host_list_w.
    */
   for (i = 0; i < loop_no_of_hosts; i++)
   {
      if (p_host_names[i][0] != '\0')
      {
         if ((position = get_host_position(fsa, p_host_names[i],
                                           old_no_of_hosts)) < 0)
         {
            if (hl != NULL)
            {
               int k;

               /*
                * Hmmm. This host is not in the FSA. So lets assume this
                * is a new host.
                */
               (void)memset(&new_fsa[i], 0, sizeof(struct filetransfer_status));
               (void)memcpy(new_fsa[i].host_alias, hl[i].host_alias, MAX_HOSTNAME_LENGTH + 1);
               new_fsa[i].host_id = get_str_checksum(new_fsa[i].host_alias);
               (void)snprintf(new_fsa[i].host_dsp_name, MAX_HOSTNAME_LENGTH + 2,
                              "%-*s", MAX_HOSTNAME_LENGTH, hl[i].host_alias);
               new_fsa[i].toggle_pos = strlen(new_fsa[i].host_alias);
               (void)memcpy(new_fsa[i].real_hostname[0], hl[i].real_hostname[0], MAX_REAL_HOSTNAME_LENGTH);
               (void)memcpy(new_fsa[i].real_hostname[1], hl[i].real_hostname[1], MAX_REAL_HOSTNAME_LENGTH);
               new_fsa[i].host_toggle = HOST_ONE;
               if (hl[i].host_toggle_str[0] != '\0')
               {
                  (void)memcpy(new_fsa[i].host_toggle_str, hl[i].host_toggle_str, MAX_TOGGLE_STR_LENGTH);
                  if (hl[i].host_toggle_str[0] == AUTO_TOGGLE_OPEN)
                  {
                     new_fsa[i].auto_toggle = ON;
                  }
                  else
                  {
                     new_fsa[i].auto_toggle = OFF;
                  }
                  new_fsa[i].original_toggle_pos = DEFAULT_TOGGLE_HOST;
                  new_fsa[i].host_dsp_name[(int)new_fsa[i].toggle_pos] = hl[i].host_toggle_str[(int)new_fsa[i].original_toggle_pos];
               }
               else
               {
                  new_fsa[i].host_toggle_str[0] = '\0';
                  new_fsa[i].original_toggle_pos = NONE;
                  new_fsa[i].auto_toggle = OFF;
               }
               (void)memcpy(new_fsa[i].proxy_name, hl[i].proxy_name, MAX_PROXY_NAME_LENGTH + 1);
               new_fsa[i].transfer_rate_limit = hl[i].transfer_rate_limit;
               new_fsa[i].allowed_transfers = hl[i].allowed_transfers;
               for (k = 0; k < new_fsa[i].allowed_transfers; k++)
               {
                  new_fsa[i].job_status[k].connect_status = DISCONNECT;
                  new_fsa[i].job_status[k].proc_id = -1;
#ifdef _WITH_BURST_2
                  new_fsa[i].job_status[k].job_id = NO_ID;
#endif
               }
               for (k = new_fsa[i].allowed_transfers; k < MAX_NO_PARALLEL_JOBS; k++)
               {
                  new_fsa[i].job_status[k].no_of_files = -1;
                  new_fsa[i].job_status[k].proc_id = -1;
               }
               new_fsa[i].max_errors = hl[i].max_errors;
               new_fsa[i].retry_interval = hl[i].retry_interval;
               new_fsa[i].block_size = hl[i].transfer_blksize;
               new_fsa[i].max_successful_retries = hl[i].successful_retries;
               new_fsa[i].file_size_offset = hl[i].file_size_offset;
               new_fsa[i].transfer_timeout = hl[i].transfer_timeout;
               new_fsa[i].protocol = hl[i].protocol;
               new_fsa[i].protocol_options = hl[i].protocol_options;
               new_fsa[i].ttl = hl[i].ttl;
               new_fsa[i].special_flag = 0;
               if (hl[i].host_status & HOST_CONFIG_HOST_DISABLED)
               {
                  new_fsa[i].special_flag |= HOST_DISABLED;
               }
               new_fsa[i].host_status = 0;
               if (hl[i].host_status & STOP_TRANSFER_STAT)
               {
                  new_fsa[i].host_status |= STOP_TRANSFER_STAT;
               }
               if (hl[i].host_status & PAUSE_QUEUE_STAT)
               {
                  new_fsa[i].host_status |= PAUSE_QUEUE_STAT;
               }
            }
            else
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("AAAaaaarrrrghhhh!!! Could not find hostname `%s'"),
                          p_host_names[i]);
               (void)close(fd);
               exit(INCORRECT);
            }
         }
         else
         {
            if ((position != INCORRECT) && (position != i))
            {
               int k;

               for (k = 0; k < no_of_dirs; k++)
               {
#ifdef NEW_FRA
                  if ((fra[k].host_alias[0] != '\0') &&
                      (fra[k].host_id == fsa[position].host_id))
#else
                  if ((fra[k].host_alias[0] != '\0') &&
                      (CHECK_STRCMP(fra[k].host_alias, fsa[position].host_alias) == 0))
#endif
                  {
                     fra[k].fsa_pos = i;
                  }
               }
            }
            (void)memcpy(&new_fsa[i], &fsa[position],
                         sizeof(struct filetransfer_status));
         }
      }
   }
#ifdef HAVE_MMAP
   if (msync(ptr - AFD_WORD_OFFSET, new_fsa_size, MS_SYNC) == -1)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "msync() error : %s", strerror(errno));
   }
#endif

   if (fra_detach() < 0)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 _("Failed to detach from FRA."));
   }

   if (fsa_detach(NO) < 0)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 _("Failed to detach from old FSA."));
   }

   /* Now "attach" to the new FSA. */
   fsa = new_fsa;
   fsa_fd = new_fsa_fd;
   fsa_id = current_fsa_id;
#ifdef HAVE_MMAP
   fsa_size = new_fsa_size;
#endif

   /* Go to beginning in file. */
   if (lseek(fd, 0, SEEK_SET) < 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not seek() to beginning of `%s' : %s"),
                 fsa_id_file, strerror(errno));
   }

   /* Write new value into FSA_ID_FILE file. */
   if (write(fd, &fsa_id, sizeof(int)) != sizeof(int))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Could not write value to FSA ID file : %s"),
                 strerror(errno));
      exit(INCORRECT);
   }

   /* Release the lock. */
   if (close(fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 _("close() error : %s"), strerror(errno));
   }

   /* Remove the old FSA file. */
   (void)snprintf(new_fsa_stat, MAX_PATH_LENGTH, "%s%s%s.%d",
                  p_work_dir, FIFO_DIR, FSA_STAT_FILE, current_fsa_id - 1);
   if (unlink(new_fsa_stat) < 0)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 _("unlink() error : %s"), strerror(errno));
   }

   return;
}
