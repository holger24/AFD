/*
 *  init_gf_burst2.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2014 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_gf_burst2 - initialises all variables for gf_xxx for a burst
 **
 ** SYNOPSIS
 **   void init_gf_burst2(struct job   *p_new_db,
 **                       unsigned int *values_changed)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   If successfull it will return the number of files to be send. It
 **   will exit with INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.07.2014 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                    /* strcpy(), strcat(), strerror() */
#include <stdlib.h>                    /* malloc()                       */
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern int                        fsa_fd;
extern long                       transfer_timeout;
extern char                       *p_work_dir;
extern struct fileretrieve_status *fra;
extern struct filetransfer_status *fsa;
extern struct job                 db;


/*########################## init_gf_burst2() ###########################*/
void
init_gf_burst2(struct job   *p_new_db,
               unsigned int *values_changed)
{
#ifdef _WITH_BURST_2
   /* Initialise variables. */
   if (p_new_db != NULL)
   {
      unsigned char old_no_of_time_entries;

      db.port         = p_new_db->port;
      db.chmod        = p_new_db->chmod;
      db.dir_mode     = p_new_db->dir_mode;
      db.chmod_str[0] = p_new_db->chmod_str[0];
      if (db.chmod_str[0] != '\0')
      {
         db.chmod_str[1] = p_new_db->chmod_str[1];
         db.chmod_str[2] = p_new_db->chmod_str[2];
         db.chmod_str[3] = p_new_db->chmod_str[3];
         db.chmod_str[4] = p_new_db->chmod_str[4];
      }
      db.dir_mode_str[0] = p_new_db->dir_mode_str[0];
      if (db.dir_mode_str[0] != '\0')
      {
         db.dir_mode_str[1] = p_new_db->dir_mode_str[1];
         db.dir_mode_str[2] = p_new_db->dir_mode_str[2];
         db.dir_mode_str[3] = p_new_db->dir_mode_str[3];
         db.dir_mode_str[4] = p_new_db->dir_mode_str[4];
      }
      if (values_changed != NULL)
      {
         *values_changed = 0;
         if (CHECK_STRCMP(db.active_user, p_new_db->user) != 0)
         {
            *values_changed |= USER_CHANGED;
            free(db.user_home_dir);
            db.user_home_dir = NULL;
         }
         if (CHECK_STRCMP(db.active_target_dir, p_new_db->target_dir) != 0)
         {
            *values_changed |= TARGET_DIR_CHANGED;
         }
         if (db.active_transfer_mode != p_new_db->transfer_mode)
         {
            *values_changed |= TYPE_CHANGED;
         }
# ifdef WITH_SSL
         if (db.active_auth != p_new_db->tls_auth)
         {
            *values_changed |= AUTH_CHANGED;
         }
# endif
      }
      (void)strcpy(db.user, p_new_db->user);
      (void)strcpy(db.target_dir, p_new_db->target_dir);
      if (((db.transfer_mode == 'A') || (db.transfer_mode == 'D')) &&
          (p_new_db->transfer_mode == 'N'))
      {
         db.transfer_mode = 'I';
      }
      else
      {
         db.transfer_mode = p_new_db->transfer_mode;
      }
# ifdef WITH_SSL
      db.tls_auth = p_new_db->tls_auth;
# endif
      (void)strcpy(db.password, p_new_db->password);
      if (p_new_db->http_proxy[0] == '\0')
      {
         db.http_proxy[0] = '\0';
      }
      else
      {
         (void)strcpy(db.http_proxy, p_new_db->http_proxy);
      }
      if (db.special_ptr != NULL)
      {
         free(db.special_ptr);
      }
      db.special_ptr       = p_new_db->special_ptr;
      db.special_flag      = p_new_db->special_flag;
      db.mode_flag         = p_new_db->mode_flag;
      old_no_of_time_entries = db.no_of_time_entries;
      db.no_of_time_entries = fra->no_of_time_entries;
      if (old_no_of_time_entries == 0)
      {
         if (db.no_of_time_entries != 0)
         {
            if (db.te_malloc == YES)
            {
               free(db.te);
            }
            db.te_malloc = NO;
            db.te = &fra->te[0];
            (void)strcpy(db.timezone, fra->timezone);
         }
      }
      else
      {
         if (db.no_of_time_entries == 0)
         {
            if ((db.te = malloc(sizeof(struct bd_time_entry))) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not malloc() memory : %s", strerror(errno));
               db.te_malloc = NO;
            }
            else
            {
               db.te_malloc = YES;
               if (eval_time_str("* * * * *", db.te, NULL) != SUCCESS)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to evaluate time string [* * * * *].");
               }
            }
            db.timezone[0] = '\0';
         }
         else
         {
            if (db.te_malloc == YES)
            {
               free(db.te);
            }
            db.te_malloc = NO;
            db.te = &fra->te[0];
            (void)strcpy(db.timezone, fra->timezone);
         }
      }
      if (db.index_file != NULL)
      {
         free(db.index_file);
      }
      db.index_file = p_new_db->index_file;

      free(p_new_db);
      p_new_db = NULL;
   }

   /* Do we want to display the status? */
   if (gsf_check_fsa((struct job *)&db) != NEITHER)
   {
# ifdef LOCK_DEBUG
      rlock_region(fsa_fd, db.lock_offset, __FILE__, __LINE__);
# else
      rlock_region(fsa_fd, db.lock_offset);
# endif
      if (db.protocol & FTP_FLAG)
      {
         fsa->job_status[(int)db.job_no].connect_status = FTP_BURST2_TRANSFER_ACTIVE;
      }
      else if (db.protocol & SFTP_FLAG)
           {
              fsa->job_status[(int)db.job_no].connect_status = SFTP_BURST_TRANSFER_ACTIVE;
           }
      else if (db.protocol & SCP_FLAG)
           {
              fsa->job_status[(int)db.job_no].connect_status = SCP_BURST_TRANSFER_ACTIVE;
           }
      fsa->job_status[(int)db.job_no].no_of_files = 0;
      fsa->job_status[(int)db.job_no].file_size = 0;
      fsa->job_status[(int)db.job_no].job_id = db.id.dir;
# ifdef LOCK_DEBUG
      unlock_region(fsa_fd, db.lock_offset, __FILE__, __LINE__);
# else
      unlock_region(fsa_fd, db.lock_offset);
# endif

      /* Set the timeout value. */
      transfer_timeout = fsa->transfer_timeout;
   }

   (void)strcpy(db.active_user, db.user);
   (void)strcpy(db.active_target_dir, db.target_dir);
   db.active_transfer_mode = db.transfer_mode;
# ifdef WITH_SSL
   db.active_auth = db.tls_auth;
# endif
#endif /* _WITH_BURST_2 */

   return;
}
