/*
 *  handle_error_queue.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2006 - 2015 Deutscher Wetterdienst (DWD),
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
 **   handle_error_queue - set of functions to handle the AFD error queue
 **
 ** SYNOPSIS
 **   int  attach_error_queue(void)
 **   int  detach_error_queue(void)
 **   void add_to_error_queue(unsigned int               job_id,
 **                           struct filetransfer_status *fsa,
 **                           int                        fsa_pos,
 **                           int                        fsa_fd,
 **                           int                        error_id,
 **                           time_t                     next_retry_time)
 **   int  check_error_queue(unsigned int job_id,
 **                          int          queue_threshold,
 **                          time_t       now,
 **                          int          retry_interval)
 **   int  host_check_error_queue(unsigned int host_id,
 **                               time_t       now,
 **                               int          retry_interval)
 **   int  remove_from_error_queue(unsigned int               job_id,
 **                                struct filetransfer_status *fsa,
 **                                int                        fsa_pos,
 **                                int                        fsa_fd)
 **   int  update_time_error_queue(unsigned int job_id, time_t next_retry_time)
 **   void validate_error_queue(int                        no_of_ids,
 **                             unsigned int               *cml,
 **                             int                        no_of_hosts,
 **                             struct filetransfer_status *fsa,
 **                             int                        fsa_fd)
 **   int  print_error_queue(FILE *fp)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.11.2006 H.Kiehl Created
 **   04.09.2007 H.Kiehl Added function validate_error_queue().
 **   24.06.2009 H.Kiehl Added next_retry_time to structure.
 **   05.08.2009 H.Kiehl Added function host_check_error_queue().
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif
#include <errno.h>

/* External global definitions. */
extern char               *p_work_dir;

/* Local definitions. */
#define RETRY_IN_USE                1
#define CURRENT_ERROR_QUEUE_VERSION 1
#define ERROR_QUE_BUF_SIZE          2
struct error_queue
       {
          time_t       next_retry_time;
          unsigned int job_id;
          unsigned int no_to_be_queued; /* Number to be queued. */
          unsigned int host_id;
          unsigned int special_flag;
       };

/* Local variables. */
static int                eq_fd = -1,
                          *no_of_error_ids;
static size_t             eq_size;
static struct error_queue *eq = NULL;

/* Local function prototypes. */
static void               check_error_queue_space(void);


/*####################### attach_error_queue() ##########################*/
int
attach_error_queue(void)
{
   if (eq_fd == -1)
   {
      char fullname[MAX_PATH_LENGTH],
           *ptr;

      eq_size = (ERROR_QUE_BUF_SIZE * sizeof(struct error_queue)) + AFD_WORD_OFFSET;
      (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s",
                     p_work_dir, FIFO_DIR, ERROR_QUEUE_FILE);
      if ((ptr = attach_buf(fullname, &eq_fd, &eq_size, NULL,
                            FILE_MODE, NO)) == (caddr_t) -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("Failed to mmap() `%s' : %s"), fullname, strerror(errno));
         if (eq_fd != -1)
         {
            (void)close(eq_fd);
            eq_fd = -1;
         }
         return(INCORRECT);
      }
      if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_ERROR_QUEUE_VERSION)
      {
         if ((ptr = convert_error_queue(eq_fd, fullname, &eq_size, ptr,
                                        *(ptr + SIZEOF_INT + 1 + 1 + 1),
                                        CURRENT_ERROR_QUEUE_VERSION)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to convert error queue file %s!",
                       fullname);
            (void)detach_error_queue();

            return(INCORRECT);
         }
      }
      no_of_error_ids = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      eq = (struct error_queue *)ptr;
   }

   return(SUCCESS);
}


/*####################### detach_error_queue() ##########################*/
int
detach_error_queue(void)
{
   if (eq_fd > 0)
   {
      if (close(eq_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("close() error : %s"), strerror(errno));
      }
      eq_fd = -1;
   }

   if (eq != NULL)
   {
      /* Detach from error queue. */
#ifdef HAVE_MMAP
      if (munmap(((char *)eq - AFD_WORD_OFFSET), eq_size) == -1)
#else
      if (munmap_emu((void *)((char *)eq - AFD_WORD_OFFSET)) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to munmap() from error queue : %s"),
                    strerror(errno));
         return(INCORRECT);
      }
      eq = NULL;
   }

   return(SUCCESS);
}


/*####################### add_to_error_queue() ##########################*/
void
add_to_error_queue(unsigned int               job_id,
                   struct filetransfer_status *fsa,
                   int                        fsa_pos,
                   int                        fsa_fd,
                   int                        error_id,
                   time_t                     next_retry_time)
{
   int detach,
       i;

   if (eq_fd == -1)
   {
      if (attach_error_queue() == SUCCESS)
      {
         detach = YES;
      }
      else
      {
         return;
      }
   }
   else
   {
      detach = NO;
   }


#ifdef LOCK_DEBUG
   lock_region_w(eq_fd, 1, __FILE__, __LINE__);
#else
   lock_region_w(eq_fd, 1);
#endif
   for (i = 0; i < *no_of_error_ids; i++)
   {
      if (eq[i].job_id == job_id)
      {
         eq[i].next_retry_time = next_retry_time;
         if ((fsa[fsa_pos].host_status & ERROR_QUEUE_SET) == 0)
         {
#ifdef LOCK_DEBUG
            lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (fsa_pos * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
            lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (fsa_pos * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
            fsa[fsa_pos].host_status |= ERROR_QUEUE_SET;
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, (AFD_WORD_OFFSET + (fsa_pos * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
            unlock_region(eq_fd, 1, __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, (AFD_WORD_OFFSET + (fsa_pos * sizeof(struct filetransfer_status)) + LOCK_HS));
            unlock_region(eq_fd, 1);
#endif
            event_log(0L, EC_HOST, ET_EXT, EA_START_ERROR_QUEUE, "%s%c%x%c%x",
                      fsa[fsa_pos].host_alias, SEPARATOR_CHAR, job_id,
                      SEPARATOR_CHAR, error_id);
         }
         else
         {
#ifdef LOCK_DEBUG
            unlock_region(eq_fd, 1, __FILE__, __LINE__);
#else
            unlock_region(eq_fd, 1);
#endif
         }
         if (detach == YES)
         {
            (void)detach_error_queue();
         }

         return;
      }
   }
   check_error_queue_space();
   eq[*no_of_error_ids].next_retry_time = next_retry_time;
   eq[*no_of_error_ids].job_id = job_id;
   eq[*no_of_error_ids].no_to_be_queued = 0;
   eq[*no_of_error_ids].host_id = fsa[fsa_pos].host_id;
   eq[*no_of_error_ids].special_flag = 0;
   (*no_of_error_ids)++;
#ifdef LOCK_DEBUG
   lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (fsa_pos * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
   lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (fsa_pos * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
   fsa[fsa_pos].host_status |= ERROR_QUEUE_SET;
#ifdef LOCK_DEBUG
   unlock_region(fsa_fd, (AFD_WORD_OFFSET + (fsa_pos * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
   unlock_region(eq_fd, 1, __FILE__, __LINE__);
#else
   unlock_region(fsa_fd, (AFD_WORD_OFFSET + (fsa_pos * sizeof(struct filetransfer_status)) + LOCK_HS));
   unlock_region(eq_fd, 1);
#endif
   event_log(0L, EC_HOST, ET_EXT, EA_START_ERROR_QUEUE, "%s%c%x%c%x",
             fsa[fsa_pos].host_alias, SEPARATOR_CHAR, job_id,
             SEPARATOR_CHAR, error_id);

   if (detach == YES)
   {
      (void)detach_error_queue();
   }

   return;
}


/*######################## check_error_queue() ##########################*/
/*                         -------------------                           */
/* NOTE: queue_threshold when set MUST always be larger then             */
/*       MAX_NO_PARALLEL_JOBS, otherwise it can happen when files are    */
/*       being deleted due to age limit or duplicate check that the flag */
/*       ERROR_QUEUE_SET never gets unset!                               */
/*#######################################################################*/
int
check_error_queue(unsigned int job_id,
                  int          queue_threshold,
                  time_t       now,
                  int          retry_interval)
{
   int detach,
       i,
       ret = NO;

   if (eq_fd == -1)
   {
      if (attach_error_queue() == SUCCESS)
      {
         detach = YES;
      }
      else
      {
         return(INCORRECT);
      }
   }
   else
   {
      detach = NO;
   }

#ifdef LOCK_DEBUG
   lock_region_w(eq_fd, 1, __FILE__, __LINE__);
#else
   lock_region_w(eq_fd, 1);
#endif
   for (i = 0; i < *no_of_error_ids; i++)
   {
      if (eq[i].job_id == job_id)
      {
         if ((queue_threshold == -1) ||
             (eq[i].no_to_be_queued >= queue_threshold))
         {
            if (now == 0)
            {
               ret = YES;
            }
            else
            {
               if (now < eq[i].next_retry_time)
               {
                  ret = YES;
               }
               else
               {
                  if (eq[i].special_flag & RETRY_IN_USE)
                  {
                     if (now < (eq[i].next_retry_time + retry_interval))
                     {
                        ret = YES;
                     }
                  }
                  else
                  {
                     eq[i].special_flag |= RETRY_IN_USE;
                  }
               }
            }
         }
         else
         {
            eq[i].no_to_be_queued++;
         }
         break;
      }
   }
#ifdef LOCK_DEBUG
   unlock_region(eq_fd, 1, __FILE__, __LINE__);
#else
   unlock_region(eq_fd, 1);
#endif

   if (detach == YES)
   {
      (void)detach_error_queue();
   }
   return(ret);
}


/*###################### host_check_error_queue() #######################*/
int
host_check_error_queue(unsigned int host_id, time_t now, int retry_interval)
{
   int detach,
       entries_found = 0,
       i;

   if (eq_fd == -1)
   {
      if (attach_error_queue() == SUCCESS)
      {
         detach = YES;
      }
      else
      {
         return(INCORRECT);
      }
   }
   else
   {
      detach = NO;
   }

#ifdef LOCK_DEBUG
   lock_region_w(eq_fd, 1, __FILE__, __LINE__);
#else
   lock_region_w(eq_fd, 1);
#endif
   for (i = 0; i < *no_of_error_ids; i++)
   {
      if (eq[i].host_id == host_id)
      {
         if ((eq[i].next_retry_time + (4 * retry_interval)) < now)
         {
            /* Lets remove this entry from the error queue. */
            system_log(DEBUG_SIGN, NULL, 0,
                       _("Hmm, removed possible stuck job #%x from error queue."),
                       eq[i].job_id);
            if (i != (*no_of_error_ids - 1))
            {
               size_t move_size;

               move_size = (*no_of_error_ids - 1 - i) * sizeof(struct error_queue);
               (void)memmove(&eq[i], &eq[i + 1], move_size);
            }
            (*no_of_error_ids)--;
            i--;
         }
         else
         {
            entries_found++;
         }
      }
   }
#ifdef LOCK_DEBUG
   unlock_region(eq_fd, 1, __FILE__, __LINE__);
#else
   unlock_region(eq_fd, 1);
#endif

   if (detach == YES)
   {
      (void)detach_error_queue();
   }

   return(entries_found);
}


/*##################### remove_from_error_queue() #######################*/
int
remove_from_error_queue(unsigned int               job_id,
                        struct filetransfer_status *fsa,
                        int                        fsa_pos,
                        int                        fsa_fd)
{
   int detach,
       gotcha = NO,   /* Are there other ID's from the same host? */
       i;

   if (eq_fd == -1)
   {
      if (attach_error_queue() == SUCCESS)
      {
         detach = YES;
      }
      else
      {
         return(INCORRECT);
      }
   }
   else
   {
      detach = NO;
   }

#ifdef LOCK_DEBUG
   lock_region_w(eq_fd, 1, __FILE__, __LINE__);
#else
   lock_region_w(eq_fd, 1);
#endif
   for (i = 0; i < *no_of_error_ids; i++)
   {
      if (eq[i].job_id == job_id)
      {
         if (i != (*no_of_error_ids - 1))
         {
            size_t move_size;

            move_size = (*no_of_error_ids - 1 - i) * sizeof(struct error_queue);
            (void)memmove(&eq[i], &eq[i + 1], move_size);
         }
         (*no_of_error_ids)--;

         if (gotcha == NO)
         {
            int j;

            for (j = i; j < *no_of_error_ids; j++)
            {
               if (eq[j].host_id == fsa->host_id)
               {
                  gotcha = YES;
                  break;
               }
            }
            if ((gotcha == NO) && (fsa->host_status & ERROR_QUEUE_SET))
            {
#ifdef LOCK_DEBUG
               lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (fsa_pos * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
               lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (fsa_pos * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
               fsa->host_status &= ~ERROR_QUEUE_SET;
#ifdef LOCK_DEBUG
               unlock_region(fsa_fd, (AFD_WORD_OFFSET + (fsa_pos * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
               unlock_region(fsa_fd, (AFD_WORD_OFFSET + (fsa_pos * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
               event_log(0L, EC_HOST, ET_EXT, EA_STOP_ERROR_QUEUE, "%s%c%x",
                         fsa->host_alias, SEPARATOR_CHAR, job_id);
            }
#ifdef WITH_REPORT_RM_ERROR_JOBS
            else
            {
               system_log(DEBUG_SIGN, NULL, 0,
                          _("%s: Removed job #%x from error queue."),
                          fsa->host_dsp_name, job_id);
            }
#endif
         }

         check_error_queue_space();
#ifdef LOCK_DEBUG
         unlock_region(eq_fd, 1, __FILE__, __LINE__);
#else
         unlock_region(eq_fd, 1);
#endif

         if (detach == YES)
         {
            (void)detach_error_queue();
         }
         return(SUCCESS);
      }
      else
      {
         if (eq[i].host_id == fsa->host_id)
         {
            gotcha = YES;
         }
      }
   }
#ifdef LOCK_DEBUG
   unlock_region(eq_fd, 1, __FILE__, __LINE__);
#else
   unlock_region(eq_fd, 1);
#endif

   if (detach == YES)
   {
      (void)detach_error_queue();
   }
   return(INCORRECT);
}


/*####################### validate_error_queue() ########################*/
void
validate_error_queue(int                        no_of_ids,
                     unsigned int               *cml,
                     int                        no_of_hosts,
                     struct filetransfer_status *fsa,
                     int                        fsa_fd)
{
   int          detach,
                gotcha,
                i,
                j;
   unsigned int prev_host_id = 0;

   if (eq_fd == -1)
   {
      if (attach_error_queue() == SUCCESS)
      {
         detach = YES;
      }
      else
      {
         return;
      }
   }
   else
   {
      detach = NO;
   }

#ifdef LOCK_DEBUG
   lock_region_w(eq_fd, 1, __FILE__, __LINE__);
#else
   lock_region_w(eq_fd, 1);
#endif

   for (i = 0; i < *no_of_error_ids; i++)
   {
      gotcha = NO;
      for (j = 0; j < no_of_ids; j++)
      {
         if (eq[i].job_id == cml[j])
         {
            gotcha = YES;
            break;
         }
      }
      if (gotcha == NO)
      {
         /*
          * A job that is no longer in the current job list
          * must be removed.
          */
         if (prev_host_id != eq[i].host_id)
         {
            prev_host_id = 0;
            for (j = 0; j < no_of_hosts; j++)
            {
               if (fsa[j].host_id == eq[i].host_id)
               {
                  prev_host_id = eq[i].host_id;
                  break;
               }
            }
         }
         else
         {
            j = -1;
         }
         if ((prev_host_id == 0) || (j == -1))
         {
            system_log(DEBUG_SIGN, NULL, 0,
                       _("%u: Removed job #%x from error queue, since it was removed from DIR_CONFIG."),
                       eq[i].host_id, eq[i].job_id);
         }
         else
         {
            system_log(DEBUG_SIGN, NULL, 0,
                       _("%s: Removed job #%x from error queue, since it was removed from DIR_CONFIG."),
                       fsa[j].host_dsp_name, eq[i].job_id);

            /*
             * This is a good time to reset the error counter. The queue
             * could have stopped automatically, so this will enable the
             * queue again.
             */
            if (fsa[j].error_counter > 0)
            {
               int lock_offset;

               lock_offset = AFD_WORD_OFFSET +
                             (j * sizeof(struct filetransfer_status));
#ifdef LOCK_DEBUG
               lock_region_w(fsa_fd, lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
               lock_region_w(fsa_fd, lock_offset + LOCK_EC);
#endif
               fsa[j].error_counter = 0;
#ifdef LOCK_DEBUG
               unlock_region(fsa_fd, lock_offset + LOCK_EC, __FILE__, __LINE__);
#else
               unlock_region(fsa_fd, lock_offset + LOCK_EC);
#endif
            }
         }
         if (i != (*no_of_error_ids - 1))
         {
            size_t move_size;

            move_size = (*no_of_error_ids - 1 - i) * sizeof(struct error_queue);
            (void)memmove(&eq[i], &eq[i + 1], move_size);
         }
         i--;
         (*no_of_error_ids)--;
      }
      else
      {
         /*
          * Job is still in DIR_CONFIG. Lets see if ERROR_QUEUE_SET flag
          * is set for this host. If not lets remove this job from the
          * error queue.
          */
         for (j = 0; j < no_of_hosts; j++)
         {
            if (fsa[j].host_id == eq[i].host_id)
            {
               if ((fsa[j].host_status & ERROR_QUEUE_SET) == 0)
               {
                  system_log(DEBUG_SIGN, NULL, 0,
                             _("%s: Removed job #%x from error queue, since the error queue flag is not set."),
                             fsa[j].host_dsp_name, eq[i].job_id);
                  if (i != (*no_of_error_ids - 1))
                  {
                     size_t move_size;

                     move_size = (*no_of_error_ids - 1 - i) * sizeof(struct error_queue);
                     (void)memmove(&eq[i], &eq[i + 1], move_size);
                  }
                  i--;
                  (*no_of_error_ids)--;
               }
            }
         }
      }
   }
#ifdef LOCK_DEBUG
   unlock_region(eq_fd, 1, __FILE__, __LINE__);
#else
   unlock_region(eq_fd, 1);
#endif

   if (*no_of_error_ids == 0)
   {
      for (i = 0; i < no_of_hosts; i++)
      {
         if (fsa[i].host_status & ERROR_QUEUE_SET)
         {
#ifdef LOCK_DEBUG
            lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
            lock_region_w(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
            fsa[i].host_status &= ~ERROR_QUEUE_SET;
#ifdef LOCK_DEBUG
            unlock_region(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS), __FILE__, __LINE__);
#else
            unlock_region(fsa_fd, (AFD_WORD_OFFSET + (i * sizeof(struct filetransfer_status)) + LOCK_HS));
#endif
            event_log(0L, EC_HOST, ET_AUTO, EA_STOP_ERROR_QUEUE,
                      "%s%cCorrecting since error queue is empty.",
                      fsa[i].host_alias, SEPARATOR_CHAR);
         }
      }
   }

   if (detach == YES)
   {
      (void)detach_error_queue();
   }

   return;
}


/*###################### update_time_error_queue() ######################*/
int
update_time_error_queue(unsigned int job_id, time_t next_retry_time)
{
   int detach,
       i,
       ret = NEITHER;

   if (eq_fd == -1)
   {
      if (attach_error_queue() == SUCCESS)
      {
         detach = YES;
      }
      else
      {
         return(INCORRECT);
      }
   }
   else
   {
      detach = NO;
   }

#ifdef LOCK_DEBUG
   lock_region_w(eq_fd, 1, __FILE__, __LINE__);
#else
   lock_region_w(eq_fd, 1);
#endif
   for (i = 0; i < *no_of_error_ids; i++)
   {
      if (eq[i].job_id == job_id)
      {
         eq[i].next_retry_time = next_retry_time;
         eq[i].special_flag &= ~RETRY_IN_USE;
         ret = SUCCESS;
         break;
      }
   }
#ifdef LOCK_DEBUG
   unlock_region(eq_fd, 1, __FILE__, __LINE__);
#else
   unlock_region(eq_fd, 1);
#endif

   if (detach == YES)
   {
      (void)detach_error_queue();
   }
   return(ret);
}


/*######################## print_error_queue() ##########################*/
int
print_error_queue(FILE *fp)
{
   int detach,
       i;

   if (eq_fd == -1)
   {
      if (attach_error_queue() == SUCCESS)
      {
         detach = YES;
      }
      else
      {
         return(INCORRECT);
      }
   }
   else
   {
      detach = NO;
   }

   for (i = 0; i < *no_of_error_ids; i++)
   {
      (void)fprintf(fp, "%x %u %x %u %s",
                    eq[i].job_id, eq[i].no_to_be_queued, eq[i].host_id,
                    eq[i].special_flag, ctime(&eq[i].next_retry_time));
   }

   if (detach == YES)
   {
      (void)detach_error_queue();
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++ check_error_queue_space() ++++++++++++++++++++++*/
static void
check_error_queue_space(void)
{
   if ((*no_of_error_ids != 0) &&
       ((*no_of_error_ids % ERROR_QUE_BUF_SIZE) == 0))
   {
      char *ptr;

      eq_size = (((*no_of_error_ids / ERROR_QUE_BUF_SIZE) + 1) *
                ERROR_QUE_BUF_SIZE * sizeof(struct error_queue)) +
                AFD_WORD_OFFSET;
      ptr = (char *)eq - AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(eq_fd, ptr, eq_size)) == (caddr_t) -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    _("mmap() error : %s"), strerror(errno));
         exit(INCORRECT);
      }
      no_of_error_ids = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      eq = (struct error_queue *)ptr;
   }
   return;
}
