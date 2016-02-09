/*
 *  lookup_job_id.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   lookup_job_id - searches for a job ID
 **
 ** SYNOPSIS
 **   void lookup_job_id(struct instant_db *p_db, int *jid_number)
 **
 ** DESCRIPTION
 **   The function lookup_job_id() searches for the job ID of the
 **   job described in 'p_db'. If it is found, the job_id is
 **   initialised with the position in the job_id_data structure.
 **   If the job ID is not found, it gets appended to the current
 **   structure and a new message is created in the message
 **   directory.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.01.1998 H.Kiehl Created
 **   24.08.2003 H.Kiehl Use CRC-32 checksum to create job ID's.
 **   26.08.2003 H.Kiehl Ensure that the options are not to long.
 **   05.01.2004 H.Kiehl Store DIR_CONFIG ID.
 **   17.10.2004 H.Kiehl Create outgoing job directory.
 **   26.06.2008 H.Kiehl Added recipient + host ID.
 **   26.10.2009 H.Kiehl Use host ID to calculate CRC checksum.
 **   17.10.2012 H.Kiehl Make loptions MAX_NO_OPTIONS * MAX_OPTION_LENGTH
 **                      long.
 **
 */
DESCR__E_M3

#include <stdio.h>           /* sprintf()                                */
#include <string.h>          /* strcmp(), strcpy(), strlen(), strerror() */
#include <stdlib.h>          /* realloc()                                */
#include <sys/types.h>
#include <utime.h>           /* utime()                                  */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                 jd_fd,
                           *no_of_dir_names,
                           *no_of_job_ids;
extern char                msg_dir[],
#ifdef WITH_GOTCHA_LIST
                           *gotcha,
#endif
                           outgoing_file_dir[],
                           *p_msg_dir;
extern struct file_mask    *fml;
extern struct job_id_data  *jd;
extern struct dir_name_buf *dnb;

#define PRINT_DUP_JOB


/*########################### lookup_job_id() ###########################*/
void
lookup_job_id(struct instant_db *p_db, unsigned int *jid_number)
{
   register int i;
   int          buf_size,
                offset;
   size_t       length;
   char         *buffer,
                *p_buf_crc,
                *p_outgoing_file_dir;

   for (i = 0; i < *no_of_job_ids; i++)
   {
#ifdef WITH_GOTCHA_LIST
      if ((gotcha[i] == NO) &&
          (jd[i].dir_config_id == p_db->dir_config_id) &&
#else
      if ((jd[i].dir_config_id == p_db->dir_config_id) &&
#endif
          (jd[i].dir_id == p_db->dir_id) &&
          (jd[i].priority == p_db->priority) &&
          (jd[i].file_mask_id == p_db->file_mask_id) &&
          (jd[i].no_of_loptions == p_db->no_of_loptions) &&
          (jd[i].no_of_soptions == p_db->no_of_soptions) &&
          (jd[i].host_id == p_db->host_id) && /* can differ via aias.list! */
          (jd[i].recipient_id == p_db->recipient_id))
      {
         /*
          * NOTE: Since all standart options are stored in a character
          *       array separated by a newline, it is NOT necessary
          *       to check each element.
          */
         if (jd[i].no_of_soptions > 0)
         {
            if (CHECK_STRCMP(jd[i].soptions, p_db->soptions) != 0)
            {
               continue;
            }
         }

         /*
          * NOTE: Local options are stored in an array separated by a
          *       binary zero. Thus we have to go through the list
          *       and check each element.
          */
         if (jd[i].no_of_loptions > 0)
         {
            register int gotcha_local = NO,
                         j;
            char         *p_loptions_db = p_db->loptions,
                         *p_loptions_jd = jd[i].loptions;

            for (j = 0; j < jd[i].no_of_loptions; j++)
            {
               if (CHECK_STRCMP(p_loptions_jd, p_loptions_db) != 0)
               {
                  gotcha_local = YES;
                  break;
               }
               NEXT(p_loptions_db);
               NEXT(p_loptions_jd);
            }
            if (gotcha_local == YES)
            {
               continue;
            }
         }

#ifdef WITH_GOTCHA_LIST
         gotcha[i] = YES;
#endif
         p_db->job_id = jd[i].job_id;
         (void)snprintf(p_db->str_job_id, MAX_INT_HEX_LENGTH, "%x", jd[i].job_id);

         /* Touch the message file so FD knows this is a new file. */
         (void)snprintf(p_msg_dir, MAX_PATH_LENGTH - (p_msg_dir - msg_dir),
                        "%x", p_db->job_id);
         if (utime(msg_dir, NULL) == -1)
         {
            if (errno == ENOENT)
            {
               /*
                * If the message file has been removed for whatever
                * reason, recreate the message or else the FD will
                * not know what to do with it.
                */
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Message %x not there, recreating it.", p_db->job_id);
               if (create_message(p_db->job_id, p_db->recipient,
                                  p_db->soptions) != SUCCESS)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             "Failed to create message for JID %x.",
                             p_db->job_id);
                  exit(INCORRECT);
               }
            }
            else
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to change modification time of %s : %s",
                          msg_dir, strerror(errno));
            }
         }
         return;
      }
   } /* for (i = 0; i < *no_of_job_ids; i++) */

   /*
    * This is a brand new job! Append this to the job_id_data structure.
    * But first check if there is still enough space in the structure.
    */
   if ((*no_of_job_ids != 0) &&
       ((*no_of_job_ids % JOB_ID_DATA_STEP_SIZE) == 0))
   {
      char   *ptr;
      size_t new_size = (((*no_of_job_ids / JOB_ID_DATA_STEP_SIZE) + 1) *
                        JOB_ID_DATA_STEP_SIZE * sizeof(struct job_id_data)) +
                        AFD_WORD_OFFSET;

      ptr = (char *)jd - AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(jd_fd, ptr, new_size)) == (caddr_t) -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "mmap_resize() error : %s", strerror(errno));
         exit(INCORRECT);
      }
      no_of_job_ids = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      jd = (struct job_id_data *)ptr;

#ifdef WITH_GOTCHA_LIST
      /*
       * Do not forget to increase the gotcha list or else we will get
       * a core dump.
       */
      new_size = ((*no_of_job_ids / JOB_ID_DATA_STEP_SIZE) + 1) *
                 JOB_ID_DATA_STEP_SIZE * sizeof(char);

      if ((gotcha = realloc(gotcha, new_size)) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "realloc() error : %s", strerror(errno));
         exit(INCORRECT);
      }
#endif
   }
   buf_size = sizeof(unsigned int) +                /* p_db->dir_config_id */
              sizeof(unsigned int) +                /* p_db->dir_id */
              sizeof(unsigned int) +                /* p_db->host_id */
              sizeof(unsigned int) +                /* p_db->file_mask_id */
              sizeof(char) +                        /* p_db->priority */
              sizeof(int) +                         /* p_db->no_of_files */
              sizeof(int) +                         /* p_db->no_of_loptions */
              sizeof(int) +                         /* p_db->no_of_soptions */
              MAX_RECIPIENT_LENGTH +                /* p_db->recipient */
              p_db->fbl +                           /* p_db->files */
#ifdef _NEW_JID
              (MAX_NO_OPTIONS * MAX_OPTION_LENGTH) +/* p_db->loptions */
#else
              MAX_OPTION_LENGTH +                   /* p_db->loptions */
#endif
              MAX_OPTION_LENGTH;                    /* p_db->soptions */
   if ((buffer = malloc(buf_size)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() %d bytes : %s",
                 buf_size, strerror(errno));
      exit(INCORRECT);
   }
   (void)memset(buffer, 0, buf_size);
   (void)memcpy(buffer, &p_db->dir_config_id, sizeof(unsigned int));
   offset = sizeof(unsigned int);
   (void)memcpy(&buffer[offset], &p_db->dir_id, sizeof(unsigned int));
   offset += sizeof(unsigned int);
   (void)memcpy(&buffer[offset], &p_db->host_id, sizeof(unsigned int));
   offset += sizeof(unsigned int);
   (void)memcpy(&buffer[offset], &p_db->file_mask_id, sizeof(unsigned int));
   offset += sizeof(unsigned int);
   buffer[offset] = p_db->priority;
   offset += sizeof(char);
   (void)memcpy(&buffer[offset], &p_db->no_of_files, sizeof(int));
   offset += sizeof(int);
   (void)memcpy(&buffer[offset], &p_db->no_of_loptions, sizeof(int));
   offset += sizeof(int);
   (void)memcpy(&buffer[offset], &p_db->no_of_soptions, sizeof(int));
   offset += sizeof(int);
   for (i = 0; i < *no_of_dir_names; i++)
   {
      if (p_db->dir_id == dnb[i].dir_id)
      {
         jd[*no_of_job_ids].dir_id_pos = i;
         break;
      }
   }
   jd[*no_of_job_ids].priority = p_db->priority;
   jd[*no_of_job_ids].no_of_loptions = p_db->no_of_loptions;
   jd[*no_of_job_ids].no_of_soptions = p_db->no_of_soptions;
   jd[*no_of_job_ids].dir_id = p_db->dir_id;
   jd[*no_of_job_ids].host_id = p_db->host_id;
   jd[*no_of_job_ids].dir_config_id = p_db->dir_config_id;
   jd[*no_of_job_ids].file_mask_id = p_db->file_mask_id;
   jd[*no_of_job_ids].recipient_id = p_db->recipient_id; /* Not used for CRC. */

   /*
    * NOTE: p_db->recipient is NOT MAX_RECIPIENT_LENGTH, it has the
    *       string length of p_db->recipient.
    */
   length = strlen(p_db->recipient) + 1; /* + 1 => '\0' */
   (void)memcpy(jd[*no_of_job_ids].recipient, p_db->recipient, length);
   (void)memcpy(&buffer[offset], p_db->recipient, length);
   offset += length;
   (void)memcpy(&buffer[offset], p_db->files, p_db->fbl);
   offset += p_db->fbl;
   if (p_db->loptions != NULL)
   {
      char *ptr;

      length = 0;
      ptr = p_db->loptions;
#ifdef _NEW_JID
      for (i = 0; ((i < p_db->no_of_loptions) && (length < (MAX_NO_OPTIONS * MAX_OPTION_LENGTH))); i++)
#else
      for (i = 0; ((i < p_db->no_of_loptions) && (length < MAX_OPTION_LENGTH)); i++)
#endif
      {
#ifdef _NEW_JID
         while ((*ptr != '\0') && ((length + 1) < (MAX_NO_OPTIONS * MAX_OPTION_LENGTH)))
#else
         while ((*ptr != '\0') && ((length + 1) < MAX_OPTION_LENGTH))
#endif
         {
            ptr++; length++;
         }
         ptr++; length++;
      }
#ifdef _NEW_JID
      if (length >= (MAX_NO_OPTIONS * MAX_OPTION_LENGTH))
#else
      if (length >= MAX_OPTION_LENGTH)
#endif
      {
         int j;

         for (j = i; j < p_db->no_of_loptions; j++)
         {
            while (*ptr != '\0')
            {
               ptr++;
            }
            ptr++;
         }
         length = ptr - p_db->loptions;
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Unable to store all AMG options in job data structure [%d >= %d].",
#ifdef _NEW_JID
                    length, (MAX_NO_OPTIONS * MAX_OPTION_LENGTH));
         length = (MAX_NO_OPTIONS * MAX_OPTION_LENGTH) - 1;
#else
                    length, MAX_OPTION_LENGTH);
         length = MAX_OPTION_LENGTH - 1;
#endif
         (void)memcpy(jd[*no_of_job_ids].loptions, p_db->loptions, length);
         (void)memcpy(&buffer[offset], p_db->loptions, length);
         jd[*no_of_job_ids].loptions[length] = '\0';
         buffer[offset + length] = '\0';
      }
      else
      {
         (void)memcpy(jd[*no_of_job_ids].loptions, p_db->loptions, length);
         (void)memcpy(&buffer[offset], p_db->loptions, length);
      }
      offset += length;
   }

   /*
    * With soptions the last byte is used to manipulate the checksum
    * in case we do encounter the unusual situation (but not impossible)
    * where the checksum is the same for two different jobs.
    */
   if (p_db->soptions != NULL)
   {
      length = strlen(p_db->soptions) + 1;
      if (length >= (MAX_OPTION_LENGTH - 1))
      {
         system_log(WARN_SIGN,  __FILE__, __LINE__,
                    "Unable to store all FD options in job data structure [%d >= %d].",
                    length, MAX_OPTION_LENGTH - 1);
         (void)memcpy(jd[*no_of_job_ids].soptions, p_db->soptions,
                      MAX_OPTION_LENGTH);
         (void)memcpy(&buffer[offset], p_db->soptions, MAX_OPTION_LENGTH);
         p_buf_crc = &buffer[offset + (MAX_OPTION_LENGTH - 1)];
         jd[*no_of_job_ids].soptions[MAX_OPTION_LENGTH - 2] = '\0';
         buffer[offset + MAX_OPTION_LENGTH - 2] = '\0';
         offset += MAX_OPTION_LENGTH;
      }
      else
      {
         (void)memcpy(jd[*no_of_job_ids].soptions, p_db->soptions, length);
         (void)memcpy(&buffer[offset], p_db->soptions, length);
         p_buf_crc = &buffer[offset + length];
         offset += (length + 1);
      }
   }
   else
   {
      p_buf_crc = &buffer[offset];
      offset++;
   }
   jd[*no_of_job_ids].soptions[MAX_OPTION_LENGTH - 1] = 0;
   (void)memcpy(jd[*no_of_job_ids].host_alias, p_db->host_alias,
                MAX_HOSTNAME_LENGTH + 1);

   /*
    * Lets generate a new checksum for this job. To ensure that the
    * checksum is unique, check that it does not appear anywhere in
    * struct job_id_data.
    */
   *jid_number = get_checksum(INITIAL_CRC, buffer, offset);
   for (i = 0; i < *no_of_job_ids; i++)
   {
      if (jd[i].job_id == *jid_number)
      {
         unsigned int new_jid_number;

         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Hmmm, same checksum (%x) for two different jobs (%d %d)!",
                    *jid_number, i, *no_of_job_ids);
#ifdef PRINT_DUP_JOB
         system_log(DEBUG_SIGN, NULL, 0, "dir_id : %x %x",
                    jd[i].dir_id, jd[*no_of_job_ids].dir_id);
         system_log(DEBUG_SIGN, NULL, 0, "file_mask_id : %x %x",
                    jd[i].file_mask_id, jd[*no_of_job_ids].file_mask_id);
         system_log(DEBUG_SIGN, NULL, 0, "dir_config_id : %x %x",
                    jd[i].dir_config_id, jd[*no_of_job_ids].dir_config_id);
         system_log(DEBUG_SIGN, NULL, 0, "dir_id_pos : %d %d",
                    jd[i].dir_id_pos, jd[*no_of_job_ids].dir_id_pos);
         system_log(DEBUG_SIGN, NULL, 0, "priority : %d %d",
                    (int)jd[i].priority, (int)jd[*no_of_job_ids].priority);
         system_log(DEBUG_SIGN, NULL, 0, "no_of_loptions : %d %d",
                    jd[i].no_of_loptions, jd[*no_of_job_ids].no_of_loptions);
         system_log(DEBUG_SIGN, NULL, 0, "no_of_soptions : %d %d",
                    jd[i].no_of_soptions, jd[*no_of_job_ids].no_of_soptions);
         system_log(DEBUG_SIGN, NULL, 0, "recipient : %s %s",
                    jd[i].recipient, jd[*no_of_job_ids].recipient);
         system_log(DEBUG_SIGN, NULL, 0, "host_alias : %s %s",
                    jd[i].host_alias, jd[*no_of_job_ids].host_alias);
#endif
         do
         {
            (*p_buf_crc)++;
            if ((unsigned char)(*p_buf_crc) > 254)
            {
               system_log(ERROR_SIGN,  __FILE__, __LINE__,
                          "Unable to produce a different checksum for `%x'. There are two different jobs with the same checksum!",
                          *jid_number);
               break;
            }
         } while ((new_jid_number = get_checksum(INITIAL_CRC, buffer,
                                                 offset)) == *jid_number);

         if (new_jid_number != *jid_number)
         {
            system_log(DEBUG_SIGN, NULL, 0,
                       "Was able to get a new job ID `%x' instead of `%x' after %d tries.",
                       new_jid_number, *jid_number, (int)(*p_buf_crc),
                      __FILE__, __LINE__);
            jd[*no_of_job_ids].soptions[MAX_OPTION_LENGTH - 1] = *p_buf_crc;
            *jid_number = new_jid_number;
         }
         break;
      }
   }
   free(buffer);
   p_db->job_id = *jid_number;
   (void)snprintf(p_db->str_job_id, MAX_INT_HEX_LENGTH, "%x", *jid_number);
   jd[*no_of_job_ids].job_id = *jid_number;
   (*no_of_job_ids)++;

   /*
    * Create the outgoing directory for this job.
    */
   p_outgoing_file_dir = outgoing_file_dir + strlen(outgoing_file_dir);
   *p_outgoing_file_dir = '/';
   (void)strcpy(p_outgoing_file_dir + 1, p_db->str_job_id);
   if ((mkdir(outgoing_file_dir, DIR_MODE) == -1) && (errno != EEXIST))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to mkdir() %s : %s",
                 outgoing_file_dir, strerror(errno));
      exit(INCORRECT);
   }
   *p_outgoing_file_dir = '\0';

   /*
    * Generate a message in the message directory.
    */
   if (create_message(p_db->job_id, p_db->recipient, p_db->soptions) != SUCCESS)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to create message for JID %x.", p_db->job_id);
      exit(INCORRECT);
   }

   return;
}
