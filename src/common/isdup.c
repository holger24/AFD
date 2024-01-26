/*
 *  isdup.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   isdup - checks for duplicates
 **
 ** SYNOPSIS
 **   int  isdup(char         *fullname,
 **              char         *filename,
 **              off_t        size,
 **              unsigned int id,
 **              time_t       timeout,
 **              unsigned int flag,
 **              int          rm_flag,
 **              int          have_hw_crc32,
 **              int          stay_attached,
 **              int          lock)
 **   void isdup_detach(void)
 **   int  isdup_rm(char         *fullname,
 **                 char         *filename,
 **                 off_t        size,
 **                 unsigned int id,
 **                 unsigned int flag,
 **                 int          have_hw_crc32,
 **                 int          stay_attached,
 **                 int          lock)
 **
 ** DESCRIPTION
 **   The function isdup() checks if this file is a duplicate. The
 **   variable fullname is the filename with the full path. If the
 **   filename is part of the CRC it will cut it out from fullname,
 **   unless filename is not NULL. In that case it will take what
 **   is in filename. This allows one to perform a check before the
 **   file is being renamed.
 **
 ** RETURN VALUES
 **   isdup() returns YES if the file is duplicate, otherwise NO is
 **   returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.06.2005 H.Kiehl Created
 **   09.03.2006 H.Kiehl Added parameter to remove a CRC.
 **   20.12.2006 H.Kiehl Added option to ignore last suffix of filename.
 **   05.03.2008 H.Kiehl Added option to test for filename and size.
 **   19.11.2012 H.Kiehl Added option to stay attached.
 **   21.11.2012 H.Kiehl Added option to disable locking.
 **   23.11.2012 H.Kiehl Included support for another CRC-32 algoritm
 **                      which also supports SSE4.2 on some CPU's.
 **   01.02.2013 H.Kiehl Added support to make timeout being fixed, ie.
 **                      is not cumulative.
 **   03.01.2014 H.Kiehl Since attach_buf() can take a while set variable
 **                      current_time after this function call.
 **   12.01.2014 H.Kiehl Added optional parameter filename, so it is
 **                      possible to do a check on a filename before
 **                      it is renamed.
 **   15.12.2020 H.Kiehl Added murmurhash3.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* #define DEBUG_ISDUP 2600083675 */

/* External global variables. */
extern char *p_work_dir;

/* Local global variables. */
static int            cdb_fd = -1,
                      *no_of_crcs;
static char           *p_cdb_time;
static struct crc_buf *cdb = NULL;

/* Local function prototypes. */
static unsigned int   get_crc(char *, char *, off_t,
#if defined (DEBUG_ISDUP) && defined (_MAINTAINER_LOG)
                              unsigned int, time_t, int, int,
#endif
#ifdef HAVE_HW_CRC32
                              int,
#endif
                              int);


/*############################### isdup() ###############################*/
int
isdup(char         *fullname,
      char         *filename,
      off_t        size,
      unsigned int id,
      time_t       timeout,
      unsigned int flag,
      int          rm_flag,
#ifdef HAVE_HW_CRC32
      int          have_hw_crc32,
#endif
      int          stay_attached,
      int          lock)
{
   int          dup,
                i;
   unsigned int crc;
   char         *ptr;

   crc = get_crc(fullname, filename, size,
#if defined (DEBUG_ISDUP) && defined (_MAINTAINER_LOG)
                 id, timeout, stay_attached, lock,
#endif
#ifdef HAVE_HW_CRC32
                 have_hw_crc32,
#endif
                 flag);
   if (crc == 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
# if SIZEOF_TIME_T == 4
                 "Failed to get a CRC value for %s [id=%x filename=%s size=%ld timeout=%ld stay_attached=%d lock=%d flag=%u]",
# else
                 "Failed to get a CRC value for %s [id=%x filename=%s size=%ld timeout=%lld stay_attached=%d lock=%d flag=%u]",
# endif
#else
# if SIZEOF_TIME_T == 4
                 "Failed to get a CRC value for %s [id=%x filename=%s size=%lld timeout=%ld stay_attached=%d lock=%d flag=%u]",
# else
                 "Failed to get a CRC value for %s [id=%x filename=%s size=%lld timeout=%lld stay_attached=%d lock=%d flag=%u]",
# endif
#endif
                 fullname, id, filename, (pri_off_t)size, (pri_time_t)timeout,
                 stay_attached, lock, flag);
      return(NO);
   }
#if defined (DEBUG_ISDUP) && defined (_MAINTAINER_LOG)
   if (id == DEBUG_ISDUP)
   {
      maintainer_log(INFO_SIGN, __FILE__, __LINE__,
                     "%x| isdup() checking crc=%x fullname=%s filename=%s",
                     id, crc, fullname, (filename == NULL) ? "(null)" : filename);
   }
#endif
   if ((stay_attached == NO) || (cdb == NULL))
   {
      size_t new_size;
      time_t current_time;
      char   crcfile[MAX_PATH_LENGTH];

      /*
       * Map to checksum file of this job. If it does not exist create it.
       */
      (void)snprintf(crcfile, MAX_PATH_LENGTH, "%s%s%s/%x",
                     p_work_dir, AFD_FILE_DIR, CRC_DIR, id);
      new_size = (CRC_STEP_SIZE * sizeof(struct crc_buf)) + AFD_WORD_OFFSET;
      if ((ptr = attach_buf(crcfile, &cdb_fd, &new_size,
                            (lock == YES) ? "isdup()" : NULL,
                            FILE_MODE, YES)) == (caddr_t) -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to mmap() `%s' : %s"), crcfile, strerror(errno));
         return(NO);
      }
      no_of_crcs = (int *)ptr;
      p_cdb_time = ptr + SIZEOF_INT + 4;

      /* Check that time has sane value (it's not initialized). */
      current_time = time(NULL);
      if ((*(time_t *)p_cdb_time < 100000) ||
          (*(time_t *)p_cdb_time > (current_time - DUPCHECK_MIN_CHECK_TIME)))
      {
         *(time_t *)p_cdb_time = current_time;
      }

      ptr += AFD_WORD_OFFSET;
      cdb = (struct crc_buf *)ptr;
   }
#ifdef THIS_DOES_NOT_WORK
   else if (lock == YES)
        {
# ifdef LOCK_DEBUG
           lock_region_w(cdb_fd, 0, __FILE__, __LINE__);
# else
           lock_region_w(cdb_fd, 0);
# endif
        }
#endif /* THIS_DOES_NOT_WORK */
   dup = NO;
#if defined (DEBUG_ISDUP) && defined (_MAINTAINER_LOG)
   if (id == DEBUG_ISDUP)
   {
      int ii;

      for (ii = 0; ii < *no_of_crcs; ii++)
      {
         maintainer_log(INFO_SIGN, __FILE__, __LINE__,
                        "%x| Current table %d: crc=%x timeout=%lld flag=%u",
                        id, ii, cdb[ii].crc, cdb[ii].timeout, cdb[ii].flag);
      }
   }
#endif

   if (rm_flag != YES)
   {
      time_t current_time;

      /*
       * At certain intervalls always check if the crc values we have
       * stored have not timed out. If so remove them from the list.
       */
      current_time = time(NULL);
      if (current_time > *(time_t *)p_cdb_time)
      {
         time_t dupcheck_check_time;

         if (timeout > DUPCHECK_MAX_CHECK_TIME)
         {
            dupcheck_check_time = DUPCHECK_MAX_CHECK_TIME;
         }
         else if (timeout < DUPCHECK_MIN_CHECK_TIME)
              {
                 dupcheck_check_time = DUPCHECK_MIN_CHECK_TIME;
              }
              else
              {
                 dupcheck_check_time = timeout;
              }

         for (i = 0; i < *no_of_crcs; i++)
         {
            if (cdb[i].timeout <= current_time)
            {
               int end_pos = i + 1;

               while ((++end_pos <= *no_of_crcs) &&
                      (cdb[end_pos - 1].timeout <= current_time))
               {
                  /* Nothing to be done. */;
               }
#if defined (DEBUG_ISDUP) && defined (_MAINTAINER_LOG)
               if (id == DEBUG_ISDUP)
               {
                  int j;

                  maintainer_log(INFO_SIGN, __FILE__, __LINE__,
                                 "%x| no_of_crcs=%d deleting=%d => %d current_time=%lld end_pos=%d",
                                 id, *no_of_crcs, end_pos - 1 - i, (*no_of_crcs) - (end_pos - 1 - i),
                                 current_time, end_pos);
                  for (j = i; j < (end_pos - 1); j++)
                  {
                     maintainer_log(INFO_SIGN, __FILE__, __LINE__,
                                    "%x| Removing: crc=%x timeout=%lld flag=%u",
                                    id, cdb[j].crc, cdb[j].timeout, cdb[j].flag);
                  }
               }
#endif
               if (end_pos <= *no_of_crcs)
               {
                  size_t move_size = (*no_of_crcs - (end_pos - 1)) *
                                     sizeof(struct crc_buf);

                  (void)memmove(&cdb[i], &cdb[end_pos - 1], move_size);
               }
               (*no_of_crcs) -= (end_pos - 1 - i);
#if defined (DEBUG_ISDUP) && defined (_MAINTAINER_LOG)
               if (id == DEBUG_ISDUP)
               {
                  int ii;

                  for (ii = 0; ii < *no_of_crcs; ii++)
                  {
                     maintainer_log(INFO_SIGN, __FILE__, __LINE__,
                                    "%x| Table after delete %d: crc=%x timeout=%lld flag=%u",
                                    id, ii, cdb[ii].crc, cdb[ii].timeout, cdb[ii].flag);
                  }
               }
#endif
            }
         }
         *(time_t *)p_cdb_time = ((time(NULL) / dupcheck_check_time) *
                                  dupcheck_check_time) + dupcheck_check_time;
      }

      if (timeout > 0L)
      {
         current_time = time(NULL);
         for (i = 0; i < *no_of_crcs; i++)
         {
            if ((crc == cdb[i].crc) && (flag == cdb[i].flag))
            {
               if (current_time <= cdb[i].timeout)
               {
                  dup = YES;
               }
               else
               {
                  dup = NEITHER;
               }
               if ((flag & TIMEOUT_IS_FIXED) == 0)
               {
                  cdb[i].timeout = current_time + timeout;
               }
               break;
            }
         }
      }

      if (dup == NO)
      {
         if ((*no_of_crcs != 0) &&
             ((*no_of_crcs % CRC_STEP_SIZE) == 0))
         {
            size_t new_size;

            new_size = (((*no_of_crcs / CRC_STEP_SIZE) + 1) *
                        CRC_STEP_SIZE * sizeof(struct crc_buf)) +
                       AFD_WORD_OFFSET;
            ptr = (char *)cdb - AFD_WORD_OFFSET;
            if ((ptr = mmap_resize(cdb_fd, ptr, new_size)) == (caddr_t) -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("mmap_resize() error : %s"), strerror(errno));
               (void)close(cdb_fd);
               return(NO);
            }
            p_cdb_time = ptr + SIZEOF_INT + 4;
            no_of_crcs = (int *)ptr;
            ptr += AFD_WORD_OFFSET;
            cdb = (struct crc_buf *)ptr;
         }
         cdb[*no_of_crcs].crc = crc;
         cdb[*no_of_crcs].flag = flag;
         cdb[*no_of_crcs].timeout = time(NULL) + timeout;
#if defined (DEBUG_ISDUP) && defined (_MAINTAINER_LOG)
         if (id == DEBUG_ISDUP)
         {
            maintainer_log(INFO_SIGN, __FILE__, __LINE__,
                           "%x| Adding: crc=%x timeout=%lld flag=%u",
                           id, cdb[*no_of_crcs].crc, cdb[*no_of_crcs].timeout,
                           cdb[*no_of_crcs].flag);
         }
#endif
         (*no_of_crcs)++;
#if defined (DEBUG_ISDUP) && defined (_MAINTAINER_LOG)
         if (id == DEBUG_ISDUP)
         {
            int ii;

            for (ii = 0; ii < *no_of_crcs; ii++)
            {
               maintainer_log(INFO_SIGN, __FILE__, __LINE__,
                              "%x| Table after adding %d: crc=%x timeout=%lld flag=%u",
                              id, ii, cdb[ii].crc, cdb[ii].timeout,
                              cdb[ii].flag);
            }
         }
#endif
      }
      else if (dup == NEITHER)
           {
              dup = NO;
           }
   }
   else
   {
      if (timeout > 0L)
      {
         for (i = 0; i < *no_of_crcs; i++)
         {
            if ((crc == cdb[i].crc) && (flag == cdb[i].flag))
            {
               if (i <= *no_of_crcs)
               {
                  size_t move_size = (*no_of_crcs - (i - 1)) *
                                     sizeof(struct crc_buf);

                  (void)memmove(&cdb[i], &cdb[i - 1], move_size);
               }
               (*no_of_crcs) -= 1;
               break;
            }
         }
      }
   }

   if (stay_attached == NO)
   {
      unmap_data(cdb_fd, (void *)&cdb);
      cdb_fd = -1;

      /* Note: cdb is set to NULL by unmap_data(). */
   }
#ifdef THIS_DOES_NOT_WORK
   else if (lock == YES)
        {
# ifdef LOCK_DEBUG
           unlock_region(cdb_fd, 0, __FILE__, __LINE__);
# else                                                                   
           unlock_region(cdb_fd, 0);
# endif
        }
#endif /* THIS_DOES_NOT_WORK */

   return(dup);
}


/*############################## isdup_rm() #############################*/
int
isdup_rm(char         *fullname,
         char         *filename,
         off_t        size,
         unsigned int id,
         unsigned int flag,
#ifdef HAVE_HW_CRC32
         int          have_hw_crc32,
#endif
         int          stay_attached,
         int          lock)
{
   int          ret = INCORRECT;
   unsigned int crc;

   crc = get_crc(fullname, filename, size,
#if defined (DEBUG_ISDUP) && defined (_MAINTAINER_LOG)
                 id, 0, stay_attached, lock,
#endif
#ifdef HAVE_HW_CRC32
                 have_hw_crc32,
#endif
                 flag);
   if (crc == 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to get a CRC value for %s", fullname);
      return(INCORRECT);
   }
   else
   {
      int i;

#if defined (DEBUG_ISDUP) && defined (_MAINTAINER_LOG)
      if (id == DEBUG_ISDUP)
      {
         maintainer_log(INFO_SIGN, __FILE__, __LINE__,
                        "%x| isdup_rm() check rm crc=%x fullname=%s filename=%s",
                        id, crc, fullname, (filename == NULL) ? "(null)" : filename);
      }
#endif
      if ((stay_attached == NO) || (cdb == NULL))
      {
         size_t new_size;
         time_t current_time;
         char   crcfile[MAX_PATH_LENGTH],
                *ptr;

         /*
          * Map to checksum file of this job. If it does not exist create it.
          */
         (void)snprintf(crcfile, MAX_PATH_LENGTH, "%s%s%s/%x",
                        p_work_dir, AFD_FILE_DIR, CRC_DIR, id);
         new_size = (CRC_STEP_SIZE * sizeof(struct crc_buf)) + AFD_WORD_OFFSET;
         if ((ptr = attach_buf(crcfile, &cdb_fd, &new_size,
                               (lock == YES) ? "isdup()" : NULL,
                               FILE_MODE, YES)) == (caddr_t) -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to mmap() `%s' : %s"), crcfile, strerror(errno));
            return(INCORRECT);
         }
         no_of_crcs = (int *)ptr;
         p_cdb_time = ptr + SIZEOF_INT + 4;

         /* Check that time has sane value (it's not initialized). */
         current_time = time(NULL);
         if ((*(time_t *)p_cdb_time < 100000) ||
             (*(time_t *)p_cdb_time > (current_time - DUPCHECK_MIN_CHECK_TIME)))
         {
            *(time_t *)p_cdb_time = current_time;
         }

         ptr += AFD_WORD_OFFSET;
         cdb = (struct crc_buf *)ptr;
      }
#ifdef THIS_DOES_NOT_WORK
      else if (lock == YES)
           {
# ifdef LOCK_DEBUG
              lock_region_w(cdb_fd, 0, __FILE__, __LINE__);
# else
              lock_region_w(cdb_fd, 0);
# endif
           }
#endif /* THIS_DOES_NOT_WORK */

      for (i = 0; i < *no_of_crcs; i++)
      {
         if ((crc == cdb[i].crc) && (flag == cdb[i].flag))
         {
            if (i <= *no_of_crcs)
            {
               (void)memmove(&cdb[i], &cdb[i + 1],
                             ((*no_of_crcs - 1 - i) * sizeof(struct crc_buf)));
            }
            (*no_of_crcs) -= 1;
            ret = SUCCESS;
            break;
         }
      }
#if defined (DEBUG_ISDUP) && defined (_MAINTAINER_LOG)
      if (id == DEBUG_ISDUP)
      {
         if (ret == SUCCESS)
         {
            int ii;

            for (ii = 0; ii < *no_of_crcs; ii++)
            {
               maintainer_log(INFO_SIGN, __FILE__, __LINE__,
                              "%x| isdup_rm() Table after removing %d: crc=%x timeout=%lld flag=%u",
                              id, ii, cdb[ii].crc, cdb[ii].timeout,
                              cdb[ii].flag);
            }
         }
         else
         {
            maintainer_log(WARN_SIGN, __FILE__, __LINE__,
                           "%x| Failed to locate crc %x", id, crc);
         }
      }
#endif
      if (stay_attached == NO)
      {
         unmap_data(cdb_fd, (void *)&cdb);
         cdb_fd = -1;

         /* Note: cdb is set to NULL by unmap_data(). */
      }
#ifdef THIS_DOES_NOT_WORK
      else if (lock == YES)
           {
# ifdef LOCK_DEBUG
              unlock_region(cdb_fd, 0, __FILE__, __LINE__);
# else                                                                   
              unlock_region(cdb_fd, 0);
# endif
           }
#endif /* THIS_DOES_NOT_WORK */
   }

   return(ret);
}


/*############################ isdup_detach() ###########################*/
void
isdup_detach(void)
{
   if (cdb != NULL)
   {
      unmap_data(cdb_fd, (void *)&cdb);
      cdb_fd = -1;
   }

   return;
}


/*++++++++++++++++++++++++++++++ get_crc() ++++++++++++++++++++++++++++++*/
static unsigned int
get_crc(char         *fullname,
        char         *filename,
        off_t        size,
#if defined (DEBUG_ISDUP) && defined (_MAINTAINER_LOG)
        unsigned int id,
        time_t       timeout,
        int          stay_attached,
        int          lock,
#endif
#ifdef HAVE_HW_CRC32
        int          have_hw_crc32,
#endif
        int          flag)
{
   int          i;
   unsigned int crc = 0;
   char         *ptr;

   if (flag & DC_FILENAME_ONLY)
   {
      char *p_end;

      if (filename == NULL)
      {
         p_end = ptr = fullname + strlen(fullname);
         while ((*ptr != '/') && (ptr != fullname))
         {
            ptr--;
         }
         if (*ptr != '/')
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Unable to find filename in `%s'."), fullname);
            return(0);
         }
         ptr++;
      }
      else
      {
         ptr = filename;
         p_end = filename  + strlen(filename);
      }

      if (flag & DC_CRC32C)
      {
         crc = get_checksum_crc32c(INITIAL_CRC, ptr,
#ifdef HAVE_HW_CRC32
                                   (p_end - ptr), have_hw_crc32);
#else
                                   (p_end - ptr));
#endif
      }
      else if (flag & DC_MURMUR3)
           {
              crc = get_checksum_murmur3(INITIAL_CRC, (unsigned char *)ptr,
                                         (size_t)(p_end - ptr));
           }
           else
           {
              crc = get_checksum(INITIAL_CRC, ptr, (p_end - ptr));
           }
   }
   else if (flag & DC_FILENAME_AND_SIZE)
        {
           char filename_size[MAX_FILENAME_LENGTH + 1 + MAX_OFF_T_LENGTH + 1];

           if (filename == NULL)
           {
              ptr = fullname + strlen(fullname);
              while ((*ptr != '/') && (ptr != fullname))
              {
                 ptr--;
              }
              if (*ptr != '/')
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            _("Unable to find filename in `%s'."), fullname);
                 return(0);
              }
              ptr++;
           }
           else
           {
              ptr = filename;
           }

           i = 0;
           while ((*ptr != '\0') && (i < MAX_FILENAME_LENGTH))
           {
              filename_size[i] = *ptr;
              i++; ptr++;
           }
           filename_size[i++] = ' ';
           (void)memcpy(&filename_size[i], &size, sizeof(off_t));
           if (flag & DC_CRC32C)
           {
              crc = get_checksum_crc32c(INITIAL_CRC, filename_size,
#ifdef HAVE_HW_CRC32
                                        i + sizeof(off_t), have_hw_crc32);
#else
                                        i + sizeof(off_t));
#endif
           }
           else if (flag & DC_MURMUR3)
                {
                   crc = get_checksum_murmur3(INITIAL_CRC,
                                              (unsigned char *)filename_size,
                                              (size_t)(i + sizeof(off_t)));
                }
                else
                {
                   crc = get_checksum(INITIAL_CRC, filename_size,
                                      i + sizeof(off_t));
                }
        }
   else if (flag & DC_NAME_NO_SUFFIX)
        {
           char *p_end;

           if (filename == NULL)
           {
              p_end = ptr = fullname + strlen(fullname);
              while ((*ptr != '.') && (*ptr != '/') && (ptr != fullname))
              {
                 ptr--;
              }
              if (*ptr == '.')
              {
                 p_end = ptr;
              }
              while ((*ptr != '/') && (ptr != fullname))
              {
                 ptr--;
              }
              if (*ptr != '/')
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            _("Unable to find filename in `%s'."), fullname);
                 return(0);
              }
              ptr++;
           }
           else
           {
              p_end = ptr = filename  + strlen(filename);
              while ((*ptr != '.') && (ptr != filename))
              {
                 ptr--;
              }
              if (*ptr == '.')
              {
                 p_end = ptr;
                 ptr = filename;
              }
           }

           if (flag & DC_CRC32C)
           {
              crc = get_checksum_crc32c(INITIAL_CRC, ptr,
#ifdef HAVE_HW_CRC32
                                        (p_end - ptr), have_hw_crc32);
#else
                                        (p_end - ptr));
#endif
           }
           else if (flag & DC_MURMUR3)
                {
                   crc = get_checksum_murmur3(INITIAL_CRC, (unsigned char *)ptr,
                                              (size_t)(p_end - ptr));
                }
                else
                {
                   crc = get_checksum(INITIAL_CRC, ptr, (p_end - ptr));
                }
#if defined (DEBUG_ISDUP) && defined (_MAINTAINER_LOG)
           if (id == DEBUG_ISDUP)
           {
              maintainer_log(INFO_SIGN, __FILE__, __LINE__,
                             "id=%x crc=%x ptr=%s size=%d timeout=%lld flag=%d lock=%d stay_attached=%d",
                             id, crc, ptr, (p_end - ptr), timeout, flag, lock,
                             stay_attached);
           }
#endif
        }
   else if (flag & DC_FILE_CONTENT)
        {
           int  fd,
                ret;
           char buffer[4096];

           if ((fd = open(fullname, O_RDONLY)) == -1)
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         _("Failed to open() `%s' : %s"),
                         fullname, strerror(errno));
              return(0);
           }

           if (flag & DC_CRC32C)
           {
              ret = get_file_checksum_crc32c(fd, buffer, 4096, 0,
#ifdef HAVE_HW_CRC32
                                             &crc, have_hw_crc32);
#else
                                             &crc);
#endif
           }
           else if (flag & DC_MURMUR3)
                {
                   ret = get_file_checksum_murmur3(fd, buffer, 4096, 0, &crc);
                }
                else
                {
                   ret = get_file_checksum(fd, buffer, 4096, 0, &crc);
                }
           if (ret != SUCCESS)
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         _("Failed to determine checksum for `%s'."), fullname);
              (void)close(fd);
              return(0);
           }

           if (close(fd) == -1)
           {
              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                         _("Failed to close() `%s' : %s"),
                         fullname, strerror(errno));
           }
        }
   else if (flag & DC_FILE_CONT_NAME)
        {
           int  fd,
                ret;
           char buffer[4096],
                *p_end;

           if (filename == NULL)
           {
              p_end = ptr = fullname + strlen(fullname);
              while ((*ptr != '/') && (ptr != fullname))
              {
                 ptr--;
              }
              if (*ptr != '/')
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            _("Unable to find filename in `%s'."), fullname);
                 return(0);
              }
              ptr++;
           }
           else
           {
              ptr = filename;
              p_end = filename  + strlen(filename);
           }

           if ((fd = open(fullname, O_RDONLY)) == -1)
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         _("Failed to open() `%s' : %s"),
                         fullname, strerror(errno));
              return(0);
           }
           (void)memcpy(buffer, ptr, (p_end - ptr));

           if (flag & DC_CRC32C)
           {
              ret = get_file_checksum_crc32c(fd, buffer, 4096, (p_end - ptr),
#ifdef HAVE_HW_CRC32
                                             &crc, have_hw_crc32);
#else
                                             &crc);
#endif
           }
           else if (flag & DC_CRC32C)
                {
                   ret = get_file_checksum_murmur3(fd, buffer, 4096,
                                                   (size_t)(p_end - ptr),
                                                   &crc);
                }
                else
                {
                   ret = get_file_checksum(fd, buffer, 4096, (p_end - ptr),
                                           &crc);
                }
           if (ret != SUCCESS)
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         _("Failed to determine checksum for `%s'."),
                         fullname);
              (void)close(fd);
              return(0);
           }

           if (close(fd) == -1)
           {
              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                         _("Failed to close() `%s' : %s"),
                         fullname, strerror(errno));
           }
        }

   return(crc);
}
