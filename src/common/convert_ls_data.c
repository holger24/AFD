/*
 *  convert_ls_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 - 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   convert_ls_data - converts AFD ls data from an old format to a new one
 **
 ** SYNOPSIS
 **   char *convert_ls_data(int           old_rl_fd,
 **                         char          *old_rl_file,
 **                         off_t         *old_rl_size,
 **                         int           old_no_of_listed_files,
 **                         char          *old_rl_ptr,
 **                         unsigned char old_version,
 **                         unsigned char new_version)
 **
 ** DESCRIPTION
 **   When there is a change in AFD ls data structure this function
 **   converts from the old structure to the new one. Currently it
 **   can only convert the versions 0, 1 and 2.
 **
 ** RETURN VALUES
 **   When successful it returns a pointer to the converted structure,
 **   otherwise NULL is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.06.2009 H.Kiehl Created
 **   03.09.2017 H.Kiehl Added version 2.
 **   23.03.2021 H.Kiehl Added version 3.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                /* strcpy(), strerror()               */
#include <stdlib.h>                /* malloc(), free()                   */
#include <sys/types.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif
#include <errno.h>

/* Version 0 */
#define AFD_WORD_OFFSET_0      (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define MAX_FILENAME_LENGTH_0  256
struct retrieve_list_0
       {
          char          file_name[MAX_FILENAME_LENGTH_0];
          char          got_date;
          char          retrieved;
          char          in_list;
          off_t         size;
          time_t        file_mtime;
       };

/* Version 1 */
#define AFD_WORD_OFFSET_1      (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define MAX_FILENAME_LENGTH_1  256
struct retrieve_list_1
       {
          char          file_name[MAX_FILENAME_LENGTH_1];
          unsigned char assigned;        /* New */
          unsigned char special_flag;    /* New */
          char          got_date;
          char          retrieved;
          char          in_list;
          off_t         size;
          time_t        file_mtime;
       };

/* Version 2 */
#define AFD_WORD_OFFSET_2      (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define MAX_FILENAME_LENGTH_2  256
struct retrieve_list_2
       {
          char          file_name[MAX_FILENAME_LENGTH_2];
          unsigned char assigned;
          unsigned char special_flag;
          char          got_date;
          char          retrieved;
          char          in_list;
          off_t         size;
          off_t         prev_size;       /* New */
          time_t        file_mtime;
       };

#ifdef _WITH_EXTRA_CHECK
/* Version 3 */
#define AFD_WORD_OFFSET_3          (SIZEOF_INT + 4 + SIZEOF_INT + 4)
#define MAX_FILENAME_LENGTH_3      256
#define MAX_EXTRA_LS_DATA_LENGTH_3 90                           /* New */
struct retrieve_list_3
       {
          char          file_name[MAX_FILENAME_LENGTH_3];
          char          extra_data[MAX_EXTRA_LS_DATA_LENGTH_3]; /* New */
          unsigned char assigned;
          unsigned char special_flag;
          char          got_date;
          char          retrieved;
          char          in_list;
          off_t         size;
          off_t         prev_size;
          time_t        file_mtime;
       };
#endif


/*########################## convert_ls_data() ##########################*/
char *
convert_ls_data(int           old_rl_fd,
                char          *old_rl_file,
                off_t         *old_rl_size,
                int           old_no_of_listed_files,
                char          *old_rl_ptr,
                unsigned char old_version,
                unsigned char new_version)
{
   char *ptr;

   if ((old_version == 0) && (new_version == 1))
   {
      int                    i;
      size_t                 new_size;
      struct retrieve_list_0 *old_rl;
      struct retrieve_list_1 *new_rl;

      old_rl = (struct retrieve_list_0 *)old_rl_ptr;
      new_size = old_no_of_listed_files * sizeof(struct retrieve_list_1);
      if ((ptr = malloc(new_size)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "malloc() error [%d %d] : %s",
                    old_no_of_listed_files, new_size, strerror(errno));
         ptr = (char *)old_rl;
         ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
         if (munmap(ptr, *old_rl_size) == -1)
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to munmap() %s : %s",
                       old_rl_file, strerror(errno));
         }
         *old_rl_size = -1;
         return(NULL);
      }
      (void)memset(ptr, 0, new_size);
      new_rl = (struct retrieve_list_1 *)ptr;

      /*
       * Copy all the old data into the new region.
       */
      for (i = 0; i < old_no_of_listed_files; i++)
      {
         (void)strcpy(new_rl[i].file_name, old_rl[i].file_name);
         new_rl[i].assigned     = 0;
         new_rl[i].special_flag = 0;
         new_rl[i].got_date     = old_rl[i].got_date;
         new_rl[i].retrieved    = old_rl[i].retrieved;
         new_rl[i].in_list      = old_rl[i].in_list;
         new_rl[i].size         = old_rl[i].size;
         new_rl[i].file_mtime   = old_rl[i].file_mtime;
      }

      ptr = (char *)old_rl;
      ptr -= AFD_WORD_OFFSET_0;

      /*
       * Resize the old retrieve list to the size of new one and then copy
       * the new structure into it. Then update the retrieve list version
       * number.
       */
      if ((ptr = mmap_resize(old_rl_fd, ptr, new_size + AFD_WORD_OFFSET_1)) == (caddr_t) -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to mmap_resize() %s : %s",
                    old_rl_file, strerror(errno));
         free((void *)new_rl);
         return(NULL);
      }

      ptr += AFD_WORD_OFFSET_1;
      (void)memcpy(ptr, new_rl, new_size);
      free((void *)new_rl);
      ptr -= AFD_WORD_OFFSET_1;
      *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
      *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
      *(int *)(ptr + SIZEOF_INT + 4) = 0;            /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
      *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
      *old_rl_size = new_size + AFD_WORD_OFFSET_1;

      system_log(INFO_SIGN, NULL, 0,
                 "Converted retrieve list for %s from version %d to %d.",
                 basename(old_rl_file), (int)old_version, (int)new_version);
   }
   else if ((old_version == 0) && (new_version == 2))
        {
           int                    i;
           size_t                 new_size;
           struct retrieve_list_0 *old_rl;
           struct retrieve_list_2 *new_rl;

           old_rl = (struct retrieve_list_0 *)old_rl_ptr;
           new_size = old_no_of_listed_files * sizeof(struct retrieve_list_2);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_listed_files, new_size, strerror(errno));
              ptr = (char *)old_rl;
              ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
              if (munmap(ptr, *old_rl_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_rl_file, strerror(errno));
              }
              *old_rl_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_rl = (struct retrieve_list_2 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_listed_files; i++)
           {
              (void)strcpy(new_rl[i].file_name, old_rl[i].file_name);
              new_rl[i].assigned     = 0;
              new_rl[i].special_flag = 0;
              new_rl[i].got_date     = old_rl[i].got_date;
              new_rl[i].retrieved    = old_rl[i].retrieved;
              new_rl[i].in_list      = old_rl[i].in_list;
              new_rl[i].size         = old_rl[i].size;
              new_rl[i].prev_size    = 0;
              new_rl[i].file_mtime   = old_rl[i].file_mtime;
           }

           ptr = (char *)old_rl;
           ptr -= AFD_WORD_OFFSET_0;

           /*
            * Resize the old retrieve list to the size of new one and then copy
            * the new structure into it. Then update the retrieve list version
            * number.
            */
           if ((ptr = mmap_resize(old_rl_fd, ptr, new_size + AFD_WORD_OFFSET_2)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_rl_file, strerror(errno));
              free((void *)new_rl);
              return(NULL);
           }

           ptr += AFD_WORD_OFFSET_2;
           (void)memcpy(ptr, new_rl, new_size);
           free((void *)new_rl);
           ptr -= AFD_WORD_OFFSET_2;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           *(int *)(ptr + SIZEOF_INT + 4) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_rl_size = new_size + AFD_WORD_OFFSET_2;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted retrieve list for %s from version %d to %d.",
                      basename(old_rl_file), (int)old_version, (int)new_version);
        }
#ifdef _WITH_EXTRA_CHECK
   else if ((old_version == 0) && (new_version == 3))
        {
           int                    i;
           size_t                 new_size;
           struct retrieve_list_0 *old_rl;
           struct retrieve_list_3 *new_rl;

           old_rl = (struct retrieve_list_0 *)old_rl_ptr;
           new_size = old_no_of_listed_files * sizeof(struct retrieve_list_3);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_listed_files, new_size, strerror(errno));
              ptr = (char *)old_rl;
              ptr -= AFD_WORD_OFFSET_0;
#ifdef HAVE_MMAP
              if (munmap(ptr, *old_rl_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_rl_file, strerror(errno));
              }
              *old_rl_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_rl = (struct retrieve_list_3 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_listed_files; i++)
           {
              (void)strcpy(new_rl[i].file_name, old_rl[i].file_name);
              new_rl[i].extra_data[0] = '\0';
              new_rl[i].assigned      = 0;
              new_rl[i].special_flag  = 0;
              new_rl[i].got_date      = old_rl[i].got_date;
              new_rl[i].retrieved     = old_rl[i].retrieved;
              new_rl[i].in_list       = old_rl[i].in_list;
              new_rl[i].size          = old_rl[i].size;
              new_rl[i].prev_size     = 0;
              new_rl[i].file_mtime    = old_rl[i].file_mtime;
           }

           ptr = (char *)old_rl;
           ptr -= AFD_WORD_OFFSET_0;

           /*
            * Resize the old retrieve list to the size of new one and then copy
            * the new structure into it. Then update the retrieve list version
            * number.
            */
           if ((ptr = mmap_resize(old_rl_fd, ptr, new_size + AFD_WORD_OFFSET_3)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_rl_file, strerror(errno));
              free((void *)new_rl);
              return(NULL);
           }

           ptr += AFD_WORD_OFFSET_3;
           (void)memcpy(ptr, new_rl, new_size);
           free((void *)new_rl);
           ptr -= AFD_WORD_OFFSET_3;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           *(int *)(ptr + SIZEOF_INT + 4) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_rl_size = new_size + AFD_WORD_OFFSET_3;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted retrieve list for %s from version %d to %d.",
                      basename(old_rl_file), (int)old_version, (int)new_version);
        }
#endif
   else if ((old_version == 1) && (new_version == 2))
        {
           int                    i;
           size_t                 new_size;
           struct retrieve_list_1 *old_rl;
           struct retrieve_list_2 *new_rl;

           old_rl = (struct retrieve_list_1 *)old_rl_ptr;
           new_size = old_no_of_listed_files * sizeof(struct retrieve_list_2);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_listed_files, new_size, strerror(errno));
              ptr = (char *)old_rl;
              ptr -= AFD_WORD_OFFSET_1;
#ifdef HAVE_MMAP
              if (munmap(ptr, *old_rl_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_rl_file, strerror(errno));
              }
              *old_rl_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_rl = (struct retrieve_list_2 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_listed_files; i++)
           {
              (void)strcpy(new_rl[i].file_name, old_rl[i].file_name);
              new_rl[i].assigned     = old_rl[i].assigned;
              new_rl[i].special_flag = old_rl[i].special_flag;
              new_rl[i].got_date     = old_rl[i].got_date;
              new_rl[i].retrieved    = old_rl[i].retrieved;
              new_rl[i].in_list      = old_rl[i].in_list;
              new_rl[i].size         = old_rl[i].size;
              new_rl[i].prev_size    = 0;
              new_rl[i].file_mtime   = old_rl[i].file_mtime;
           }

           ptr = (char *)old_rl;
           ptr -= AFD_WORD_OFFSET_1;

           /*
            * Resize the old retrieve list to the size of new one and then copy
            * the new structure into it. Then update the retrieve list version
            * number.
            */
           if ((ptr = mmap_resize(old_rl_fd, ptr, new_size + AFD_WORD_OFFSET_2)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_rl_file, strerror(errno));
              free((void *)new_rl);
              return(NULL);
           }

           ptr += AFD_WORD_OFFSET_2;
           (void)memcpy(ptr, new_rl, new_size);
           free((void *)new_rl);
           ptr -= AFD_WORD_OFFSET_2;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           *(int *)(ptr + SIZEOF_INT + 4) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_rl_size = new_size + AFD_WORD_OFFSET_2;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted retrieve list for %s from version %d to %d.",
                      basename(old_rl_file), (int)old_version, (int)new_version);
        }
#ifdef _WITH_EXTRA_CHECK
   else if ((old_version == 1) && (new_version == 3))
        {
           int                    i;
           size_t                 new_size;
           struct retrieve_list_1 *old_rl;
           struct retrieve_list_3 *new_rl;

           old_rl = (struct retrieve_list_1 *)old_rl_ptr;
           new_size = old_no_of_listed_files * sizeof(struct retrieve_list_3);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_listed_files, new_size, strerror(errno));
              ptr = (char *)old_rl;
              ptr -= AFD_WORD_OFFSET_1;
#ifdef HAVE_MMAP
              if (munmap(ptr, *old_rl_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_rl_file, strerror(errno));
              }
              *old_rl_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_rl = (struct retrieve_list_3 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_listed_files; i++)
           {
              (void)strcpy(new_rl[i].file_name, old_rl[i].file_name);
              new_rl[i].extra_data[0] = '\0';
              new_rl[i].assigned      = old_rl[i].assigned;
              new_rl[i].special_flag  = old_rl[i].special_flag;
              new_rl[i].got_date      = old_rl[i].got_date;
              new_rl[i].retrieved     = old_rl[i].retrieved;
              new_rl[i].in_list       = old_rl[i].in_list;
              new_rl[i].size          = old_rl[i].size;
              new_rl[i].prev_size     = 0;
              new_rl[i].file_mtime    = old_rl[i].file_mtime;
           }

           ptr = (char *)old_rl;
           ptr -= AFD_WORD_OFFSET_1;

           /*
            * Resize the old retrieve list to the size of new one and then copy
            * the new structure into it. Then update the retrieve list version
            * number.
            */
           if ((ptr = mmap_resize(old_rl_fd, ptr, new_size + AFD_WORD_OFFSET_3)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_rl_file, strerror(errno));
              free((void *)new_rl);
              return(NULL);
           }

           ptr += AFD_WORD_OFFSET_3;
           (void)memcpy(ptr, new_rl, new_size);
           free((void *)new_rl);
           ptr -= AFD_WORD_OFFSET_3;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           *(int *)(ptr + SIZEOF_INT + 4) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_rl_size = new_size + AFD_WORD_OFFSET_3;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted retrieve list for %s from version %d to %d.",
                      basename(old_rl_file), (int)old_version, (int)new_version);
        }
   else if ((old_version == 2) && (new_version == 3))
        {
           int                    i;
           size_t                 new_size;
           struct retrieve_list_2 *old_rl;
           struct retrieve_list_3 *new_rl;

           old_rl = (struct retrieve_list_2 *)old_rl_ptr;
           new_size = old_no_of_listed_files * sizeof(struct retrieve_list_3);
           if ((ptr = malloc(new_size)) == NULL)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "malloc() error [%d %d] : %s",
                         old_no_of_listed_files, new_size, strerror(errno));
              ptr = (char *)old_rl;
              ptr -= AFD_WORD_OFFSET_2;
#ifdef HAVE_MMAP
              if (munmap(ptr, *old_rl_size) == -1)
#else
              if (munmap_emu(ptr) == -1)
#endif
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Failed to munmap() %s : %s",
                            old_rl_file, strerror(errno));
              }
              *old_rl_size = -1;
              return(NULL);
           }
           (void)memset(ptr, 0, new_size);
           new_rl = (struct retrieve_list_3 *)ptr;

           /*
            * Copy all the old data into the new region.
            */
           for (i = 0; i < old_no_of_listed_files; i++)
           {
              (void)strcpy(new_rl[i].file_name, old_rl[i].file_name);
              new_rl[i].extra_data[0] = '\0';
              new_rl[i].assigned      = old_rl[i].assigned;
              new_rl[i].special_flag  = old_rl[i].special_flag;
              new_rl[i].got_date      = old_rl[i].got_date;
              new_rl[i].retrieved     = old_rl[i].retrieved;
              new_rl[i].in_list       = old_rl[i].in_list;
              new_rl[i].size          = old_rl[i].size;
              new_rl[i].prev_size     = old_rl[i].prev_size;
              new_rl[i].file_mtime    = old_rl[i].file_mtime;
           }

           ptr = (char *)old_rl;
           ptr -= AFD_WORD_OFFSET_2;

           /*
            * Resize the old retrieve list to the size of new one and then copy
            * the new structure into it. Then update the retrieve list version
            * number.
            */
           if ((ptr = mmap_resize(old_rl_fd, ptr, new_size + AFD_WORD_OFFSET_3)) == (caddr_t) -1)
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to mmap_resize() %s : %s",
                         old_rl_file, strerror(errno));
              free((void *)new_rl);
              return(NULL);
           }

           ptr += AFD_WORD_OFFSET_3;
           (void)memcpy(ptr, new_rl, new_size);
           free((void *)new_rl);
           ptr -= AFD_WORD_OFFSET_3;
           *(ptr + SIZEOF_INT + 1 + 1) = 0;               /* Not used. */
           *(ptr + SIZEOF_INT + 1 + 1 + 1) = new_version;
           *(int *)(ptr + SIZEOF_INT + 4) = 0;            /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT) = 0;      /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 1) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 2) = 0;  /* Not used. */
           *(ptr + SIZEOF_INT + 4 + SIZEOF_INT + 3) = 0;  /* Not used. */
           *old_rl_size = new_size + AFD_WORD_OFFSET_3;

           system_log(INFO_SIGN, NULL, 0,
                      "Converted retrieve list for %s from version %d to %d.",
                      basename(old_rl_file), (int)old_version, (int)new_version);
        }
#endif
        else
        {
           system_log(ERROR_SIGN, NULL, 0,
                      "Don't know how to convert a version %d of AFD ls data type to version %d.",
                      old_version, new_version);
           ptr = NULL;
        }

   return(ptr);
}
