/*
 *  fsa_view.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2023 Deutscher Wetterdienst (DWD),
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

DESCR__S_M1
/*
 ** NAME
 **   fsa_view - shows all information in the FSA about a specific
 **              host
 **
 ** SYNOPSIS
 **   fsa_view [--version] [-w working directory] [-l|-s] position|host-alias-id|host-alias
 **
 ** DESCRIPTION
 **   This program shows all information about a specific host in the
 **   FSA.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.02.1996 H.Kiehl Created
 **   05.01.1997 H.Kiehl Added support for burst mode.
 **   21.08.1997 H.Kiehl Show real hostname as well.
 **   12.10.1997 H.Kiehl Show bursting and mailing.
 **   05.12.2000 H.Kiehl If available show host toggle string.
 **   04.08.2001 H.Kiehl Show more details of special_flag and added
 **                      active|passive mode and idle time to protocol.
 **   16.02.2006 H.Kiehl Added SFTP, ignore_bin, socket send and
 **                      socket receive buffer.
 **   27.03.2006 H.Kiehl Option with long view with full filenames.
 **   18.10.2013 H.Kiehl Beautified output so it shows the table aligned
 **                      properly.
 **   22.07.2019 H.Kiehl Added posibility to specify as search value
 **                      the host-alias-id.
 **
 */
DESCR__E_M1

#include <stdio.h>                       /* fprintf(), stderr, stdout    */
#include <string.h>                      /* strcpy(), strerror()         */
#include <stdlib.h>                      /* atoi(), strtoul()            */
#include <ctype.h>                       /* isdigit()                    */
#include <time.h>                        /* ctime()                      */
#include <sys/types.h>
#include <unistd.h>                      /* STDERR_FILENO                */
#include <errno.h>
#include <math.h>
#include "version.h"

#define SHORT_VIEW    1
#define LONG_VIEW     2

#define GET_MAX_DIGIT(value)                          \
        {                                             \
           if ((value) > 999999999)                   \
           {                                          \
              nr_of_digits = (int)log10((value)) + 1; \
              if (nr_of_digits > max_digits)          \
              {                                       \
                 max_digits = nr_of_digits;           \
              }                                       \
           }                                          \
        }

/* Local functions. */
static void usage(void);

int                        sys_log_fd = STDERR_FILENO,   /* Not used!    */
                           fsa_id,
                           fsa_fd = -1,
                           no_of_hosts = 0;
#ifdef HAVE_MMAP
off_t                      fsa_size;
#endif
char                       *p_work_dir;
struct filetransfer_status *fsa;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$ fsa_view() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          i, j,
                last = 0,
                position = -1,
                view_type = SHORT_VIEW;
   unsigned int host_id = 0;
   char         hostname[MAX_HOSTNAME_LENGTH + 1],
                *ptr,
                work_dir[MAX_PATH_LENGTH];

   if ((get_arg(&argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage();
      exit(SUCCESS);
   }

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;
   if (get_arg(&argc, argv, "-l", NULL, 0) == SUCCESS)
   {
      view_type = LONG_VIEW;
   }
   if (get_arg(&argc, argv, "-s", NULL, 0) == SUCCESS)
   {
      view_type = SHORT_VIEW;
   }

   /* Do not start if binary dataset matches the one stort on disk. */
   if (check_typesize_data(NULL, stdout, NO) > 0)
   {
      (void)fprintf(stderr,
                    "The compiled binary does not match stored database.\n");
      (void)fprintf(stderr,
                    "Initialize database with the command : afd -i\n");
      exit(INCORRECT);
   }

   if (argc == 2)
   {
      i = 0;
      while (isdigit((int)((*(argv + 1))[i])))
      {
         i++;
      }
      if ((*(argv + 1))[i] == '\0')
      {
         position = atoi(argv[1]);
         last = position + 1;
      }
      else
      {
         i = 0;
         while (isxdigit((int)((*(argv + 1))[i])))
         {
            i++;
         }
         if ((*(argv + 1))[i] == '\0')
         {
            host_id = (unsigned int)strtoul(argv[1], (char **)NULL, 16);
         }
         else
         {
            t_hostname(argv[1], hostname);
         }
      }
   }
   else if (argc == 1)
        {
           position = -2;
        }
        else
        {
           usage();
           exit(INCORRECT);
        }

   if ((j = fsa_attach_passive(NO, "fsa_view")) != SUCCESS)
   {
      if (j == INCORRECT_VERSION)
      {
         (void)fprintf(stderr,
                       _("ERROR   : This program is not able to attach to the FSA due to incorrect version. (%s %d)\n"),
                       __FILE__, __LINE__);
      }
      else
      {
         if (j < 0)
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to FSA. (%s %d)\n"),
                          __FILE__, __LINE__);
         }
         else
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to attach to FSA : %s (%s %d)\n"),
                          strerror(j), __FILE__, __LINE__);
         }
      }
      exit(INCORRECT);
   }

   if (position == -1)
   {
      if (host_id == 0)
      {
         if ((position = get_host_position(fsa, hostname, no_of_hosts)) < 0)
         {
            (void)fprintf(stderr,
                          _("WARNING : Could not find host `%s' in FSA. (%s %d)\n"),
                          hostname, __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      else
      {
         if ((position = get_host_id_position(fsa, host_id, no_of_hosts)) < 0)
         {
            /* As a fallback to old behaviour, try it as alias. */
            t_hostname(argv[1], hostname);
            if ((position = get_host_position(fsa, hostname, no_of_hosts)) < 0)
            {
               (void)fprintf(stderr,
                             _("WARNING : Could not find host ID %x in FSA. (%s %d)\n"),
                             host_id, __FILE__, __LINE__);
               exit(INCORRECT);
            }
         }
      }
      last = position + 1;
   }
   else if (position == -2)
        {
           last = no_of_hosts;
           position = 0;
        }
   else if (position >= no_of_hosts)
        {
           /* Hmm, maybe the user meant a alias-host-id? */
           host_id = (unsigned int)strtoul(argv[1], (char **)NULL, 16);
           if (((position = get_host_id_position(fsa, host_id, no_of_hosts)) < 0) ||
               (position >= no_of_hosts))
           {
              (void)fprintf(stderr,
                            _("WARNING : There are only %d hosts in the FSA. (%s %d)\n"),
                            no_of_hosts, __FILE__, __LINE__);
              exit(INCORRECT);
           }
           last = position + 1;
        }

   ptr =(char *)fsa;
   ptr -= AFD_WORD_OFFSET;
   (void)fprintf(stdout, _("    Number of hosts: %d   FSA ID: %d  Struct Version: %d  Pagesize: %d\n"),
                 no_of_hosts, fsa_id, (int)(*(ptr + SIZEOF_INT + 1 + 1 + 1)),
                 *(int *)(ptr + SIZEOF_INT + 4));
   (void)fprintf(stdout, _("    First errors offline: %d\n\n"),
                 *(unsigned char *)((char *)ptr + AFD_START_ERROR_OFFSET_START));
   for (j = position; j < last; j++)
   {
      (void)fprintf(stdout, "=============================> %s (%d) <=============================\n",
                    fsa[j].host_alias, j);
      (void)fprintf(stdout, "Host alias CRC       : %x\n", fsa[j].host_id);
      if (fsa[j].real_hostname[0][0] == GROUP_IDENTIFIER)
      {
         (void)fprintf(stdout, "Real hostname 1      :\n");
      }
      else
      {
         (void)fprintf(stdout, "Real hostname 1      : %s\n", fsa[j].real_hostname[0]);
      }
      (void)fprintf(stdout, "Real hostname 2      : %s\n", fsa[j].real_hostname[1]);
      (void)fprintf(stdout, "Hostname (display)   : >%s<\n", fsa[j].host_dsp_name);
      if (fsa[j].host_toggle == HOST_ONE)
      {
         (void)fprintf(stdout, "Host toggle          : HOST_ONE\n");
      }
      else if (fsa[j].host_toggle == HOST_TWO)
           {
              (void)fprintf(stdout, "Host toggle          : HOST_TWO\n");
           }
           else
           {
              (void)fprintf(stdout, "Host toggle          : HOST_???\n");
           }
      if (fsa[j].auto_toggle == ON)
      {
         (void)fprintf(stdout, "Auto toggle          : ON\n");
      }
      else
      {
         (void)fprintf(stdout, "Auto toggle          : OFF\n");
      }
      if (fsa[j].original_toggle_pos == HOST_ONE)
      {
         (void)fprintf(stdout, "Original toggle      : HOST_ONE\n");
      }
      else if (fsa[j].original_toggle_pos == HOST_TWO)
           {
              (void)fprintf(stdout, "Original toggle      : HOST_TWO\n");
           }
      else if (fsa[j].original_toggle_pos == NONE)
           {
              (void)fprintf(stdout, "Original toggle      : NONE\n");
           }
           else
           {
              (void)fprintf(stdout, "Original toggle      : HOST_???\n");
           }
      (void)fprintf(stdout, "Toggle position      : %d\n", fsa[j].toggle_pos);
      if (fsa[j].host_toggle_str[0] != '\0')
      {
         (void)fprintf(stdout, "Host toggle string   : %s\n",
                       fsa[j].host_toggle_str);
      }
      (void)fprintf(stdout, "Protocol(%11x): ", fsa[j].protocol);
      if (fsa[j].protocol & FTP_FLAG)
      {
         (void)fprintf(stdout, "FTP ");
         if (fsa[j].protocol_options & FTP_PASSIVE_MODE)
         {
            if (fsa[j].protocol_options & FTP_EXTENDED_MODE)
            {
               (void)fprintf(stdout, "extended passive ");
            }
            else
            {
               (void)fprintf(stdout, "passive ");
            }
         }
         else
         {
            (void)fprintf(stdout, "active ");
         }
         if (fsa[j].protocol_options & FTP_ALLOW_DATA_REDIRECT)
         {
            (void)fprintf(stdout, "allow_redirect ");
         }
         if (fsa[j].protocol_options & SET_IDLE_TIME)
         {
            (void)fprintf(stdout, "idle ");
         }
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
         if (fsa[j].protocol_options & STAT_KEEPALIVE)
         {
            (void)fprintf(stdout, "stat_keepalive ");
         }
#endif
         if (fsa[j].protocol_options & FTP_FAST_MOVE)
         {
            (void)fprintf(stdout, "fast_move ");
         }
         if (fsa[j].protocol_options & FTP_FAST_CD)
         {
            (void)fprintf(stdout, "fast_cd ");
         }
         if (fsa[j].protocol_options & FTP_IGNORE_BIN)
         {
            (void)fprintf(stdout, "ignore_bin ");
         }
         if (fsa[j].protocol_options & CHECK_SIZE)
         {
            (void)fprintf(stdout, "check_size ");
         }
         if (fsa[j].protocol_options & FTP_USE_LIST)
         {
            (void)fprintf(stdout, "use_list ");
         }
         if (fsa[j].protocol_options & USE_STAT_LIST)
         {
            (void)fprintf(stdout, "use_stat_list ");
         }
         if (fsa[j].protocol_options & FTP_DISABLE_MLST)
         {
            (void)fprintf(stdout, "disable_mlst ");
         }
         if (fsa[j].protocol_options2 & FTP_SEND_UTF8_ON)
         {
            (void)fprintf(stdout, "send_utf8_on ");
         }
         if (fsa[j].protocol_options & IMPLICIT_FTPS)
         {
            (void)fprintf(stdout, "implicit_ftps ");
         }
         if (fsa[j].protocol_options & KEEP_CONNECTED_DISCONNECT)
         {
            (void)fprintf(stdout, "keep_connected_disconnect ");
         }
      }
      if (fsa[j].protocol & SFTP_FLAG)
      {
         (void)fprintf(stdout, "SFTP ");
         if ((fsa[j].protocol & FTP_FLAG) == 0)
         {
            if (fsa[j].protocol_options & FTP_FAST_CD)
            {
               (void)fprintf(stdout, "fast_cd ");
            }
            if (fsa[j].protocol_options & CHECK_SIZE)
            {
               (void)fprintf(stdout, "check_size ");
            }
         }
         if (fsa[j].protocol_options & ENABLE_COMPRESSION)
         {
            (void)fprintf(stdout, "compression ");
         }
         if (fsa[j].protocol_options & DISABLE_STRICT_HOST_KEY)
         {
            (void)fprintf(stdout, "disable_strict_host_key ");
         }
      }
      if (fsa[j].protocol & LOC_FLAG)
      {
         (void)fprintf(stdout, "LOC ");
      }
      if (fsa[j].protocol & HTTP_FLAG)
      {
         (void)fprintf(stdout, "HTTP ");
         if (fsa[j].protocol_options & HTTP_BUCKETNAME_IN_PATH)
         {
            (void)fprintf(stdout, "bucketname_in_path ");
         }
      }
      if (fsa[j].protocol & SMTP_FLAG)
      {
         (void)fprintf(stdout, "SMTP ");
      }
#ifdef _WITH_DE_MAIL_SUPPORT
      if (fsa[j].protocol & DE_MAIL_FLAG)
      {
         (void)fprintf(stdout, "DEMAIL ");
      }
#endif
#ifdef _WITH_MAP_SUPPORT
      if (fsa[j].protocol & MAP_FLAG)
      {
         (void)fprintf(stdout, "MAP ");
      }
#endif
#ifdef _WITH_DFAX_SUPPORT
      if (fsa[j].protocol & DFAX_FLAG)
      {
         (void)fprintf(stdout, "DFAX ");
      }
#endif
#ifdef _WITH_SCP_SUPPORT
      if (fsa[j].protocol & SCP_FLAG)
      {
         (void)fprintf(stdout, "SCP ");
         if ((fsa[j].protocol & SFTP_FLAG) == 0)
         {
            if (fsa[j].protocol_options & ENABLE_COMPRESSION)
            {
               (void)fprintf(stdout, "compression ");
            }
            if (fsa[j].protocol_options & DISABLE_STRICT_HOST_KEY)
            {
               (void)fprintf(stdout, "disable_strict_host_key ");
            }
         }
      }
#endif
#ifdef _WITH_WMO_SUPPORT
      if (fsa[j].protocol & WMO_FLAG)
      {
         (void)fprintf(stdout, "WMO ");
      }
#endif
#ifdef WITH_SSL
      if (fsa[j].protocol & SSL_FLAG)
      {
         (void)fprintf(stdout, "TLS ");
      }
      if (fsa[j].protocol_options & TLS_STRICT_VERIFY)
      {
         (void)fprintf(stdout, "tls_strict_verify ");
      }
      if (fsa[j].protocol_options & TLS_LEGACY_RENEGOTIATION)
      {
         (void)fprintf(stdout, "tls_legacy_renegotiation ");
      }
#endif
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
      if (fsa[j].protocol_options & AFD_TCP_KEEPALIVE)
      {
         (void)fprintf(stdout, "tcp_keepalive ");
      }
#endif
      if (fsa[j].protocol_options & FILE_WHEN_LOCAL_FLAG)
      {
         (void)fprintf(stdout, "file_when_local ");
      }
      if (fsa[j].protocol_options & USE_SEQUENCE_LOCKING)
      {
         (void)fprintf(stdout, "sequence_locking ");
      }
      if (fsa[j].protocol_options & DISABLE_BURSTING)
      {
         (void)fprintf(stdout, "disable_burst ");
      }
      if (fsa[j].protocol_options & KEEP_TIME_STAMP)
      {
         (void)fprintf(stdout, "keep_time_stamp ");
      }
      if (fsa[j].protocol_options & SORT_FILE_NAMES)
      {
         (void)fprintf(stdout, "sort_file_names ");
      }
      if (fsa[j].protocol_options & NO_AGEING_JOBS)
      {
         (void)fprintf(stdout, "no_ageing_jobs ");
      }
      if (fsa[j].protocol_options & TIMEOUT_TRANSFER)
      {
         (void)fprintf(stdout, "timeout_transfer ");
      }
      if (fsa[j].protocol_options & KEEP_CON_NO_SEND_2)
      {
         (void)fprintf(stdout, "keep_con_no_send_2 ");
      }
      if (fsa[j].protocol_options & KEEP_CON_NO_FETCH_2)
      {
         (void)fprintf(stdout, "keep_con_no_fetch_2 ");
      }
      if (fsa[j].protocol_options & FTP_CCC_OPTION)
      {
         (void)fprintf(stdout, "ftp_ccc_option ");
      }
      (void)fprintf(stdout, "\n");
      (void)fprintf(stdout, "Direction            : ");
      if (fsa[j].protocol & SEND_FLAG)
      {
         (void)fprintf(stdout, "SEND ");
      }
      if (fsa[j].protocol & RETRIEVE_FLAG)
      {
         (void)fprintf(stdout, "RETRIEVE ");
      }
      (void)fprintf(stdout, "\n");
      if (fsa[j].socksnd_bufsize == 0)
      {
         (void)fprintf(stdout, "Socket send buffer   : Not set\n");
      }
      else
      {
         (void)fprintf(stdout, "Socket send buffer   : %u\n",
                       fsa[j].socksnd_bufsize);
      }
      if (fsa[j].sockrcv_bufsize == 0)
      {
         (void)fprintf(stdout, "Socket rcv buffer    : Not set\n");
      }
      else
      {
         (void)fprintf(stdout, "Socket rcv buffer    : %u\n",
                       fsa[j].sockrcv_bufsize);
      }
      if (fsa[j].keep_connected == 0)
      {
         (void)fprintf(stdout, "Keep connected       : Not set\n");
      }
      else
      {
         (void)fprintf(stdout, "Keep connected       : %u\n",
                       fsa[j].keep_connected);
      }
      if (fsa[j].proxy_name[0] != '\0')
      {
         (void)fprintf(stdout, "Proxy name           : >%s<\n", fsa[j].proxy_name);
      }
      else
      {
         (void)fprintf(stdout, "Proxy name           : NONE\n");
      }
      if (fsa[j].debug == NORMAL_MODE)
      {
         (void)fprintf(stdout, "Debug mode           : OFF\n");
      }
      else if (fsa[j].debug == DEBUG_MODE)
           {
              (void)fprintf(stdout, "Debug mode           : DEBUG\n");
           }
      else if (fsa[j].debug == TRACE_MODE)
           {
              (void)fprintf(stdout, "Debug mode           : TRACE\n");
           }
      else if (fsa[j].debug == FULL_TRACE_MODE)
           {
              (void)fprintf(stdout, "Debug mode           : FULL TRACE\n");
           }
           else
           {
              (void)fprintf(stdout, "Debug mode           : Unknown\n");
           }
#ifdef WITH_DUP_CHECK
      if (fsa[j].dup_check_timeout == 0L)
      {
         (void)fprintf(stdout, "Dupcheck timeout     : Disabled\n");
      }
      else
      {
# if SIZEOF_TIME_T == 4
         (void)fprintf(stdout, "Dupcheck timeout     : %ld\n",
# else
         (void)fprintf(stdout, "Dupcheck timeout     : %lld\n",
# endif
                       (pri_time_t)fsa[j].dup_check_timeout);
         (void)fprintf(stdout, "Dupcheck flag        : ");
         if (fsa[j].dup_check_flag & DC_FILENAME_ONLY)
         {
            (void)fprintf(stdout, "FILENAME_ONLY ");
         }
         else if (fsa[j].dup_check_flag & DC_NAME_NO_SUFFIX)
              {
                 (void)fprintf(stdout, "NAME_NO_SUFFIX ");
              }
         else if (fsa[j].dup_check_flag & DC_FILENAME_AND_SIZE)
              {
                 (void)fprintf(stdout, "NAME_SIZE ");
              }
         else if (fsa[j].dup_check_flag & DC_FILE_CONTENT)
              {
                 (void)fprintf(stdout, "FILE_CONTENT ");
              }
         else if (fsa[j].dup_check_flag & DC_FILE_CONT_NAME)
              {
                 (void)fprintf(stdout, "FILE_NAME_CONT ");
              }
              else
              {
                 (void)fprintf(stdout, "UNKNOWN_TYPE ");
              }
         if (fsa[j].dup_check_flag & DC_DELETE)
         {
            (void)fprintf(stdout, "DELETE ");
         }
         else if (fsa[j].dup_check_flag & DC_STORE)
              {
                 (void)fprintf(stdout, "STORE ");
              }
         else if (fsa[j].dup_check_flag & DC_WARN)
              {
                 (void)fprintf(stdout, "WARN ");
              }
         if (fsa[j].dup_check_flag & DC_CRC32)
         {
            (void)fprintf(stdout, "CRC32 ");
         }
         else if (fsa[j].dup_check_flag & DC_CRC32C)
              {
                 (void)fprintf(stdout, "CRC32C ");
              }
         else if (fsa[j].dup_check_flag & DC_MURMUR3)
              {
                 (void)fprintf(stdout, "MURMUR3 ");
              }
              else
              {
                 (void)fprintf(stdout, "UNKNOWN_CRC ");
              }
         if (fsa[j].dup_check_flag & TIMEOUT_IS_FIXED)
         {
            (void)fprintf(stdout, "TIMEOUT_IS_FIXED ");
         }
         if (fsa[j].dup_check_flag & USE_RECIPIENT_ID)
         {
            (void)fprintf(stdout, "USE_RECIPIENT_ID");
         }
         (void)fprintf(stdout, "\n");
      }
#endif
      (void)fprintf(stdout, "Host status (%7d): ", fsa[j].host_status);
      if (fsa[j].host_status & PAUSE_QUEUE_STAT)
      {
         (void)fprintf(stdout, "PAUSE_QUEUE ");
      }
      if (fsa[j].host_status & AUTO_PAUSE_QUEUE_STAT)
      {
         (void)fprintf(stdout, "AUTO_PAUSE_QUEUE ");
      }
#ifdef WITH_ERROR_QUEUE
      if (fsa[j].host_status & ERROR_QUEUE_SET)
      {
         (void)fprintf(stdout, "ERROR_QUEUE_SET ");
      }
#endif
      if (fsa[j].host_status & STOP_TRANSFER_STAT)
      {
         (void)fprintf(stdout, "STOP_TRANSFER ");
      }
      if (fsa[j].host_status & HOST_CONFIG_HOST_DISABLED)
      {
         (void)fprintf(stdout, "HOST_CONFIG_HOST_DISABLED ");
      }
      if ((fsa[j].special_flag & HOST_IN_DIR_CONFIG) == 0)
      {
         (void)fprintf(stdout, "HOST_NOT_IN_DIR_CONFIG ");
      }
      if (fsa[j].host_status & DANGER_PAUSE_QUEUE_STAT)
      {
         (void)fprintf(stdout, "DANGER_PAUSE_QUEUE_STAT ");
      }
      if (fsa[j].host_status & HOST_ERROR_ACKNOWLEDGED)
      {
         (void)fprintf(stdout, "HOST_ERROR_ACKNOWLEDGED ");
      }
      if (fsa[j].host_status & HOST_ERROR_ACKNOWLEDGED_T)
      {
         (void)fprintf(stdout, "HOST_ERROR_ACKNOWLEDGED_T ");
      }
      if (fsa[j].host_status & HOST_ERROR_OFFLINE)
      {
         (void)fprintf(stdout, "HOST_ERROR_OFFLINE ");
      }
      if (fsa[j].host_status & HOST_ERROR_OFFLINE_T)
      {
         (void)fprintf(stdout, "HOST_ERROR_OFFLINE_T ");
      }
      if (fsa[j].host_status & HOST_ERROR_OFFLINE_STATIC)
      {
         (void)fprintf(stdout, "HOST_ERROR_OFFLINE_STATIC ");
      }
      if (fsa[j].host_status & DO_NOT_DELETE_DATA)
      {
         (void)fprintf(stdout, "DO_NOT_DELETE_DATA ");
      }
      if (fsa[j].host_status & HOST_ACTION_SUCCESS)
      {
         (void)fprintf(stdout, "HOST_ACTION_SUCCESS ");
      }
#ifdef WITH_IP_DB
      if (fsa[j].host_status & STORE_IP)
      {
         (void)fprintf(stdout, "STORE_IP ");
      }
#endif
      if (fsa[j].host_status & SIMULATE_SEND_MODE)
      {
         (void)fprintf(stdout, "SIMULATE_SEND_MODE ");
      }
      if (fsa[j].host_status & ERROR_HOSTS_IN_GROUP)
      {
         (void)fprintf(stdout, "ERROR_HOSTS_IN_GROUP ");
      }
      if (fsa[j].host_status & WARN_HOSTS_IN_GROUP)
      {
         (void)fprintf(stdout, "WARN_HOSTS_IN_GROUP ");
      }
      if (fsa[j].real_hostname[0][0] == GROUP_IDENTIFIER)
      {
         if (fsa[j].host_status & ERROR_HOSTS_IN_GROUP)
         {
            (void)fprintf(stdout, "NOT_WORKING\n");
         }
         else if (fsa[j].host_status & WARN_HOSTS_IN_GROUP)
              {
                 (void)fprintf(stdout, "WARNING_STATUS\n");
              }
              else
              {
                 if (fsa[j].active_transfers > 0)
                 {
                    (void)fprintf(stdout, "TRANSFER_ACTIVE\n");
                 }
                 else
                 {
                    (void)fprintf(stdout, "NORMAL_STATUS\n");
                 }
              }
      }
      else
      {
         if ((fsa[j].error_counter >= fsa[j].max_errors) &&
             ((fsa[j].host_status & HOST_ERROR_ACKNOWLEDGED) == 0) &&
             ((fsa[j].host_status & HOST_ERROR_ACKNOWLEDGED_T) == 0) &&
             ((fsa[j].host_status & HOST_ERROR_OFFLINE) == 0) &&
             ((fsa[j].host_status & HOST_ERROR_OFFLINE_T) == 0) &&
             ((fsa[j].host_status & HOST_ERROR_OFFLINE_STATIC) == 0))
         {
            (void)fprintf(stdout, "NOT_WORKING\n");
         }
         else if ((fsa[j].host_status & HOST_WARN_TIME_REACHED) &&
                  ((fsa[j].host_status & HOST_ERROR_ACKNOWLEDGED) == 0) &&
                  ((fsa[j].host_status & HOST_ERROR_ACKNOWLEDGED_T) == 0) &&
                  ((fsa[j].host_status & HOST_ERROR_OFFLINE) == 0) &&
                  ((fsa[j].host_status & HOST_ERROR_OFFLINE_T) == 0) &&
                  ((fsa[j].host_status & HOST_ERROR_OFFLINE_STATIC) == 0))
              {
                 (void)fprintf(stdout, "WARNING_STATUS\n");
              }
              else
              {
                 if (fsa[j].active_transfers > 0)
                 {
                    (void)fprintf(stdout, "TRANSFER_ACTIVE\n");
                 }
                 else
                 {
                    (void)fprintf(stdout, "NORMAL_STATUS\n");
                 }
              }
      }

      (void)fprintf(stdout, "Transfer timeout     : %ld\n",
                    fsa[j].transfer_timeout);
      (void)fprintf(stdout, "File size offset     : %d\n",
                    fsa[j].file_size_offset);
      (void)fprintf(stdout, "Successful retries   : %d\n",
                    fsa[j].successful_retries);
      (void)fprintf(stdout, "MaxSuccessful ret.   : %d\n",
                    fsa[j].max_successful_retries);
      (void)fprintf(stdout, "Special flag (%3d)   : ", fsa[j].special_flag);
      if (fsa[j].special_flag & KEEP_CON_NO_FETCH)
      {
         (void)fprintf(stdout, "KEEP_CON_NO_FETCH ");
      }
      if (fsa[j].special_flag & KEEP_CON_NO_SEND)
      {
         (void)fprintf(stdout, "KEEP_CON_NO_SEND ");
      }
      if (fsa[j].special_flag & HOST_DISABLED)
      {
         (void)fprintf(stdout, "HOST_DISABLED ");
      }
      if (fsa[j].special_flag & HOST_IN_DIR_CONFIG)
      {
         (void)fprintf(stdout, "HOST_IN_DIR_CONFIG");
      }
      (void)fprintf(stdout, "\nError counter        : %d\n",
                    fsa[j].error_counter);
      (void)fprintf(stdout, "Total errors         : %u\n",
                    fsa[j].total_errors);
      (void)fprintf(stdout, "Max. errors          : %d\n",
                    fsa[j].max_errors);
      (void)fprintf(stdout, "Error history        : %03d -> %s\n",
                    fsa[j].error_history[0],
                    get_error_str(fsa[j].error_history[0]));
      for (i = 1; i < ERROR_HISTORY_LENGTH; i++)
      {
         (void)fprintf(stdout, "                       %03d -> %s\n",
                       fsa[j].error_history[i],
                       get_error_str(fsa[j].error_history[i]));
      }
      (void)fprintf(stdout, "Retry interval       : %d\n",
                    fsa[j].retry_interval);
      (void)fprintf(stdout, "Transfer block size  : %d\n",
                    fsa[j].block_size);
      (void)fprintf(stdout, "TTL                  : %d\n",
                    fsa[j].ttl);
      (void)fprintf(stdout, "Time of last retry   : %s",
                    ctime(&fsa[j].last_retry_time));
      (void)fprintf(stdout, "Last connection      : %s",
                    ctime(&fsa[j].last_connection));
      if (fsa[j].first_error_time == 0L)
      {
         (void)fprintf(stdout, "First error time     : Not set.\n");
      }
      else
      {
         (void)fprintf(stdout, "First error time     : %s",
                       ctime(&fsa[j].first_error_time));
      }
      if (fsa[j].start_event_handle == 0L)
      {
         (void)fprintf(stdout, "Start event handle   : Not set.\n");
      }
      else
      {
         (void)fprintf(stdout, "Start event handle   : %s",
                       ctime(&fsa[j].start_event_handle));
      }
      if (fsa[j].end_event_handle == 0L)
      {
         (void)fprintf(stdout, "End event handle     : Not set.\n");
      }
      else
      {
         (void)fprintf(stdout, "End event handle     : %s",
                       ctime(&fsa[j].end_event_handle));
      }
      if (fsa[j].warn_time == 0L)
      {
         (void)fprintf(stdout, "Warn time            : Not set.\n");
      }
      else
      {
#if SIZEOF_TIME_T == 4
         (void)fprintf(stdout, "Warn time            : %ld\n",
#else
         (void)fprintf(stdout, "Warn time            : %lld\n",
#endif
                       (pri_time_t)fsa[j].warn_time);
      }
      (void)fprintf(stdout, "Total file counter   : %d\n",
                    fsa[j].total_file_counter);
#if SIZEOF_OFF_T == 4
      (void)fprintf(stdout, "Total file size      : %ld\n",
#else
      (void)fprintf(stdout, "Total file size      : %lld\n",
#endif
                    (pri_off_t)fsa[j].total_file_size);
      (void)fprintf(stdout, "File counter done    : %u\n",
                    fsa[j].file_counter_done);
#if SIZEOF_OFF_T == 4
      (void)fprintf(stdout, "Bytes send           : %lu\n",
#else
      (void)fprintf(stdout, "Bytes send           : %llu\n",
#endif
                    fsa[j].bytes_send);
      (void)fprintf(stdout, "Connections          : %u\n",
                    fsa[j].connections);
      (void)fprintf(stdout, "Jobs queued          : %u\n",
                    fsa[j].jobs_queued);
      (void)fprintf(stdout, "Active transfers     : %d\n",
                    fsa[j].active_transfers);
      (void)fprintf(stdout, "Allowed transfers    : %d\n",
                    fsa[j].allowed_transfers);
#if SIZEOF_OFF_T == 4
      (void)fprintf(stdout, "Rate limit           : %ld\n",
                    (pri_off_t)fsa[j].transfer_rate_limit);
      (void)fprintf(stdout, "Rate limit per proc  : %ld\n",
                    (pri_off_t)fsa[j].trl_per_process);
#else
      (void)fprintf(stdout, "Rate limit           : %lld\n",
                    (pri_off_t)fsa[j].transfer_rate_limit);
      (void)fprintf(stdout, "Rate limit per proc  : %lld\n",
                    (pri_off_t)fsa[j].trl_per_process);
#endif

      if (fsa[j].real_hostname[0][0] != GROUP_IDENTIFIER)
      {
         if (view_type == SHORT_VIEW)
         {
            int                        k,
                                       max_digits = 9,
                                       nr_of_digits;
            struct filetransfer_status sfsa;

            /*
             * Lets get the longest value so we can setup the table as
             * compact as possible. For this to work we need to keep the
             * values in FSA constant until we are done. So just copy them
             * to a new static storage and use that then.
             */
            (void)memcpy(&sfsa, &fsa[j], sizeof(struct filetransfer_status));
            for (i = 0; i < sfsa.allowed_transfers; i++)
            {
               GET_MAX_DIGIT(sfsa.job_status[i].proc_id);
               GET_MAX_DIGIT(sfsa.job_status[i].no_of_files);
               GET_MAX_DIGIT(sfsa.job_status[i].no_of_files_done);
               GET_MAX_DIGIT(sfsa.job_status[i].file_size);
               GET_MAX_DIGIT(sfsa.job_status[i].file_size_done);
               GET_MAX_DIGIT(sfsa.job_status[i].bytes_send);
               GET_MAX_DIGIT(sfsa.job_status[i].file_size_in_use);
               GET_MAX_DIGIT(sfsa.job_status[i].file_size_in_use_done);
            }
            (void)fprintf(stdout, "                    ");
            for (i = 0; i < sfsa.allowed_transfers; i++)
            {
               if (i < 10)
               {
                  k = fprintf(stdout, "| Job %d", i);
               }
               else if (i < 100)
                    {
                       k = fprintf(stdout, "| Job %d", i);
                    }
               else if (i < 1000)
                    {
                       k = fprintf(stdout, "| Job %d", i);
                    }
               else if (i < 10000)
                    {
                       k = fprintf(stdout, "| Job %d", i);
                    }
                    else
                    {
                       k = fprintf(stdout, "| Job XXXX");
                    }

               if (k < max_digits)
               {
                  int m;

                  for (m = 0; m < (max_digits - k + 2); m++)
                  {
                     (void)fprintf(stdout, " ");
                  }
               }
            }
            (void)fprintf(stdout, "\n");
            (void)fprintf(stdout, "--------------------");
            for (i = 0; i < sfsa.allowed_transfers; i++)
            {
               (void)fprintf(stdout, "+");
               for (k = 0; k < (max_digits + 1); k++)
               {
                  (void)fprintf(stdout, "-");
               }
            }
            (void)fprintf(stdout, "\n");
            (void)fprintf(stdout, "PID                 ");
            for (i = 0; i < sfsa.allowed_transfers; i++)
            {
#if SIZEOF_PID_T == 4
               (void)fprintf(stdout, "|%*d ", max_digits,
#else
               (void)fprintf(stdout, "|%*lld ", max_digits,
#endif
                             (pri_pid_t)sfsa.job_status[i].proc_id);
            }
            (void)fprintf(stdout, "\n");
            (void)fprintf(stdout, "Connect status      ");
            for (i = 0; i < fsa[j].allowed_transfers; i++)
            {
               switch (sfsa.job_status[i].connect_status)
               {
                  case CONNECTING :
                     if (sfsa.protocol & LOC_FLAG)
                     {
                        if ((sfsa.protocol & FTP_FLAG) ||
                            (sfsa.protocol & SFTP_FLAG) ||
                            (sfsa.protocol & HTTP_FLAG) ||
#ifdef _WITH_MAP_SUPPORT
                            (sfsa.protocol & MAP_FLAG) ||
#endif
#ifdef _WITH_SCP_SUPPORT
                            (sfsa.protocol & SCP_FLAG) ||
#endif
#ifdef _WITH_WMO_SUPPORT
                            (sfsa.protocol & WMO_FLAG) ||
#endif
                            (sfsa.protocol & SMTP_FLAG))
                        {
                           (void)fprintf(stdout, "|CONNECTING ");
                        }
                        else
                        {
                           (void)fprintf(stdout, "|CON or LOCB");
                        }
                     }
                     else
                     {
                        (void)fprintf(stdout, "|CONNECTING ");
                     }
                     break;

                  case DISCONNECT :
                     (void)fprintf(stdout, "|DISCONNECT ");
                     break;

                  case NOT_WORKING :
                     (void)fprintf(stdout, "|NOT WORKING");
                     break;

                  case FTP_ACTIVE :
                     (void)fprintf(stdout, "|    FTP    ");
                     break;

                  case FTP_BURST2_TRANSFER_ACTIVE :
                     (void)fprintf(stdout, "| FTP BURST ");
                     break;

                  case FTP_RETRIEVE_ACTIVE :
                     (void)fprintf(stdout, "| FTP RETR  ");
                     break;

                  case SFTP_ACTIVE :
#ifdef _WITH_MAP_SUPPORT
                     /* or MAP_ACTIVE */
                     (void)fprintf(stdout, "| SFTP/MAP  ");
#else
                     (void)fprintf(stdout, "|    SFTP   ");
#endif
                     break;

                  case SFTP_BURST_TRANSFER_ACTIVE :
                     (void)fprintf(stdout, "| SFTP BURST");
                     break;

                  case SFTP_RETRIEVE_ACTIVE :
#ifdef _WITH_SCP_SUPPORT
                     if (fsa[j].protocol & SFTP_FLAG)
                     {
                        (void)fprintf(stdout, "| SFTP RETR ");
                     }
                     else
                     {
                        (void)fprintf(stdout, "| SCP BURST");
                     }
#else
                     (void)fprintf(stdout, "| SFTP RETR ");
#endif
                     break;

                  case LOC_ACTIVE :
                     (void)fprintf(stdout, "|    LOC    ");
                     break;

                  case HTTP_ACTIVE :
                     (void)fprintf(stdout, "|    HTTP   ");
                     break;

                  case HTTP_RETRIEVE_ACTIVE :
                     (void)fprintf(stdout, "| HTTP RETR ");
                     break;

                  case SMTP_BURST_TRANSFER_ACTIVE :
                     (void)fprintf(stdout, "| SMTP BURST");
                     break;

                  case SMTP_ACTIVE :
                     (void)fprintf(stdout, "|    SMTP   ");
                     break;

#ifdef _WITH_SCP_SUPPORT
                  case SCP_ACTIVE :
                     (void)fprintf(stdout, "| SCP ACTIV");
                     break;
#endif
#ifdef _WITH_WMO_SUPPORT
                  case WMO_BURST_TRANSFER_ACTIVE :
                     (void)fprintf(stdout, "| WMO BURST ");
                     break;

                  case WMO_ACTIVE :
                     (void)fprintf(stdout, "| WMO ACTIV ");
                     break;
#endif

                  case CLOSING_CONNECTION :
                     (void)fprintf(stdout, "|CLOSING CON");
                     break;

                  default :
                     (void)fprintf(stdout, "|  Unknown  ");
                     break;
               }
               if (max_digits > 10)
               {
                  int m;

                  for (m = 0; m < (max_digits - 10); m++)
                  {
                     (void)fprintf(stdout, " ");
                  }
               }
            }
            (void)fprintf(stdout, "\n");
            (void)fprintf(stdout, "Special flag        ");
            for (i = 0; i < sfsa.allowed_transfers; i++)
            {
               (void)fprintf(stdout, "|%*d ", max_digits,
                             (int)sfsa.job_status[i].special_flag);
            }
            (void)fprintf(stdout, "\n");
            (void)fprintf(stdout, "Number of files     ");
            for (i = 0; i < sfsa.allowed_transfers; i++)
            {
               (void)fprintf(stdout, "|%*d ",
                             max_digits, sfsa.job_status[i].no_of_files);
            }
            (void)fprintf(stdout, "\n");
            (void)fprintf(stdout, "No. of files done   ");
            for (i = 0; i < sfsa.allowed_transfers; i++)
            {
               (void)fprintf(stdout, "|%*d ",
                             max_digits, sfsa.job_status[i].no_of_files_done);
            }
            (void)fprintf(stdout, "\n");
            (void)fprintf(stdout, "File size           ");
            for (i = 0; i < sfsa.allowed_transfers; i++)
            {
#if SIZEOF_OFF_T == 4
               (void)fprintf(stdout, "|%*d ", max_digits,
#else
               (void)fprintf(stdout, "|%*lld ", max_digits,
#endif
                             (pri_off_t)sfsa.job_status[i].file_size);
            }
            (void)fprintf(stdout, "\n");
            (void)fprintf(stdout, "File size done      ");
            for (i = 0; i < sfsa.allowed_transfers; i++)
            {
#if SIZEOF_OFF_T == 4
               (void)fprintf(stdout, "|%*lu ", max_digits,
#else
               (void)fprintf(stdout, "|%*llu ", max_digits,
#endif
                             sfsa.job_status[i].file_size_done);
            }
            (void)fprintf(stdout, "\n");
            (void)fprintf(stdout, "Bytes send          ");
            for (i = 0; i < sfsa.allowed_transfers; i++)
            {
#if SIZEOF_OFF_T == 4
               (void)fprintf(stdout, "|%*lu ", max_digits,
#else
               (void)fprintf(stdout, "|%*llu ", max_digits,
#endif
                             sfsa.job_status[i].bytes_send);
            }
            (void)fprintf(stdout, "\n");
            (void)fprintf(stdout, "File name in use    ");
            for (i = 0; i < sfsa.allowed_transfers; i++)
            {
               (void)fprintf(stdout, "|%*.*s", max_digits + 1, max_digits + 1,
                             sfsa.job_status[i].file_name_in_use);
            }
            (void)fprintf(stdout, "\n");
            (void)fprintf(stdout, "File size in use    ");
            for (i = 0; i < sfsa.allowed_transfers; i++)
            {
#if SIZEOF_OFF_T == 4
               (void)fprintf(stdout, "|%*d ", max_digits,
#else
               (void)fprintf(stdout, "|%*lld ", max_digits,
#endif
                             (pri_off_t)sfsa.job_status[i].file_size_in_use);
            }
            (void)fprintf(stdout, "\n");
            (void)fprintf(stdout, "Filesize in use done");
            for (i = 0; i < sfsa.allowed_transfers; i++)
            {
#if SIZEOF_OFF_T == 4
               (void)fprintf(stdout, "|%*d ", max_digits,
#else
               (void)fprintf(stdout, "|%*lld ", max_digits,
#endif
                             (pri_off_t)sfsa.job_status[i].file_size_in_use_done);
            }
            (void)fprintf(stdout, "\n");
#ifdef _WITH_BURST_2
            (void)fprintf(stdout, "Unique name         ");
            for (i = 0; i < sfsa.allowed_transfers; i++)
            {
               if ((sfsa.job_status[i].unique_name[1] == 1) ||
                   (sfsa.job_status[i].unique_name[1] == 2) ||
                   (sfsa.job_status[i].unique_name[1] == 3) ||
                   (sfsa.job_status[i].unique_name[1] == 4) ||
                   (sfsa.job_status[i].unique_name[1] == 5) ||
                   (sfsa.job_status[i].unique_name[1] == 6) ||
                   (sfsa.job_status[i].unique_name[2] == 1) ||
                   (sfsa.job_status[i].unique_name[2] == 2) ||
                   (sfsa.job_status[i].unique_name[2] == 3) ||
                   (sfsa.job_status[i].unique_name[2] == 4) ||
                   (sfsa.job_status[i].unique_name[2] == 5) ||
                   (sfsa.job_status[i].unique_name[2] == 6) ||
                   (sfsa.job_status[i].unique_name[3] == 1) ||
                   (sfsa.job_status[i].unique_name[3] == 2) ||
                   (sfsa.job_status[i].unique_name[3] == 3) ||
                   (sfsa.job_status[i].unique_name[3] == 4) ||
                   (sfsa.job_status[i].unique_name[3] == 5) ||
                   (sfsa.job_status[i].unique_name[3] == 6))
               {
                  (void)fprintf(stdout, "|<%d><%d><%d><%d><%d>",
                                (int)sfsa.job_status[i].unique_name[0],
                                (int)sfsa.job_status[i].unique_name[1],
                                (int)sfsa.job_status[i].unique_name[2],
                                (int)sfsa.job_status[i].unique_name[3],
                                (int)sfsa.job_status[i].unique_name[4]);
               }
               else if ((sfsa.job_status[i].unique_name[1] == '\0') ||
                        (sfsa.job_status[i].unique_name[2] == '\0'))
                    {
                       (void)fprintf(stdout, "|%*.*s",
                                     max_digits + 1, max_digits + 1, " ");
                    }
                    else
                    {
                       (void)fprintf(stdout, "|%*.*s",
                                     max_digits + 1, max_digits + 1,
                                     sfsa.job_status[i].unique_name);
                    }
            }
            (void)fprintf(stdout, "\n");
            (void)fprintf(stdout, "Job ID              ");
            for (i = 0; i < sfsa.allowed_transfers; i++)
            {
               (void)fprintf(stdout, "|%*x ",
                             max_digits, (int)sfsa.job_status[i].job_id);
            }
            (void)fprintf(stdout, "\n");
#endif
         }
         else
         {
            for (i = 0; i < fsa[j].allowed_transfers; i++)
            {
               (void)fprintf(stdout,
                             "-------- Job %2d -----+------------------------------------------------------\n",
                             i);
#if SIZEOF_PID_T == 4
               (void)fprintf(stdout, "PID                  : %d\n",
#else
               (void)fprintf(stdout, "PID                  : %lld\n",
#endif
                             (pri_pid_t)fsa[j].job_status[i].proc_id);
               switch (fsa[j].job_status[i].connect_status)
               {
                  case CONNECTING :
                     if (fsa[j].protocol & LOC_FLAG)
                     {
                        if ((fsa[j].protocol & FTP_FLAG) ||
                            (fsa[j].protocol & SFTP_FLAG) ||
                            (fsa[j].protocol & HTTP_FLAG) ||
#ifdef _WITH_MAP_SUPPORT
                            (fsa[j].protocol & MAP_FLAG) ||
#endif
#ifdef _WITH_SCP_SUPPORT
                            (fsa[j].protocol & SCP_FLAG) ||
#endif
#ifdef _WITH_WMO_SUPPORT
                            (fsa[j].protocol & WMO_FLAG) ||
#endif
                            (fsa[j].protocol & SMTP_FLAG))
                        {
                           (void)fprintf(stdout, "Connect status       : CONNECTING or LOC burst\n");
                        }
                        else
                        {
                           (void)fprintf(stdout, "Connect status       : CONNECTING\n");
                        }
                     }
                     else
                     {
                        (void)fprintf(stdout, "Connect status       : CONNECTING\n");
                     }
                     break;

                  case DISCONNECT :
                     (void)fprintf(stdout, "Connect status       : DISCONNECT\n");
                     break;

                  case NOT_WORKING :
                     (void)fprintf(stdout, "Connect status       : NOT working\n");
                     break;

                  case FTP_ACTIVE :
                     (void)fprintf(stdout, "Connect status       : FTP active\n");
                     break;

                  case FTP_BURST2_TRANSFER_ACTIVE :
                     (void)fprintf(stdout, "Connect status       : FTP burst active\n");
                     break;

                  case SFTP_ACTIVE :
#ifdef _WITH_MAP_SUPPORT
                     /* or MAP_ACTIVE */
                     (void)fprintf(stdout, "Connect status       : SFTP/MAP active\n");
#else
                     (void)fprintf(stdout, "Connect status       : SFTP active\n");
#endif
                     break;

                  case SFTP_BURST_TRANSFER_ACTIVE :
                     (void)fprintf(stdout, "Connect status       : SFTP burst active\n");
                     break;

                  case SFTP_RETRIEVE_ACTIVE :
                     (void)fprintf(stdout, "Connect status       : SFTP retrieve active\n");
                     break;

                  case LOC_ACTIVE :
                     (void)fprintf(stdout, "Connect status       : LOC active\n");
                     break;

                  case HTTP_ACTIVE :
                     (void)fprintf(stdout, "Connect status       : HTTP active\n");
                     break;

                  case HTTP_RETRIEVE_ACTIVE :
                     (void)fprintf(stdout, "Connect status       : HTTP retrieve active\n");
                     break;

                  case SMTP_BURST_TRANSFER_ACTIVE :
                     (void)fprintf(stdout, "Connect status       : SMTP burst active\n");
                     break;

                  case SMTP_ACTIVE :
                     (void)fprintf(stdout, "Connect status       : SMTP active\n");
                     break;

#ifdef _WITH_SCP_SUPPORT
                  case SCP_BURST_TRANSFER_ACTIVE :
                     (void)fprintf(stdout, "Connect status       : SCP burst active\n");
                     break;

                  case SCP_ACTIVE :
                     (void)fprintf(stdout, "Connect status       : SCP active\n");
                     break;
#endif
#ifdef _WITH_WMO_SUPPORT
                  case WMO_BURST_TRANSFER_ACTIVE :
                     (void)fprintf(stdout, "Connect status       : WMO burst active\n");
                     break;

                  case WMO_ACTIVE :
                     (void)fprintf(stdout, "Connect status       : WMO active\n");
                     break;
#endif

                  case CLOSING_CONNECTION :
                     (void)fprintf(stdout, "Connect status       : Closing connection\n");
                     break;

                  default :
                     (void)fprintf(stdout, "Connect status       : Unknown status\n");
                     break;
               }
               (void)fprintf(stdout, "Special flag         : %d\n",
                             (int)fsa[j].job_status[i].special_flag);
               (void)fprintf(stdout, "Number of files      : %d\n",
                             fsa[j].job_status[i].no_of_files);
               (void)fprintf(stdout, "No. of files done    : %d\n",
                             fsa[j].job_status[i].no_of_files_done);
#if SIZEOF_OFF_T == 4
               (void)fprintf(stdout, "File size            : %ld\n",
#else
               (void)fprintf(stdout, "File size            : %lld\n",
#endif
                             (pri_off_t)fsa[j].job_status[i].file_size);
#if SIZEOF_OFF_T == 4
               (void)fprintf(stdout, "File size done       : %lu\n",
#else
               (void)fprintf(stdout, "File size done       : %llu\n",
#endif
                             fsa[j].job_status[i].file_size_done);
#if SIZEOF_OFF_T == 4
               (void)fprintf(stdout, "Bytes send           : %lu\n",
#else
               (void)fprintf(stdout, "Bytes send           : %llu\n",
#endif
                             fsa[j].job_status[i].bytes_send);
               (void)fprintf(stdout, "File name in use     : %s\n",
                             fsa[j].job_status[i].file_name_in_use);
#if SIZEOF_OFF_T == 4
               (void)fprintf(stdout, "File size in use     : %ld\n",
#else
               (void)fprintf(stdout, "File size in use     : %lld\n",
#endif
                             (pri_off_t)fsa[j].job_status[i].file_size_in_use);
#if SIZEOF_OFF_T == 4
               (void)fprintf(stdout, "File size in use done: %ld\n",
#else
               (void)fprintf(stdout, "File size in use done: %lld\n",
#endif
                             (pri_off_t)fsa[j].job_status[i].file_size_in_use_done);
#ifdef _WITH_BURST_2
               if ((fsa[j].job_status[i].unique_name[1] == 1) ||
                   (fsa[j].job_status[i].unique_name[1] == 2) ||
                   (fsa[j].job_status[i].unique_name[1] == 3) ||
                   (fsa[j].job_status[i].unique_name[1] == 4) ||
                   (fsa[j].job_status[i].unique_name[1] == 5) ||
                   (fsa[j].job_status[i].unique_name[1] == 6) ||
                   (fsa[j].job_status[i].unique_name[2] == 1) ||
                   (fsa[j].job_status[i].unique_name[2] == 2) ||
                   (fsa[j].job_status[i].unique_name[2] == 3) ||
                   (fsa[j].job_status[i].unique_name[2] == 4) ||
                   (fsa[j].job_status[i].unique_name[2] == 5) ||
                   (fsa[j].job_status[i].unique_name[2] == 6) ||
                   (fsa[j].job_status[i].unique_name[3] == 1) ||
                   (fsa[j].job_status[i].unique_name[3] == 2) ||
                   (fsa[j].job_status[i].unique_name[3] == 3) ||
                   (fsa[j].job_status[i].unique_name[3] == 4) ||
                   (fsa[j].job_status[i].unique_name[3] == 5) ||
                   (fsa[j].job_status[i].unique_name[3] == 6))
               {
                  (void)fprintf(stdout, "|<%d><%d><%d><%d><%d>",
                                (int)fsa[j].job_status[i].unique_name[0],
                                (int)fsa[j].job_status[i].unique_name[1],
                                (int)fsa[j].job_status[i].unique_name[2],
                                (int)fsa[j].job_status[i].unique_name[3],
                                (int)fsa[j].job_status[i].unique_name[4]);
               }
               else if ((fsa[j].job_status[i].unique_name[1] == '\0') ||
                        (fsa[j].job_status[i].unique_name[2] == '\0'))
                    {
                       (void)fprintf(stdout, "Unique name          : \n");
                    }
                    else
                    {
                       (void)fprintf(stdout, "Unique name          : %s\n",
                                     fsa[j].job_status[i].unique_name);
                    }
               (void)fprintf(stdout, "Job ID               : %x\n",
                             fsa[j].job_status[i].job_id);
#endif
            }
         }
      }
   }

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ usage() ++++++++++++++++++++++++++++++*/ 
static void
usage(void)
{
   (void)fprintf(stderr,
                 _("SYNTAX  : fsa_view [--version] [-w working directory] [-l|-s] position|host-alias-id|host-alias\n"));
   (void)fprintf(stderr,
                 _("          Options:\n"));
   (void)fprintf(stderr,
                 _("             -l         Long view.\n"));
   (void)fprintf(stderr,
                 _("             -s         Short view.\n"));
   return;
}
