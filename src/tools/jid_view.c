/*
 *  jid_view.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   jid_view - shows all jobs that are held by the AFD
 **
 ** SYNOPSIS
 **   jid_view [-w <AFD work dir>] [--version] [<job ID> [...<job ID n>]]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.02.1998 H.Kiehl Created
 **   15.05.1999 H.Kiehl Option to show only one job.
 **   24.06.2000 H.Kiehl Completely revised.
 **   30.12.2003 H.Kiehl File masks are now in a seperate file.
 **   05.01.2004 H.Kiehl Show DIR_CONFIG ID.
 **   12.09.2007 H.Kiehl Show directory name and make it possible to
 **                      supply multiple job ID's.
 **   07.11.2014 H.Kiehl Added more details so it can be used via the
 **                      view_dc dialog.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                 /* atoi()                            */
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>                   /* time(), strftime()                */
#include <fcntl.h>                  /* open()                            */
#include <unistd.h>                 /* fstat(), lseek(), write()         */
#include <sys/mman.h>               /* mmap()                            */
#include <errno.h>
#include "permission.h"
#include "amgdefs.h"
#ifdef WITH_AFD_MON
# include "aldadefs.h"
#endif
#include "version.h"

/* Global variables. */
int                        fra_fd = -1,
                           fsa_fd = -1,
                           fra_id,
                           fsa_id,
                           max_hostname_length = MAX_HOSTNAME_LENGTH,
                           no_of_dirs = 0,
                           no_of_hosts = 0,
                           sys_log_fd = STDERR_FILENO;
#ifdef WITH_AFD_MON
unsigned int               adl_entries = 0,
                           ahl_entries = 0;
#endif
char                       *p_work_dir = NULL;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
const char                 *sys_log_name = SYSTEM_LOG_FIFO;
struct fileretrieve_status *fra = NULL;
struct filetransfer_status *fsa = NULL;
#ifdef WITH_AFD_MON
struct jid_data            jidd;
struct afd_dir_list        *adl = NULL;
struct afd_host_list       *ahl = NULL;
struct afd_typesize_data   *atd = NULL;
#endif


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                     dir_config_view_mode,
                           fd,
                           fml_offset,
                           i,
                           mask_offset,
                           no_of_dc_ids,
                           no_of_dir_names,
                           no_of_file_masks_id,
                           no_of_job_ids,
                           no_of_search_ids,
                           view_passwd = NO;
   unsigned int            *job_id;
   off_t                   dcl_size = 0,
                           dnb_size = 0,
                           fmd_size = 0,
                           jid_size = 0;
#ifdef WITH_AFD_MON
   char                    afd_alias[MAX_AFDNAME_LENGTH + 1];
#endif
   char                    fake_user[MAX_FULL_USER_ID_LENGTH],
                           file[MAX_PATH_LENGTH + FIFO_DIR_LENGTH + MAX_FILENAME_LENGTH],
                           *fmd = NULL,
                           option_buffer[MAX_OPTION_LENGTH],
                           *perm_buffer,
                           *ptr,
                           profile[MAX_PROFILE_NAME_LENGTH + 1],
                           tmp_value[MAX_PATH_LENGTH],
                           work_dir[MAX_PATH_LENGTH];
#ifdef HAVE_STATX
   struct statx            stat_buf;
#else
   struct stat             stat_buf;
#endif
   struct job_id_data     *jd;
   struct dir_name_buf    *dnb = NULL;
   struct dir_config_list *dcl = NULL;
#ifdef WITH_AFD_MON
   struct afd_dir_list    *adl = NULL;
#endif
   struct dir_options     d_o;

   if ((get_arg(&argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      (void)fprintf(stdout,
                    _("Usage: %s [-w <AFD work dir>] [-u <fake user>] [-p <user profile>] [--version] [-r <remote AFD alias>] [<job ID> [...<job ID n>]]\n"),
                    argv[0]);
      exit(SUCCESS);
   }

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;
   if (get_arg(&argc, argv, "-p", profile, MAX_PROFILE_NAME_LENGTH) == INCORRECT)
   {
      profile[0] = '\0';
   }
#ifdef WITH_AFD_MON
   if (get_arg(&argc, argv, "-r", afd_alias, MAX_AFDNAME_LENGTH) == INCORRECT)
   {
      afd_alias[0] = '\0';
   }
#endif
   if (get_arg(&argc, argv, "--dir_config", NULL, 0) == SUCCESS)
   {
      dir_config_view_mode = YES;
   }
   else
   {
      dir_config_view_mode = NO;
   }

   /* Check if user may view the password. */
   check_fake_user(&argc, argv, AFD_CONFIG_FILE, fake_user);
   switch (get_permissions(&perm_buffer, fake_user, profile))
   {
      case NO_ACCESS : /* Cannot access afd.users file. */
         {
            char afd_user_file[MAX_PATH_LENGTH];

            (void)strcpy(afd_user_file, p_work_dir);                       
            (void)strcat(afd_user_file, ETC_DIR);   
            (void)strcat(afd_user_file, AFD_USER_FILE);

            (void)fprintf(stderr,
                          _("Failed to access `%s', unable to determine users permissions.\n"),
                          afd_user_file);                                                      
         }
         exit(INCORRECT);

      case NONE :
         (void)fprintf(stderr, "%s (%s %d)\n",
                       PERMISSION_DENIED_STR, __FILE__, __LINE__);
         exit(INCORRECT);

      case SUCCESS :
         /* Lets evaluate the permissions and see what */
         /* the user may do.                           */
         if ((perm_buffer[0] == 'a') &&
             (perm_buffer[1] == 'l') &&
             (perm_buffer[2] == 'l') &&
             ((perm_buffer[3] == '\0') ||
             (perm_buffer[3] == ',') ||
             (perm_buffer[3] == ' ') ||        
             (perm_buffer[3] == '\t')))
         {
            view_passwd = YES;
            free(perm_buffer);
            break;
         }
         else
         {
            if (lposi(perm_buffer, VIEW_DIR_CONFIG_PERM,
                      VIEW_DIR_CONFIG_PERM_LENGTH) == NULL)
            {
               (void)fprintf(stderr, "%s (%s %d)\n",
                             PERMISSION_DENIED_STR, __FILE__, __LINE__);
               exit(INCORRECT);
            }
            if (lposi(perm_buffer, VIEW_PASSWD_PERM,
                      VIEW_PASSWD_PERM_LENGTH) != NULL)
            {
               view_passwd = YES;
            }
         }
         free(perm_buffer);
         break;

      case INCORRECT:
         break;

      default :
         (void)fprintf(stderr, _("Impossible!! Remove the programmer!\n"));
         exit(INCORRECT);
   }

   if (argc > 1)
   {
      no_of_search_ids = argc - 1;
      if ((job_id = malloc(no_of_search_ids * sizeof(unsigned int))) != NULL)
      {
         for (i = 0; i < no_of_search_ids; i++)
         {
            job_id[i] = (unsigned int)strtoul(argv[i + 1], (char **)NULL, 16);
         }
      }
      else
      {
         (void)fprintf(stderr, _("malloc() error : %s (%s %d)\n"),
                       strerror(errno), __FILE__, __LINE__);
      }
   }
   else
   {
      no_of_search_ids = 0;
      job_id = NULL;
   }

#ifdef WITH_AFD_MON
   if (afd_alias[0] != '\0')
   {
      attach_ahl(afd_alias);
      attach_atd(afd_alias);
      alloc_jid(afd_alias);

      if (jidd.no_of_job_ids > 0)
      {
         int  j = 0,
              lines_drawn = 0;
         char *host_alias_destination,
              *host_alias_source;
#ifdef _NEW_JID
         char time_str[25];
#endif

         attach_adl(afd_alias);

         if ((host_alias_destination = malloc(max_hostname_length + 1)) == NULL)
         {
            (void)fprintf(stderr, _("malloc() error : %s (%s %d)\n"),
                          strerror(errno), __FILE__, __LINE__);
         }
         if ((host_alias_source = malloc(max_hostname_length + 1)) == NULL)
         {
            (void)fprintf(stderr, _("malloc() error : %s (%s %d)\n"),
                          strerror(errno), __FILE__, __LINE__);
         }

         for (i = 0; i < jidd.no_of_job_ids; i++)
         {
            if (no_of_search_ids > 0)
            {
               for (j = 0; j < no_of_search_ids; j++)
               {
                  if (job_id[j] == jidd.ajl[i].job_id)
                  {
                     break;
                  }
               }
               if (j == no_of_search_ids)
               {
                  continue;
               }
            }
            if ((job_id == NULL) || (job_id[j] == jidd.ajl[i].job_id))
            {
               (void)fprintf(stdout, "Job-ID          : %x\n", jidd.ajl[i].job_id);
#ifdef _NEW_JID
               (void)strftime(time_str, 25, "%c", localtime(&jidd.ajl[i].creation_time));
               (void)fprintf(stdout, "Creation time   : %s\n", time_str);
#endif
               (void)fprintf(stdout, "DIR_CONFIG-ID   : # Not available!\n");

               if (host_alias_destination != NULL)
               {
                  if (url_evaluate(jidd.ajl[i].recipient, NULL,
                                   NULL, NULL, NULL,
# ifdef WITH_SSH_FINGERPRINT
                                   NULL, NULL,
# endif
                                   NULL, NO, host_alias_destination, NULL,
                                   NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                                   NULL, NULL) < 4)
                  {
                     j = 0;
                     while ((host_alias_destination[j] != '\0') &&
                            (host_alias_destination[j] != '\n') &&
                            (host_alias_destination[j] != ':') &&
                            (host_alias_destination[j] != '.') &&
                            (j != max_hostname_length))
                     {
                        j++;
                     }
                     host_alias_destination[j] = '\0';
                  }
               }

               if ((dir_config_view_mode == YES) && (no_of_search_ids == 1))
               {
                  int adl_pos = 0,
                      gotcha = NO;

                  (void)fprintf(stdout, "File-Mask-ID    : # Not available!\n");
                  (void)fprintf(stdout, "Destination-ID  : # Not available!\n");
                  (void)fprintf(stdout, "Host-Alias-ID   : # Not available!\n");

                  if (adl != NULL)
                  {
                     for (j = 0; j < adl_entries; j++)
                     {
                        if (adl[j].dir_id == jidd.ajl[i].dir_id)
                        {
                           adl_pos = j;
                           gotcha = YES;
                           break;
                        }
                     }
                     if ((gotcha == YES) && (host_alias_source != NULL))
                     {
                        unsigned int scheme;

                        if ((url_evaluate(adl[adl_pos].orig_dir_name, &scheme,
                                          NULL, NULL, NULL,
# ifdef WITH_SSH_FINGERPRINT
                                          NULL, NULL,
# endif
                                          NULL, NO, host_alias_source, NULL,
                                          NULL, NULL, NULL, NULL, NULL, NULL,
                                          NULL, NULL, NULL) < 4) &&
                            (scheme != LOC_FLAG) && (scheme != UNKNOWN_FLAG))
                        {
                           j = 0;
                           while ((host_alias_source[j] != '\0') &&
                                  (host_alias_source[j] != '\n') &&
                                  (host_alias_source[j] != ':') &&
                                  (host_alias_source[j] != '.') &&
                                  (j != max_hostname_length))
                           {
                              j++;
                           }
                           host_alias_source[j] = '\0';
                        }
                        if ((scheme != LOC_FLAG) && (scheme != UNKNOWN_FLAG) &&
                            (host_alias_source[0] != '\0'))
                        {
                           for (j = 0; j < ahl_entries; j++)
                           {
                              if (my_strcmp(host_alias_source, ahl[j].host_alias) == 0)
                              {
                                 if (ahl[j].real_hostname[0][0] == GROUP_IDENTIFIER)
                                 {
                                    (void)fprintf(stdout, "Real hostname(S):\n");
                                 }
                                 else
                                 {
                                    /* Since we do not have the information */
                                    /* which host is currently active, show */
                                    /* them both.                           */
                                    (void)fprintf(stdout,
                                                  "Real hostname(S): %s",
                                                  ahl[j].real_hostname[0]);
                                    if (ahl[j].real_hostname[1][0] == '\0')
                                    {
                                       (void)fprintf(stdout, "\n");
                                    }
                                    else
                                    {
                                       (void)fprintf(stdout, " %s\n",
                                                     ahl[j].real_hostname[1]);
                                    }
                                 }
                                 break;
                              }
                           }
                        }
                     }

                     if ((ahl != NULL) && (host_alias_destination != NULL) &&
                         (host_alias_destination[0] != '\0'))
                     {
                        for (j = 0; j < ahl_entries; j++)
                        {
                           if (my_strcmp(host_alias_destination,
                                         ahl[j].host_alias) == 0)
                           {
                              if (ahl[j].real_hostname[0][0] == GROUP_IDENTIFIER)
                              {
                                 (void)fprintf(stdout, "Real hostname(D):\n");
                              }
                              else
                              {
                                 /* Since we do not have the information */
                                 /* which host is currently active, show */
                                 /* them both.                           */
                                 (void)fprintf(stdout, "Real hostname(D): %s",
                                               ahl[j].real_hostname[0]);
                                 if (ahl[j].real_hostname[1][0] == '\0')
                                 {
                                    (void)fprintf(stdout, "\n");
                                 }
                                 else
                                 {
                                    (void)fprintf(stdout, " %s\n",
                                                  ahl[j].real_hostname[1]);
                                 }
                              }
                              break;
                           }
                        }
                     }

                     (void)fprintf(stdout, "--------------------------------------------------------------------------------\n");

                     if (gotcha == YES)
                     {
                        (void)strcpy(tmp_value, adl[adl_pos].orig_dir_name);
                        url_insert_password(tmp_value, (view_passwd == YES) ? NULL : "XXXXX");
                        (void)fprintf(stdout, "%s %s\n%s\n",
                                      DIR_IDENTIFIER, adl[adl_pos].dir_alias,
                                      tmp_value);
                        if (CHECK_STRCMP(adl[adl_pos].dir_name,
                                         adl[adl_pos].orig_dir_name) != 0)
                        {
                           (void)fprintf(stdout, "# %s\n\n", adl[adl_pos].dir_name);
                        }
                        else
                        {
                           (void)fprintf(stdout, "\n");
                        }
                     }
                  }

                  (void)fprintf(stdout, "   %s\n", DIR_OPTION_IDENTIFIER);
                  (void)fprintf(stdout, "   # Not available\n");

                  (void)fprintf(stdout, "   %s\n   # Not available\n",
                                FILE_IDENTIFIER);

                  (void)fprintf(stdout, "\n      %s\n\n         %s\n",
                                DESTINATION_IDENTIFIER, RECIPIENT_IDENTIFIER);
                  (void)strcpy(tmp_value, jidd.ajl[i].recipient);
                  url_insert_password(tmp_value, (view_passwd == YES) ? NULL : "XXXXX");
                  (void)fprintf(stdout, "         %s\n", tmp_value);

                  /* Show all available options. */
                  (void)fprintf(stdout, "\n         %s\n         %s %c\n         # No further options available (no_of_loptions=%d)\n",
                                OPTION_IDENTIFIER, PRIORITY_ID,
                                jidd.ajl[i].priority,
                                jidd.ajl[i].no_of_loptions);

                  (void)fprintf(stdout, "\n");
               }
               else
               {
                  if (adl != NULL)
                  {
                     for (j = 0; j < adl_entries; j++)
                     {
                        if (adl[j].dir_id == jidd.ajl[i].dir_id)
                        {
                           (void)strcpy(tmp_value, adl[j].orig_dir_name);
                           url_insert_password(tmp_value, (view_passwd == YES) ? NULL : "XXXXX");
                           (void)fprintf(stdout, "Source-Directory: %s\n", tmp_value);
                           if (CHECK_STRCMP(adl[j].dir_name, adl[j].orig_dir_name) != 0)
                           {
                              (void)fprintf(stdout, "Local-Source-Dir: %s\n",
                                            adl[j].dir_name);
                           }
                           break;
                        }
                     }
                  }
                  (void)fprintf(stdout, "Dir-ID          : %x\n", jidd.ajl[i].dir_id);
                  (void)fprintf(stdout, "Dir position    : # Not available!\n");
                  (void)fprintf(stdout, "DIR-options     : # Not available!\n");

                  (void)fprintf(stdout, "File filters    : # Not available!\n");
                  (void)fprintf(stdout, "File-Mask-ID    : # Not available!\n");

                  (void)strcpy(tmp_value, jidd.ajl[i].recipient);
                  url_insert_password(tmp_value, (view_passwd == YES) ? NULL : "XXXXX");
                  (void)fprintf(stdout, "Destination     : %s\n", tmp_value);
                  (void)fprintf(stdout, "Destination-ID  : # Not available!\n");
                  (void)fprintf(stdout, "Host alias      : %s\n", host_alias_destination);
                  (void)fprintf(stdout, "Host-Alias-ID   : # Not available!\n");

                  if ((ahl != NULL) && (host_alias_destination != NULL) &&
                      (host_alias_destination[0] != '\0'))
                  {
                     for (j = 0; j < ahl_entries; j++)
                     {
                        if (my_strcmp(host_alias_destination,
                                      ahl[j].host_alias) == 0)
                        {
                           if (ahl[j].real_hostname[0][0] == GROUP_IDENTIFIER)
                           {
                              (void)fprintf(stdout, "Real hostname   :\n");
                           }
                           else
                           {
                              (void)fprintf(stdout, "Real hostname   : %s ",
                                            ahl[j].real_hostname[0]);
                              if (ahl[j].real_hostname[1][0] == '\0')
                              {
                                 (void)fprintf(stdout, "\n");
                              }
                              else
                              {
                                 (void)fprintf(stdout, " %s\n",
                                               ahl[j].real_hostname[1]);
                              }
                           }
                           break;
                        }
                     }
                  }

                  (void)fprintf(stdout, "Priority        : %c\n", jidd.ajl[i].priority);
                  if (((no_of_search_ids > 0) && ((lines_drawn + 1) < no_of_search_ids)) ||
                      ((no_of_search_ids == 0) && ((i + 1) < jidd.no_of_job_ids)))
                  {
                     (void)fprintf(stdout, "--------------------------------------------------------------------------------\n");
                     lines_drawn++;
                  }
               }
            }

            free(host_alias_source);
            free(host_alias_destination);
         }

         detach_adl();
         detach_ahl();
      }
      else
      {
         (void)fprintf(stdout, _("Job ID list is empty.\n"));
      }

      dealloc_jid();
      detach_atd();
   }
   else
   {
#endif /* WITH_AFD_MON */
      /* Map to JID file. */
      (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, JOB_ID_DATA_FILE);
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
      if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                      stat_buf.stx_size, PROT_READ,
# else
                      stat_buf.st_size, PROT_READ,
# endif
                      MAP_SHARED, fd, 0)) == (caddr_t)-1)
#else
      if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                          stat_buf.stx_size, PROT_READ,
# else
                          stat_buf.st_size, PROT_READ,
# endif
                          MAP_SHARED, file, 0)) == (caddr_t)-1)
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
      if (*(ptr + SIZEOF_INT + 1 + 1 + 1) != CURRENT_JID_VERSION)
      {
         (void)fprintf(stderr, _("Incorrect JID version (data=%d current=%d)!\n"),
                       *(ptr + SIZEOF_INT + 1 + 1 + 1), CURRENT_JID_VERSION);
         exit(INCORRECT);
      }
      no_of_job_ids = *(int *)ptr;
      ptr += AFD_WORD_OFFSET;
      jd = (struct job_id_data *)ptr;
#ifdef HAVE_STATX
      jid_size = stat_buf.stx_size;
#else
      jid_size = stat_buf.st_size;
#endif

      /* Map to file mask file. */
      (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, FILE_MASK_FILE);
      if ((fd = open(file, O_RDONLY)) == -1)
      {
         (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
         no_of_file_masks_id = 0;
      }
      else
      {
#ifdef HAVE_STATX
         if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &stat_buf) == -1)
#else
         if (fstat(fd, &stat_buf) == -1)
#endif
         {
            (void)fprintf(stderr, _("Failed to access `%s' : %s (%s %d)\n"),
                          file, strerror(errno), __FILE__, __LINE__);
            no_of_file_masks_id = 0;
         }
         else
         {
#ifdef HAVE_MMAP
            if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                            stat_buf.stx_size, PROT_READ,
# else
                            stat_buf.st_size, PROT_READ,
# endif
                            MAP_SHARED, fd, 0)) == (caddr_t)-1)
#else
            if ((ptr = mmap_emu(NULL,
# ifdef HAVE_STATX
                                stat_buf.stx_size, PROT_READ,
# else
                                stat_buf.st_size, PROT_READ,
# endif
                                MAP_SHARED, file, 0)) == (caddr_t)-1)
#endif
            {
               (void)fprintf(stderr, _("Failed to mmap() `%s' : %s (%s %d)\n"),
                             file, strerror(errno), __FILE__, __LINE__);
               fmd = NULL;
               no_of_file_masks_id = 0;
            }
            else
            {
               no_of_file_masks_id = *(int *)ptr;
               ptr += AFD_WORD_OFFSET;
               fmd = ptr;
#ifdef HAVE_STATX
               fmd_size = stat_buf.stx_size;
#else
               fmd_size = stat_buf.st_size;
#endif
               fml_offset = sizeof(int) + sizeof(int);
               mask_offset = fml_offset + sizeof(int) + sizeof(unsigned int) +
                             sizeof(unsigned char);
            }
         }

         if (close(fd) == -1)
         {
            (void)fprintf(stderr, _("Failed to close() `%s' : %s (%s %d)\n"),
                          file, strerror(errno), __FILE__, __LINE__);
         }
      }

      /* Map to directory_names file. */
      (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, DIR_NAME_FILE);
      if ((fd = open(file, O_RDONLY)) == -1)
      {
         (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
         no_of_dir_names = 0;
      }
      else
      {
#ifdef HAVE_STATX
         if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &stat_buf) == -1)
#else
         if (fstat(fd, &stat_buf) == -1)
#endif
         {
            (void)fprintf(stderr, _("Failed to access `%s' : %s (%s %d)\n"),
                          file, strerror(errno), __FILE__, __LINE__);
            no_of_dir_names = 0;
         }
         else
         {
#ifdef HAVE_MMAP
            if ((ptr = mmap(NULL,
# ifdef HAVE_STATX
                            stat_buf.stx_size, PROT_READ,
# else
                            stat_buf.st_size, PROT_READ,
# endif
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
               dnb = NULL;
               no_of_dir_names = 0;
            }
            else
            {
               no_of_dir_names = *(int *)ptr;
               ptr += AFD_WORD_OFFSET;
               dnb = (struct dir_name_buf *)ptr;
#ifdef HAVE_STATX
               dnb_size = stat_buf.stx_size;
#else
               dnb_size = stat_buf.st_size;
#endif
            }
         }

         if (close(fd) == -1)
         {
            (void)fprintf(stderr, _("Failed to close() `%s' : %s (%s %d)\n"),
                          file, strerror(errno), __FILE__, __LINE__);
         }
      }

      /* Map to DIR_CONFIG name database. */
      (void)sprintf(file, "%s%s%s", p_work_dir, FIFO_DIR, DC_LIST_FILE);
      if ((fd = open(file, O_RDONLY)) != -1)
      {
#ifdef HAVE_STATX
         if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                   STATX_SIZE, &stat_buf) != -1)
#else
         if (fstat(fd, &stat_buf) != -1)
#endif
         {
#ifdef HAVE_STATX
            if (stat_buf.stx_size > 0)
#else
            if (stat_buf.st_size > 0)
#endif
            {
               char *ptr;

               if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                               stat_buf.stx_size, PROT_READ,
#else
                               stat_buf.st_size, PROT_READ,
#endif
                               MAP_SHARED, fd, 0)) != (caddr_t) -1)
               {
#ifdef HAVE_STATX
                  dcl_size = stat_buf.stx_size;
#else
                  dcl_size = stat_buf.st_size;
#endif
                  no_of_dc_ids = *(int *)ptr; 
                  ptr += AFD_WORD_OFFSET;
                  dcl = (struct dir_config_list *)ptr;
               }
               else
               {
                  (void)fprintf(stderr,
                                _("Failed to mmap() to `%s' : %s (%s %d)\n"),
                                file, strerror(errno), __FILE__, __LINE__);
               }
            }
            else
            {
               (void)fprintf(stderr,
                             _("File mask database file is empty. (%s %d)\n"),
                             __FILE__, __LINE__);
            }
         }
         else
         {
            (void)fprintf(stderr, _("Failed to fstat() `%s' : %s (%s %d)\n"),
                          file, strerror(errno), __FILE__, __LINE__);
         }
         if (close(fd) == -1)
         {
            (void)fprintf(stderr, _("close() error : %s (%s %d)\n"),
                          strerror(errno), __FILE__, __LINE__);     
         }
      }
      else
      {
         (void)fprintf(stderr, _("Failed to open() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
      }

      (void)fsa_attach_passive(NO, "jid_view");

      if (no_of_job_ids > 0)
      {
         int  j = 0,
              lines_drawn = 0;
#ifdef _NEW_JID
         char time_str[25];
#endif

         for (i = 0; i < no_of_job_ids; i++)
         {
            if (no_of_search_ids > 0)
            {
               for (j = 0; j < no_of_search_ids; j++)
               {
                  if (job_id[j] == jd[i].job_id)
                  {
                     break;
                  }
               }
               if (j == no_of_search_ids)
               {
                  continue;
               }
            }
            if ((job_id == NULL) || (job_id[j] == jd[i].job_id))
            {
               (void)fprintf(stdout, "Job-ID          : %x\n", jd[i].job_id);
#ifdef _NEW_JID
               (void)strftime(time_str, 25, "%c", localtime(&jd[i].creation_time));
               (void)fprintf(stdout, "Creation time   : %s\n", time_str);
#endif
               if (dcl != NULL)
               {
                  for (j = 0; j < no_of_dc_ids; j++)
                  {
                     if (dcl[j].dc_id == jd[i].dir_config_id)
                     {
                        (void)fprintf(stdout, "DIR_CONFIG      : %s\n",
                                      dcl[j].dir_config_file);
                        break;
                     }
                  }
               }
               (void)fprintf(stdout, "DIR_CONFIG-ID   : %x\n", jd[i].dir_config_id);

               if ((dir_config_view_mode == YES) && (no_of_search_ids == 1) &&
                   (dnb != NULL) && (fmd != NULL) && (fsa != NULL) &&
                   (fra_attach_passive() == SUCCESS))
               {
                  int fra_pos = 0,
                      gotcha = NO,
                      position;

                  (void)fprintf(stdout, "File-Mask-ID    : %x\n", jd[i].file_mask_id);
                  (void)fprintf(stdout, "Destination-ID  : %x\n", jd[i].recipient_id);
                  (void)fprintf(stdout, "Host-Alias-ID   : %x\n", jd[i].host_id);

                  for (j = 0; j < no_of_dirs; j++)
                  {
                     if (fra[j].dir_id == jd[i].dir_id)
                     {
                        fra_pos = j;
                        gotcha = YES;
                        break;
                     }
                  }
                  if ((gotcha == YES) && (fra[fra_pos].fsa_pos != -1))
                  {
                     if (fsa[fra[fra_pos].fsa_pos].real_hostname[1][0] == '\0')
                     {
                        (void)fprintf(stdout, "Real hostname(S): %s\n",
                                      fsa[fra[fra_pos].fsa_pos].real_hostname[0]);
                     }
                     else
                     {
                        int second_host;

                        (void)fprintf(stdout, "Real hostname(S): %s ",
                                      fsa[fra[fra_pos].fsa_pos].real_hostname[(int)fsa[fra[fra_pos].fsa_pos].host_toggle - 1]);
                        if (fsa[fra[fra_pos].fsa_pos].host_toggle == HOST_ONE)
                        {
                           second_host = 1;
                        }
                        else
                        {
                           second_host = 0;
                        }
                        if (fsa[fra[fra_pos].fsa_pos].auto_toggle == ON)
                        {
                           (void)fprintf(stdout, "%c%s%c\n",
                                         AUTO_TOGGLE_OPEN,
                                         fsa[fra[fra_pos].fsa_pos].real_hostname[second_host],
                                         AUTO_TOGGLE_CLOSE);
                        }
                        else
                        {
                           (void)fprintf(stdout, "%c%s%c\n",
                                         STATIC_TOGGLE_OPEN,
                                         fsa[fra[fra_pos].fsa_pos].real_hostname[second_host],
                                         STATIC_TOGGLE_CLOSE);
                        }
                     }
                  }

                  if ((position = get_host_id_position(fsa, jd[i].host_id,
                                                       no_of_hosts)) >= 0)
                  {
                     if (fsa[position].real_hostname[1][0] == '\0')
                     {
                        (void)fprintf(stdout, "Real hostname(D): %s\n",
                                      fsa[position].real_hostname[0]);
                     }
                     else
                     {
                        int second_host;

                        (void)fprintf(stdout, "Real hostname(D): %s ",
                                      fsa[position].real_hostname[(int)fsa[position].host_toggle - 1]);
                        if (fsa[position].host_toggle == HOST_ONE)
                        {
                           second_host = 1;
                        }
                        else
                        {
                           second_host = 0;
                        }
                        if (fsa[position].auto_toggle == ON)
                        {
                           (void)fprintf(stdout, "%c%s%c\n",
                                         AUTO_TOGGLE_OPEN,
                                         fsa[position].real_hostname[second_host],
                                         AUTO_TOGGLE_CLOSE);
                        }
                        else
                        {
                           (void)fprintf(stdout, "%c%s%c\n",
                                         STATIC_TOGGLE_OPEN,
                                         fsa[position].real_hostname[second_host],
                                         STATIC_TOGGLE_CLOSE);
                        }
                     }
                  }
                  (void)fprintf(stdout, "--------------------------------------------------------------------------------\n");

                  for (j = 0; j < no_of_dir_names; j++)
                  {
                     if (dnb[j].dir_id == jd[i].dir_id)
                     {
                        (void)strcpy(tmp_value, dnb[j].orig_dir_name);
                        url_insert_password(tmp_value, (view_passwd == YES) ? NULL : "XXXXX");
                        if ((gotcha == YES) &&
                            (fra[fra_pos].in_dc_flag & DIR_ALIAS_IDC))
                        {
                           (void)fprintf(stdout, "%s %s\n%s\n",
                                         DIR_IDENTIFIER, fra[fra_pos].dir_alias,
                                         tmp_value);
                        }
                        else
                        {
                           (void)fprintf(stdout, "%s\n%s\n",
                                         DIR_IDENTIFIER, tmp_value);
                        }

                        if (CHECK_STRCMP(dnb[j].dir_name, dnb[j].orig_dir_name) != 0)
                        {
                           (void)fprintf(stdout, "# %s\n\n", dnb[j].dir_name);
                        }
                        else
                        {
                           (void)fprintf(stdout, "\n");
                        }
                        break;
                     }
                  }

                  get_dir_options(jd[i].dir_id, &d_o);
                  if (d_o.no_of_dir_options > 0)
                  {
                     (void)fprintf(stdout, "   %s\n", DIR_OPTION_IDENTIFIER);
                     for (j = 0; j < d_o.no_of_dir_options; j++)
                     {
                        (void)fprintf(stdout, "   %s\n", d_o.aoptions[j]);
                     }
                     (void)fprintf(stdout, "\n");
                  }

                  ptr = fmd;
                  gotcha = NO;
                  for (j = 0; j < no_of_file_masks_id; j++)
                  {
                     if (*(unsigned int *)(ptr + fml_offset + sizeof(int)) == jd[i].file_mask_id)
                     {
                        int    k;
                        time_t now = time(NULL);
                        char   *p_file = ptr + mask_offset,
                               tmp_mask[MAX_FILENAME_LENGTH];

                        if (expand_filter(p_file, tmp_mask, now) == YES)
                        {
                           (void)fprintf(stdout, "   %s\n   %s # %s\n",
                                         FILE_IDENTIFIER, p_file, tmp_mask);
                        }
                        else
                        {
                            (void)fprintf(stdout, "   %s\n   %s\n",
                                          FILE_IDENTIFIER, p_file);
                        }
                        NEXT(p_file);
                        for (k = 1; k < *(int *)ptr; k++)
                        {
                           if (expand_filter(p_file, tmp_mask, now) == YES)
                           {
                              (void)fprintf(stdout, "   %s # %s\n",
                                            p_file, tmp_mask);
                           }
                           else
                           {
                              (void)fprintf(stdout, "   %s\n", p_file);
                           }
                           NEXT(p_file);
                        }
                        gotcha = YES;
                        break;
                     }
                     ptr += (mask_offset + *(int *)(ptr + fml_offset) +
                             sizeof(char) + *(ptr + mask_offset - 1));
                  }
                  if (gotcha == NO)
                  {
                     (void)fprintf(stdout,
                                   "   %s\n   * # Filter database broken, assuming this filter!!!\n",
                                   FILE_IDENTIFIER);
                  }

                  (void)fprintf(stdout, "\n      %s\n\n         %s\n",
                                DESTINATION_IDENTIFIER, RECIPIENT_IDENTIFIER);
                  (void)strcpy(tmp_value, jd[i].recipient);
                  url_insert_password(tmp_value, (view_passwd == YES) ? NULL : "XXXXX");
                  (void)fprintf(stdout, "         %s\n", tmp_value);

                  /* Show all options. */
                  (void)fprintf(stdout, "\n         %s\n         %s %c\n",
                                OPTION_IDENTIFIER, PRIORITY_ID,
                                jd[i].priority);

                  /* Show all AMG options. */
                  if (jd[i].no_of_loptions > 0)
                  {
                     char *ptr = jd[i].loptions;

                     for (j = 0; j < jd[i].no_of_loptions; j++)
                     {
                        (void)fprintf(stdout, "         %s\n", ptr);
                        NEXT(ptr);
                     }
                  }

                  /* Show all FD options. */
                  if (jd[i].no_of_soptions > 0)
                  {
                     int  counter;
                     char *ptr = jd[i].soptions;

                     for (j = 0; j < jd[i].no_of_soptions; j++)
                     {
                        counter = 0;
                        while ((*ptr != '\n') && (*ptr != '\0'))
                        {
                           tmp_value[counter] = *ptr;
                           ptr++; counter++;
                        }
                        tmp_value[counter] = '\0';
                        (void)fprintf(stdout, "         %s\n", tmp_value);
                        if (*ptr == '\0')
                        {
                           break;
                        }
                        ptr++;
                     }
                  }
                  (void)fprintf(stdout, "\n");

                  (void)fra_detach();
               }
               else
               {
                  if (dnb != NULL)
                  {
                     for (j = 0; j < no_of_dir_names; j++)
                     {
                        if (dnb[j].dir_id == jd[i].dir_id)
                        {
                           (void)strcpy(tmp_value, dnb[j].orig_dir_name);
                           url_insert_password(tmp_value, (view_passwd == YES) ? NULL : "XXXXX");
                           (void)fprintf(stdout, "Source-Directory: %s\n", tmp_value);
                           if (CHECK_STRCMP(dnb[j].dir_name, dnb[j].orig_dir_name) != 0)
                           {
                              (void)fprintf(stdout, "Local-Source-Dir: %s\n",
                                            dnb[j].dir_name);
                           }
                           break;
                        }
                     }
                  }
                  (void)fprintf(stdout, "Dir-ID          : %x\n", jd[i].dir_id);
                  (void)fprintf(stdout, "Dir position    : %d\n", jd[i].dir_id_pos);
                  get_dir_options(jd[i].dir_id, &d_o);
                  if (d_o.no_of_dir_options > 0)
                  {
                     (void)fprintf(stdout, "DIR-options     : %s\n", d_o.aoptions[0]);
                     for (j = 1; j < d_o.no_of_dir_options; j++)
                     {
                        (void)fprintf(stdout, "                  %s\n", d_o.aoptions[j]);
                     }
                  }
                  if (fmd != NULL)
                  {
                     ptr = fmd;

                     for (j = 0; j < no_of_file_masks_id; j++)
                     {
                        if (*(unsigned int *)(ptr + fml_offset + sizeof(int)) == jd[i].file_mask_id)
                        {
                           if (*(int *)ptr == 1)
                           {
                              (void)fprintf(stdout, "File filters    : %s\n",
                                            (ptr + mask_offset));
                           }
                           else
                           {
                              char *p_file = ptr + mask_offset;

                              (void)fprintf(stdout, "File filters    : %s\n", p_file);
                              NEXT(p_file);
                              for (j = 1; j < *(int *)ptr; j++)
                              {
                                 (void)fprintf(stdout, "                  %s\n", p_file);
                                 NEXT(p_file);
                              }
                           }
                           break;
                        }
                        ptr += (mask_offset + *(int *)(ptr + fml_offset) +
                                sizeof(char) + *(ptr + mask_offset - 1));
                     }
                  }
                  (void)fprintf(stdout, "File-Mask-ID    : %x\n", jd[i].file_mask_id);
                  (void)strcpy(tmp_value, jd[i].recipient);
                  url_insert_password(tmp_value, (view_passwd == YES) ? NULL : "XXXXX");
                  (void)fprintf(stdout, "Destination     : %s\n", tmp_value);
                  (void)fprintf(stdout, "Destination-ID  : %x\n", jd[i].recipient_id);
                  (void)fprintf(stdout, "Host alias      : %s\n", jd[i].host_alias);
                  (void)fprintf(stdout, "Host-Alias-ID   : %x\n", jd[i].host_id);
                  if (fsa != NULL)
                  {
                     int position;

                     if ((position = get_host_id_position(fsa, jd[i].host_id,
                                                          no_of_hosts)) >= 0)
                     {
                        if (fsa[position].real_hostname[1][0] == '\0')
                        {
                           (void)fprintf(stdout, "Real hostname   : %s\n",
                                         fsa[position].real_hostname[0]);
                        }
                        else
                        {
                           int second_host;

                           (void)fprintf(stdout, "Real hostname   : %s ",
                                         fsa[position].real_hostname[(int)fsa[position].host_toggle - 1]);
                           if (fsa[position].host_toggle == HOST_ONE)
                           {
                              second_host = 1;
                           }
                           else
                           {
                              second_host = 0;
                           }
                           if (fsa[position].auto_toggle == ON)
                           {
                              (void)fprintf(stdout, "%c%s%c\n",
                                            AUTO_TOGGLE_OPEN,
                                            fsa[position].real_hostname[second_host],
                                            AUTO_TOGGLE_CLOSE);
                           }
                           else
                           {
                              (void)fprintf(stdout, "%c%s%c\n",
                                            STATIC_TOGGLE_OPEN,
                                            fsa[position].real_hostname[second_host],
                                            STATIC_TOGGLE_CLOSE);
                           }
                        }
                     }
                  }
                  if (jd[i].no_of_loptions > 0)
                  {
                     if (jd[i].no_of_loptions == 1)
                     {
                        (void)fprintf(stdout, "AMG options     : %s\n",
                                      jd[i].loptions);
                     }
                     else
                     {
                        char *ptr = jd[i].loptions;

                        (void)fprintf(stdout, "AMG options     : %s\n", ptr);
                        NEXT(ptr);
                        for (j = 1; j < jd[i].no_of_loptions; j++)
                        {
                           (void)fprintf(stdout, "                  %s\n", ptr);
                           NEXT(ptr);
                        }
                     }
                  }
                  if (jd[i].no_of_soptions > 0)
                  {
                     if (jd[i].no_of_soptions == 1)
                     {
                        (void)fprintf(stdout, "FD options      : %s\n",
                                      jd[i].soptions);
                     }
                     else
                     {
                        char *ptr,
                             *ptr_start;

                        ptr = ptr_start = option_buffer;

                        (void)strcpy(option_buffer, jd[i].soptions);
                        while ((*ptr != '\n') && (*ptr != '\0'))
                        {
                           ptr++;
                        }
                        *ptr = '\0';
                        ptr++;
                        (void)fprintf(stdout, "FD options      : %s\n", ptr_start);
                        for (j = 1; j < jd[i].no_of_soptions; j++)
                        {
                           ptr_start = ptr;
                           while ((*ptr != '\n') && (*ptr != '\0'))
                           {
                              ptr++;
                           }
                           *ptr = '\0';
                           ptr++;
                           (void)fprintf(stdout, "                  %s\n", ptr_start);
                        }
                     }
                  }
                  (void)fprintf(stdout, "Priority        : %c\n", jd[i].priority);
                  if (((no_of_search_ids > 0) && ((lines_drawn + 1) < no_of_search_ids)) ||
                      ((no_of_search_ids == 0) && ((i + 1) < no_of_job_ids)))
                  {
                     (void)fprintf(stdout, "--------------------------------------------------------------------------------\n");
                     lines_drawn++;
                  }
               }
            }
         }
      }
      else
      {
         (void)fprintf(stdout, _("Job ID list is empty.\n"));
      }

      if (fmd != NULL)
      {
         if (munmap((char *)fmd - AFD_WORD_OFFSET, fmd_size) == -1)
         {
            (void)fprintf(stderr, _("Failed to munmap() `%s' : %s (%s %d)\n"),
                          FILE_MASK_FILE, strerror(errno), __FILE__, __LINE__);
         }
      }
      if (dnb != NULL)
      {
         if (munmap((char *)dnb - AFD_WORD_OFFSET, dnb_size) == -1)
         {
            (void)fprintf(stderr, _("Failed to munmap() `%s' : %s (%s %d)\n"),
                          DIR_NAME_FILE, strerror(errno), __FILE__, __LINE__);
         }
      }
      if (dcl != NULL)
      {
         if (munmap((char *)dcl - AFD_WORD_OFFSET, dcl_size) == -1)
         {
            (void)fprintf(stderr, _("Failed to munmap() `%s' : %s (%s %d)\n"),
                          DC_LIST_FILE, strerror(errno), __FILE__, __LINE__);
         }
      }
      if (munmap((char *)jd - AFD_WORD_OFFSET, jid_size) == -1)
      {
         (void)fprintf(stderr, _("Failed to munmap() `%s' : %s (%s %d)\n"),
                       file, strerror(errno), __FILE__, __LINE__);
      }
      (void)fsa_detach(NO);
#ifdef WITH_AFD_MON
   }
#endif

   exit(SUCCESS);
}
