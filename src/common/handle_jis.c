/*
 *  handle_jis.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_jis - set of functions to handle the AFD job id status
 **                database (JIS)
 **
 ** SYNOPSIS
 **   int  attach_jis(void)
 **   int  detach_jis(void)
 **   int  get_current_ip_hl(char **ip_hl)
 **   void add_to_ip_db(char *host_name, char *ip_str)
 **   int  lookup_ip_from_ip_db(char *host_name, char *ip_str, int length)
 **   int  remove_from_ip_db(char *host_name)
 **   void set_store_ip(int val)
 **   int  get_store_ip(void)
 **   int  get_and_reset_store_ip(void)
 **   void print_ip_db(FILE *fp, char *host_name)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.12.2015 H.Kiehl Created
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
#define JIS_BUF_SIZE        10
#define JIS_STEP_SIZE       5
#define CURRENT_JIS_VERSION 0
struct job_id_stat
       {
          double       nbs;             /* Number of bytes send.         */
          time_t       creation_time;   /* Time when job was created.    */
          time_t       usage_time;      /* Last time this job was used.  */
          unsigned int special_flag;    /*+------+----------------------+*/
                                        /*|Bit(s)|      Meaning         |*/
                                        /*+------+----------------------+*/
                                        /*|1 - 32| Not used.            |*/
                                        /*+------+----------------------+*/
          unsigned int nfs;             /* Number of files send.         */
          unsigned int ne;              /* Number of errors.             */
       };

/* Local variables. */
static int                jis_fd = -1,
                          *no_of_entries;
static size_t             jis_size;
static struct job_id_stat *jis = NULL;

/* Local function prototypes. */
static void               check_jis_space(void);
static char               *convert_jis(int, char *, size_t *, char *,
                                       unsigned char, unsigned char);


/*########################### attach_jis() ##############################*/
int
attach_jis(void)
{
   if (jis_fd == -1)
   {
      char fullname[MAX_PATH_LENGTH],
           *ptr;

      jis_size = (JIS_BUF_SIZE * sizeof(struct job_id_stat)) + AFD_WORD_OFFSET;
      (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s",
                     p_work_dir, FIFO_DIR, JIS_FILE);
      if ((ptr = attach_buf(fullname, &jis_fd, &jis_size, NULL,
                            FILE_MODE, NO)) == (caddr_t) -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to mmap() `%s' : %s"), fullname, strerror(errno));
         if (jis_fd != -1)
         {
            (void)close(jis_fd);
            jis_fd = -1;
         }
         return(INCORRECT);
      }
      if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JIS_VERSION)
      {
         if ((ptr = convert_jis(jis_fd, fullname, &jis_size, ptr,
                                  *(ptr + SIZEOF_INT + 1 + 1 + 1),
                                  CURRENT_JIS_VERSION)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to convert JIS file %s!", fullname);
            (void)detach_jis();

            return(INCORRECT);
         }
      }
      no_of_entries = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      jis = (struct job_id_stat *)ptr;
   }

   return(SUCCESS);
}


/*########################### detach_jis() ##############################*/
int
detach_jis(void)
{
   if (jis_fd > 0)
   {
      if (close(jis_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("close() error : %s"), strerror(errno));
      }
      jis_fd = -1;
   }

   if (jis != NULL)
   {
      /* Detach from JIS. */
#ifdef HAVE_MMAP
      if (munmap(((char *)jis - AFD_WORD_OFFSET), jis_size) == -1)
#else
      if (munmap_emu((void *)((char *)jis - AFD_WORD_OFFSET)) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to munmap() from JIS : %s"),
                    strerror(errno));
         return(INCORRECT);
      }
      jis = NULL;
   }

   return(SUCCESS);
}


#ifdef WHEN_WE_KNOW
/*####################### get_current_ip_hl() ###########################*/
int
get_current_ip_hl(char **ip_hl)
{
   int detach,
       ret;

   if (ip_db_fd == -1)
   {
      if (attach_ip_db() == SUCCESS)
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

   if ((*ip_hl = malloc((*no_of_entries * MAX_REAL_HOSTNAME_LENGTH))) != NULL)
   {
      int  i;
      char *ptr = *ip_hl;

#ifdef LOCK_DEBUG
      lock_region_w(ip_db_fd, 1, __FILE__, __LINE__);
#else
      lock_region_w(ip_db_fd, 1);
#endif
      for (i = 0; i < *no_of_entries; i++)
      {
         (void)memcpy(ptr, ipdb[i].host_name, MAX_REAL_HOSTNAME_LENGTH);
         ptr += MAX_REAL_HOSTNAME_LENGTH;
      }
      ret = *no_of_entries;

#ifdef LOCK_DEBUG
      unlock_region(ip_db_fd, 1, __FILE__, __LINE__);
#else
      unlock_region(ip_db_fd, 1);
#endif
   }
   else
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("malloc() error : %s"), strerror(errno));
      ret = INCORRECT;
   }

   if (detach == YES)
   {
      (void)detach_ip_db();
   }

   return(ret);
}


/*########################## add_to_ip_db() #############################*/
void
add_to_ip_db(char *host_name, char *ip_str)
{
   if (store_ip == YES)
   {
      /* Do not add host names that are already IP numbers. */
      if (CHECK_STRCMP(host_name, ip_str) != 0)
      {
         int detach,
             i;

         if (ip_db_fd == -1)
         {
            if (attach_ip_db() == SUCCESS)
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
         lock_region_w(ip_db_fd, 1, __FILE__, __LINE__);
#else
         lock_region_w(ip_db_fd, 1);
#endif
         for (i = 0; i < *no_of_entries; i++)
         {
            if (CHECK_STRCMP(ipdb[i].host_name, host_name) == 0)
            {
               if (CHECK_STRCMP(ipdb[i].ip_str, ip_str) != 0)
               {
                  (void)strcpy(ipdb[i].ip_str, ip_str);
                  ipdb[i].last_mod_time = time(NULL);
               }
#ifdef LOCK_DEBUG
               unlock_region(ip_db_fd, 1, __FILE__, __LINE__);
#else
               unlock_region(ip_db_fd, 1);
#endif
               if (detach == YES)
               {
                  (void)detach_ip_db();
               }
               store_ip = DONE;

               return;
            }
         }

         check_ip_db_space();

         (void)strcpy(ipdb[*no_of_entries].host_name, host_name);
         (void)strcpy(ipdb[*no_of_entries].ip_str, ip_str);
         ipdb[*no_of_entries].last_mod_time = time(NULL);
         (*no_of_entries)++;
#ifdef LOCK_DEBUG
         unlock_region(ip_db_fd, 1, __FILE__, __LINE__);
#else
         unlock_region(ip_db_fd, 1);
#endif

         if (detach == YES)
         {
            (void)detach_ip_db();
         }
      }
      store_ip = DONE;
   }

   return;
}


/*###################### lookup_ip_from_ip_db() #########################*/
int
lookup_ip_from_ip_db(char *host_name, char *ip_str, int length)
{
   int detach,
       i;

   if (ip_db_fd == -1)
   {
      if (attach_ip_db() == SUCCESS)
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
   lock_region_w(ip_db_fd, 1, __FILE__, __LINE__);
#else
   lock_region_w(ip_db_fd, 1);
#endif
   for (i = 0; i < *no_of_entries; i++)
   {
      if (CHECK_STRCMP(ipdb[i].host_name, host_name) == 0)
      {
         (void)my_strncpy(ip_str, ipdb[i].ip_str, length);
#ifdef LOCK_DEBUG
         unlock_region(ip_db_fd, 1, __FILE__, __LINE__);
#else
         unlock_region(ip_db_fd, 1);
#endif
         if (detach == YES)
         {
            (void)detach_ip_db();
         }

         return(SUCCESS);
      }
   }

#ifdef LOCK_DEBUG
   unlock_region(ip_db_fd, 1, __FILE__, __LINE__);
#else
   unlock_region(ip_db_fd, 1);
#endif

   if (detach == YES)
   {
      (void)detach_ip_db();
   }

   return(INCORRECT);
}


/*######################## remove_from_ip_db() ##########################*/
int
remove_from_ip_db(char *host_name)
{
   int detach,
       i;

   if (ip_db_fd == -1)
   {
      if (attach_ip_db() == SUCCESS)
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
   lock_region_w(ip_db_fd, 1, __FILE__, __LINE__);
#else
   lock_region_w(ip_db_fd, 1);
#endif
   for (i = 0; i < *no_of_entries; i++)
   {
      if (CHECK_STRCMP(ipdb[i].host_name, host_name) == 0)
      {
         if (i != (*no_of_entries - 1))
         {
            size_t move_size;

            move_size = (*no_of_entries - 1 - i) * sizeof(struct ip_db);
            (void)memmove(&ipdb[i], &ipdb[i + 1], move_size);
         }
         (*no_of_entries)--;
         check_ip_db_space();
#ifdef LOCK_DEBUG
         unlock_region(ip_db_fd, 1, __FILE__, __LINE__);
#else
         unlock_region(ip_db_fd, 1);
#endif

         if (detach == YES)
         {
            (void)detach_ip_db();
         }
         return(SUCCESS);
      }
   }
#ifdef LOCK_DEBUG
   unlock_region(ip_db_fd, 1, __FILE__, __LINE__);
#else
   unlock_region(ip_db_fd, 1);
#endif

   if (detach == YES)
   {
      (void)detach_ip_db();
   }
   return(INCORRECT);
}


/*########################### print_ip_db() #############################*/
int
print_ip_db(FILE *fp, char *host_name)
{
   int detach,
       i;

   if (ip_db_fd == -1)
   {
      if (attach_ip_db() == SUCCESS)
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

   for (i = 0; i < *no_of_entries; i++)
   {
      if ((host_name == NULL) ||
          (CHECK_STRCMP(ipdb[i].host_name, host_name) == 0))
      {
         (void)fprintf(fp, "%s %s %s",
                       ipdb[i].host_name, ipdb[i].ip_str,
                       ctime(&ipdb[i].last_mod_time));
      }
   }

   if (detach == YES)
   {
      (void)detach_ip_db();
   }

   return(SUCCESS);
}


/*########################### set_store_ip() ############################*/
void
set_store_ip(int val)
{
   store_ip = val;

   return;
}


/*########################### get_store_ip() ############################*/
int
get_store_ip(void)
{
   return(store_ip);
}


/*###################### get_and_reset_store_ip() #######################*/
int
get_and_reset_store_ip(void)
{
   int val = store_ip;

   store_ip = NO;

   return(val);
}
#endif /* WHEN_WE_KNOW */


/*++++++++++++++++++++++++++ check_jis_space() ++++++++++++++++++++++++++*/
static void
check_jis_space(void)
{
   if ((*no_of_entries != 0) &&
       ((*no_of_entries % JIS_STEP_SIZE) == 0))
   {
      char   *ptr;
      size_t new_size;

      new_size = (((*no_of_entries / JIS_STEP_SIZE) + 1) *
                  JIS_STEP_SIZE * sizeof(struct job_id_stat)) + AFD_WORD_OFFSET;
      ptr = (char *)jis - AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(jis_fd, ptr, new_size)) == (caddr_t) -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("mmap_resize() error : %s"), strerror(errno));
         (void)close(jis_fd);
         return;
      }
      no_of_entries = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      jis = (struct job_id_stat *)ptr;
   }
   return;
}


/*+++++++++++++++++++++++++++ convert_ip_db() +++++++++++++++++++++++++++*/
static char *
convert_jis(int           old_jis_fd,
            char          *old_jis_file,
            size_t        *old_jis_size,
            char          *old_jis_ptr,
            unsigned char old_version,
            unsigned char new_version)
{
   char *ptr = old_jis_ptr;

   if ((old_version == 0) && (new_version == 1))
   {
      system_log(INFO_SIGN, NULL, 0, "Code still needs to be written!");
      ptr = NULL;
   }
   else
   {
      system_log(ERROR_SIGN, NULL, 0,
                 "Don't know how to convert a version %d of JIS to version %d.",
                 old_version, new_version);
      ptr = NULL;
   }

   return(ptr);
}
