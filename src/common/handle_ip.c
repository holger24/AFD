/*
 *  handle_ip.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_ip - set of functions to handle the AFD ip database
 **
 ** SYNOPSIS
 **   int  attach_ip_db(void)
 **   int  detach_ip_db(void)
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
 **   02.02.2014 H.Kiehl Created
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
#define CURRENT_IP_DB_VERSION        0
#define IP_DB_BUF_SIZE               10
#define MAX_REAL_HOSTNAME_LENGTH_NEW 65 /* NOTE: Remove this once         */
                                        /*       MAX_REAL_HOSTNAME_LENGTH */
                                        /*       is also 65!              */
#define IPDB_STEP_SIZE               5
struct ip_db
       {
          time_t last_mod_time;
          char   host_name[MAX_REAL_HOSTNAME_LENGTH_NEW];
          char   ip_str[MAX_AFD_INET_ADDRSTRLEN];
       };

/* Local variables. */
static int          ip_db_fd = -1,
                    *no_of_entries,
                    store_ip = NO;
static size_t       ip_db_size;
static struct ip_db *ipdb = NULL;

/* Local function prototypes. */
static void         check_ip_db_space(void);
static char *       convert_ip_db(int, char *, size_t *, char *,
                                  unsigned char, unsigned char);


/*########################## attach_ip_db() #############################*/
int
attach_ip_db(void)
{
   if (ip_db_fd == -1)
   {
      char fullname[MAX_PATH_LENGTH],
           *ptr;

      ip_db_size = (IP_DB_BUF_SIZE * sizeof(struct ip_db)) + AFD_WORD_OFFSET;
      (void)snprintf(fullname, MAX_PATH_LENGTH, "%s%s%s",
                     p_work_dir, FIFO_DIR, IP_DB_FILE);
      if ((ptr = attach_buf(fullname, &ip_db_fd, &ip_db_size, NULL,
                            FILE_MODE, NO)) == (caddr_t) -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to mmap() `%s' : %s"), fullname, strerror(errno));
         if (ip_db_fd != -1)
         {
            (void)close(ip_db_fd);
            ip_db_fd = -1;
         }
         return(INCORRECT);
      }
      if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_IP_DB_VERSION)
      {
         if ((ptr = convert_ip_db(ip_db_fd, fullname, &ip_db_size, ptr,
                                  *(ptr + SIZEOF_INT + 1 + 1 + 1),
                                  CURRENT_IP_DB_VERSION)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to convert IP database file %s!",
                       fullname);
            (void)detach_ip_db();

            return(INCORRECT);
         }
      }
      no_of_entries = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      ipdb = (struct ip_db *)ptr;
   }

   return(SUCCESS);
}


/*########################## detach_ip_db() #############################*/
int
detach_ip_db(void)
{
   if (ip_db_fd > 0)
   {
      if (close(ip_db_fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("close() error : %s"), strerror(errno));
      }
      ip_db_fd = -1;
   }

   if (ipdb != NULL)
   {
      /* Detach from IP database. */
#ifdef HAVE_MMAP
      if (munmap(((char *)ipdb - AFD_WORD_OFFSET), ip_db_size) == -1)
#else
      if (munmap_emu((void *)((char *)ipdb - AFD_WORD_OFFSET)) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to munmap() from IP database : %s"),
                    strerror(errno));
         return(INCORRECT);
      }
      ipdb = NULL;
   }

   return(SUCCESS);
}


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


/*+++++++++++++++++++++++++ check_ip_db_space() +++++++++++++++++++++++++*/
static void
check_ip_db_space(void)
{
   if ((*no_of_entries != 0) &&
       ((*no_of_entries % IPDB_STEP_SIZE) == 0))
   {
      char   *ptr;
      size_t new_size;

      new_size = (((*no_of_entries / IPDB_STEP_SIZE) + 1) *
                  IPDB_STEP_SIZE * sizeof(struct ip_db)) + AFD_WORD_OFFSET;
      ptr = (char *)ipdb - AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(ip_db_fd, ptr, new_size)) == (caddr_t) -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("mmap_resize() error : %s"), strerror(errno));
         (void)close(ip_db_fd);
         return;
      }
      no_of_entries = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      ipdb = (struct ip_db *)ptr;
   }
   return;
}


/*+++++++++++++++++++++++++++ convert_ip_db() +++++++++++++++++++++++++++*/
static char *
convert_ip_db(int           old_ip_db_fd,
              char          *old_ip_db_file,
              size_t        *old_ip_db_size,
              char          *old_ip_db_ptr,
              unsigned char old_version,
              unsigned char new_version)
{
   char *ptr = old_ip_db_ptr;

   if ((old_version == 0) && (new_version == 1))
   {
      system_log(INFO_SIGN, NULL, 0, "Code still needs to be written!");
      ptr = NULL;
   }
   else
   {
      system_log(ERROR_SIGN, NULL, 0,
                 "Don't know how to convert a version %d of IP database to version %d.",
                 old_version, new_version);
      ptr = NULL;
   }

   return(ptr);
}
