/*
 *  get_dir_name.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2018 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_dir_name - determines the directory name from the directory alias
 **
 ** SYNOPSIS
 **   void get_dir_name(char *alias, char *name)
 **
 ** DESCRIPTION
 **   The function get_dir_name() determines the real directory name
 **   from the given alias name.
 **
 ** RETURN VALUES
 **   Returns the directory name in name. Note, on a critical error
 **   it calls exit().
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.05.2018 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                  /* fprintf(), stderr                 */
#include <string.h>                 /* strcpy(), strerror()              */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                 /* close()                           */
#include <stdlib.h>                 /* exit()                            */
#ifdef HAVE_MMAP
# include <sys/mman.h>              /* mmap(), munmap()                  */
#endif
#include <fcntl.h>
#include <errno.h>
#include "statdefs.h"

#ifdef HAVE_MMAP
# ifndef MAP_FILE    /* Required for BSD          */
#  define MAP_FILE 0 /* All others do not need it */
# endif
#endif


/* Local global variables. */
static int                        *no_of_dir_names;
#ifdef HAVE_MMAP
static off_t                      dnb_size;
#endif
static struct dir_name_buf        *dnb = NULL;

/* External global variables. */
extern int                        max_alias_name_length,
                                  no_of_dirs;
extern char                       *p_work_dir;
extern struct fileretrieve_status *fra;


/*############################ get_dir_name() ###########################*/
void
get_dir_name(char *alias, char *name)
{
   int i, j;

   if (dnb == NULL)
   {
      int          fd;
      char         file[MAX_PATH_LENGTH],
                   *ptr;
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat  stat_buf;
#endif

      (void)sprintf(file, "%s%s%s", p_work_dir, FIFO_DIR, DIR_NAME_FILE);
      if ((fd = open(file, O_RDONLY)) == -1)
      {
         (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         (void)fprintf(stderr, _("Failed to access `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
      dnb_size = stat_buf.stx_size;
# else
      dnb_size = stat_buf.st_size;
# endif

      if ((ptr = mmap(NULL, dnb_size, PROT_READ,
                      MAP_SHARED, fd, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                          stat_buf.stx_size, PROT_READ,
# else
                          stat_buf.st_size, PROT_READ,
# endif
                          MAP_SHARED, file, 0)) == (caddr_t) -1)
#endif
      {
         (void)fprintf(stderr, _("Failed to mmap() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (close(fd) == -1)
      {
         (void)fprintf(stderr, _("Failed to close() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
      }
      no_of_dir_names = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)ptr;
   }
   if (fra == NULL)
   {
      (void)fra_attach_passive();
   }

   for (i = 0; i < no_of_dirs; i++)
   {
      if (strcmp(fra[i].dir_alias, alias) == 0)
      {
         for (j = 0; j < *no_of_dir_names; j++)
         {
            if (dnb[j].dir_id == fra[i].dir_id)
            {
               (void)strcpy(name, dnb[j].dir_name);
               return;
            }
         }
      }
   }

   /* It does not bring much to show an error, just use original alias. */
   (void)strcpy(name, alias);

   return;
}


/*######################## get_max_name_length() ########################*/
void
get_max_name_length(void)
{
   int j,
       length,
       max_length = 0;

   if (dnb == NULL)
   {
      int          fd;
      char         file[MAX_PATH_LENGTH],
                   *ptr;
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat  stat_buf;
#endif

      (void)sprintf(file, "%s%s%s", p_work_dir, FIFO_DIR, DIR_NAME_FILE);
      if ((fd = open(file, O_RDONLY)) == -1)
      {
         (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         (void)fprintf(stderr, _("Failed to access `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
      dnb_size = stat_buf.stx_size;
# else
      dnb_size = stat_buf.st_size;
# endif

      if ((ptr = mmap(NULL, dnb_size, PROT_READ,
                      MAP_SHARED, fd, 0)) == (caddr_t) -1)
#else
      if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                          stat_buf.stx_size, PROT_READ,
# else
                          stat_buf.st_size, PROT_READ,
# endif
                          MAP_SHARED, file, 0)) == (caddr_t) -1)
#endif
      {
         (void)fprintf(stderr, _("Failed to mmap() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      if (close(fd) == -1)
      {
         (void)fprintf(stderr, _("Failed to close() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
      }
      no_of_dir_names = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)ptr;
   }

   /* Determine longest dir_name. */
   for (j = 0; j < *no_of_dir_names; j++)
   {
      length = strlen(dnb[j].dir_name);
      if (length > max_length)
      {
         max_length = length;
      }
   }
   if (max_length > max_alias_name_length)
   {
      max_alias_name_length = max_length;
   }

   return;
}


/*######################### free_get_dir_name() #########################*/
void
free_get_dir_name(void)
{
   if (dnb != NULL)
   {
#ifdef HAVE_MMAP
      if (munmap(((char *)dnb - AFD_WORD_OFFSET), dnb_size) < 0)
#else
      if (munmap_emu(((char *)dnb - AFD_WORD_OFFSET)) == -1)
#endif
      {  
         (void)fprintf(stderr, "munmap() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
      }
      else             
      {     
         dnb = NULL;
      }
      fra_detach();
   }

   return;
}
