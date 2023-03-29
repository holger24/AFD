/*
 *  eval_dir_config.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2023 Deutscher Wetterdienst (DWD),
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
 **   eval_dir_config - reads the DIR_CONFIG file and evaluates it
 **
 ** SYNOPSIS
 **   int eval_dir_config(off_t        db_size,
 **                       unsigend int *warn_counter,
 **                       FILE         *debug_fp,
 **                       int          *using_groups)
 **
 ** DESCRIPTION
 **   The function eval_dir_config() reads the DIR_CONFIG file of the
 **   AMG and writes the contents in a compact form to a shared memory
 **   region. This region is then used by other process (eg. dir_check)
 **   to create jobs (messages) for the FD.
 **
 **   An DIR_CONFIG entry consists of the following sections:
 **   directory, files, destination, recipient and options.
 **   The directory entry always marks the beginning of a new section
 **   and contains one user directory. In the files group we specify
 **   which files are to be send from this directory. It can have
 **   more then one entry and wild cards are allowed too. The
 **   destination group contains the two sub groups recipient and
 **   options. Each file group may contain more then one destination
 **   group. In the recipient group is the address of the recipient
 **   for these files with these specific options. The recipient
 **   group may contain more then one recipient. In the option
 **   group all options for this recipient are specified. Here follows
 **   an example entry:
 **
 **   [directory]
 **   /home/afd/data
 **
 **          [files]
 **          a*
 **          file?
 **
 **                  [destination]
 **
 **                          [recipient]
 **                          ftp://user:passwd@hostname:22/dir1/dir2
 **                          mail://user@hostname3
 **
 **                          [options]
 **                          archive 3
 **                          lock DOT
 **                          priority 0
 **
 **   This entry would instruct the AFD to send all files starting with
 **   an a and files starting with file and one more character from
 **   the directory /home/afd/data to two remote hosts (hostname1 and
 **   hostname2) with a priority of 0. All files that have been
 **   transmitted will be archived for three days.
 **
 ** RETURN VALUES
 **   Returns NO_VALID_ENTRIES when it fails to find any valid entries
 **   in any database file (DIR_CONFIG's). Otherwise SUCCESS is returned,
 **   or SUCCESS_WITH_GROUPS when groups are being used.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.05.1995 H.Kiehl Created
 **   15.12.1996 H.Kiehl Changed to URL format
 **   05.06.1997 H.Kiehl Memory for struct dir_group is now allocated with
 **                      malloc().
 **   21.08.1997 H.Kiehl Added support for real hostname, so that the
 **                      message will only contain an alias name.
 **   25.08.1997 H.Kiehl When trying to lock the FSA_ID_FILE, we will
 **                      block until the lock is released.
 **   08.11.1997 H.Kiehl Try create source directory recursively if it does
 **                      not exist.
 **   19.11.1997 H.Kiehl Return of the HOST_CONFIG file!
 **   21.09.1999 H.Kiehl Make it possible to specify ~username as part
 **                      of directory.
 **   14.02.2000 H.Kiehl Additional shared memory area for remote
 **                      directories.
 **   26.03.2000 H.Kiehl Addition of new header field DIR_OPTION_IDENTIFIER.
 **   16.05.2002 H.Kiehl Removed shared memory stuff.
 **   16.03.2003 H.Kiehl Support for reading multiple DIR_CONFIG files.
 **   03.06.2003 H.Kiehl Directories may now also contain spaces.
 **   05.01.2004 H.Kiehl Remember which job belongs to which DIR_CONFIG.
 **   09.04.2004 H.Kiehl Support for TSL/SSL.
 **   28.05.2005 H.Kiehl Added support for mbox.
 **   08.06.2005 H.Kiehl When searching for next file group we did not
 **                      check if this was commented out.
 **   24.06.2006 H.Kiehl Allow for duplicate directories when the
 **                      directories are in different DIR_CONFIGS.
 **   03.05.2007 H.Kiehl Return the number of warnings that where generated.
 **   17.05.2007 H.Kiehl Added check for options.
 **   20.04.2008 H.Kiehl Let url_evaluate() handle the URL parts once.
 **   17.01.2010 H.Kiehl Give url_evaluate() dummy variables for values we
 **                      do not need, to fool it to do syntax checking.
 **   10.04.2017 H.Kiehl Added debug_fp parameter to print warnings and
 **                      errors.
 **   12.02.2023 H.Kiehl Variable group_name was used by init_dir_group_name()
 **                      init_recipient_group_name() thus leaking memory.
 **                      Each need their own variable.
 **
 */
DESCR__E_M3

#include <stdio.h>                  /* fprintf()                         */
#include <string.h>                 /* strlen(), strncmp(), strcpy()     */
                                    /* memmove(), strerror()             */
#include <stdlib.h>                 /* calloc(), realloc(), free(),      */
                                    /* atoi(), malloc()                  */
#include <ctype.h>                  /* isdigit()                         */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>              /* mmap(), munmap()                  */
#endif
#include <pwd.h>                    /* getpwuid(), getpwnam()            */
#include <unistd.h>                 /* R_OK, W_OK, X_OK                  */
#include <errno.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"

#define USE_INOTIFY_FOR_REMOTE_DIRS

/* External global variables. */
#ifdef _DEBUG
extern FILE                   *p_debug_file;
#endif
extern char                   *alfiles,          /* Additional locked files */
                              *p_work_dir;
extern int                    alfbl,/* Additional locked file buffer length */
                              alfc, /* Additional locked file counter       */
                              create_source_dir,
                              create_source_dir_disabled,
                              default_no_parallel_jobs,
                              default_max_errors,
                              default_retry_interval,
                              default_successful_retries,
                              default_transfer_blocksize,
                              dnb_fd,
                              data_length,/* The size of data for one job.  */
                              *no_of_dir_names,
                              no_of_dir_configs,
                              no_of_hosts,/* The number of remote hosts to  */
                                          /* which files have to be         */
                                          /* transfered.                    */
                              remove_unused_hosts;
extern unsigned int           default_error_offline_flag;
extern long                   default_transfer_timeout;
extern mode_t                 create_source_dir_mode;
extern struct host_list       *hl;        /* Structure that holds all the   */
                                          /* hosts.                         */
extern struct dir_name_buf    *dnb;
extern struct dir_config_buf  *dc_dcl;
#ifdef WITH_ONETIME
extern struct dir_config_buf  *ot_dcl;
#endif

/* Global variables. */
int                           no_of_local_dirs,
                              *no_of_passwd,
                              no_of_rule_headers,
                              pwb_fd,
                              job_no;   /* By job number we here mean for   */
                                        /* each destination specified!      */
struct rule                   *rule;
struct dir_data               *dd = NULL;
struct p_array                *pp;
struct passwd_buf             *pwb = NULL;
static char                   *p_t = NULL;   /* Start of directory table.   */

/* Local variables. */
static off_t                  data_alloc_size;

/* Local function prototypes. */
static int                    check_hostname_list(char *, char *, char *,
                                                  unsigned int, unsigned int),
#ifdef MBOX_DIR
                              create_mbox_dir(char *, char *),
#endif
                              count_new_lines(char *, char *),
                              optimise_dir(char *);
static void                   copy_job(int, int, struct dir_group *),
#ifdef WITH_ONETIME
                              copy_to_file(int),
#else
                              copy_to_file(void),
#endif
                              expand_file_filter(struct dir_group *, int *,
                                                 char *, char *, char *,
                                                 unsigned int *, FILE *),
                              insert_dir(struct dir_group *),
                              insert_hostname(struct dir_group *),
                              sort_jobs(void);
static char                   *posi_identifier(char *, char *, size_t);

/* #define _LINE_COUNTER_TEST */

/* The following macro checks for spaces and comments. */
#define CHECK_SPACE()                                       \
        {                                                   \
           if ((*ptr == ' ') || (*ptr == '\t'))             \
           {                                                \
              tmp_ptr = ptr;                                \
              while ((*tmp_ptr == ' ') || (*tmp_ptr == '\t')) \
              {                                             \
                 tmp_ptr++;                                 \
              }                                             \
              switch (*tmp_ptr)                             \
              {                                             \
                 case '#' :  /* Found comment. */           \
                    while ((*tmp_ptr != '\n') && (*tmp_ptr != '\0')) \
                    {                                       \
                       tmp_ptr++;                           \
                    }                                       \
                    ptr = tmp_ptr;                          \
                    continue;                               \
                 case '\0':  /* Found end for this entry. */\
                    ptr = tmp_ptr;                          \
                    continue;                               \
                 case '\n':  /* End of line reached. */     \
                    ptr = tmp_ptr;                          \
                    continue;                               \
                 default  :  /* Option goes on. */          \
                    if (i == 0)                             \
                    {                                       \
                       ptr = tmp_ptr;                       \
                    }                                       \
                    break;                                  \
              }                                             \
           }                                                \
           else if (*ptr == '#')                            \
                {                                           \
                   tmp_ptr = ptr;                           \
                   while ((*tmp_ptr != '\n') && (*tmp_ptr != '\0')) \
                   {                                        \
                      tmp_ptr++;                            \
                   }                                        \
                   ptr = tmp_ptr;                           \
                   continue;                                \
                }                                           \
        }


/*########################## eval_dir_config() ##########################*/
int
eval_dir_config(off_t        db_size,
                unsigned int *warn_counter,
                FILE         *debug_fp,
#ifdef WITH_ONETIME
                int          onetime
#endif
                int          *using_groups)
{
   unsigned int          error_mask;
   int                   dcd = 0,             /* DIR_CONFIG's done.       */
                         dir_group_type = NEITHER, /* Groups can either   */
                                              /* be of type file or       */
                                              /* directory. Note, it is   */
                                              /* initialized to NEITHER   */
                                              /* to silence the compiler. */
                         i,
                         j,
#ifdef _DEBUG
                         k,
                         m,
#endif
                         dummy_port,          /* Is actually not required */
                                              /* for url_evaluate(). But  */
                                              /* we supply it so we fool  */
                                              /* the function to do syntax*/
                                              /* checking on the URL.     */
#ifdef HAVE_HW_CRC32
                         have_hw_crc32,
#endif
                         ret,                 /* Return value.            */
                         t_dgc = 0,           /* Total number of          */
                                              /* destination groups found.*/
                         t_rc = 0,            /* Total number of          */
                                              /* recipients found.        */
                         unique_file_counter, /* Counter for creating a   */
                                              /* unique file name.        */
                         unique_dest_counter; /* Counter for creating a   */
                                              /* unique destination name. */
   uid_t                 current_uid;
   char                  *database = NULL,
                         *dir_group_loop_ptr,
                         *dir_ptr,
                         dummy_transfer_mode,
#ifdef WITH_SSH_FINGERPRINT
                         dummy_ssh_fingerprint[MAX_FINGERPRINT_LENGTH + 1],
                         dummy_key_type,
#endif
                         *error_ptr,          /* Pointer showing where we */
                                              /* fail to see that the     */
                                              /* directory is available   */
                                              /* for us.                  */
                         *ptr,                /* Main pointer that walks  */
                                              /* through buffer.          */
                         *tmp_ptr = NULL,     /* */
                         *search_ptr = NULL,  /* Pointer used in          */
                                              /* conjunction with the     */
                                              /* posi() function.         */
                         tmp_dir_char = 0,    /* Buffer that holds torn   */
                                              /* out character.           */
                         *end_dir_ptr = NULL, /* Pointer that marks the   */
                                              /* end of a directory entry.*/
                         other_dir_flag,      /* If set, there is another */
                                              /* directory entry.         */
                         tmp_file_char = 1,   /* Buffer that holds torn   */
                                              /* out character.           */
                         *end_file_ptr = NULL,/* Pointer that marks the   */
                                              /* end of a file entry.     */
                         other_file_flag,     /* If set, there is another */
                                              /* file entry.              */
                         tmp_dest_char = 1,   /* Buffer that holds torn   */
                                              /* out character.           */
                         *end_dest_ptr = NULL,/* Pointer that marks the   */
                                              /* end of a destination     */
                                              /* entry.                   */
                         other_dest_flag,     /* If set, there is another */
                                              /* destination entry.       */
                         created_path[MAX_PATH_LENGTH],
                         dir_user[MAX_USER_NAME_LENGTH],
                         dir_group_name[MAX_GROUPNAME_LENGTH],
                         recipient_group_name[MAX_GROUPNAME_LENGTH],
                         prev_user_name[MAX_USER_NAME_LENGTH],
                         prev_user_dir[MAX_PATH_LENGTH],
                         user[MAX_USER_NAME_LENGTH],
                         smtp_user[MAX_USER_NAME_LENGTH],
                         password[MAX_USER_NAME_LENGTH],
                         directory[MAX_RECIPIENT_LENGTH],
                         dummy_directory[MAX_RECIPIENT_LENGTH],
                         dummy_region[MAX_REAL_HOSTNAME_LENGTH],
                         smtp_server[MAX_REAL_HOSTNAME_LENGTH];
   unsigned char         dummy_auth,
                         dummy_service,
                         dummy_ssh_protocol,
                         smtp_auth;
   struct dir_group      *dir;
   struct dir_config_buf *dcl;

   /* Allocate memory for the directory structure. */
   if ((dir = malloc(sizeof(struct dir_group))) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      exit(INCORRECT);
   }
   else
   {
#ifdef HAVE_STATX
      struct statx stat_buf;

      if (statx(0, p_work_dir, AT_STATX_SYNC_AS_STAT,
                STATX_UID, &stat_buf) == -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to statx() `%s' : %s", p_work_dir, strerror(errno));
         exit(INCORRECT);
      }
      current_uid = stat_buf.stx_uid;
#else
      struct stat stat_buf;

      if (stat(p_work_dir, &stat_buf) == -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to stat() `%s' : %s", p_work_dir, strerror(errno));
         exit(INCORRECT);
      }
      current_uid = stat_buf.st_uid;
#endif
   }
   dir->file = NULL;
   prev_user_name[0] = '\0';

   /* Create temporal storage area for job. */
   if ((p_t = calloc(db_size, sizeof(char))) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not allocate memory : %s", strerror(errno));
      free((void *)dir);
      exit(INCORRECT);
   }
   data_alloc_size = db_size;

#ifdef WITH_ONETIME
   if (onetime == NO)
   {
#else
      dcl = dc_dcl;
#endif
      if (dnb == NULL)
      {
         size_t size = (DIR_NAME_BUF_SIZE * sizeof(struct dir_name_buf)) +
                       AFD_WORD_OFFSET;
         char   dir_name_file[MAX_PATH_LENGTH],
                *p_dir_buf;

         /*
          * Map to the directory name database.
          */
         (void)strcpy(dir_name_file, p_work_dir);
         (void)strcat(dir_name_file, FIFO_DIR);
         (void)strcat(dir_name_file, DIR_NAME_FILE);
         if ((p_dir_buf = attach_buf(dir_name_file, &dnb_fd,
                                     &size, "AMG", FILE_MODE, NO)) == (caddr_t) -1)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to mmap() to %s : %s",
                       dir_name_file, strerror(errno));
            exit(INCORRECT);
         }
         no_of_dir_names = (int *)p_dir_buf;
         p_dir_buf += AFD_WORD_OFFSET;
         dnb = (struct dir_name_buf *)p_dir_buf;
      } /* if (dnb == NULL) */

      /* Lock the dir_name_buf structure so we do not get caught */
      /* when the FD is removing a directory.                    */
#ifdef LOCK_DEBUG
      lock_region_w(dnb_fd, (off_t)1, __FILE__, __LINE__);
#else
      lock_region_w(dnb_fd, (off_t)1);
#endif
#ifdef WITH_ONETIME
   }
   else
   {
      dcl = ot_dcl;
   }
#endif 
#ifdef HAVE_HW_CRC32
   have_hw_crc32 = detect_cpu_crc32();
#endif

   /* Initialise variables. */
   pp = NULL;
   job_no = 0;
   data_length = 0;
   unique_file_counter = 0;
   unique_dest_counter = 0;
   no_of_local_dirs = 0;

   /* Evaluate each DIR_CONFIG each by itself. */
   do
   {
#ifdef WITH_ONETIME
      if (onetime == NO)
      {
#endif
         system_log(DEBUG_SIGN, NULL, 0, "Reading %s",
                    dcl[dcd].dir_config_file);
#ifdef WITH_ONETIME
      }
      else
      {
         receive_log(DEBUG_SIGN, NULL, 0, 0L,
                     "Reading %s", dcl[dcd].dir_config_file);
      }
#endif

      /* Read database file and store it into memory. */
      if ((read_file_no_cr(dcl[dcd].dir_config_file, &database, YES, __FILE__, __LINE__) == INCORRECT) ||
          (database == NULL) || (database[0] == '\0'))
      {
         if (database == NULL)
         {
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          "Configuration file `%s' could not be read.",
                          dcl[dcd].dir_config_file);
         }
         else if (database[0] == '\0')
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                               "Configuration file `%s' is empty.",
                               dcl[dcd].dir_config_file);
              }
         dcd++;
         continue;
      }
      ptr = database;

      /* Read each directory entry one by one until we reach the end. */
      while ((search_ptr = posi_identifier(ptr, DIR_IDENTIFIER,
                                           DIR_IDENTIFIER_LENGTH)) != NULL)
      {
#ifdef _LINE_COUNTER_TEST
         system_log(INFO_SIGN, NULL, 0, "Found DIR_IDENTIFIER at line %d in %s",
                    count_new_lines(database, search_ptr),
                    dcl[dcd].dir_config_file);
#endif
         /* Initialise directory structure. */
         (void)memset((void *)dir, 0, sizeof(struct dir_group));

         /* Check if an alias is specified for this directory. */
         i = 0;
         if (*(search_ptr - 1) != '\n')
         {
            /* Ignore any data directly behind the identifier. */
            while ((*search_ptr != '\n') && (*search_ptr != '\0'))
            {
               if (*search_ptr == '#')
               {
                  while ((*search_ptr != '\n') && (*search_ptr != '\0'))
                  {
                     search_ptr++;
                  }
               }
               else if ((*search_ptr == ' ') || (*search_ptr == '\t'))
                    {
                       search_ptr++;
                    }
                    else
                    {
                       if (*search_ptr == '/')
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                        "In %s line %d, directory alias name has a / which is not permitted.",
                                        dcl[dcd].dir_config_file,
                                        count_new_lines(database, search_ptr));
                          i = 0;
                          while ((*search_ptr != '\n') && (*search_ptr != '\0'))
                          {
                             search_ptr++;
                          }
                       }
                       else
                       {
                          dir->alias[i] = *search_ptr;
                          search_ptr++; i++;
                          if (i == MAX_DIR_ALIAS_LENGTH)
                          {
                             /* No more space left, so ignore rest. */
                             while ((*search_ptr != '\n') &&
                                    (*search_ptr != '\0'))
                             {
                                search_ptr++;
                             }
                          }
                       }
                    }
            }
            dir->alias[i] = '\0';
            if (*search_ptr == '\n')
            {
               search_ptr++;
            }
         }
         ptr = search_ptr;

         /*
          ********************* Read directory ********************
          */
         /* Check if there is a directory. */
         if (*ptr == '\n')
         {
            /* Generate warning, that last dir entry does not */
            /* have a destination.                            */
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          "In %s line %d, directory entry does not have a directory.",
                          dcl[dcd].dir_config_file,
                          count_new_lines(database, search_ptr));
            ptr++;

            continue;
         }

         /* Check comment line. */
         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         while (*ptr == '#')
         {
            while ((*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
            if (*ptr == '\n')
            {
               ptr++;
               while ((*ptr == ' ') || (*ptr == '\t'))
               {
                  ptr++;
               }
            }
         }

         /* Store directory name. */
         i = 0;
         dir_group_name[0] = '\0';
         while ((*ptr != '\n') && (*ptr != '\0') && (i < (MAX_PATH_LENGTH - 2)))
         {
            if ((*ptr == '\\') && ((*(ptr + 1) == '#') ||
                (*(ptr + 1) == GROUP_SIGN) || (*(ptr + 1) == ' ') ||
                (*(ptr + 1) == '\t')))
            {
               dir->location[i] = *(ptr + 1);
               i++;
               ptr += 2;
            }
            else
            {
               if (*ptr == '#')
               {
                  while ((*ptr != '\n') && (*ptr != '\0'))
                  {
                     ptr++;
                  }

                  /* Lets remove all spaces behind the directory name. */
                  while ((i > 0) && ((dir->location[i - 1] == ' ') ||
                                     (dir->location[i - 1] == '\t')))
                  {
                     i--;
                  }
               }
               else if ((*ptr == GROUP_SIGN) &&
                        ((*(ptr + 1) == CURLY_BRACKET_OPEN) ||
                         (*(ptr + 1) == SQUARE_BRACKET_OPEN)))
                    {
                       char close_bracket;

                       if (*(ptr + 1) == CURLY_BRACKET_OPEN)
                       {
                          dir_group_type = YES;
                          close_bracket = CURLY_BRACKET_CLOSE;
                       }
                       else
                       {
                          dir_group_type = NO;
                          close_bracket = SQUARE_BRACKET_CLOSE;
                       }
                       dir->location[i] = *ptr;
                       dir->location[i + 1] = *(ptr + 1);
                       ptr += 2;
                       i += 2;
                       j = 0;
                       while ((*ptr != close_bracket) &&
                              (*ptr != '\n') && (*ptr != '\0'))
                       {
                          dir_group_name[j] = *ptr;
                          dir->location[i] = *ptr;
                          j++; ptr++; i++;
                       }
                       if (*ptr == close_bracket)
                       {
                          dir_group_name[j] = '\0';
                          dir->location[i] = *ptr;
                          ptr++; i++;
                          *using_groups = YES;
                       }
                       else
                       {
                          /* No end marker, so just take it as a normal */
                          /* directory entry.                           */
                          dir_group_name[0] = '\0';
                       }
                    }
                    else
                    {
                       dir->location[i] = *ptr;
                       i++; ptr++;
                    }
            }
         }
         if ((*ptr == '\n') && (i > 0))
         {
            ptr++;
         }
         else if (i >= (MAX_PATH_LENGTH - 2))
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                               "In `%s' line %d, directory entry longer then %d, unable to store it.",
                               dcl[dcd].dir_config_file,
                               count_new_lines(database, search_ptr),
                               (MAX_PATH_LENGTH - 2));
                 continue;
              }
         dir->location[i] = '\0';
         dir->location_length = i;

         dir_group_loop_ptr = ptr;
         if (dir_group_name[0] != '\0')
         {
            init_dir_group_name(dir->location, &dir->location_length,
                                dir_group_name, dir_group_type);
         }

         do
         {
            /* For each directory group entry reset ptr. */
            ptr = dir_group_loop_ptr;

            /* Lets resolve any tilde signs ~. */
            if (dir->location[0] == '~')
            {
               char tmp_char,
                    tmp_location[MAX_PATH_LENGTH];

               (void)memcpy(dir->orig_dir_name, dir->location,
                            dir->location_length + 1);
               tmp_ptr = dir->location;
               while ((*tmp_ptr != '/') && (*tmp_ptr != '\n') &&
                      (*tmp_ptr != '\0') && (*tmp_ptr != ' ') &&
                      (*tmp_ptr != '\t'))
               {
                  tmp_ptr++;
               }
               tmp_char = *tmp_ptr;
               *tmp_ptr = '\0';
               if ((prev_user_name[0] == '\0') ||
                   (CHECK_STRCMP(dir->location, prev_user_name) != 0))
               {
                  char          *p_end;
                  struct passwd *pwd;

                  if (*(tmp_ptr - 1) == '~')
                  {
                     if ((pwd = getpwuid(current_uid)) == NULL)
                     {
                        update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                      "Cannot find working directory for user with the user ID %d in /etc/passwd (ignoring directory from %s) : %s",
                                      current_uid, dcl[dcd].dir_config_file,
                                      strerror(errno));
                        *tmp_ptr = tmp_char;
                        continue;
                     }
                  }
                  else
                  {
                     if ((pwd = getpwnam(&dir->location[1])) == NULL)
                     {
                        update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                      "Cannot find users %s working directory in /etc/passwd (ignoring directory from %s) : %s",
                                      &dir->location[1], dcl[dcd].dir_config_file,
                                      strerror(errno));
                        *tmp_ptr = tmp_char;
                        continue;
                     }
                  }
                  (void)strcpy(prev_user_name, dir->location);
                  (void)strcpy(prev_user_dir, pwd->pw_dir);

                  /*
                   * Cut away /./ at end of user directory. This information
                   * is used by some FTP-servers so they chroot to this
                   * directory. We don't need that here.
                   */
                  p_end = prev_user_dir + strlen(prev_user_dir) - 1;
                  while ((p_end > prev_user_dir) &&
                         ((*p_end == '/') || (*p_end == '.')))
                  {
                     *p_end = '\0';
                     p_end--;
                  }
               }
               *tmp_ptr = tmp_char;
               (void)strcpy(tmp_location, prev_user_dir);
               if (*tmp_ptr == '/')
               {
                  (void)strcat(tmp_location, tmp_ptr);
               }
               (void)strcpy(dir->location, tmp_location);
               dir->location_length = optimise_dir(dir->location);
               dir->protocol = LOC;
            }
            else if (dir->location[0] == '/')
                 {
                    (void)memcpy(dir->orig_dir_name, dir->location,
                                 dir->location_length + 1);
                    dir->location_length = optimise_dir(dir->location);
                    dir->type = LOCALE_DIR;
                    dir->protocol = LOC;
                 }
            /* Assume it is url format. */
            else if ((error_mask = url_evaluate(dir->location, &dir->scheme,
                                                dir_user, &smtp_auth, smtp_user,
#ifdef WITH_SSH_FINGERPRINT
                                                dummy_ssh_fingerprint,
                                                &dummy_key_type,
#endif
#ifdef WITH_PASSWD_IN_MSG
                                                password, NO,
#else
                                                password, YES,
#endif
                                                dir->real_hostname,
                                                &dummy_port, directory, NULL,
                                                NULL, &dummy_transfer_mode,
                                                &dummy_ssh_protocol,
                                                &dummy_auth,
                                                dummy_region,
                                                &dummy_service, NULL)) < 4)
                 {
                    if (dir->scheme & FTP_FLAG)
                    {
                       dir->type = REMOTE_DIR;
                       dir->protocol = FTP;
                       if (password[0] != '\0')
                       {
                          store_passwd(dir_user, dir->real_hostname, password);
                       }
                       t_hostname(dir->real_hostname, dir->host_alias);
                       (void)strcpy(dir->url, dir->location);
                       (void)strcpy(dir->orig_dir_name, dir->url);
                    }
                    else if (dir->scheme & LOC_FLAG)
                         {
                            (void)memcpy(dir->orig_dir_name, dir->location,
                                         dir->location_length + 1);
                            dir->type = LOCALE_DIR;
                            dir->protocol = LOC;
                            if ((dir->real_hostname[0] != '\0') &&
                                (dir->alias[0] == '\0'))
                            {
                               (void)my_strncpy(dir->alias, dir->real_hostname,
                                                MAX_DIR_ALIAS_LENGTH + 1);
                            }
                            if (directory[0] != '/')
                            {
                               if ((prev_user_name[0] == '\0') ||
                                   (CHECK_STRCMP(dir_user, prev_user_name) != 0))
                               {
                                  char          *p_end;
                                  struct passwd *pwd;

                                  if (dir_user[0] == '\0')
                                  {
                                     if ((pwd = getpwuid(current_uid)) == NULL)
                                     {
                                        update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                                      "Cannot find working directory for user with the user ID %d in /etc/passwd (ignoring directory from %s) : %s",
                                                      current_uid, dcl[dcd].dir_config_file,
                                                      strerror(errno));
                                        continue;
                                     }
                                  }
                                  else
                                  {
                                     if ((pwd = getpwnam(dir_user)) == NULL)
                                     {
                                        update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                                   "Cannot find users %s working directory in /etc/passwd (ignoring directory from %s) : %s",
                                                   dir_user, dcl[dcd].dir_config_file,
                                                   strerror(errno));
                                        continue;
                                     }
                                  }
                                  (void)strcpy(prev_user_name, dir_user);
                                  (void)strcpy(prev_user_dir, pwd->pw_dir);

                                  /*
                                   * Cut away /./ at end of user directory. This
                                   * information * is used by some FTP-servers so
                                   * they chroot to this * directory. We don't
                                   * need that here.
                                   */
                                  p_end = prev_user_dir + strlen(prev_user_dir) - 1;
                                  while ((p_end > prev_user_dir) &&
                                         ((*p_end == '/') || (*p_end == '.')))
                                  {
                                     *p_end = '\0';
                                     p_end--;
                                  }
                               }
                            }
                            (void)memcpy(dir->orig_dir_name, dir->location,
                                         dir->location_length + 1);
                            if (directory[0] == '\0')
                            {
                               (void)strcpy(dir->location, prev_user_dir);
                               dir->location_length = strlen(dir->location) + 1;
                            }
                            else if (directory[0] == '/')
                                 {
                                    (void)strcpy(dir->location, directory);
                                    dir->location_length = optimise_dir(dir->location);
                                 }
                                 else
                                 {
                                    (void)snprintf(dir->location,
                                                   MAX_PATH_LENGTH + MAX_RECIPIENT_LENGTH,
                                                   "%s/%s",
                                                   prev_user_dir, directory);
                                    dir->location_length = optimise_dir(dir->location);
                                 }
                         }
                    else if (dir->scheme & HTTP_FLAG)
                         {
                            dir->type = REMOTE_DIR;
                            dir->protocol = HTTP;
                            if (password[0] != '\0')
                            {
                               store_passwd(dir_user, dir->real_hostname, password);
                            }
                            t_hostname(dir->real_hostname, dir->host_alias);
                            (void)strcpy(dir->url, dir->location);
                            (void)strcpy(dir->orig_dir_name, dir->url);
                         }
                    else if (dir->scheme & SFTP_FLAG)
                         {
                            dir->type = REMOTE_DIR;
                            dir->protocol = SFTP;
                            if (password[0] != '\0')
                            {
                               store_passwd(dir_user, dir->real_hostname, password);
                            }
                            t_hostname(dir->real_hostname, dir->host_alias);
                            (void)strcpy(dir->url, dir->location);
                            (void)strcpy(dir->orig_dir_name, dir->url);
                         }
                    else if (dir->scheme & EXEC_FLAG)
                         {
                            dir->type = REMOTE_DIR;
                            dir->protocol = EXEC;
                            t_hostname(dir->real_hostname, dir->host_alias);
                            (void)strcpy(dir->url, dir->location);
                            (void)strcpy(dir->orig_dir_name, dir->url);
                         }
                         else
                         {
                            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                          "Unknown or unsupported scheme, ignoring directory %s from %s",
                                          dir->location, dcl[dcd].dir_config_file);
                            continue;
                         }
                 }
                 else
                 {
                    char error_msg[MAX_URL_ERROR_MSG];

                    url_get_error(error_mask, error_msg, MAX_URL_ERROR_MSG);
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                  "Incorrect url `%s' in %s line %d. Error is: %s.",
                                  dir->location, dcl[dcd].dir_config_file,
                                  count_new_lines(database, search_ptr), error_msg);
                    continue;
                 }
            dir_ptr = ptr - 1;

            /* Before we go on, we have to search for the beginning of */
            /* the next directory entry so we can mark the end for     */
            /* this directory entry.                                   */
            if ((end_dir_ptr = posi_identifier(ptr, DIR_IDENTIFIER,
                                               DIR_IDENTIFIER_LENGTH)) != NULL)
            {
               /* First save char we encounter here. */
               tmp_dir_char = *end_dir_ptr;

               /* Now mark end of this directory entry. */
               *end_dir_ptr = '\0';

               /* Set flag that we found another entry. */
               other_dir_flag = YES;
            }
            else
            {
               other_dir_flag = NO;
            }

            /*
             ****************** Read Directory Options ***************
             */
            if ((search_ptr = posi_identifier(ptr, DIR_OPTION_IDENTIFIER,
                                              DIR_OPTION_IDENTIFIER_LENGTH)) != NULL)
            {
               int length = 0;

#ifdef _LINE_COUNTER_TEST
               system_log(INFO_SIGN, NULL, 0,
                          "Found DIR_OPTION_IDENTIFIER at line %d in %s",
                          count_new_lines(database, search_ptr),
                          dcl[dcd].dir_config_file);
#endif
               if (*(search_ptr - 1) != '\n')
               {
                  /* Ignore any data directly behind the identifier. */
                  while ((*search_ptr != '\n') && (*search_ptr != '\0'))
                  {
                     search_ptr++;
                  }
                  if (*search_ptr == '\n')
                  {
                     search_ptr++;
                  }
               }
               while (*search_ptr == '#')
               {
                  while ((*search_ptr != '\n') && (*search_ptr != '\0'))
                  {
                     search_ptr++;
                  }
                  if (*search_ptr == '\n')
                  {
                     search_ptr++;
                  }
               }
               ptr = search_ptr;

               while ((*ptr != '\n') && (*ptr != '\0'))
               {
                  while ((*ptr == ' ') || (*ptr == '\t'))
                  {
                     ptr++;
                  }
                  if (*ptr != '\n')
                  {
                     /* Check if this is a comment line. */
                     if (*ptr == '#')
                     {
                        while ((*ptr != '\n') && (*ptr != '\0'))
                        {
                           ptr++;
                        }
                        if (*ptr == '\n')
                        {
                           ptr++;
                        }
                        continue;
                     }

                     while ((*ptr != '\n') && (*ptr != '\0'))
                     {
                        dir->dir_options[length] = *ptr;
                        ptr++; length++;
                     }
                     dir->dir_options[length++] = '\n';
                     if (*ptr == '\n')
                     {
                        ptr++;
                     }
                  }
               }
               dir->dir_options[length] = '\0';
            }
            else
            {
               dir->dir_options[0] = '\0';
            }

            /*
             ********************** Read filenames *******************
             */
            /* Position ptr after FILE_IDENTIFIER. */
            dir->fgc = 0;
            while ((search_ptr = posi_identifier(ptr, FILE_IDENTIFIER,
                                                 FILE_IDENTIFIER_LENGTH)) != NULL)
            {
               ptr = --search_ptr;
#ifdef _LINE_COUNTER_TEST
               system_log(INFO_SIGN, NULL, 0,
                          "Found FILE_IDENTIFIER at line %d in %s",
                          count_new_lines(database, search_ptr),
                          dcl[dcd].dir_config_file);
#endif

               if ((dir->fgc % FG_BUFFER_STEP_SIZE) == 0)
               {
                  char   *ptr_start;
                  size_t new_size;

                  /* Calculate new size of file group buffer. */
                  new_size = ((dir->fgc / FG_BUFFER_STEP_SIZE) + 1) *
                              FG_BUFFER_STEP_SIZE * sizeof(struct file_group);

                  if (dir->fgc == 0)
                  {
                     if ((dir->file = malloc(new_size)) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "Could not malloc() memory : %s",
                                   strerror(errno));
                        exit(INCORRECT);
                     }
                     dir->fgc = 0;
                  }
                  else
                  {
                     if ((dir->file = realloc(dir->file, new_size)) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "Could not realloc() memory : %s",
                                   strerror(errno));
                        exit(INCORRECT);
                     }
                  }

                  if (dir->fgc > (FG_BUFFER_STEP_SIZE - 1))
                  {
                     ptr_start = (char *)(dir->file) + (dir->fgc * sizeof(struct file_group));
                  }
                  else
                  {
                     ptr_start = (char *)dir->file;
                  }
                  (void)memset(ptr_start, 0, (FG_BUFFER_STEP_SIZE * sizeof(struct file_group)));
               }

               /* Store file-group name. */
               if (*ptr != '\n') /* Is there a file group name? */
               {
                  /* Store file group name. */
                  i = 0;
                  while ((*ptr != '\n') && (*ptr != '\0'))
                  {
                     CHECK_SPACE();
                     dir->file[dir->fgc].file_group_name[i] = *ptr;
                     i++; ptr++;
                  }

                  /* Did we already reach end of current file group? */
                  if (*ptr == '\0')
                  {
                     /* Generate warning that this file entry is faulty. */
                     update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                   "In %s line %d, directory %s does not have a destination entry.",
                                   dcl[dcd].dir_config_file,
                                   count_new_lines(database, search_ptr),
                                   dir->location);

                     /* To read the next file entry, put back the char    */
                     /* that was torn out to mark end of this file entry. */
                     if (tmp_file_char != 1)
                     {
                        *end_file_ptr = tmp_file_char;
                     }

                     /* No need to carry on with this file entry. */
                     continue;
                  }

                  /* Make sure that a file group name was defined. */
                  if (dir->file[dir->fgc].file_group_name[0] == '\0')
                  {
                     /* Create a unique file group name. */
                     (void)snprintf(dir->file[dir->fgc].file_group_name, MAX_GROUP_NAME_LENGTH,
                                    "FILE_%d", unique_file_counter);
                     unique_file_counter++;
                  }
               }
               else /* No file group name defined. */
               {
                  /* Create a unique file group name. */
                  (void)snprintf(dir->file[dir->fgc].file_group_name, MAX_GROUP_NAME_LENGTH,
                                 "FILE_%d", unique_file_counter);
                  unique_file_counter++;
               }

               /* Before we go on, we have to search for the beginning of */
               /* the next file group entry so we can mark the end for   */
               /* this file entry.                                       */
               if (*ptr == '\n')
               {
                  ptr++;
               }
               do
               {
                  if ((end_file_ptr = posi_identifier(ptr, FILE_IDENTIFIER,
                                                      FILE_IDENTIFIER_LENGTH)) != NULL)
                  {
                     /* Ensure that this next FILE_IDENTIFIER is NOT */
                     /* on the next line.                            */
                     i = j = 0;
                     while ((ptr + i) < end_file_ptr)
                     {
                        if (*(ptr + i) == '\n')
                        {
                           j++;
                        }
                        i++;
                     }
                     if (j == 1)
                     {
                        ptr += i;
                        other_file_flag = NEITHER;
                     }
                     else
                     {
                        /* First save char we encounter here. */
                        tmp_file_char = *end_file_ptr;

                        /* Now mark end of this file group entry. */
                        *end_file_ptr = '\0';

                        /* Set flag that we found another entry. */
                        other_file_flag = YES;
                     }
                  }
                  else
                  {
                     other_file_flag = NO;
                  }
               } while (other_file_flag == NEITHER);

               /* Store file names. */
               if (*ptr == '\n') /* Are any files specified? */
               {
                  /* We assume that all files in this */
                  /* directory should be send.        */
                  if (alfc > 0)
                  {
                     if ((dir->file[dir->fgc].files = malloc(alfbl + 2)) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "Could not malloc() memory : %s",
                                   strerror(errno));
                        exit(INCORRECT);
                     }
                     dir->file[dir->fgc].fbl = alfbl + 2;
                     (void)memcpy(dir->file[dir->fgc].files, alfiles, alfbl);
                     dir->file[dir->fgc].files[alfbl] = '*';
                     dir->file[dir->fgc].files[alfbl + 1] = '\0';
                     dir->file[dir->fgc].fc = alfc + 1;
                  }
                  else
                  {
                     if ((dir->file[dir->fgc].files = malloc(2)) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "Could not malloc() memory : %s",
                                   strerror(errno));
                        exit(INCORRECT);
                     }
                     dir->file[dir->fgc].fbl = 2;
                     dir->file[dir->fgc].files[0] = '*';
                     dir->file[dir->fgc].files[1] = '\0';
                     dir->file[dir->fgc].fc = 1;
                  }
               }
               else /* Yes, files are specified. */
               {
                  int total_length = alfbl;

                  if ((dir->file[dir->fgc].files = malloc(alfbl + FILE_MASK_STEP_SIZE)) == NULL)
                  {
                     system_log(FATAL_SIGN, __FILE__, __LINE__,
                                "Could not malloc() memory : %s", strerror(errno));
                     exit(INCORRECT);
                  }
                  dir->file[dir->fgc].fbl = alfbl + FILE_MASK_STEP_SIZE;
                  if (alfbl > 0)
                  {
                     (void)memcpy(dir->file[dir->fgc].files, alfiles, alfbl);
                  }
                  dir->file[dir->fgc].fc = alfc;

                  /* Read each filename line by line,  */
                  /* until we encounter an empty line. */
                  do
                  {
                     /* Store file name. */
                     i = 0;
                     while ((*ptr != '\n') && (*ptr != '\0'))
                     {
                        CHECK_SPACE();

                        /* Store file name. */
                        dir->file[dir->fgc].files[total_length + i] = *ptr;
                        ptr++; i++;
                        if ((total_length + i + 1) >= dir->file[dir->fgc].fbl)
                        {
                           dir->file[dir->fgc].fbl += FILE_MASK_STEP_SIZE;
                           if ((dir->file[dir->fgc].files = realloc(dir->file[dir->fgc].files,
                                                                    dir->file[dir->fgc].fbl)) == NULL)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         "Could not realloc() memory : %s",
                                         strerror(errno));
                              exit(INCORRECT);
                           }
                        }
                     }
                     if (i != 0)
                     {
                        dir->file[dir->fgc].files[total_length + i] = '\0';
                        if ((dir->file[dir->fgc].files[total_length] == GROUP_SIGN) &&
                            (((dir->file[dir->fgc].files[total_length + 1] == CURLY_BRACKET_OPEN) &&
                              (dir->file[dir->fgc].files[total_length + i - 1] == CURLY_BRACKET_CLOSE)) ||
                             ((dir->file[dir->fgc].files[total_length + 1] == SQUARE_BRACKET_OPEN) ||
                              (dir->file[dir->fgc].files[total_length + i - 1] == SQUARE_BRACKET_CLOSE))))
                        {
                           expand_file_filter(dir, &total_length,
                                              dcl[dcd].dir_config_file,
                                              database, search_ptr,
                                              warn_counter, debug_fp);
                           *using_groups = YES;
                        }
                        else
                        {
                           total_length += i + 1;
                           dir->file[dir->fgc].fc++;
                        }
                     }
                     if (*ptr == '\n')
                     {
                        ptr++;
                     }

                     /* Check for a dummy empty line. */
                     if (*ptr != '\n')
                     {
                        search_ptr = ptr;
                        while ((*search_ptr == ' ') || (*search_ptr == '\t'))
                        {
                           search_ptr++;
                        }
                        ptr = search_ptr;
                     }
                  } while ((*ptr != '\n') && (*ptr != '\0'));
                  dir->file[dir->fgc].fbl = total_length;
                  if (dir->file[dir->fgc].fbl == 0)
                  {
                     dir->file[dir->fgc].fbl = 2;
                     dir->file[dir->fgc].files[0] = '*';
                     dir->file[dir->fgc].files[1] = '\0';
                     dir->file[dir->fgc].fc++;
                  }
               }

               /*
                ******************** Read destinations ******************
                */
               /* Position ptr after DESTINATION_IDENTIFIER. */
               ptr++;
               dir->file[dir->fgc].dgc = 0;
               while ((search_ptr = posi_identifier(ptr, DESTINATION_IDENTIFIER,
                                                    DESTINATION_IDENTIFIER_LENGTH)) != NULL)
               {
                  ptr = --search_ptr;
#ifdef _LINE_COUNTER_TEST
                  system_log(INFO_SIGN, NULL, 0,
                             "Found DESTINATION_IDENTIFIER at line %d in %s",
                             count_new_lines(database, search_ptr),
                             dcl[dcd].dir_config_file);
#endif

                  if ((dir->file[dir->fgc].dgc % DG_BUFFER_STEP_SIZE) == 0)
                  {
                     char   *ptr_start;
                     size_t new_size;

                     /* Calculate new size of destination group buffer. */
                     new_size = ((dir->file[dir->fgc].dgc / DG_BUFFER_STEP_SIZE) + 1) * DG_BUFFER_STEP_SIZE * sizeof(struct dest_group);

                     if ((dir->file[dir->fgc].dest = realloc(dir->file[dir->fgc].dest, new_size)) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "Could not realloc() memory : %s",
                                   strerror(errno));
                        exit(INCORRECT);
                     }

                     if (dir->file[dir->fgc].dgc > (DG_BUFFER_STEP_SIZE - 1))
                     {
                        ptr_start = (char *)(dir->file[dir->fgc].dest) + (dir->file[dir->fgc].dgc * sizeof(struct dest_group));
                     }
                     else
                     {
                        ptr_start = (char *)dir->file[dir->fgc].dest;
                     }
                     (void)memset(ptr_start, 0,
                                  (DG_BUFFER_STEP_SIZE * sizeof(struct dest_group)));
                  }

                  /* Store destination group name. */
                  if (*ptr != '\n') /* Is there a destination group name? */
                  {
                     /* Store group name of destination. */
                     i = 0;
                     while ((*ptr != '\n') && (*ptr != '\0'))
                     {
                        CHECK_SPACE();
                        dir->file[dir->fgc].\
                                dest[dir->file[dir->fgc].dgc].\
                                dest_group_name[i] = *ptr;
                        i++; ptr++;
                     }

                     /* Check if we encountered end of destination entry. */
                     if (*ptr == '\0')
                     {
                        /* Generate warning message, that no destination */
                        /* has been defined.                             */
                        update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                      "Directory %s in %s at line %d does not have a destination entry for file group no. %d.",
                                      dir->location, dcl[dcd].dir_config_file,
                                      count_new_lines(database, ptr), dir->fgc);

                        /* To read the next destination entry, put back the */
                        /* char that was torn out to mark end of this       */
                        /* destination entry.                               */
                        if (tmp_dest_char != 1)
                        {
                           *end_dest_ptr = tmp_dest_char;
                        }

                        continue; /* Go to next destination entry. */
                     }
                  }
                  else /* No destination group name defined. */
                  {
                     /* Create a unique destination group name. */
                     (void)snprintf(dir->file[dir->fgc].\
                                    dest[dir->file[dir->fgc].dgc].dest_group_name,
                                    MAX_GROUP_NAME_LENGTH,
                                    "DEST_%d", unique_dest_counter);
                     unique_dest_counter++;
                  }
                  ptr++;

                  /* Before we go on, we have to search for the beginning of */
                  /* the next destination entry so we can mark the end for   */
                  /* this destination entry.                                 */
                  if ((end_dest_ptr = posi_identifier(ptr, DESTINATION_IDENTIFIER,
                                                      DESTINATION_IDENTIFIER_LENGTH)) != NULL)
                  {
                     /* First save char we encounter here. */
                     tmp_dest_char = *end_dest_ptr;

                     /* Now mark end of this destination entry. */
                     *end_dest_ptr = '\0';

                     /* Set flag that we found another entry. */
                     other_dest_flag = YES;
                  }
                  else
                  {
                     other_dest_flag = NO;
                  }

                  /*++++++++++++++++++++++ Read recipient +++++++++++++++++++++*/
                  /* Position ptr after RECIPIENT_IDENTIFIER. */
                  if ((search_ptr = posi_identifier(ptr, RECIPIENT_IDENTIFIER,
                                                    RECIPIENT_IDENTIFIER_LENGTH)) != NULL)
                  {
#ifdef _LINE_COUNTER_TEST
                     system_log(INFO_SIGN, NULL, 0,
                                "Found RECIPIENT_IDENTIFIER at line %d in %s",
                                count_new_lines(database, search_ptr),
                                dcl[dcd].dir_config_file);
#endif
                     if (*(search_ptr - 1) != '\n')
                     {
                        /* Ignore any data directly behind the identifier. */
                        while ((*search_ptr != '\n') && (*search_ptr != '\0'))
                        {
                           search_ptr++;
                        }
                        if (*search_ptr == '\n')
                        {
                           search_ptr++;
                        }
                     }
                     while (*search_ptr == '#')
                     {
                        while ((*search_ptr != '\n') && (*search_ptr != '\0'))
                        {
                           search_ptr++;
                        }
                        if (*search_ptr == '\n')
                        {
                           search_ptr++;
                        }
                     }
                     ptr = search_ptr;

                     /* Read the address for each recipient line by */
                     /* line, until we encounter an empty line.     */
                     dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc = 0;

                     if ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec = malloc((RECIPIENT_STEP_SIZE * sizeof(struct recipient_group)))) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "Could not malloc() memory : %s",
                                   strerror(errno));
                        exit(INCORRECT);
                     }

                     while ((*ptr != '\n') && (*ptr != '\0'))
                     {
                        /* Take away empty spaces from begining. */
                        while ((*ptr == ' ') || (*ptr == '\t'))
                        {
                           ptr++;
                        }

                        /* Check if this is a comment line. */
                        if (*ptr == '#')
                        {
                           while ((*ptr != '\n') && (*ptr != '\0'))
                           {
                              ptr++;
                           }
                           if (*ptr == '\n')
                           {
                              ptr++;
                           }

                           goto check_dummy_line;
                        }

                        /* Store recipient. */
                        i = 0;
                        recipient_group_name[0] = '\0';
                        tmp_ptr = search_ptr = ptr;
                        while ((*ptr != '\n') && (*ptr != '\0'))
                        {
                           if ((*ptr == ' ') || (*ptr == '\t'))
                           {
                              tmp_ptr = ptr;
                              while ((*tmp_ptr == ' ') || (*tmp_ptr == '\t'))
                              {
                                 tmp_ptr++;
                              }

                              switch (*tmp_ptr)
                              {
                                 case '#' : /* Found comment. */
                                    while ((*tmp_ptr != '\n') &&
                                           (*tmp_ptr != '\0'))
                                    {
                                       tmp_ptr++;
                                    }
                                    ptr = tmp_ptr;
                                    continue;

                                 case '\0': /* Found end for this entry. */
                                    ptr = tmp_ptr;
                                    continue;

                                 case '\n': /* End of line reached. */
                                    ptr = tmp_ptr;
                                    continue;

                                 default  : /* Assume the recipient string */
                                            /* contains spaces.            */
                                    (void)memmove(&dir->file[dir->fgc].\
                                                  dest[dir->file[dir->fgc].dgc].\
                                                  rec[dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc].\
                                                  recipient[i],
                                                  ptr, tmp_ptr - ptr);
                                    i += (tmp_ptr - ptr);
                                    ptr = tmp_ptr;
                                    break;
                              }
                           }
                           else if ((*ptr == GROUP_SIGN) &&
                                    ((*(ptr + 1) == CURLY_BRACKET_OPEN) ||
                                     (*(ptr + 1) == SQUARE_BRACKET_OPEN)))
                                {
                                   char close_bracket;

                                   if (*(ptr + 1) == CURLY_BRACKET_OPEN)
                                   {
                                      dir_group_type = YES;
                                      close_bracket = CURLY_BRACKET_CLOSE;
                                   }
                                   else
                                   {
                                      dir_group_type = NO;
                                      close_bracket = SQUARE_BRACKET_CLOSE;
                                   }
                                   dir->file[dir->fgc].\
                                           dest[dir->file[dir->fgc].dgc].\
                                           rec[dir->file[dir->fgc].\
                                           dest[dir->file[dir->fgc].dgc].rc].\
                                           recipient[i] = *ptr;
                                   dir->file[dir->fgc].\
                                           dest[dir->file[dir->fgc].dgc].\
                                           rec[dir->file[dir->fgc].\
                                           dest[dir->file[dir->fgc].dgc].rc].\
                                           recipient[i + 1] = *(ptr + 1);
                                   ptr += 2;
                                   i += 2;
                                   j = 0;
                                   while ((*ptr != close_bracket) &&
                                          (*ptr != '\n') && (*ptr != '\0'))
                                   {
                                      recipient_group_name[j] = *ptr;
                                      dir->file[dir->fgc].\
                                              dest[dir->file[dir->fgc].dgc].\
                                              rec[dir->file[dir->fgc].\
                                              dest[dir->file[dir->fgc].dgc].rc].\
                                              recipient[i] = *ptr;
                                      j++; ptr++; i++;
                                   }
                                   if (*ptr == close_bracket)
                                   {
                                      recipient_group_name[j] = '\0';
                                      *using_groups = YES;
                                   }
                                   else
                                   {
                                      /* No end marker, so just take it as a normal */
                                      /* directory entry.                           */
                                      recipient_group_name[0] = '\0';
                                   }
                                }
                           dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].\
                                   rec[dir->file[dir->fgc].\
                                   dest[dir->file[dir->fgc].dgc].rc].\
                                   recipient[i] = *ptr;
                           ptr++; i++;
                        }
                        dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].\
                                rec[dir->file[dir->fgc].\
                                dest[dir->file[dir->fgc].dgc].rc].\
                                recipient[i] = '\0';
                        if (*ptr == '\n')
                        {
                           ptr++;
                        }

                        /* Make sure that we did read a line. */
                        if (i != 0)
                        {
                           if (recipient_group_name[0] != '\0')
                           {
                              init_recipient_group_name(dir->file[dir->fgc].\
                                                        dest[dir->file[dir->fgc].dgc].\
                                                        rec[dir->file[dir->fgc].\
                                                        dest[dir->file[dir->fgc].dgc].rc].\
                                                        recipient,
                                                        recipient_group_name,
                                                        dir_group_type);
                           }

                           do
                           {
                              if ((error_mask = url_evaluate(dir->file[dir->fgc].\
                                                          dest[dir->file[dir->fgc].dgc].\
                                                          rec[dir->file[dir->fgc].\
                                                          dest[dir->file[dir->fgc].dgc].rc].\
                                                          recipient,
                                                          &dir->file[dir->fgc].\
                                                          dest[dir->file[dir->fgc].dgc].\
                                                          rec[dir->file[dir->fgc].\
                                                          dest[dir->file[dir->fgc].dgc].rc].\
                                                          scheme, user,
                                                          &smtp_auth, smtp_user,
#ifdef WITH_SSH_FINGERPRINT
                                                          dummy_ssh_fingerprint,
                                                          &dummy_key_type,
#endif
                                                          password, YES,
                                                          dir->file[dir->fgc].\
                                                          dest[dir->file[dir->fgc].dgc].\
                                                          rec[dir->file[dir->fgc].\
                                                          dest[dir->file[dir->fgc].dgc].rc].real_hostname,
                                                          &dummy_port,
                                                          dummy_directory, NULL,
                                                          NULL,
                                                          &dummy_transfer_mode,
                                                          &dummy_ssh_protocol,
                                                          &dummy_auth,
                                                          dummy_region,
                                                          &dummy_service,
                                                          smtp_server)) < 4)
                              {
                                 if (user[0] == '\0')
                                 {
                                    if (dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec[dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc].real_hostname[0] == MAIL_GROUP_IDENTIFIER)
                                    {
                                       j = 0;
                                       while (dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec[dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc].real_hostname[j + 1] != '\0')
                                       {
                                          dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec[dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc].real_hostname[j] = dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec[dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc].real_hostname[j + 1];
                                          j++;
                                       }
                                       dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec[dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc].real_hostname[j] = '\0';
                                    }
                                 }
#ifdef _WITH_DE_MAIL_SUPPORT
                                 if (((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec[dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc].scheme & SMTP_FLAG) ||
                                      (dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec[dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc].scheme & DE_MAIL_FLAG)) &&
                                     (smtp_server[0] != '\0'))
#else
                                 if ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec[dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc].scheme & SMTP_FLAG) &&
                                     (smtp_server[0] != '\0'))
#endif
                                 {
                                    j = 0;
                                    while (smtp_server[j] != '\0')
                                    {
                                       dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec[dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc].real_hostname[j] = smtp_server[j];
                                       j++;
                                    }
                                    dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec[dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc].real_hostname[j] = '\0';
                                 }
                                 t_hostname(dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec[dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc].real_hostname,
                                            dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec[dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc].host_alias);

                                 if (password[0] != '\0')
                                 {
                                    if (smtp_auth == SMTP_AUTH_NONE)
                                    {
                                       store_passwd(user,
                                                    dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec[dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc].real_hostname,
                                                    password);
                                    }
                                    else
                                    {
                                       store_passwd(smtp_user,
                                                    dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec[dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc].real_hostname,
                                                    password);
                                    }
                                 }

                                 dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc++;
                                 t_rc++;
                                 if ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc % RECIPIENT_STEP_SIZE) == 0)
                                 {
                                    int new_size = ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc /
                                                     RECIPIENT_STEP_SIZE) + 1) * RECIPIENT_STEP_SIZE *
                                                   sizeof(struct recipient_group);

                                    if ((dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec = realloc(dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec,
                                                                                                         new_size)) == NULL)
                                    {
                                       system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                  "Could not realloc() memory : %s",
                                                  strerror(errno));
                                       exit(INCORRECT);
                                    }
                                 }
                              }
                              else
                              {
                                 char error_msg[MAX_URL_ERROR_MSG];

                                 url_get_error(error_mask, error_msg, MAX_URL_ERROR_MSG);
                                 update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                               "Incorrect url `%s'. Error is: %s. Ignoring the recipient in %s at line %d.",
                                               dir->file[dir->fgc].\
                                               dest[dir->file[dir->fgc].dgc].\
                                               rec[dir->file[dir->fgc].\
                                               dest[dir->file[dir->fgc].dgc].rc].\
                                               recipient, error_msg,
                                               dcl[dcd].dir_config_file,
                                               count_new_lines(database, search_ptr));
                              }
                           } while (next_recipient_group_name(dir->file[dir->fgc].\
                                                              dest[dir->file[dir->fgc].dgc].\
                                                              rec[dir->file[dir->fgc].\
                                                              dest[dir->file[dir->fgc].dgc].rc].\
                                                              recipient) == 1);
                           free_recipient_group_name();
                        } /* if (i != 0) */

check_dummy_line:
                        /* Check for a dummy empty line. */
                        if (*ptr != '\n')
                        {
                           search_ptr = ptr;
                           while ((*search_ptr == ' ') || (*search_ptr == '\t'))
                           {
                              search_ptr++;
                           }
                           ptr = search_ptr;
                        }
                     }
                  }

                  /* Make sure that at least one recipient was defined. */
                  if (dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rc == 0)
                  {
                     if (search_ptr == NULL)
                     {
                        search_ptr = ptr + 1;
                     }

                     /* Generate warning message. */
                     update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                   "No recipient specified for %s from %s at line %d.",
                                   dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].dest_group_name,
                                   dcl[dcd].dir_config_file,
                                   count_new_lines(database, search_ptr));

                     /* To read the next destination entry, put back the */
                     /* char that was torn out to mark end of this       */
                     /* destination entry.                               */
                     if (other_dest_flag == YES)
                     {
                        *end_dest_ptr = tmp_dest_char;
                     }

                     free(dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec);
                     dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].rec = NULL;

                     /* Try to read the next destination. */
                     continue;
                  }
   
                  /*++++++++++++++++++++++ Read options ++++++++++++++++++++++++*/
                  /* Position ptr after OPTION_IDENTIFIER. */
                  if ((search_ptr = posi_identifier(ptr, OPTION_IDENTIFIER,
                                                    OPTION_IDENTIFIER_LENGTH)) != NULL)
                  {
#ifdef _LINE_COUNTER_TEST
                     system_log(INFO_SIGN, NULL, 0,
                                "Found OPTION_IDENTIFIER at line %d in %s",
                                count_new_lines(database, search_ptr),
                                dcl[dcd].dir_config_file);
#endif
                     if (*(search_ptr - 1) != '\n')
                     {
                        /* Ignore any data directly behind the identifier. */
                        while ((*search_ptr != '\n') && (*search_ptr != '\0'))
                        {
                           search_ptr++;
                        }
                        if (*search_ptr == '\n')
                        {
                           search_ptr++;
                        }
                     }
                     ptr = search_ptr;

                     /* Read each option line by line, until */
                     /* we encounter an empty line.          */
                     dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].oc = 0;
                     while ((*ptr != '\n') && (*ptr != '\0') &&
                            (dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].oc < MAX_NO_OPTIONS))
                     {
                        /* Store option. */
                        i = 0;
                        while ((*ptr != '\n') && (*ptr != '\0') &&
                               (i < MAX_OPTION_LENGTH))
                        {
                           CHECK_SPACE();
                           dir->file[dir->fgc].\
                                   dest[dir->file[dir->fgc].dgc].\
                                   options[dir->file[dir->fgc].\
                                   dest[dir->file[dir->fgc].dgc].oc][i] = *ptr;
                           ptr++; i++;
                        }

                        if (i >= MAX_OPTION_LENGTH)
                        {
                           while ((*ptr != '\n') && (*ptr != '\0'))
                           {
                              ptr++;
                           }
                           update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                         "Option at line %d in %s longer then %d, ignoring this option.",
                                         count_new_lines(database, ptr),
                                         dcl[dcd].dir_config_file,
                                         MAX_OPTION_LENGTH);
                        }
                        else
                        {
                           /* Make sure that we did read a line. */
                           if (i != 0)
                           {
                              dir->file[dir->fgc].\
                                      dest[dir->file[dir->fgc].dgc].\
                                      options[dir->file[dir->fgc].\
                                      dest[dir->file[dir->fgc].dgc].oc][i] = '\0';
                              if (check_option(dir->file[dir->fgc].\
                                               dest[dir->file[dir->fgc].dgc].\
                                               options[dir->file[dir->fgc].\
                                               dest[dir->file[dir->fgc].dgc].oc],
                                               debug_fp) == SUCCESS)
                              {
                                 dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].oc++;
                              }
                              else
                              {
                                 update_db_log(WARN_SIGN, __FILE__, __LINE__,
                                               debug_fp, warn_counter,
                                               "Removing option `%s' at line %d in %s",
                                               dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].options[dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].oc],
                                               count_new_lines(database, ptr),
                                               dcl[dcd].dir_config_file);
                              }
                           }
                        }
                        if (*ptr == '\n')
                        {
                           ptr++;
                        }

                        /* Check for a dummy empty line. */
                        if (*ptr != '\n')
                        {
                           search_ptr = ptr;
                           while ((*search_ptr == ' ') || (*search_ptr == '\t'))
                           {
                              search_ptr++;
                           }
                           ptr = search_ptr;
                        }
                     } /* while ((*ptr != '\n') && (*ptr != '\0')) */

                     if (dir->file[dir->fgc].dest[dir->file[dir->fgc].dgc].oc >= MAX_NO_OPTIONS)
                     {
                        update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                      "Exceeded the number of total options (max = %d) at line %d in %s. Ignoring.",
                                      MAX_NO_OPTIONS,
                                      count_new_lines(database, ptr),
                                      dcl[dcd].dir_config_file);
                     }
                  }

                  /* To read the next destination entry, put back the */
                  /* char that was torn out to mark end of this       */
                  /* destination entry.                               */
                  if (other_dest_flag == YES)
                  {
                     *end_dest_ptr = tmp_dest_char;
                  }

                  dir->file[dir->fgc].dgc++;
                  t_dgc++;
               } /* while () searching for DESTINATION_IDENTIFIER */

               /* Check if a destination was defined. */
               if (dir->file[dir->fgc].dgc == 0)
               {
                  /* Generate error message. */
                  update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                "Directory %s in %s does not have a destination entry for file group no. %d.",
                                dir->location, dcl[dcd].dir_config_file,
                                dir->fgc);

                  free(dir->file[dir->fgc].files);
                  dir->file[dir->fgc].files = NULL;

                  /* Reduce file counter, since this one is faulty. */
                  dir->fgc--;
               }

               /* To read the next file entry, put back the char    */
               /* that was torn out to mark end of this file entry. */
               if (other_file_flag == YES)
               {
                  *end_file_ptr = tmp_file_char;
               }

               dir->fgc++;
               if (*ptr == '\0')
               {
                  break;
               }
               else
               {
                  ptr++;
               }
            } /* while () searching for FILE_IDENTIFIER */

            /* Check special case when no file identifier is found. */
            if ((dir->fgc == 0) && (dir->file != NULL))
            {
               /* We assume that all files in this dir should be send. */
               if ((dir->file[dir->fgc].files = malloc(2)) == NULL)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             "Could not malloc() memory : %s", strerror(errno));
                  exit(INCORRECT);
               }
               dir->file[dir->fgc].fbl = 2;
               dir->file[dir->fgc].files[0] = '*';
               dir->file[dir->fgc].files[1] = '\0';
               dir->fgc++;
            }

            /* To read the next directory entry, put back the char    */
            /* that was torn out to mark end of this directory entry. */
            if (other_dir_flag == YES)
            {
               *end_dir_ptr = tmp_dir_char;
            }

            /* Check if a destination was defined for the last directory. */
            if ((dir->file == NULL) || (dir->file[0].dest == NULL) ||
                (dir->file[0].dest[0].rc == 0))
            {
               char *end_ptr;

               if (search_ptr == NULL)
               {
                  end_ptr = ptr;
               }
               else
               {
                  end_ptr = search_ptr;
               }
               update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                             "In %s at line %d, no destination defined.",
                             dcl[dcd].dir_config_file,
                             count_new_lines(database, end_ptr));
            }
            else
            {
               int duplicate = NO;

               if ((no_of_local_dirs % 10) == 0)
               {
                  size_t new_size = (((no_of_local_dirs / 10) + 1) * 10) *
                                    sizeof(struct dir_data);

                  if (no_of_local_dirs == 0)
                  {
                     if ((dd = malloc(new_size)) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "malloc() error : %s", strerror(errno));
                        exit(INCORRECT);
                     }
                  }
                  else
                  {
                     if ((dd = realloc((char *)dd, new_size)) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "realloc() error : %s", strerror(errno));
                        exit(INCORRECT);
                     }
                  }
               }

               /*
                * Evaluate the directory options.
                * We need to do this here since under dir option we
                * can define where a local remote directory is located.
                */
               dd[no_of_local_dirs].in_dc_flag = 0;
               (void)strcpy(dd[no_of_local_dirs].dir_name, dir->location);
               if ((j = eval_dir_options(no_of_local_dirs, dir->type,
                                         dir->dir_options, debug_fp)) != 0)
               {
                  char *end_ptr;

                  if (search_ptr == NULL)
                  {
                     end_ptr = ptr;
                  }
                  else
                  {
                     end_ptr = search_ptr;
                  }
                  update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                "%d %s problems in %s at line %d",
                                j, DIR_OPTION_IDENTIFIER, dcl[dcd].dir_config_file,
                                count_new_lines(database, end_ptr));
               }
               dd[no_of_local_dirs].dir_name[0] = '\0';
               if (dir->type == REMOTE_DIR)
               {
                  if (dir->protocol == EXEC)
                  {
                      unsigned int crc_val;

#ifdef HAVE_HW_CRC32
                      crc_val = get_str_checksum_crc32c(directory,
                                                        have_hw_crc32);
#else
                      crc_val = get_str_checksum_crc32c(directory);
#endif
                      (void)snprintf(directory, MAX_RECIPIENT_LENGTH,
                                     "%x", crc_val);
                  }
                  if (create_remote_dir(NULL,
                                        dd[no_of_local_dirs].retrieve_work_dir,
                                        dir_user, dir->real_hostname,
                                        directory, dir->location,
                                        &dir->location_length) == INCORRECT)
                  {
                     if (dir->file != NULL)
                     {
                        int m, n;

                        for (m = 0; m < dir->fgc; m++)
                        {
                           for (n = 0; n < dir->file[m].dgc; n++)
                           {
                              free(dir->file[m].dest[n].rec);
                              dir->file[m].dest[n].rec= NULL;
                           }
                           free(dir->file[m].files);
                           free(dir->file[m].dest);
                        }
                        free(dir->file);
                        dir->file = NULL;
                     }
                     continue;
                  }
               }

               /* Check if this directory was not already specified. */
               for (j = 0; j < no_of_local_dirs; j++)
               {
                  if (my_strcmp(dir->location, dd[j].dir_name) == 0)
                  {
                     if (dcl[dcd].dc_id == dd[j].dir_config_id)
                     {
                        update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                      "Ignoring duplicate directory entry %s in %s.",
                                      dir->location, dcl[dcd].dir_config_file);
                        duplicate = YES;
                     }
                     else
                     {
                        duplicate = NEITHER;
                     }
                     break;
                  }
               }

               if (duplicate != YES)
               {
                  if (duplicate == NO)
                  {
                     int    tmp_create_source_dir;
                     mode_t tmp_create_source_dir_mode;

                     dd[no_of_local_dirs].dir_pos = lookup_dir_id(dir->location,
                                                                  dir->orig_dir_name);
                     dd[no_of_local_dirs].dir_id = dnb[dd[no_of_local_dirs].dir_pos].dir_id;
                     if (dir->alias[0] == '\0')
                     {
                        (void)snprintf(dir->alias, MAX_DIR_ALIAS_LENGTH + 1,
                                       "%x",
                                       dnb[dd[no_of_local_dirs].dir_pos].dir_id);
                     }
                     else
                     {
                        int gotcha = NO;

                        /* Check if the directory alias was not already specified. */
                        for (j = 0; j < no_of_local_dirs; j++)
                        {
                           if (CHECK_STRCMP(dir->alias, dd[j].dir_alias) == 0)
                           {
                              (void)snprintf(dir->alias, MAX_DIR_ALIAS_LENGTH + 1,
                                             "%x",
                                             dnb[dd[no_of_local_dirs].dir_pos].dir_id);
                              gotcha = YES;
                              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                            "Duplicate directory alias `%s' in `%s', giving it another alias: `%s'",
                                            dd[j].dir_alias,
                                            dcl[dcd].dir_config_file, dir->alias);
                              break;
                           }
                        }
                        if (gotcha == NO)
                        {
                           dd[no_of_local_dirs].in_dc_flag |= DIR_ALIAS_IDC;
                        }
                     }

                     (void)strcpy(dd[no_of_local_dirs].dir_alias, dir->alias);
                     if (dir->type == LOCALE_DIR)
                     {
                        dd[no_of_local_dirs].fsa_pos = -1;
                        dd[no_of_local_dirs].host_alias[0] = '\0';
                        STRNCPY(dd[no_of_local_dirs].url, dir->location,
                                MAX_RECIPIENT_LENGTH);
                        if (dir->location_length >= MAX_RECIPIENT_LENGTH)
                        {
                           dd[no_of_local_dirs].url[MAX_RECIPIENT_LENGTH - 1] = '\0';
                        }
                     }
                     else if (dir->type == REMOTE_DIR)
                          {
                             (void)strcpy(dd[no_of_local_dirs].url, dir->url);
                             dd[no_of_local_dirs].fsa_pos = check_hostname_list(dir->url,
                                                                                dir->real_hostname,
                                                                                dir->host_alias,
                                                                                dir->scheme,
                                                                                RETRIEVE_FLAG);
                             (void)strcpy(dd[no_of_local_dirs].host_alias,
                                          hl[dd[no_of_local_dirs].fsa_pos].host_alias);
                             store_file_mask(dd[no_of_local_dirs].dir_alias,
                                             dir);
                          }
                          else
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "Unknown dir type %d for %s.",
                                        dir->type, dir->alias);
                             dd[no_of_local_dirs].fsa_pos = -1;
                             dd[no_of_local_dirs].host_alias[0] = '\0';
                             STRNCPY(dd[no_of_local_dirs].url, dir->location,
                                     MAX_RECIPIENT_LENGTH);
                             if (dir->location_length >= MAX_RECIPIENT_LENGTH)
                             {
                                dd[no_of_local_dirs].url[MAX_RECIPIENT_LENGTH - 1] = '\0';
                             }
                          }
                     (void)strcpy(dd[no_of_local_dirs].dir_name, dir->location);
                     dd[no_of_local_dirs].protocol = dir->protocol;
                     dd[no_of_local_dirs].dir_config_id = dcl[dcd].dc_id;
                     dir->dir_config_id = dcl[dcd].dc_id;

#if defined (WITH_INOTIFY) && defined (USE_INOTIFY_FOR_REMOTE_DIRS)
                     if (dir->type == REMOTE_DIR)
                     {
                        dd[no_of_local_dirs].inotify_flag = INOTIFY_RENAME_FLAG;
                     }
#endif

                     /* Now lets check if this directory does exist and if we */
                     /* do have enough permissions to work in this directory. */
                     created_path[0] = '\0';
                     if (create_source_dir_disabled == NO)
                     {
                        if (dd[no_of_local_dirs].create_source_dir == YES)
                        {
                           tmp_create_source_dir = YES;
                           tmp_create_source_dir_mode = dd[no_of_local_dirs].dir_mode;
                        }
                        else
                        {
                           if (dd[no_of_local_dirs].dont_create_source_dir == YES)
                           {
                              tmp_create_source_dir = NO;
                              tmp_create_source_dir_mode = 0;
                           }
                           else
                           {
                              tmp_create_source_dir = create_source_dir;
                              tmp_create_source_dir_mode = create_source_dir_mode;
                           }
                        }
                     }
                     else
                     {
                        tmp_create_source_dir = NO;
                        tmp_create_source_dir_mode = 0;
                     }
                     if ((ret = check_create_path(dir->location,
                                                  tmp_create_source_dir_mode,
                                                  &error_ptr,
                                                  tmp_create_source_dir,
                                                  dd[no_of_local_dirs].remove,
                                                  created_path)) == CREATED_DIR)
                     {
                        update_db_log(INFO_SIGN, __FILE__, __LINE__, debug_fp, NULL,
                                      "Created directory `%s' [%s] at line %d from %s",
                                      dir->location, created_path,
                                      count_new_lines(database, ptr - 1),
                                      dcl[dcd].dir_config_file);
                     }
                     else if (ret == NO_ACCESS)
                          {
                             if (error_ptr != NULL)
                             {
                                *error_ptr = '\0';
                             }
                             if (dir->type == REMOTE_DIR)
                             {
                                update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                              "Cannot access directory `%s' at line %d from %s (Ignoring this entry) : %s",
                                              dir->location,
                                              count_new_lines(database, dir_ptr),
                                              dcl[dcd].dir_config_file,
                                              strerror(errno));
                                if (dir->file != NULL)
                                {
                                   int m, n;

                                   for (m = 0; m < dir->fgc; m++)
                                   {
                                      for (n = 0; n < dir->file[m].dgc; n++)
                                      {
                                         free(dir->file[m].dest[n].rec);
                                         dir->file[m].dest[n].rec= NULL;
                                      }
                                      free(dir->file[m].files);
                                      free(dir->file[m].dest);
                                   }
                                   free(dir->file);
                                   dir->file = NULL;
                                }
                                continue;
                             }
                             else
                             {
                                update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                              "Cannot access directory `%s' or create a subdirectory in it at line %d from %s : %s",
                                              dir->location,
                                              count_new_lines(database, dir_ptr),
                                              dcl[dcd].dir_config_file,
                                              strerror(errno));
                             }
                             if (error_ptr != NULL)
                             {
                                *error_ptr = '/';
                             }
                          }
                     else if (ret == MKDIR_ERROR)
                          {
                             if (error_ptr != NULL)
                             {
                                *error_ptr = '\0';
                             }
                             if (dir->type == REMOTE_DIR)
                             {
                                update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                              "Failed to create directory `%s' at line %d from %s (Ignoring this entry) : %s",
                                              dir->location,
                                              count_new_lines(database, dir_ptr),
                                              dcl[dcd].dir_config_file,
                                              strerror(errno));
                                if (dir->file != NULL)
                                {
                                   int m, n;

                                   for (m = 0; m < dir->fgc; m++)
                                   {
                                      for (n = 0; n < dir->file[m].dgc; n++)
                                      {
                                         free(dir->file[m].dest[n].rec);
                                         dir->file[m].dest[n].rec= NULL;
                                      }
                                      free(dir->file[m].files);
                                      free(dir->file[m].dest);
                                   }
                                   free(dir->file);
                                   dir->file = NULL;
                                }
                                continue;
                             }
                             else
                             {
                                update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                              "Failed to create directory `%s' at line %d from %s : %s",
                                              dir->location,
                                              count_new_lines(database, dir_ptr),
                                              dcl[dcd].dir_config_file,
                                              strerror(errno));
                             }
                             if (error_ptr != NULL)
                             {
                                *error_ptr = '/';
                             }
                          }
                     else if (ret == STAT_ERROR)
                          {
                             if (error_ptr != NULL)
                             {
                                *error_ptr = '\0';
                             }
                             update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
#ifdef HAVE_STATX
                                           "Failed to statx() `%s' at line %d from %s : %s",
#else
                                           "Failed to stat() `%s' at line %d from %s : %s",
#endif
                                           dir->location,
                                           count_new_lines(database, dir_ptr),
                                           dcl[dcd].dir_config_file,
                                           strerror(errno));
                             if (error_ptr != NULL)
                             {
                                *error_ptr = '/';
                             }
                          }
                     else if (ret == ALLOC_ERROR)
                          {
                             system_log(FATAL_SIGN, __FILE__, __LINE__,
                                        "Could not realloc() memory : %s",
                                        strerror(errno));
                             exit(INCORRECT);
                          }
                     else if (ret == SUCCESS)
                          {
                             /* Directory does exist, so nothing to do here. */;
                          }
                          else
                          {
                             system_log(FATAL_SIGN, __FILE__, __LINE__,
                                        "Unknown error, should not get here.");
                             exit(INCORRECT);
                          }

                     /* Increase directory counter. */
                     no_of_local_dirs++;

#ifdef _DEBUG
                     (void)fprintf(p_debug_file,
                                   "\n\n=================> Contents of directory struct %4d<=================\n",
                                   no_of_local_dirs);
                     (void)fprintf(p_debug_file,
                                   "                   =================================\n");

                     /* Print directory name and alias. */
                     (void)fprintf(p_debug_file, "%3d: %s Alias: %s DIR_CONFIG ID: %x\n",
                                   i + 1, dir->location, dir->alias,
                                   dir->dir_config_id);

                     /* Print contents of each file group. */
                     for (j = 0; j < dir->fgc; j++)
                     {
                        /* Print file group name. */
                        (void)fprintf(p_debug_file, "    >%s<\n",
                                      dir->file[j].file_group_name);

                        /* Print the name of each file. */
                        tmp_ptr = dir->file[j].files;
                        for (k = 0; k < dir->file[j].fc; k++)
                        {
                           (void)fprintf(p_debug_file, "\t%3d: %s\n", k + 1,
                                         tmp_ptr);
                           NEXT(tmp_ptr);
                        }

                        /* Print all destinations. */
                        for (k = 0; k < dir->file[j].dgc; k++)
                        {
                           /* First print destination group name. */
                           (void)fprintf(p_debug_file, "\t\t%3d: >%s<\n", k + 1,
                                         dir->file[j].dest[k].dest_group_name);

                           /* Show all recipient's. */
                           (void)fprintf(p_debug_file, "\t\t\tRecipients:\n");
                           for (m = 0; m < dir->file[j].dest[k].rc; m++)
                           {
                              (void)fprintf(p_debug_file, "\t\t\t%3d: %s\n", m + 1,
                                            dir->file[j].dest[k].rec[m].recipient);
                           }

                           /* Show all options. */
                           if (dir->file[j].dest[k].oc > 0)
                           {
                              (void)fprintf(p_debug_file, "\t\t\tOptions:\n");
                              for (m = 0; m < dir->file[j].dest[k].oc; m++)
                              {
                                 (void)fprintf(p_debug_file, "\t\t\t%3d: %s\n", m + 1,
                                               dir->file[j].dest[k].options[m]);
                              }
                           }
                        }
                     }
                     (void)fflush(p_debug_file);
#endif
                  }
                  else
                  {
                     (void)strcpy(dir->alias, dd[j].dir_alias);
                     dir->dir_config_id = dcl[dcd].dc_id;
                     if (dir->type == REMOTE_DIR)
                     {
                        add_file_mask(dd[j].dir_alias, dir);
                     }
                  }

                  /* Insert directory into temporary memory. */
                  insert_dir(dir);

                  /* Insert hostnames into temporary memory. */
                  insert_hostname(dir);
               } /* if (duplicate == NO) */
            }

            for (j = 0; j < dir->fgc; j++)
            {
               int m;

               for (m = 0; m < dir->file[j].dgc; m++)
               {
                  free(dir->file[j].dest[m].rec);
                  dir->file[j].dest[m].rec= NULL;
               }
               free(dir->file[j].files);
               free(dir->file[j].dest);
            }
            free(dir->file);
            dir->file = NULL;
         } while (next_dir_group_name(dir->location, &dir->location_length,
                                      dir->alias) == 1);

         if (dir_group_name[0] != '\0')
         {
            free_dir_group_name();
         }
      } /* while ((search_ptr = posi_identifier(ptr, DIR_IDENTIFIER, DIR_IDENTIFIER_LENGTH)) != NULL) */
      free(database);
      database = NULL;
      dcd++;
   } while (dcd < no_of_dir_configs);

   /* Remove any unused hosts. */
   if (remove_unused_hosts == YES)
   {
      for (i = 0; i < no_of_hosts; i++)
      {
         if (hl[i].in_dir_config != YES)
         {
            update_db_log(DEBUG_SIGN, NULL, 0, debug_fp, NULL,
                          "Removing unused host %s.", hl[i].host_alias);

            /* Remove any nnn counter files for this host. */
            remove_nnn_files(get_str_checksum(hl[i].host_alias));

            if ((no_of_hosts > 1) && ((i + 1) < no_of_hosts))
            {
               size_t move_size = (no_of_hosts - (i + 1)) * sizeof(struct host_list);

               (void)memmove(&hl[i], &hl[i + 1], move_size);
            }
            no_of_hosts--;
            i--;
         }
      }

      /*
       * Correct fsa_pos in structure dir_data.
       */
      for (i = 0; i < no_of_local_dirs; i++)
      {
         if (dd[i].host_alias[0] != '\0')
         {
            for (j = 0; j < no_of_hosts; j++)
            {
               if (CHECK_STRCMP(dd[i].host_alias, hl[j].host_alias) == 0)
               {
                  dd[i].fsa_pos = j;
                  j = no_of_hosts;
               }
            }
         }
      }
   }

#ifdef WITH_ONETIME
   if (onetime == NO)
   {
#endif
      /* See if there are any valid directory entries. */
      if (no_of_local_dirs == 0)
      {
         /* We assume there is no entry in database   */
         /* or we have finished reading the database. */
         ret = NO_VALID_ENTRIES;
      }
      else
      {
         /* Before we copy the data to a file lets sort  */
         /* the directories so that same directories are */
         /* in one group and not scattered over the hole */
         /* list. Remember we might have multiple        */
         /* DIR_CONFIG's where the user specifies the    */
         /* same directory in each DIR_CONFIG.           */
         sort_jobs();

         /* Now copy data and pointers in their relevant */
         /* shared memory areas.                         */
#ifdef WITH_ONETIME
         copy_to_file(NO);
#else
         copy_to_file();
#endif

         /* Don't forget to create the FSA (Filetransfer */
         /* Status Area).                                */
         create_sa(no_of_local_dirs);

         /* Tell user what we have found in DIR_CONFIG. */
         if (no_of_local_dirs > 1)
         {
            update_db_log(INFO_SIGN, NULL, 0, debug_fp, NULL,
                          "Found %d directory entries with %d recipients in %d destinations.",
                          no_of_local_dirs, t_rc, t_dgc);
         }
         else if ((no_of_local_dirs == 1) && (t_rc == 1))
              {
                 update_db_log(INFO_SIGN, NULL, 0, debug_fp, NULL,
                               "Found one directory entry with %d recipient in %d destination.",
                                t_rc, t_dgc);
              }
         else if ((no_of_local_dirs == 1) && (t_rc > 1) && (t_dgc == 1))
              {
                 update_db_log(INFO_SIGN, NULL, 0, debug_fp, NULL,
                                "Found one directory entry with %d recipients in %d destination.",
                                t_rc, t_dgc);
              }
              else
              {
                 update_db_log(INFO_SIGN, NULL, 0, debug_fp, NULL,
                               "Found %d directory entry with %d recipients in %d destinations.",
                               no_of_local_dirs, t_rc, t_dgc);
              }
         ret = SUCCESS;
      }
#ifdef WITH_ONETIME
   }
   else
   {
      copy_to_file(YES);
   }
#endif

#ifdef WITH_ONETIME
   if (onetime == NO)
   {
#endif
      /* Release directory name buffer for FD. */
#ifdef LOCK_DEBUG
      unlock_region(dnb_fd, 1, __FILE__, __LINE__);
#else
      unlock_region(dnb_fd, 1);
#endif
#ifdef WITH_ONETIME
   }
#endif

   /* Free all memory we allocated. */
   free((void *)dd);
   free(p_t);
   free(pp);
   free((void *)dir);
   if (pwb != NULL)
   {
      unmap_data(pwb_fd, (void *)&pwb);
   }

   return(ret);
}


/*++++++++++++++++++++++++ expand_file_filter() ++++++++++++++++++++++++*/
static void
expand_file_filter(struct dir_group *dir,
                   int              *total_length,
                   char             *dir_config_file,
                   char             *database,
                   char             *search_ptr,
                   unsigned int     *warn_counter,
                   FILE             *debug_fp)
{
   int  file_group_type,
        i = 0;
   char closing_bracket,
        group_name[MAX_GROUPNAME_LENGTH],
        *ptr;

   if (dir->file[dir->fgc].files[*total_length + 1] == CURLY_BRACKET_OPEN)
   {
      closing_bracket = CURLY_BRACKET_CLOSE;
      file_group_type = YES;
   }
   else
   {
      closing_bracket = SQUARE_BRACKET_CLOSE;
      file_group_type = NO;
   }
   ptr = &dir->file[dir->fgc].files[*total_length + 2];

   while ((*ptr != closing_bracket) && (*ptr != '\0'))
   {
      group_name[i] = *ptr;
      ptr++; i++;
   }

   if (*ptr == closing_bracket)
   {
      group_name[i] = '\0';
      get_file_group(group_name, file_group_type, dir, total_length);
   }
   else
   {
      /* No end marker, so just take it as a normal file entry. */
      /* This should not happen because we did the check before */
      /* calling this function.                                 */
      update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                    "In %s line %d, no closing bracket found.",
                    dir_config_file,
                    count_new_lines(database, search_ptr));
   }

   return;
}


/*+++++++++++++++++++++++++ insert_hostname() ++++++++++++++++++++++++++*/
static void
insert_hostname(struct dir_group *dir)
{
   register int i, j, k;

   for (i = 0; i < dir->fgc; i++)
   {
      for (j = 0; j < dir->file[i].dgc; j++)
      {
         for (k = 0; k < dir->file[i].dest[j].rc; k++)
         {
            (void)check_hostname_list(dir->file[i].dest[j].rec[k].recipient,
                                      dir->file[i].dest[j].rec[k].real_hostname,
                                      dir->file[i].dest[j].rec[k].host_alias,
                                      dir->file[i].dest[j].rec[k].scheme,
                                      SEND_FLAG);
         }
      }
   }

   return;
}


/*------------------------- check_hostname_list() -----------------------*/
static int
check_hostname_list(char         *recipient,
                    char         *real_hostname,
                    char         *host_alias,
                    unsigned int scheme,
                    unsigned int flag)
{
   int  i;
   char new;

   /* Check if host already exists. */
   new = YES;
   for (i = 0; i < no_of_hosts; i++)
   {
      if (CHECK_STRCMP(hl[i].host_alias, host_alias) == 0)
      {
         new = NO;

         if (hl[i].fullname[0] == '\0')
         {
            (void)strcpy(hl[i].fullname, real_hostname);
         }
         hl[i].in_dir_config = YES;
         hl[i].protocol |= (scheme | flag);
         break;
      }
   }

   /* Add host to host list if it is new. */
   if (new == YES)
   {
      /* Check if buffer for host list structure is large enough. */
      if ((no_of_hosts % HOST_BUF_SIZE) == 0)
      {
         int new_size,
             offset;

         /* Calculate new size for host list. */
         new_size = ((no_of_hosts / HOST_BUF_SIZE) + 1) *
                    HOST_BUF_SIZE * sizeof(struct host_list);

         /* Now increase the space. */
         if ((hl = realloc(hl, new_size)) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Could not reallocate memory for host list : %s",
                       strerror(errno));
            exit(INCORRECT);
         }

         /* Initialise the new memory area. */
         new_size = HOST_BUF_SIZE * sizeof(struct host_list);
         offset = (no_of_hosts / HOST_BUF_SIZE) * new_size;
         (void)memset((char *)hl + offset, 0, new_size);
      }

      /* Store host data. */
      (void)strcpy(hl[no_of_hosts].host_alias, host_alias);
      (void)strcpy(hl[no_of_hosts].fullname, real_hostname);
      hl[no_of_hosts].real_hostname[0][0] = '\0';
      hl[no_of_hosts].real_hostname[1][0] = '\0';
      hl[no_of_hosts].host_toggle_str[0]  = '\0';
      hl[no_of_hosts].proxy_name[0]       = '\0';
      hl[no_of_hosts].allowed_transfers   = default_no_parallel_jobs;
      hl[no_of_hosts].max_errors          = default_max_errors;
      hl[no_of_hosts].retry_interval      = default_retry_interval;
      hl[no_of_hosts].transfer_blksize    = default_transfer_blocksize;
      hl[no_of_hosts].successful_retries  = default_successful_retries;
      hl[no_of_hosts].file_size_offset    = DEFAULT_FILE_SIZE_OFFSET;
      hl[no_of_hosts].transfer_timeout    = default_transfer_timeout;
      hl[no_of_hosts].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
      hl[no_of_hosts].in_dir_config       = YES;
      hl[no_of_hosts].protocol            = scheme | flag;
      hl[no_of_hosts].protocol_options    = DEFAULT_PROTOCOL_OPTIONS;
      hl[no_of_hosts].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;
      hl[no_of_hosts].transfer_rate_limit = 0;
      hl[no_of_hosts].host_status         = default_error_offline_flag;
      i = no_of_hosts;
      no_of_hosts++;
   }
   return(i);
}


/*++++++++++++++++++++++++++++ insert_dir() ++++++++++++++++++++++++++++*/
static void
insert_dir(struct dir_group *dir)
{
   int i,
       j;

   for (i = 0; i < dir->fgc; i++)
   {
      for (j = 0; j < dir->file[i].dgc; j++)
      {
         copy_job(i, j, dir);
      } /* Next destination group. */
   } /* Next file group. */

   return;
}


/*----------------------------- copy_job() -----------------------------*/
/*                              ----------                              */
/* Description: This function copies one job into the temporary memory  */
/*              p_t created in the top level function eval_dir_config().*/
/*              Each recipient in the DIR_CONFIG is counted as one job. */
/*              Data is stored as followed: job type, priority,         */
/*              directory, no. of files/filters, file/filters, number   */
/*              of local options, local options, number of standard     */
/*              options, standard options and recipient. Additionally a */
/*              pointer array is created that always keeps a pointer    */
/*              offset to each of the above listed items. Since all     */
/*              data for each recipient in a recipient group is the     */
/*              same, except for the recipient itself, we just store a  */
/*              new pointer array for each new recipient.               */
/*----------------------------------------------------------------------*/
static void
copy_job(int file_no, int dest_no, struct dir_group *dir)
{
   int            i, j, k,
                  offset,
                  options,       /* Counts no. of local options found.  */
                  priority,
                  /*
                   * NOTE: TIME_NO_COLLECT_ID and TIMEZONE_ID option __must__
                   *       be checked before TIME_ID. Since both start with
                   *       time and TIME_ID only consists only of the word
                   *       time.
                   */
                  loption_length[LOCAL_OPTION_POOL_SIZE] =
                  {
                     RENAME_ID_LENGTH,
                     SRENAME_ID_LENGTH,
                     EXEC_ID_LENGTH,
                     TIMEZONE_ID_LENGTH,
                     TIME_NO_COLLECT_ID_LENGTH,
                     TIME_ID_LENGTH,
                     BASENAME_ID_LENGTH,
                     EXTENSION_ID_LENGTH,
                     ADD_PREFIX_ID_LENGTH,
                     DEL_PREFIX_ID_LENGTH,
                     TOUPPER_ID_LENGTH,
                     TOLOWER_ID_LENGTH,
#ifdef _WITH_AFW2WMO
                     AFW2WMO_ID_LENGTH,
#endif
                     FAX2GTS_ID_LENGTH,
                     TIFF2GTS_ID_LENGTH,
                     GTS2TIFF_ID_LENGTH,
                     GRIB2WMO_ID_LENGTH,
                     EXTRACT_ID_LENGTH,
                     ASSEMBLE_ID_LENGTH,
                     WMO2ASCII_ID_LENGTH,
                     DELETE_ID_LENGTH,
                     CONVERT_ID_LENGTH,
                     LCHMOD_ID_LENGTH
                  };
   unsigned int   full_job_size,
                  loptions_flag[LOCAL_OPTION_POOL_SIZE] =
                  {
                     RENAME_ID_FLAG,
                     SRENAME_ID_FLAG,
                     EXEC_ID_FLAG,
                     TIMEZONE_ID_FLAG,
                     TIME_NO_COLLECT_ID_FLAG,
                     TIME_ID_FLAG,
                     BASENAME_ID_FLAG,
                     EXTENSION_ID_FLAG,
                     ADD_PREFIX_ID_FLAG,
                     DEL_PREFIX_ID_FLAG,
                     TOUPPER_ID_FLAG,
                     TOLOWER_ID_FLAG,
#ifdef _WITH_AFW2WMO
                     AFW2WMO_ID_FLAG,
#endif
                     FAX2GTS_ID_FLAG,
                     TIFF2GTS_ID_FLAG,
                     GTS2TIFF_ID_FLAG,
                     GRIB2WMO_ID_FLAG,
                     EXTRACT_ID_FLAG,
                     ASSEMBLE_ID_FLAG,
                     WMO2ASCII_ID_FLAG,
                     DELETE_ID_FLAG,
                     CONVERT_ID_FLAG,
                     LCHMOD_ID_FLAG
                  },
                  options_flag;
   size_t         new_size;
   char           buffer[MAX_INT_LENGTH],
                  *ptr,            /* Pointer where data is to be       */
                                   /* stored.                           */
                  *p_start,        /* Points to start of local options. */
                  *p_offset,
                  *p_loption[LOCAL_OPTION_POOL_SIZE] =
                  {
                     RENAME_ID,
                     SRENAME_ID,
                     EXEC_ID,
                     TIMEZONE_ID,
                     TIME_NO_COLLECT_ID,
                     TIME_ID,
                     BASENAME_ID,
                     EXTENSION_ID,
                     ADD_PREFIX_ID,
                     DEL_PREFIX_ID,
                     TOUPPER_ID,
                     TOLOWER_ID,
#ifdef _WITH_AFW2WMO
                     AFW2WMO_ID,
#endif
                     FAX2GTS_ID,
                     TIFF2GTS_ID,
                     GTS2TIFF_ID,
                     GRIB2WMO_ID,
                     EXTRACT_ID,
                     ASSEMBLE_ID,
                     WMO2ASCII_ID,
                     DELETE_ID,
                     CONVERT_ID,
                     LCHMOD_ID
                  };
   struct p_array *p_ptr;

   /* Check if the buffer for pointers is large enough. */
   if ((job_no % PTR_BUF_SIZE) == 0)
   {
      /* Calculate new size of pointer buffer. */
      new_size = ((job_no / PTR_BUF_SIZE) + 1) *
                 PTR_BUF_SIZE * sizeof(struct p_array);

      /* Increase the space for pointers by PTR_BUF_SIZE. */
      if ((pp = realloc(pp, new_size)) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not allocate memory for pointer buffer : %s",
                    strerror(errno));
         exit(INCORRECT);
      }
   }

   /* Check if the buffer for storing the actual data is large enough. */
   if (dest_no == 0)
   {
      char *p_file = dir->file[file_no].files;

      for (i = 0; i < dir->file[file_no].fc; i++)
      {
         NEXT(p_file);
      }
      offset = p_file - dir->file[file_no].files;
   }
   else
   {
      offset = -1;
   }
   full_job_size = sizeof(char) +                 /* Priority. */
                   MAX_PATH_LENGTH +              /* Directory. */
                   MAX_DIR_ALIAS_LENGTH + 1 +     /* Dir alias. */
                   MAX_INT_LENGTH +               /* File filter counter. */
                   offset + 1 +                   /* File filters. */
                   (dir->file[file_no].dest[dest_no].oc *
                    MAX_OPTION_LENGTH) +          /* Local options + Standart options. */
                   MAX_INT_HEX_LENGTH +           /* Number of local options. */
                   MAX_INT_LENGTH +               /* Number of standart options. */
                   (dir->file[file_no].dest[dest_no].rc *
                    (MAX_RECIPIENT_LENGTH + 1 +   /* Recipient. */
                     MAX_INT_LENGTH +             /* Scheme. */
                     MAX_HOSTNAME_LENGTH + 1)) +  /* Host alias. */
                   MAX_INT_HEX_LENGTH;            /* DIR_CONFIG ID. */
   if (data_alloc_size < (data_length + full_job_size))
   {
      /* Calculate new size of pointer buffer. */
      new_size = data_length + (10 * full_job_size);

      /* Increase the space for pointers by PTR_BUF_SIZE. */
      if ((p_t = realloc(p_t, new_size)) == NULL)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not allocate memory for job data buffer : %s",
                    strerror(errno));
         exit(INCORRECT);
      }

      system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
# if SIZEOF_SIZE_T == 4
                 "Resizing data buffer from %ld to %ld (job size = %u)",
# else
                 "Resizing data buffer from %ld to %lld (job size = %u)",
# endif
#else
# if SIZEOF_SIZE_T == 4
                 "Resizing data buffer from %lld to %ld (job size = %u)",
# else
                 "Resizing data buffer from %lld to %lld (job size = %u)",
# endif
#endif
                 (pri_off_t)data_alloc_size,
                 (pri_size_t)new_size, full_job_size);

      data_alloc_size = new_size;
   }

   /* Set pointer array. */
   p_ptr = pp;

   /* Position the data pointer. */
   ptr = p_t + data_length;
   p_offset = p_t;

   /* Insert priority. */
   priority = DEFAULT_PRIORITY;
   for (i = 0; i < dir->file[file_no].dest[dest_no].oc; i++)
   {
      if (CHECK_STRNCMP(dir->file[file_no].dest[dest_no].options[i],
                        PRIORITY_ID, PRIORITY_ID_LENGTH) == 0)
      {
         char *tmp_ptr = &dir->file[file_no].dest[dest_no].options[i][PRIORITY_ID_LENGTH];

         while ((*tmp_ptr == ' ') || (*tmp_ptr == '\t'))
         {
            tmp_ptr++;
         }
         if (isdigit((int)(*tmp_ptr)))
         {
            priority = *tmp_ptr;
         }

         /* Remove priority, since it is no longer needed. */
         for (j = i; j <= dir->file[file_no].dest[dest_no].oc; j++)
         {
            (void)strcpy(dir->file[file_no].dest[dest_no].options[j],
                         dir->file[file_no].dest[dest_no].options[j + 1]);
         }
         dir->file[file_no].dest[dest_no].oc--;

         break;
      }
   }
   *ptr = priority;
   p_ptr[job_no].ptr[PRIORITY_PTR_POS] = ptr - p_offset;
   ptr++;

   /* Insert directory. */
   if ((file_no == 0) && (dest_no == 0))
   {
      p_ptr[job_no].ptr[DIRECTORY_PTR_POS] = ptr - p_offset;
      ptr += snprintf(ptr, MAX_PATH_LENGTH, "%s", dir->location) + 1;

      p_ptr[job_no].ptr[ALIAS_NAME_PTR_POS] = ptr - p_offset;
      ptr += snprintf(ptr, MAX_DIR_ALIAS_LENGTH + 1, "%s", dir->alias) + 1;
   }
   else
   {
      p_ptr[job_no].ptr[DIRECTORY_PTR_POS] = p_ptr[job_no - 1].ptr[DIRECTORY_PTR_POS]; /* Directory. */
      p_ptr[job_no].ptr[ALIAS_NAME_PTR_POS] = p_ptr[job_no - 1].ptr[ALIAS_NAME_PTR_POS]; /* Alias. */
   }

   /* Insert file masks. */
   p_ptr[job_no].ptr[NO_OF_FILES_PTR_POS] = ptr - p_offset;
   ptr += snprintf(ptr, MAX_INT_LENGTH, "%d", dir->file[file_no].fc) + 1;
   p_ptr[job_no].ptr[FILE_PTR_POS] = ptr - p_offset;
   if (dest_no == 0)
   {
      (void)memcpy(ptr, dir->file[file_no].files, offset);
      ptr += offset + 1;
   }
   else
   {
      p_ptr[job_no].ptr[FILE_PTR_POS] = p_ptr[job_no - 1].ptr[FILE_PTR_POS];
   }

   /* Insert local options. These are the options that AMG */
   /* has to handle. They are:                             */
   /*        - priority       (special option)             */
   /*        - rename                                      */
   /*        - srename                                     */
   /*        - exec, execd and execD                       */
   /*        - basename                                    */
   /*        - prefix                                      */
   /*        - tiff2gts and gts2tiff                       */
   /*        - assemble                                    */
   /*        - extract                                     */
   /*        - time                                        */
   /*        - toupper and tolower                         */
   /*        - delete                                      */
   p_ptr[job_no].ptr[NO_LOCAL_OPTIONS_PTR_POS] = ptr - p_offset;
   if (dir->file[file_no].dest[dest_no].oc > 0)
   {
      p_start = ptr;
      options = 0;
      options_flag = 0;
      for (i = 0; i < dir->file[file_no].dest[dest_no].oc; i++)
      {
         for (k = 0; k < LOCAL_OPTION_POOL_SIZE; k++)
         {
            if (CHECK_STRNCMP(dir->file[file_no].dest[dest_no].options[i],
                              p_loption[k], loption_length[k]) == 0)
            {
               /* Save the local option in shared memory region. */
               ptr += snprintf(ptr, MAX_OPTION_LENGTH, "%s",
                               dir->file[file_no].dest[dest_no].options[i]) + 1;
               options++;
               options_flag |= loptions_flag[k];

               /* Remove the option. */
               for (j = i; j <= dir->file[file_no].dest[dest_no].oc; j++)
               {
                  (void)strcpy(dir->file[file_no].dest[dest_no].options[j],
                               dir->file[file_no].dest[dest_no].options[j + 1]);
               }
               dir->file[file_no].dest[dest_no].oc--;
               i--;

               break;
            }
         }
      }
      if (options > 0)
      {
         ptr++;
         offset = snprintf(buffer, MAX_INT_LENGTH, "%d", options) + 1;

         /* Now move local options 'offset' bytes forward so we can */
         /* store the no. of local options before the actual data.  */
         memmove((p_start + offset), p_start, (ptr - p_start));
         ptr += (offset - 1);

         /* Store position of data for local options. */
         p_ptr[job_no].ptr[LOCAL_OPTIONS_PTR_POS] = p_start + offset - p_offset;

         /* Store number of local options. */
         (void)memcpy(p_start, buffer, offset);

         /* Insert local options flag. */
         p_ptr[job_no].ptr[LOCAL_OPTIONS_FLAG_PTR_POS] = ptr - p_offset;
         ptr += snprintf(ptr, MAX_INT_HEX_LENGTH, "%x", options_flag) + 1;
      }
      else
      {
         /* No local options. */
         *ptr = '0';
         *(ptr + 1) = '\0';
         ptr += 2;
         p_ptr[job_no].ptr[LOCAL_OPTIONS_PTR_POS] = -1;
         p_ptr[job_no].ptr[LOCAL_OPTIONS_FLAG_PTR_POS] = -1;
      }

      /* Insert standard options. */
      p_ptr[job_no].ptr[NO_STD_OPTIONS_PTR_POS] = ptr - p_offset;
      ptr += snprintf(ptr, MAX_INT_LENGTH, "%d",
                      dir->file[file_no].dest[dest_no].oc) + 1;
      p_ptr[job_no].ptr[STD_OPTIONS_PTR_POS] = ptr - p_offset;

      if (dir->file[file_no].dest[dest_no].oc > 0)
      {
         for (i = 0; i < dir->file[file_no].dest[dest_no].oc; i++)
         {
            ptr += snprintf(ptr, MAX_OPTION_LENGTH, "%s\n",
                            dir->file[file_no].dest[dest_no].options[i]);
         }
         *(ptr - 1) = '\0';

         /*
          * NOTE: Here we insert a new-line after each option, except
          *       for the the last one. When creating the message, all
          *       that needs to be done is to copy the string
          *       db[i].soptions. (see process dir_config)
          */
      }
      else /* No standard options. */
      {
         p_ptr[job_no].ptr[STD_OPTIONS_PTR_POS] = -1;
      }
   }
   else /* No local and standard options. */
   {
      /* No local options. */
      *ptr = '0';
      *(ptr + 1) = '\0';
      ptr += 2;
      p_ptr[job_no].ptr[LOCAL_OPTIONS_PTR_POS] = -1;
      p_ptr[job_no].ptr[LOCAL_OPTIONS_FLAG_PTR_POS] = -1;

      /* No standard options. */
      p_ptr[job_no].ptr[NO_STD_OPTIONS_PTR_POS] = ptr - p_offset;
      *ptr = '0';
      *(ptr + 1) = '\0';
      ptr += 2;
      p_ptr[job_no].ptr[STD_OPTIONS_PTR_POS] = -1;
   }

   /* Insert recipient. */
   p_ptr[job_no].ptr[RECIPIENT_PTR_POS] = ptr - p_offset;
   ptr += snprintf(ptr, MAX_RECIPIENT_LENGTH + 1, "%s",
                   dir->file[file_no].dest[dest_no].rec[0].recipient) + 1;

   /* Insert scheme. */
   p_ptr[job_no].ptr[SCHEME_PTR_POS] = ptr - p_offset;
   ptr += snprintf(ptr, MAX_INT_LENGTH, "%u",
                   dir->file[file_no].dest[dest_no].rec[0].scheme) + 1;

   /* Insert host alias. */
   p_ptr[job_no].ptr[HOST_ALIAS_PTR_POS] = ptr - p_offset;
   ptr += snprintf(ptr, MAX_HOSTNAME_LENGTH + 1, "%s",
                   dir->file[file_no].dest[dest_no].rec[0].host_alias) + 1;

   /* Insert DIR_CONFIG ID. */
   p_ptr[job_no].ptr[DIR_CONFIG_ID_PTR_POS] = ptr - p_offset;
   ptr += snprintf(ptr, MAX_INT_HEX_LENGTH, "%x", dir->dir_config_id) + 1;

   /* Increase job number. */
   job_no++;

   /*
    * Since each recipient counts as one job, create and store
    * the data for each recipient. To save memory lets not always
    * copy all the data. This can be done by just saving the
    * recipient, since this is the only information that differs
    * for this recipient group.
    */
   for (i = 1; i < dir->file[file_no].dest[dest_no].rc; i++)
   {
      /* Check if the buffer for pointers is large enough. */
      if ((job_no % PTR_BUF_SIZE) == 0)
      {
         /* Calculate new size of pointer buffer. */
         new_size = ((job_no / PTR_BUF_SIZE) + 1) *
                    PTR_BUF_SIZE * sizeof(struct p_array);

         /* Increase the space for pointers by PTR_BUF_SIZE. */
         if ((pp = realloc(pp, new_size)) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Could not allocate memory for pointer buffer : %s",
                       strerror(errno));
            exit(INCORRECT);
         }

         /* Set pointer array. */
         p_ptr = pp;
      }

      (void)memcpy(&p_ptr[job_no], &p_ptr[job_no - i], sizeof(struct p_array));
      p_ptr[job_no].ptr[RECIPIENT_PTR_POS] = ptr - p_offset;
      ptr += snprintf(ptr, MAX_RECIPIENT_LENGTH + 1, "%s",
                      dir->file[file_no].dest[dest_no].rec[i].recipient) + 1;
      p_ptr[job_no].ptr[SCHEME_PTR_POS] = ptr - p_offset;
      ptr += snprintf(ptr, MAX_INT_LENGTH, "%u",
                      dir->file[file_no].dest[dest_no].rec[i].scheme) + 1;
      p_ptr[job_no].ptr[HOST_ALIAS_PTR_POS] = ptr - p_offset;
      ptr += snprintf(ptr, MAX_HOSTNAME_LENGTH + 1, "%s",
                      dir->file[file_no].dest[dest_no].rec[i].host_alias) + 1;

      /* Increase job number. */
      job_no++;
   } /* for (i = 1; i < dir->file[file_no].dest[dest_no].rc; i++) */

   /* Save data length. */
   data_length = (ptr - p_offset) * sizeof(char);

   return;
}


/*+++++++++++++++++++++++++++++++ sort_jobs() +++++++++++++++++++++++++++*/
static void
sort_jobs(void)
{
   int            i, j, k, m;
   size_t         move_size_1,
                  move_size_2;
   char           *buffer,
                  *ptr;
   struct p_array *p_ptr;

   p_ptr = pp;
   for (i = 0; i < (job_no - 1); i++)
   {
      while (p_ptr[i].ptr[DIRECTORY_PTR_POS] == p_ptr[i + 1].ptr[DIRECTORY_PTR_POS])
      {
         i++;
      }
      for (j = (i + 1); j < job_no; j++)
      {
         if (my_strcmp(p_t + p_ptr[i].ptr[DIRECTORY_PTR_POS],
                       p_t + p_ptr[j].ptr[DIRECTORY_PTR_POS]) == 0)
         {
            int start_j = j;

            while ((j < (job_no - 1)) &&
                   (p_ptr[j].ptr[DIRECTORY_PTR_POS] == p_ptr[j + 1].ptr[DIRECTORY_PTR_POS]))
            {
               j++;
            }

            /* First copy the real data into its correct place. */
            if (p_ptr[j].ptr[DIR_CONFIG_ID_PTR_POS] > p_ptr[j].ptr[HOST_ALIAS_PTR_POS])
            {
               ptr = p_t + p_ptr[j].ptr[DIR_CONFIG_ID_PTR_POS];
            }
            else
            {
               ptr = p_t + p_ptr[j].ptr[HOST_ALIAS_PTR_POS];
            }
            while (*ptr != '\0')
            {
               ptr++;
            }
            move_size_1 = ptr + 1 - (p_t + p_ptr[start_j].ptr[PRIORITY_PTR_POS]);
            if ((buffer = malloc(move_size_1)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to malloc() %d bytes : %s",
                          move_size_1, strerror(errno));
               exit(INCORRECT);
            }
            (void)memcpy(buffer, (p_t + p_ptr[start_j].ptr[PRIORITY_PTR_POS]),
                         move_size_1);

            if (p_ptr[i].ptr[DIR_CONFIG_ID_PTR_POS] > p_ptr[i].ptr[HOST_ALIAS_PTR_POS])
            {
               ptr = p_t + p_ptr[i].ptr[DIR_CONFIG_ID_PTR_POS];
            }
            else
            {
               ptr = p_t + p_ptr[i].ptr[HOST_ALIAS_PTR_POS];
            }
            while (*ptr != '\0')
            {
               ptr++;
            }
            move_size_2 = p_t + p_ptr[start_j].ptr[PRIORITY_PTR_POS] - (ptr + 1);
            (void)memmove((p_t + p_ptr[i + 1].ptr[PRIORITY_PTR_POS] + move_size_1),
                          (p_t + p_ptr[i + 1].ptr[PRIORITY_PTR_POS]),
                          move_size_2);
            (void)memcpy((p_t + p_ptr[i + 1].ptr[PRIORITY_PTR_POS]),
                         buffer, move_size_1);

            /* Correct all pointer positions. */
            for (k = 0; k < (j + 1 - start_j); k++)
            {
               for (m = 0; m < MAX_DATA_PTRS; m++)
               {
                  p_ptr[start_j + k].ptr[m] -= move_size_2;
               }
            }
            for (k = (i + 1); k < start_j; k++)
            {
               for (m = 0; m < MAX_DATA_PTRS; m++)
               {
                  p_ptr[k].ptr[m] += move_size_1;
               }
            }

            /* Now copy the pointer array. */
            m = (j + 1 - start_j) * sizeof(struct p_array);
            if (move_size_1 < m)
            {
               if ((buffer = realloc(buffer, m)) == NULL)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             "Failed to realloc() %d bytes : %s",
                             m, strerror(errno));
                  exit(INCORRECT);
               }
            }
            move_size_1 = m;
            (void)memcpy(buffer, &p_ptr[start_j], move_size_1);
            move_size_2 = (start_j - (i + 1)) * sizeof(struct p_array);
            (void)memmove(&p_ptr[i + 1 + (j + 1 - start_j)], &p_ptr[i + 1],
                          move_size_2);
            (void)memcpy(&p_ptr[i + 1], buffer, move_size_1);

            /* Let all same directories point to the directory position,  */
            /* in case we do have more same directories. We do waste some */
            /* memory here but it is not worth the trouble resetting all  */
            /* pointers if we free the memory.                            */
            for (m = 0; m < (j + 1 - start_j); m++)
            {
               p_ptr[i + 1 + m].ptr[DIRECTORY_PTR_POS] = p_ptr[i].ptr[DIRECTORY_PTR_POS]; /* Directory. */
               p_ptr[i + 1 + m].ptr[ALIAS_NAME_PTR_POS] = p_ptr[i].ptr[ALIAS_NAME_PTR_POS]; /* Alias. */
            }

            /* Free memory for buffer. */
            free(buffer);

            /* No need to check those directories we just have moved. */
            i += (j - start_j + 1);
         }
         else
         {
            while (((j + 1) < job_no) &&
                   (p_ptr[j].ptr[DIRECTORY_PTR_POS] == p_ptr[j + 1].ptr[DIRECTORY_PTR_POS]))
            {
               j++;
            }
         }
      }
   }

   return;
}


/*+++++++++++++++++++++++++++++ copy_to_file() ++++++++++++++++++++++++++*/
/*                              --------------                           */
/* Description: Creates a file and copies the number of                  */
/*              jobs, the pointer array and the data into it.            */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
#ifdef WITH_ONETIME
copy_to_file(int onetime)
#else
copy_to_file(void)
#endif
{
   /* First test if there is any data at all for this job type. */
   if (data_length > 0)
   {
      int            fd,
                     i,
                     loops,
                     rest,
                     size_ptr_array;
      size_t         size;
      char           *ptr,
                     *p_data,
                     *p_mmap,
                     buffer[4096],
                     amg_data_file[MAX_PATH_LENGTH],
                     tmp_amg_data_file[MAX_PATH_LENGTH];
      struct p_array *p_ptr;

      /* First calculate the size for each region. */
      size_ptr_array = job_no * sizeof(struct p_array);
      size = sizeof(int) + data_length + size_ptr_array + sizeof(char);

      /*
       * In case some forked dir_check process is still using
       * the old data lets rename it so it can still be used. It can
       * also be very handy when the disk is full. This is really not
       * necessary, however the overhead is very small, so lets just do it.
       */
#ifdef WITH_ONETIME
     if (onetime == YES)
     {
         ptr = tmp_amg_data_file + snprintf(tmp_amg_data_file, MAX_PATH_LENGTH,
                                            "%s%s%s.%u", p_work_dir,
                                            AFD_MSG_DIR, AMG_ONETIME_DATA_FILE,
                                            onetime_jid);
     }
     else
     {
#endif
         ptr = tmp_amg_data_file + snprintf(tmp_amg_data_file, MAX_PATH_LENGTH,
                                            "%s%s%s", p_work_dir, FIFO_DIR,
                                            AMG_DATA_FILE);
#ifdef WITH_ONETIME
     }
#endif
      (void)strcpy(amg_data_file, tmp_amg_data_file);
      *ptr = '.';
      *(ptr + 1) = 't';
      *(ptr + 2) = 'm';
      *(ptr + 3) = 'p';
      *(ptr + 4) = '\0';
      if ((rename(amg_data_file, tmp_amg_data_file) == -1) &&
          (errno != ENOENT))
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to rename() %s to %s : %s",
                    amg_data_file, tmp_amg_data_file, strerror(errno));
      }

      /* Create a new mmap file to store all data for dir_check. */
      if ((fd = open(amg_data_file, (O_RDWR | O_CREAT | O_TRUNC),
#ifdef GROUP_CAN_WRITE
                     (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))) == -1)
#else
                     (S_IRUSR | S_IWUSR))) == -1)
#endif
      {
         if (errno == ENOSPC)
         {
            (void)unlink(tmp_amg_data_file);
            tmp_amg_data_file[0] = '\0';
            if ((fd = open(amg_data_file, (O_RDWR | O_CREAT | O_TRUNC),
#ifdef GROUP_CAN_WRITE
                           (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP))) == -1)
#else
                           (S_IRUSR | S_IWUSR))) == -1)
#endif
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to open() %s : %s",
                          amg_data_file, strerror(errno));
               exit(INCORRECT);
            }
         }
         else
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to open() %s : %s",
                       amg_data_file, strerror(errno));
            exit(INCORRECT);
         }
      }
      (void)memset(buffer, 0, 4096);
      loops = size / 4096;
      rest = size % 4096;
      for (i = 0; i < loops; i++)
      {
         if (write(fd, buffer, 4096) != 4096)
         {
            if ((tmp_amg_data_file[0] != '\0') && (errno == ENOSPC))
            {
               (void)unlink(tmp_amg_data_file);
               tmp_amg_data_file[0] = '\0';
               if (write(fd, buffer, 4096) != 4096)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             "Failed to write() to `%s' : %s",
                             amg_data_file, strerror(errno));
                  exit(INCORRECT);
               }
            }
            else
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to write() to `%s' : %s",
                          amg_data_file, strerror(errno));
               exit(INCORRECT);
            }
         }
      }
      if (rest > 0)
      {
         if (write(fd, buffer, rest) != rest)
         {
            if ((tmp_amg_data_file[0] != '\0') && (errno == ENOSPC))
            {
               (void)unlink(tmp_amg_data_file);
               tmp_amg_data_file[0] = '\0';
               if (write(fd, buffer, rest) != rest)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             "Failed to write() to `%s' : %s",
                             amg_data_file, strerror(errno));
                  exit(INCORRECT);
               }
            }
            else
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to write() to `%s' : %s",
                          amg_data_file, strerror(errno));
               exit(INCORRECT);
            }
         }
      }
#ifdef HAVE_MMAP
      if ((p_mmap = mmap(NULL, size, (PROT_READ | PROT_WRITE),
                         MAP_SHARED, fd, 0)) == (caddr_t) -1)
#else
      if ((p_mmap = mmap_emu(NULL, size, (PROT_READ | PROT_WRITE),
                             MAP_SHARED, amg_data_file, 0)) == (caddr_t) -1)
#endif
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to mmap() %s : %s", amg_data_file, strerror(errno));
         exit(INCORRECT);
      }

      ptr = p_mmap;
      p_ptr = pp;
      p_data = p_t;

      /* Copy number of pointers into shared memory. */
      *(int*)ptr = job_no;
      ptr += sizeof(int);

      /* Copy pointer array into shared memory. */
      (void)memcpy(ptr, p_ptr, size_ptr_array);
      ptr += size_ptr_array;

      /* Copy data into shared memory region. */
      (void)memcpy(ptr, p_data, data_length);

#ifdef HAVE_MMAP
      if (munmap(p_mmap, size) == -1)
#else
      if (munmap_emu((void *)p_mmap) == -1)
#endif
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Could not munmap() from %s : %s",
                    amg_data_file, strerror(errno));
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }

      /*
       * NOTE: Do not delete tmp_amg_data_file, it can be very useful
       *       if the disk is full.
       */
   }

   return;
}


/*++++++++++++++++++++++++ count_new_lines() ++++++++++++++++++++++++++++*/
static int
count_new_lines(char *p_start, char *p_end)
{
   int line_counter = 0;

   while (p_start != p_end)
   {
      if (*p_start == '\n')
      {
         line_counter++;
      }
      p_start++;
   }
   if (*p_start == '\n')
   {
      line_counter++;
   }

   return(line_counter);
}


/*+++++++++++++++++++++++++ optimise_dir() ++++++++++++++++++++++++++++++*/
static int
optimise_dir(char *path)
{
   int  modified = NO;
   char *read_ptr,
        resolved_path[MAX_PATH_LENGTH],
        *write_ptr;

   write_ptr = resolved_path;
   read_ptr = path;

   /* Expand each slash-separated pathname component. */
   while (*read_ptr != '\0')
   {
      /* Ignore stray "/". */
      if (*read_ptr == '/')
      {
         if (read_ptr[1] == '/')
         {
            modified = YES;
            if (read_ptr == path)
            {
               *(write_ptr++) = *read_ptr;
            }
         }
         else if ((read_ptr[1] == '\0') || (read_ptr == path))
              {
                 *(write_ptr++) = *read_ptr;
              }
         read_ptr++;
         continue;
      }

      if (*read_ptr == '.')
      {
         /* Ignore ".". */
         if ((read_ptr[1] == '/') || (read_ptr[1] == '\0'))
         {
            read_ptr++;
            modified = YES;
            continue;
         }

         if (read_ptr[1] == '.')
         {
            if ((read_ptr[2] == '/') || (read_ptr[2] == '\0'))
            {
               read_ptr += 2;
               modified = YES;

               /* Ignore ".." at root. */
               if (write_ptr == (resolved_path + 1))
               {
                  continue;
               }

               /* Handle ".." by backing up. */
               while ((--write_ptr)[-1] != '/')
               {
                  ;
               }
               continue;
            }
         }
      }

      /* Safely copy the next pathname component. */
      while ((*read_ptr != '/') && (*read_ptr != '\0'))
      {
         *(write_ptr++) = *(read_ptr++);
      }

      *(write_ptr++) = '/';
   }

   /* Delete trailing slash but don't whomp a lone slash. */
   if ((write_ptr != (resolved_path + 1)) && (write_ptr[-1] == '/'))
   {
      write_ptr--;
   }

   if (modified == YES)
   {
      modified = write_ptr - resolved_path + 1;

      /* Make sure it's null terminated. */
      *write_ptr = '\0';

      (void)memcpy(path, resolved_path, modified);
   }
   else
   {
      modified = write_ptr - resolved_path + 1;
   }

   return(modified);
}


/*########################### posi_identifier() #########################*/
static char *
posi_identifier(char *search_text, char *search_string, size_t string_length)
{
   int  hit = 0;
   char *p_start_string = search_string,
        *p_start_text = search_text;

   while (*search_text != '\0')
   {
      if (*(search_text++) == *(search_string++))
      {
         if (++hit == string_length)
         {
            char *ptr = search_text - string_length;

            while ((ptr > p_start_text) && (*ptr != '\n') && (*ptr != '#'))
            {
               ptr--;
            }

            if (*ptr != '#')
            {
               return(++search_text);
            }
            search_text++;
            hit = 0;
            search_string = p_start_string;
         }
      }
      else
      {
         if ((hit == 1) && (*(search_string - 2) == *(search_text - 1)))
         {
            search_string--;
         }
         else
         {
            search_string -= hit + 1;
            hit = 0;
         }
      }
   }

   return(NULL); /* Found nothing. */
}
