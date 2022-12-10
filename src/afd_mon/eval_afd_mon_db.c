/*
 *  eval_afd_mon_db.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2022 Deutscher Wetterdienst (DWD),
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
 **   eval_afd_mon_db - reads the AFD_MON_CONFIG file and stores the
 **                     contents in the structure mon_status_area
 **
 ** SYNOPSIS
 **   void eval_afd_mon_db(struct mon_status_area **nml)
 **
 ** DESCRIPTION
 **   This function evaluates the AFD_MON_CONFIG file which must have
 **   the following format:
 **
 **   # +------------------------------------------------> Alias
 **   # |         +--------------------------------------> Hostname
 **   # |         |       +------------------------------> Port
 **   # |         |       |  +---------------------------> Interval
 **   # |         |       |  |  +------------------------> Connect time
 **   # |         |       |  |  |   +--------------------> Disconnect time
 **   # |         |       |  |  |   |  +-----------------> Options
 **   # |         |       |  |  |   |  |  +--------------> Remote cmd
 **   # |         |       |  |  |   |  |  |       +------> Convert user name
 **   # |         |       |  |  |   |  |  |       |
 **   Hamburg ha.dwd.de 4444 5  30 600 0 rsh afd->testuser
 **   Essen   es.dwd.de 4545 10 0  0   1 ssh
 **
 **   Currently the following options are possible:
 **                0 - no option
 **                1 - SSH compression
 **                2 - SSH, use -Y instead of -X when starting X applications
 **                4 - do not use full path to rafdd_cmd/rafdd_cmd_ssh script
 **                8 - Enable SSL encryption
 **               16 - send SYSTEM_LOG data
 **               32 - send RECEIVE_LOG data
 **               64 - send TRANSFER_LOG data
 **              128 - send TRANSFER_DEBUG_LOG data
 **              256 - send INPUT_LOG data
 **              512 - send PRODUCTION_LOG data
 **             1024 - send OUTPUT_LOG data
 **             2048 - send DELETE_LOG data
 **             4096 - send job data
 **             8192 - compression method 1
 **            16384 - send EVENT_LOG data
 **            32768 - send DISTRIBUTION_LOG data
 **            65536 - send CONFIRMATION_LOG data
 **          8388608 - no strict SSH host key check
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.08.1998 H.Kiehl Created
 **   24.01.1999 H.Kiehl Added remote user name.
 **   30.09.2000 H.Kiehl Multiple convert username.
 **   24.02.2003 H.Kiehl Added "Remote Command" part.
 **   10.07.2003 H.Kiehl Added options part.
 **   03.12.2003 H.Kiehl Added connection and disconnection time.
 **   27.02.2005 H.Kiehl Option to switch between two AFD's.
 **   08.12.2005 H.Kiehl Option not to use full path to
 **                      rafdd_cmd/rafdd_cmd_ssh script.
 **
 */
DESCR__E_M3

#include <string.h>             /* strerror(), strcpy()                  */
#include <ctype.h>              /* isdigit()                             */
#include <stdlib.h>             /* malloc(), free(), atoi()              */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <unistd.h>
#include <errno.h>
#include "mondefs.h"
#include "afdddefs.h"

#define MEM_STEP_SIZE 20

/* External global variables. */
extern int  sys_log_fd,
            no_of_afds;
extern char afd_mon_db_file[];


/*++++++++++++++++++++++++++ eval_afd_mon_db() ++++++++++++++++++++++++++*/
void
eval_afd_mon_db(struct mon_list **nml)
{
   int    cu_counter,
          fd,
          i;
   size_t new_size;
   char   *afdbase = NULL,
          number[MAX_INT_LENGTH + 1],
          *ptr;

   /* Read the contents of the AFD_MON_CONFIG file into a buffer. */
   if ((fd = open(afd_mon_db_file, O_RDONLY)) != -1)
   {
#ifdef HAVE_STATX
      struct statx stat_buf;
#else
      struct stat stat_buf;
#endif

#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE, &stat_buf) != -1)
#else
      if (fstat(fd, &stat_buf) != -1)
#endif
      {
#ifdef HAVE_STATX
         if ((afdbase = malloc(stat_buf.stx_size + 2)) != NULL)
#else
         if ((afdbase = malloc(stat_buf.st_size + 2)) != NULL)
#endif
         {
#ifdef HAVE_STATX
            if (read(fd, &afdbase[1], stat_buf.stx_size) != stat_buf.stx_size)
#else
            if (read(fd, &afdbase[1], stat_buf.st_size) != stat_buf.st_size)
#endif
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to read() from `%s' : %s",
                          afd_mon_db_file, strerror(errno));
               exit(INCORRECT);
            }
#ifdef HAVE_STATX
            afdbase[1 + stat_buf.stx_size] = '\0';
#else
            afdbase[1 + stat_buf.st_size] = '\0';
#endif
         }
         else
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                       "Failed to malloc() %ld bytes : %s",
#else
                       "Failed to malloc() %lld bytes : %s",
#endif
#ifdef HAVE_STATX
                       (pri_off_t)(stat_buf.stx_size + 2),
#else
                       (pri_off_t)(stat_buf.st_size + 2),
#endif
                       strerror(errno));
            exit(INCORRECT);
         }
      }
      else
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to access `%s' : %s",
                    afd_mon_db_file, strerror(errno));
         exit(INCORRECT);
      }

      if (close(fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "close() error : %s", strerror(errno));
      }
   }
   else
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to open() `%s' : %s",
                 afd_mon_db_file, strerror(errno));
      exit(INCORRECT);
   }

   afdbase[0] = '\n';
   ptr = afdbase;
   no_of_afds = 0;

   /*
    * Cut off any comments before the AFD names come.
    */
   for (;;)
   {
      if ((*ptr != '\n') && (*ptr != '#') && (*ptr != ' ') && (*ptr != '\t'))
      {
         break;
      }
      while ((*ptr != '\0') && (*ptr != '\n'))
      {
         ptr++;
      }
      if (*ptr == '\n')
      {
         while (*ptr == '\n')
         {
            ptr++;
         }
      }
   }

   do
   {
      if (*ptr == '#')
      {
         /* Check if line is a comment. */
         while ((*ptr != '\0') && (*ptr != '\n'))
         {
            ptr++;
         }
         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            continue;
         }
         else
         {
            break;
         }
      }
      else
      {
         while (*ptr == '\n')
         {
            ptr++;
         }
      }

      /* Check if buffer for afd list structure is large enough. */
      if ((no_of_afds % MEM_STEP_SIZE) == 0)
      {
         char *ptr_start;

         /* Calculate new size for afd list. */
         new_size = ((no_of_afds / MEM_STEP_SIZE) + 1) *
                    MEM_STEP_SIZE * sizeof(struct mon_status_area);

         /* Now increase the space. */
         if ((*nml = realloc(*nml, new_size)) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Could not reallocate memory for AFD monitor list : %s",
                       strerror(errno));
            exit(INCORRECT);
         }

         /* Initialise the new memory area. */
         if (no_of_afds > (MEM_STEP_SIZE - 1))
         {
            ptr_start = (char *)(*nml) + (no_of_afds * sizeof(struct mon_status_area));
         }
         else
         {
            ptr_start = (char *)(*nml);
         }
         new_size = MEM_STEP_SIZE * sizeof(struct mon_status_area);
         /* offset = (int)(*nml + (no_of_afds / MEM_STEP_SIZE)); */
         (void)memset(ptr_start, 0, new_size);
      }

      for (i = 0; i < MAX_CONVERT_USERNAME; i++)
      {
         (*nml)[no_of_afds].convert_username[i][0][0] = '\0';
      }

      /* Store AFD alias. */
      i = 0;
      while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
             (*ptr != '\0') && (i <= MAX_AFDNAME_LENGTH))
      {
         (*nml)[no_of_afds].afd_alias[i] = *ptr;
         ptr++; i++;
      }
      if (i > MAX_AFDNAME_LENGTH)
      {
         (*nml)[no_of_afds].afd_alias[MAX_AFDNAME_LENGTH] = '\0';
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Maximum length for AFD alias name %s exceeded in AFD_MON_CONFIG. Will be truncated to %d characters.",
                    (*nml)[no_of_afds].afd_alias, MAX_AFDNAME_LENGTH);
         while ((*ptr != ' ') && (*ptr != '\t') &&
                (*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
      }
      else
      {
         (*nml)[no_of_afds].afd_alias[i] = '\0';
      }
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         (void)strcpy((*nml)[no_of_afds].hostname[0], (*nml)[no_of_afds].afd_alias);
         (void)strcpy((*nml)[no_of_afds].hostname[1], (*nml)[no_of_afds].afd_alias);
         (*nml)[no_of_afds].port[0] = atoi(DEFAULT_AFD_PORT_NO);
         (*nml)[no_of_afds].port[1] = (*nml)[no_of_afds].port[0];
         (*nml)[no_of_afds].afd_switching = NO_SWITCHING;
         (*nml)[no_of_afds].poll_interval = DEFAULT_POLL_INTERVAL;
         (*nml)[no_of_afds].connect_time = DEFAULT_CONNECT_TIME;
         (*nml)[no_of_afds].disconnect_time = DEFAULT_DISCONNECT_TIME;
         (*nml)[no_of_afds].options = DEFAULT_OPTION_ENTRY;
         (*nml)[no_of_afds].rcmd[0] = '\0';

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_afds++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Real Hostname. */
      i = 0;
      while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '|') &&
             (*ptr != '/') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_REAL_HOSTNAME_LENGTH))
      {
         (*nml)[no_of_afds].hostname[0][i] = *ptr;
         ptr++; i++;
      }
      if (i == MAX_REAL_HOSTNAME_LENGTH)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Maximum length for real hostname for %s exceeded in AFD_MON_CONFIG. Will be truncated to %d characters.",
                    (*nml)[no_of_afds].afd_alias, (MAX_REAL_HOSTNAME_LENGTH - 1));
         while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '|') &&
                (*ptr != '/') && (*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
         i--;
      }
      (*nml)[no_of_afds].hostname[0][i] = '\0';
      if ((*ptr == '|') || (*ptr == '/'))
      {
         char *tmp_ptr = ptr;;

         ptr++; /* Away with | or /. */
         i = 0;
         while ((*ptr != ' ') && (*ptr != '\t') &&
                (*ptr != '\n') && (*ptr != '\0') &&
                (i < MAX_REAL_HOSTNAME_LENGTH))
         {
            (*nml)[no_of_afds].hostname[1][i] = *ptr;
            ptr++; i++;
         }
         if (i == MAX_REAL_HOSTNAME_LENGTH)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Maximum length for second real hostname for %s exceeded in AFD_MON_CONFIG. Will be truncated to %d characters.",
                       (*nml)[no_of_afds].afd_alias, (MAX_REAL_HOSTNAME_LENGTH - 1));
            while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '|') &&
                   (*ptr != '/') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
            i--;
         }
         (*nml)[no_of_afds].hostname[1][i] = '\0';
         if (*tmp_ptr == '|')
         {
            (*nml)[no_of_afds].afd_switching = AUTO_SWITCHING;
         }
         else
         {
            (*nml)[no_of_afds].afd_switching = USER_SWITCHING;
         }
      }
      else
      {
         (*nml)[no_of_afds].afd_switching = NO_SWITCHING;
         (void)strcpy((*nml)[no_of_afds].hostname[1],
                      (*nml)[no_of_afds].hostname[0]);
      }
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         (*nml)[no_of_afds].port[0] = atoi(DEFAULT_AFD_PORT_NO);
         (*nml)[no_of_afds].port[1] = (*nml)[no_of_afds].port[0];
         (*nml)[no_of_afds].poll_interval = DEFAULT_POLL_INTERVAL;
         (*nml)[no_of_afds].connect_time = DEFAULT_CONNECT_TIME;
         (*nml)[no_of_afds].disconnect_time = DEFAULT_DISCONNECT_TIME;
         (*nml)[no_of_afds].options = DEFAULT_OPTION_ENTRY;
         (void)strcpy((*nml)[no_of_afds].rcmd, DEFAULT_REMOTE_CMD);

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_afds++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store TCP port number. */
      i = 0;
      while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
             (*ptr != '|') && (*ptr != '/') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Non numeric character <%d> in TCP port field for AFD %s, using default %s.",
                       (int)*ptr, (*nml)[no_of_afds].afd_alias,
                       DEFAULT_AFD_PORT_NO);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ' ') && (*ptr != '\t') &&
                   (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      if (i == 0)
      {
         (*nml)[no_of_afds].port[0] = atoi(DEFAULT_AFD_PORT_NO);
      }
      else if (i == MAX_INT_LENGTH)
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         "Numeric value allowed transfers to large (>%d characters) for AFD %s to store as integer.",
                         (MAX_INT_LENGTH - 1), (*nml)[no_of_afds].afd_alias);
              system_log(WARN_SIGN, NULL, 0,
                         "Setting it to the default value %s.",
                         DEFAULT_AFD_PORT_NO);
              (*nml)[no_of_afds].port[0] = atoi(DEFAULT_AFD_PORT_NO);
              while ((*ptr != ' ') && (*ptr != '\t') &&
                     (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              number[i] = '\0';
              (*nml)[no_of_afds].port[0] = atoi(number);
           }
      if ((*ptr == '|') || (*ptr == '/'))
      {
         char *tmp_ptr = ptr;

         ptr++; /* Away with | or /. */
         i = 0;
         while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
                (*ptr != '|') && (*ptr != '/') && (*ptr != '\0') &&
                (i < MAX_INT_LENGTH))
         {
            if (isdigit((int)(*ptr)))
            {
               number[i] = *ptr;
               ptr++; i++;
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Non numeric character <%d> in TCP port field for AFD %s, disabling second port.",
                          (int)*ptr, (*nml)[no_of_afds].afd_alias);

               /* Ignore this entry. */
               i = 0;
               while ((*ptr != ' ') && (*ptr != '\t') &&
                      (*ptr != '\n') && (*ptr != '\0'))
               {
                  ptr++;
               }
            }
         }
         if (i == 0)
         {
            (*nml)[no_of_afds].port[1] = (*nml)[no_of_afds].port[0];
         }
         else if (i == MAX_INT_LENGTH)
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Numeric value allowed transfers to large (>%d characters) for AFD %s to store as integer.",
                            (MAX_INT_LENGTH - 1), (*nml)[no_of_afds].afd_alias);
                 system_log(WARN_SIGN, NULL, 0, "Disabling second port.");
                 (*nml)[no_of_afds].port[1] = (*nml)[no_of_afds].port[0];
                 while ((*ptr != ' ') && (*ptr != '\t') &&
                        (*ptr != '\n') && (*ptr != '\0'))
                 {
                    ptr++;
                 }
              }
              else
              {
                 number[i] = '\0';
                 (*nml)[no_of_afds].port[1] = atoi(number);
                 if (*tmp_ptr == '|')
                 {
                    (*nml)[no_of_afds].afd_switching = AUTO_SWITCHING;
                 }
                 else
                 {
                    (*nml)[no_of_afds].afd_switching = USER_SWITCHING;
                 }
              }
      }
      else
      {
         (*nml)[no_of_afds].port[1] = (*nml)[no_of_afds].port[0];
      }

      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         (*nml)[no_of_afds].poll_interval = DEFAULT_POLL_INTERVAL;
         (*nml)[no_of_afds].connect_time = DEFAULT_CONNECT_TIME;
         (*nml)[no_of_afds].disconnect_time = DEFAULT_DISCONNECT_TIME;
         (*nml)[no_of_afds].options = DEFAULT_OPTION_ENTRY;
         (void)strcpy((*nml)[no_of_afds].rcmd, DEFAULT_REMOTE_CMD);

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_afds++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store poll interval. */
      i = 0;
      while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
             (*ptr != '\0') && (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Non numeric character <%d> in poll interval field for AFD %s, using default %d.",
                       (int)*ptr, (*nml)[no_of_afds].afd_alias,
                       DEFAULT_POLL_INTERVAL);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ' ') && (*ptr != '\t') &&
                   (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      if (i == 0)
      {
         (*nml)[no_of_afds].poll_interval = DEFAULT_POLL_INTERVAL;
      }
      else if (i == MAX_INT_LENGTH)
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         "Numeric value for poll interval to large (>%d characters) for AFD %s to store as integer.",
                         (MAX_INT_LENGTH - 1), (*nml)[no_of_afds].afd_alias);
              system_log(WARN_SIGN, NULL, 0,
                         "Setting it to the default value %s.",
                         DEFAULT_POLL_INTERVAL);
              (*nml)[no_of_afds].poll_interval = DEFAULT_POLL_INTERVAL;
              while ((*ptr != ' ') && (*ptr != '\t') &&
                     (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              number[i] = '\0';
              (*nml)[no_of_afds].poll_interval = atoi(number);
           }
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         (*nml)[no_of_afds].connect_time = DEFAULT_CONNECT_TIME;
         (*nml)[no_of_afds].disconnect_time = DEFAULT_DISCONNECT_TIME;
         (*nml)[no_of_afds].options = DEFAULT_OPTION_ENTRY;
         (void)strcpy((*nml)[no_of_afds].rcmd, DEFAULT_REMOTE_CMD);

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_afds++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Connect Time. */
      i = 0;
      while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
             (*ptr != '\0') && (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Non numeric character <%d> in connect time field for AFD %s, using default %d.",
                       (int)*ptr, (*nml)[no_of_afds].afd_alias,
                       DEFAULT_CONNECT_TIME);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ' ') && (*ptr != '\t') &&
                   (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      if (i == 0)
      {
         (*nml)[no_of_afds].connect_time = DEFAULT_CONNECT_TIME;
      }
      else if (i == MAX_INT_LENGTH)
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         "Numeric value for connect time to large (>%d characters) for AFD %s to store as integer.",
                         (MAX_INT_LENGTH - 1), (*nml)[no_of_afds].afd_alias);
              system_log(WARN_SIGN, NULL, 0,
                         "Setting it to the default value %s.",
                         DEFAULT_CONNECT_TIME);
              (*nml)[no_of_afds].connect_time = DEFAULT_CONNECT_TIME;
              while ((*ptr != ' ') && (*ptr != '\t') &&
                     (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              number[i] = '\0';
              (*nml)[no_of_afds].connect_time = atoi(number);
           }
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         (*nml)[no_of_afds].disconnect_time = DEFAULT_DISCONNECT_TIME;
         (*nml)[no_of_afds].options = DEFAULT_OPTION_ENTRY;
         (void)strcpy((*nml)[no_of_afds].rcmd, DEFAULT_REMOTE_CMD);

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_afds++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Disconnect Time. */
      i = 0;
      while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
             (*ptr != '\0') && (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Non numeric character <%d> in disconnect time field for AFD %s, using default %d.",
                       (int)*ptr, (*nml)[no_of_afds].afd_alias,
                       DEFAULT_DISCONNECT_TIME);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ' ') && (*ptr != '\t') &&
                   (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      if (i == 0)
      {
         (*nml)[no_of_afds].disconnect_time = DEFAULT_DISCONNECT_TIME;
      }
      else if (i == MAX_INT_LENGTH)
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                        "Numeric value for disconnect time to large (>%d characters) for AFD %s to store as integer.",
                        (MAX_INT_LENGTH - 1), (*nml)[no_of_afds].afd_alias);
              system_log(WARN_SIGN, NULL, 0,
                         "Setting it to the default value %s.",
                         DEFAULT_DISCONNECT_TIME);
              (*nml)[no_of_afds].disconnect_time = DEFAULT_DISCONNECT_TIME;
              while ((*ptr != ' ') && (*ptr != '\t') &&
                     (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              number[i] = '\0';
              (*nml)[no_of_afds].disconnect_time = atoi(number);
           }
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         (*nml)[no_of_afds].options = DEFAULT_OPTION_ENTRY;
         (void)strcpy((*nml)[no_of_afds].rcmd, DEFAULT_REMOTE_CMD);

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_afds++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store option field. */
      i = 0;
      while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
             (*ptr != '\0') && (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Non numeric character <%d> in option field for AFD %s, using default %d.",
                       (int)*ptr, (*nml)[no_of_afds].afd_alias,
                       DEFAULT_OPTION_ENTRY);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ' ') && (*ptr != '\t') &&
                   (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      if (i == 0)
      {
         (*nml)[no_of_afds].options = DEFAULT_OPTION_ENTRY;
      }
      else if (i == MAX_INT_LENGTH)
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         "Numeric value for the options entry to large (>%d characters) for AFD %s to store as integer.",
                         (MAX_INT_LENGTH - 1), (*nml)[no_of_afds].afd_alias);
              system_log(WARN_SIGN, NULL, 0,
                         "Setting it to the default value %s.",
                         DEFAULT_OPTION_ENTRY);
              (*nml)[no_of_afds].options = DEFAULT_OPTION_ENTRY;
              while ((*ptr != ' ') && (*ptr != '\t') &&
                     (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              number[i] = '\0';
              (*nml)[no_of_afds].options = atoi(number);
           }
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         (void)strcpy((*nml)[no_of_afds].rcmd, DEFAULT_REMOTE_CMD);

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_afds++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Remote Command. */
      i = 0;
      while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
             (*ptr != '\0') && (i < MAX_REMOTE_CMD_LENGTH))
      {
         (*nml)[no_of_afds].rcmd[i] = tolower((int)(*ptr));
         ptr++; i++;
      }
      if (i == MAX_REMOTE_CMD_LENGTH)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Maximum length for remote command for %s exceeded in AFD_MON_CONFIG. Will be truncated to %d characters.",
                    (*nml)[no_of_afds].afd_alias, MAX_REMOTE_CMD_LENGTH);
         while ((*ptr != ' ') && (*ptr != '\t') &&
                (*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
         i--;
      }
      if ((i == 3) &&
          ((((*nml)[no_of_afds].rcmd[0] == 'r') ||
            ((*nml)[no_of_afds].rcmd[0] == 's')) &&
           ((*nml)[no_of_afds].rcmd[1] == 's') &&
           ((*nml)[no_of_afds].rcmd[2] == 'h')))
      {
         (*nml)[no_of_afds].rcmd[i] = '\0';
      }
      else if ((i == 4) &&
               (((*nml)[no_of_afds].rcmd[0] == 'n') &&
                ((*nml)[no_of_afds].rcmd[1] == 'o') &&
                ((*nml)[no_of_afds].rcmd[2] == 'n') &&
                ((*nml)[no_of_afds].rcmd[3] == 'e')))
           {
              (*nml)[no_of_afds].rcmd[0] = '\0';
           }
           else
           {
              (void)strcpy((*nml)[no_of_afds].rcmd, DEFAULT_REMOTE_CMD);
              if (i > 0)
              {
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Unknown remote command for %s in AFD_MON_CONFIG. Will set to default (%s).",
                            (*nml)[no_of_afds].afd_alias, DEFAULT_REMOTE_CMD);
              }
           }

      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            no_of_afds++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Convert Username. */
      cu_counter = 0;
      do
      {
         i = 0;
         while ((*ptr != '\0') && !((*ptr == '-') && (*(ptr + 1) == '>')) &&
                (*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
                (i < MAX_USER_NAME_LENGTH))
         {
            (*nml)[no_of_afds].convert_username[cu_counter][0][i] = *ptr;
            ptr++; i++;
         }
         if (i == MAX_USER_NAME_LENGTH)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Maximum length for username for %s exceeded in AFD_MON_CONFIG. Will be truncated to %d characters.",
                       (*nml)[no_of_afds].afd_alias, (MAX_USER_NAME_LENGTH - 1));
            while ((*ptr != '\0') && !((*ptr == '-') && (*(ptr + 1) == '>')) &&
                   (*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n'))
            {
               ptr++;
            }
            i--;
         }
         (*nml)[no_of_afds].convert_username[cu_counter][0][i] = '\0';
         if ((*ptr == '-') && (*(ptr + 1) == '>'))
         {
            i = 0;
            ptr += 2;
            while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
                   (*ptr != '\0') && (i < MAX_USER_NAME_LENGTH))
            {
               (*nml)[no_of_afds].convert_username[cu_counter][1][i] = *ptr;
               ptr++; i++;
            }
            if (i == MAX_USER_NAME_LENGTH)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Maximum length for username for %s exceeded in AFD_MON_CONFIG. Will be truncated to %d characters.",
                          (*nml)[no_of_afds].afd_alias, (MAX_USER_NAME_LENGTH - 1));
               while ((*ptr != ' ') && (*ptr != '\t') &&
                      (*ptr != '\n') && (*ptr != '\0'))
               {
                  ptr++;
               }
               i--;
            }
            (*nml)[no_of_afds].convert_username[cu_counter][1][i] = '\0';
         }
         else
         {
            (*nml)[no_of_afds].convert_username[cu_counter][0][0] = '\0';
            (*nml)[no_of_afds].convert_username[cu_counter][1][0] = '\0';
         }
         while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
                (*ptr != '\0'))
         {
            ptr++;
         }
         while ((*ptr == ' ') || (*ptr == '\t'))
         {
            ptr++;
         }
         cu_counter++;
      } while ((cu_counter < MAX_CONVERT_USERNAME) && (*ptr != '\n') &&
               (*ptr != '\0'));

      /* Ignore the rest of the line. We have everything we need. */
      while ((*ptr != '\n') && (*ptr != '\0'))
      {
         ptr++;
      }
      while (*ptr == '\n')
      {
         ptr++;
      }
      no_of_afds++;
   } while (*ptr != '\0');
   free(afdbase);

   return;
}
