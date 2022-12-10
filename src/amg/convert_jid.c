/*
 *  convert_jid.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   convert_jid - converts the JID from an old format to new one
 **
 ** SYNOPSIS
 **   char *convert_jid(int           old_jid_fd,
 **                     char          *old_job_id_data_file,
 **                     size_t        *old_jid_size,
 **                     int           old_no_of_job_ids,
 **                     unsigned char old_version,
 **                     unsigned char new_version)
 **
 ** DESCRIPTION
 **   When there is a change in the structure job_id_data (JID) it
 **   tries to convert this to the new structure. It currently only
 **   converts the following versions:  1 to 2, 1 to 3 and 2 to 3.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.06.2008 H.Kiehl Created
 **   17.10.2012 H.Kiehl Added version 3.
 **
 */
DESCR__E_M1

#include <stdio.h>                    /* fprintf()                       */
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <stdlib.h>                   /* exit()                          */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                /* mmap()                          */
#endif
#include <unistd.h>                   /* sysconf()                       */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>                   /* O_RDWR, O_CREAT, O_WRONLY, etc  */
#endif
#include <errno.h>
#include "amgdefs.h"

/* Version 1 */
#define MAX_OPTION_LENGTH_1        256
#define MAX_RECIPIENT_LENGTH_1     256
#define MAX_HOSTNAME_LENGTH_1      8
#define AFD_WORD_OFFSET_1          (SIZEOF_INT + 4 + SIZEOF_INT + 4)

struct job_id_data_1
       {
          unsigned int job_id;
          unsigned int dir_id;
          unsigned int file_mask_id;
          unsigned int dir_config_id;
          int          dir_id_pos;
          char         priority;
          int          no_of_loptions;
          char         loptions[MAX_OPTION_LENGTH_1];
          int          no_of_soptions;
          char         soptions[MAX_OPTION_LENGTH_1];
          char         recipient[MAX_RECIPIENT_LENGTH_1];
          char         host_alias[MAX_HOSTNAME_LENGTH_1 + 1];
       };

/* Version 2 */
#define MAX_OPTION_LENGTH_2        256
#define MAX_RECIPIENT_LENGTH_2     256
#define MAX_HOSTNAME_LENGTH_2      8
#define AFD_WORD_OFFSET_2          (SIZEOF_INT + 4 + SIZEOF_INT + 4)

struct job_id_data_2
       {
          unsigned int job_id;
          unsigned int dir_id;
          unsigned int file_mask_id;
          unsigned int dir_config_id;
          unsigned int host_id;      /* New */
          unsigned int recipient_id; /* New */
          int          dir_id_pos;
          int          no_of_loptions;
          int          no_of_soptions;
          char         loptions[MAX_OPTION_LENGTH_2];
          char         soptions[MAX_OPTION_LENGTH_2];
          char         recipient[MAX_RECIPIENT_LENGTH_2];
          char         host_alias[MAX_HOSTNAME_LENGTH_2 + 1];
          char         priority;
       };

/* Version 3 */
#define MAX_OPTION_LENGTH_3        256
#define MAX_RECIPIENT_LENGTH_3     256
#define MAX_HOSTNAME_LENGTH_3      8
#define AFD_WORD_OFFSET_3          (SIZEOF_INT + 4 + SIZEOF_INT + 4)

struct job_id_data_3
       {
          time_t       creation_time;  /* New */
          unsigned int special_flag;   /* New */
          unsigned int job_id;
          unsigned int dir_id;
          unsigned int file_mask_id;
          unsigned int dir_config_id;
          unsigned int host_id;
          unsigned int recipient_id;
          int          dir_id_pos;
          int          no_of_loptions;
          int          no_of_soptions;
          char         loptions[MAX_NO_OPTIONS * MAX_OPTION_LENGTH_3]; /* Changed */
          char         soptions[MAX_OPTION_LENGTH_3];
          char         recipient[MAX_RECIPIENT_LENGTH_3];
          char         host_alias[MAX_HOSTNAME_LENGTH_3 + 1];
          char         priority;
       };


/*############################ convert_jid() ############################*/
char *
convert_jid(int           old_jid_fd,
            char          *old_job_id_data_file,
            size_t        *old_jid_size,
            int           old_no_of_job_ids,
            unsigned char old_version,
            unsigned char new_version)
{
   char         *ptr;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   if ((old_version == 1) && (new_version == 2))
   {
      int                  i;
      size_t               new_size;
      struct job_id_data_1 *old_jid;
      struct job_id_data_2 *new_jid;

      /* Get the size of the old JID file. */
#ifdef HAVE_STATX
      if (statx(old_jid_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(old_jid_fd, &stat_buf) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    "Failed to statx() %s : %s",
#else
                    "Failed to fstat() %s : %s",
#endif
                    old_job_id_data_file, strerror(errno));
         *old_jid_size = -1;
         return(NULL);
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf.stx_size > 0)
#else
         if (stat_buf.st_size > 0)
#endif
         {
#ifdef HAVE_MMAP
            if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                            stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                            stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                            MAP_SHARED, old_jid_fd, 0)) == (caddr_t) -1)
#else
            if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                MAP_SHARED, old_job_id_data_file, 0)) == (caddr_t) -1)
#endif
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to mmap() to %s : %s",
                          old_job_id_data_file, strerror(errno));
               *old_jid_size = -1;
               return(NULL);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "FSA file %s is empty.", old_job_id_data_file);
            *old_jid_size = -1;
            return(NULL);
         }
      }

      ptr += AFD_WORD_OFFSET_1;
      old_jid = (struct job_id_data_1 *)ptr;

      new_size = ((old_no_of_job_ids / JOB_ID_DATA_STEP_SIZE) + 1) *
                 JOB_ID_DATA_STEP_SIZE * sizeof(struct job_id_data_2);
      if ((ptr = malloc(new_size)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error [%d %d] : %s",
                    old_no_of_job_ids, new_size, strerror(errno));
         ptr = (char *)old_jid;
         ptr -= AFD_WORD_OFFSET_1;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
         if (munmap(ptr, stat_buf.stx_size) == -1)
# else
         if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to munmap() %s : %s",
                       old_job_id_data_file, strerror(errno));
         }
         *old_jid_size = -1;
         return(NULL);
      }
      (void)memset(ptr, 0, new_size);
      new_jid = (struct job_id_data_2 *)ptr;

      /*
       * Copy all the old data into the new region.
       */
      for (i = 0; i < old_no_of_job_ids; i++)
      {
         (void)strcpy(new_jid[i].host_alias, old_jid[i].host_alias);
         (void)strcpy(new_jid[i].recipient, old_jid[i].recipient);
         (void)memcpy(new_jid[i].soptions, old_jid[i].soptions, MAX_OPTION_LENGTH_1);
         (void)memcpy(new_jid[i].loptions, old_jid[i].loptions, MAX_OPTION_LENGTH_1);
         new_jid[i].no_of_soptions = old_jid[i].no_of_soptions;
         new_jid[i].no_of_loptions = old_jid[i].no_of_loptions;
         new_jid[i].priority       = old_jid[i].priority;
         new_jid[i].dir_id_pos     = old_jid[i].dir_id_pos;
         new_jid[i].dir_config_id  = old_jid[i].dir_config_id;
         new_jid[i].file_mask_id   = old_jid[i].file_mask_id;
         new_jid[i].dir_id         = old_jid[i].dir_id;
         new_jid[i].job_id         = old_jid[i].job_id;
         new_jid[i].host_id        = get_str_checksum(new_jid[i].host_alias);
         new_jid[i].recipient_id   = get_str_checksum(new_jid[i].recipient);
      }

      ptr = (char *)old_jid;
      ptr -= AFD_WORD_OFFSET_1;

      /*
       * Resize the old JID to the size of new one and then copy
       * the new structure into it.
       */
      if ((ptr = mmap_resize(old_jid_fd, ptr, new_size + AFD_WORD_OFFSET_2)) == (caddr_t) -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to mmap_resize() %s : %s",
                    old_job_id_data_file, strerror(errno));
         free((void *)new_jid);
         return(NULL);
      }
      ptr += AFD_WORD_OFFSET_2;
      (void)memcpy(ptr, new_jid, new_size);
      free((void *)new_jid);
      ptr -= AFD_WORD_OFFSET_2;
      *(int *)ptr = old_no_of_job_ids;
      *old_jid_size = new_size + AFD_WORD_OFFSET_2;

      system_log(INFO_SIGN, NULL, 0,
                 "Converted JID from verion %d to %d.",
                 (int)old_version, (int)new_version);
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
# if SIZEOF_SIZE_T == 4
                 "filesize old: %ld (%d) new: %ld (%d)  Number of jobs: %d",
# else
                 "filesize old: %ld (%d) new: %lld (%d)  Number of jobs: %d",
# endif
#else
# if SIZEOF_SIZE_T == 4
                 "filesize old: %lld (%d) new: %ld (%d)  Number of jobs: %d",
# else
                 "filesize old: %lld (%d) new: %lld (%d)  Number of jobs: %d",
# endif
#endif
#ifdef HAVE_STATX
                 (pri_off_t)stat_buf.stx_size, (int)sizeof(struct job_id_data_1),
#else
                 (pri_off_t)stat_buf.st_size, (int)sizeof(struct job_id_data_1),
#endif
                 (pri_size_t)new_size + AFD_WORD_OFFSET_2,
                 (int)sizeof(struct job_id_data_2), old_no_of_job_ids);
   }
   else if ((old_version == 1) && (new_version == 3))
        {
           int                  i;
           size_t               new_size;
           struct job_id_data_1 *old_jid;
           struct job_id_data_3 *new_jid;

           /* Get the size of the old JID file. */
#ifdef HAVE_STATX
           if (statx(old_jid_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) < 0)
#else
           if (fstat(old_jid_fd, &stat_buf) < 0)
#endif
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                         "Failed to statx() %s : %s",
#else
                         "Failed to fstat() %s : %s",
#endif
                         old_job_id_data_file, strerror(errno));
              *old_jid_size = -1;
              return(NULL);
           }
           else
           {
#ifdef HAVE_STATX
              if (stat_buf.stx_size > 0)
#else
              if (stat_buf.st_size > 0)
#endif
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                                 stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                 stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                 MAP_SHARED, old_jid_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                     stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                     stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                     MAP_SHARED, old_job_id_data_file,
                                     0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                                    old_job_id_data_file, strerror(errno));
                    *old_jid_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FSA file %s is empty.", old_job_id_data_file);
                 *old_jid_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_1;
           old_jid = (struct job_id_data_1 *)ptr;

           new_size = ((old_no_of_job_ids / JOB_ID_DATA_STEP_SIZE) + 1) *
                      JOB_ID_DATA_STEP_SIZE * sizeof(struct job_id_data_3);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_job_ids, new_size, strerror(errno));
              ptr = (char *)old_jid;
              ptr -= AFD_WORD_OFFSET_1;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
              if (munmap(ptr, stat_buf.stx_size) == -1)
# else
              if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_job_id_data_file, strerror(errno));
              }
              *old_jid_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_jid = (struct job_id_data_3 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_job_ids; i++)
           {
              (void)strcpy(new_jid[i].host_alias, old_jid[i].host_alias);
              (void)strcpy(new_jid[i].recipient, old_jid[i].recipient);
              (void)memcpy(new_jid[i].soptions, old_jid[i].soptions, MAX_OPTION_LENGTH_1);
              (void)memcpy(new_jid[i].loptions, old_jid[i].loptions, MAX_OPTION_LENGTH_1);
              new_jid[i].creation_time  = 0L;
              new_jid[i].special_flag   = 0;
              new_jid[i].no_of_soptions = old_jid[i].no_of_soptions;
              new_jid[i].no_of_loptions = old_jid[i].no_of_loptions;
              new_jid[i].priority       = old_jid[i].priority;
              new_jid[i].dir_id_pos     = old_jid[i].dir_id_pos;
              new_jid[i].dir_config_id  = old_jid[i].dir_config_id;
              new_jid[i].file_mask_id   = old_jid[i].file_mask_id;
              new_jid[i].dir_id         = old_jid[i].dir_id;
              new_jid[i].job_id         = old_jid[i].job_id;
              new_jid[i].host_id        = get_str_checksum(new_jid[i].host_alias);
              new_jid[i].recipient_id   = get_str_checksum(new_jid[i].recipient);
           }

           ptr = (char *)old_jid;
           ptr -= AFD_WORD_OFFSET_1;

           /*
            * Resize the old JID to the size of new one and then copy
            * the new structure into it.
            */
           if ((ptr = mmap_resize(old_jid_fd, ptr, new_size + AFD_WORD_OFFSET_3)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_job_id_data_file, strerror(errno));
              free((void *)new_jid);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_3;
           (void)memcpy(ptr, new_jid, new_size);
           free((void *)new_jid);
           ptr -= AFD_WORD_OFFSET_3;
           *(int *)ptr = old_no_of_job_ids;
           *old_jid_size = new_size + AFD_WORD_OFFSET_3;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted JID from verion %d to %d.",
                      (int)old_version, (int)new_version);
           system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
# if SIZEOF_SIZE_T == 4
                      "filesize old: %ld (%d) new: %ld (%d)  Number of jobs: %d",
# else
                      "filesize old: %ld (%d) new: %lld (%d)  Number of jobs: %d",
# endif
#else
# if SIZEOF_SIZE_T == 4
                      "filesize old: %lld (%d) new: %ld (%d)  Number of jobs: %d",
# else
                      "filesize old: %lld (%d) new: %lld (%d)  Number of jobs: %d",
# endif
#endif
#ifdef HAVE_STATX
                      (pri_off_t)stat_buf.stx_size,
#else
                      (pri_off_t)stat_buf.st_size,
#endif
                      (int)sizeof(struct job_id_data_1),
                      (pri_size_t)new_size + AFD_WORD_OFFSET_3,
                      (int)sizeof(struct job_id_data_3), old_no_of_job_ids);
        }
   else if ((old_version == 2) && (new_version == 3))
        {
           int                  i;
           size_t               new_size;
           struct job_id_data_2 *old_jid;
           struct job_id_data_3 *new_jid;

           /* Get the size of the old JID file. */
#ifdef HAVE_STATX
           if (statx(old_jid_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                     STATX_SIZE, &stat_buf) == -1)
#else
           if (fstat(old_jid_fd, &stat_buf) == -1)
#endif
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                         "Failed to statx() %s : %s",
#else
                         "Failed to fstat() %s : %s",
#endif
                         old_job_id_data_file, strerror(errno));
              *old_jid_size = -1;
              return(NULL);
           }
           else
           {
#ifdef HAVE_STATX
              if (stat_buf.stx_size > 0)
#else
              if (stat_buf.st_size > 0)
#endif
              {
#ifdef HAVE_MMAP
                 if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                                 stat_buf.stx_size, (PROT_READ | PROT_WRITE),
# else
                                 stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                 MAP_SHARED, old_jid_fd, 0)) == (caddr_t) -1)
#else
                 if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                     stat_buf.stx_size,
# else
                                     stat_buf.st_size,
# endif
                                     (PROT_READ | PROT_WRITE),
                                     MAP_SHARED, old_job_id_data_file, 0)) == (caddr_t) -1)
#endif
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to mmap() to %s : %s",
                               old_job_id_data_file, strerror(errno));
                    *old_jid_size = -1;
                    return(NULL);
                 }
              }
              else
              {
                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                            "FSA file %s is empty.", old_job_id_data_file);
                 *old_jid_size = -1;
                 return(NULL);
              }
           }

           ptr += AFD_WORD_OFFSET_2;
           old_jid = (struct job_id_data_2 *)ptr;

           new_size = ((old_no_of_job_ids / JOB_ID_DATA_STEP_SIZE) + 1) *
                      JOB_ID_DATA_STEP_SIZE * sizeof(struct job_id_data_3);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_job_ids, new_size, strerror(errno));
              ptr = (char *)old_jid;
              ptr -= AFD_WORD_OFFSET_2;
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
              if (munmap(ptr, stat_buf.stx_size) == -1)
# else
              if (munmap(ptr, stat_buf.st_size) == -1)
# endif
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_job_id_data_file, strerror(errno));
              }
              *old_jid_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_jid = (struct job_id_data_3 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_job_ids; i++)
           {
              (void)strcpy(new_jid[i].host_alias, old_jid[i].host_alias);
              (void)strcpy(new_jid[i].recipient, old_jid[i].recipient);
              (void)memcpy(new_jid[i].soptions, old_jid[i].soptions, MAX_OPTION_LENGTH_2);
              (void)memcpy(new_jid[i].loptions, old_jid[i].loptions, MAX_OPTION_LENGTH_2);
              new_jid[i].creation_time  = 0L;
              new_jid[i].special_flag   = 0;
              new_jid[i].no_of_soptions = old_jid[i].no_of_soptions;
              new_jid[i].no_of_loptions = old_jid[i].no_of_loptions;
              new_jid[i].priority       = old_jid[i].priority;
              new_jid[i].dir_id_pos     = old_jid[i].dir_id_pos;
              new_jid[i].dir_config_id  = old_jid[i].dir_config_id;
              new_jid[i].file_mask_id   = old_jid[i].file_mask_id;
              new_jid[i].dir_id         = old_jid[i].dir_id;
              new_jid[i].job_id         = old_jid[i].job_id;
              new_jid[i].host_id        = old_jid[i].host_id;
              new_jid[i].recipient_id   = old_jid[i].recipient_id;
           }

           ptr = (char *)old_jid;
           ptr -= AFD_WORD_OFFSET_2;

           /*
            * Resize the old JID to the size of new one and then copy
            * the new structure into it.
            */
           if ((ptr = mmap_resize(old_jid_fd, ptr, new_size + AFD_WORD_OFFSET_3)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_job_id_data_file, strerror(errno));
              free((void *)new_jid);
              return(NULL);
           }
           ptr += AFD_WORD_OFFSET_3;
           (void)memcpy(ptr, new_jid, new_size);
           free((void *)new_jid);
           ptr -= AFD_WORD_OFFSET_3;
           *(int *)ptr = old_no_of_job_ids;
           *old_jid_size = new_size + AFD_WORD_OFFSET_3;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted JID from verion %d to %d.",
                      (int)old_version, (int)new_version);
           system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
# if SIZEOF_SIZE_T == 4
                      "filesize old: %ld (%d) new: %ld (%d)  Number of jobs: %d",
# else
                      "filesize old: %ld (%d) new: %lld (%d)  Number of jobs: %d",
# endif
#else
# if SIZEOF_SIZE_T == 4
                      "filesize old: %lld (%d) new: %ld (%d)  Number of jobs: %d",
# else
                      "filesize old: %lld (%d) new: %lld (%d)  Number of jobs: %d",
# endif
#endif
#ifdef HAVE_STATX
                      (pri_off_t)stat_buf.stx_size,
#else
                      (pri_off_t)stat_buf.st_size,
#endif
                      (int)sizeof(struct job_id_data_2),
                      (pri_size_t)new_size + AFD_WORD_OFFSET_3,
                      (int)sizeof(struct job_id_data_3), old_no_of_job_ids);
        }
        else
        {
           system_log(ERROR_SIGN, NULL, 0,
                      "Don't know how to convert a version %d JID to version %d.",
                      old_version, new_version);
           ptr = NULL;
        }

   return(ptr);
}
