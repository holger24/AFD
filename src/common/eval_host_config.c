/*
 *  eval_host_config.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   eval_host_config - reads the HOST_CONFIG file and stores the
 **                      contents in the structure host_list
 **
 ** SYNOPSIS
 **   int eval_host_config(int              *hosts_found,
 **                        char             *host_config_file,
 **                        struct host_list **hl,
 **                        unsigned int     *warn_counter,
 **                        FILE             *debug_fp,
 **                        int              first_time)
 **
 ** DESCRIPTION
 **   This function reads the HOST_CONFIG file which has the following
 **   format:
 **
 **   Protocol options 2    <-------------------------------------------------------+
 **   Warn time             <---------------------------------------------------+   |
 **   Keep connected        <------------------------------------------------+  |   |
 **   Duplicate check flag    <-------------------------------------------+  |  |   |
 **   Duplicate check timeout <----------------------------------------+  |  |  |   |
 **   Socket receive buffer <---------------------------------------+  |  |  |  |   |
 **   Socket send buffer    <-----------------------------------+   |  |  |  |  |   |
 **                                                             |   |  |  |  |  |   |
 **   AH:HN1:HN2:HT:PXY:AT:ME:RI:TB:SR:FSO:TT:NB:HS:PO:TRL:TTL:SSB:SRB:DT:DF:KC:WT:PO2
 **   |   |   |   |  |  |  |  |  |  |   |  |  |  |  |   |   |
 **   |   |   |   |  |  |  |  |  |  |   |  |  |  |  |   |   +-> TTL
 **   |   |   |   |  |  |  |  |  |  |   |  |  |  |  |   +-----> Transfer rate
 **   |   |   |   |  |  |  |  |  |  |   |  |  |  |  |           limit
 **   |   |   |   |  |  |  |  |  |  |   |  |  |  |  +---------> Protocol options
 **   |   |   |   |  |  |  |  |  |  |   |  |  |  +------------> Host status
 **   |   |   |   |  |  |  |  |  |  |   |  |  +---------------> Number of no
 **   |   |   |   |  |  |  |  |  |  |   |  |                    bursts
 **   |   |   |   |  |  |  |  |  |  |   |  +------------------> Transfer timeout
 **   |   |   |   |  |  |  |  |  |  |   +---------------------> File size offset
 **   |   |   |   |  |  |  |  |  |  +-------------------------> Successful
 **   |   |   |   |  |  |  |  |  |                              retries
 **   |   |   |   |  |  |  |  |  +----------------------------> Transfer block
 **   |   |   |   |  |  |  |  |                                 size
 **   |   |   |   |  |  |  |  +-------------------------------> Retry interval
 **   |   |   |   |  |  |  +----------------------------------> Max. errors
 **   |   |   |   |  |  +-------------------------------------> Allowed transfers
 **   |   |   |   |  +----------------------------------------> Host toggle
 **   |   |   |   +-------------------------------------------> Proxy name
 **   |   |   +-----------------------------------------------> Real hostname 2
 **   |   +---------------------------------------------------> Real hostname 1
 **   +-------------------------------------------------------> Alias hostname
 **
 **   The data is the stored in the global structure host_list. 
 **   Any value that is not set is initialised with its default value.
 **
 ** RETURN VALUES
 **   Will return YES when it finds an incorrect or incomplete entry.
 **   Otherwise it will exit() with INCORRECT when it fails to allocate
 **   memory for the host_list structure.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.11.1997 H.Kiehl Created
 **   02.03.1998 H.Kiehl Put host switching information into HOST_CONFIG.
 **   03.08.2001 H.Kiehl Remember if we stopped the queue or transfer
 **                      and some protocol specific information.
 **   10.06.2004 H.Kiehl Added transfer rate limit.
 **   27.06.2004 H.Kiehl Added TTL.
 **   16.02.2006 H.Kiehl Added socket send and receive buffer.
 **   28.02.2006 H.Kiehl Added keep connected parameter.
 **   09.03.2006 H.Kiehl Added dupcheck on a per host basis.
 **   31.12.2007 H.Kiehl Added warn time.
 **   21.09.2009 H.Kiehl Ensure that one cannot enter 0 as transfer block
 **                      size.
 **   04.03.2017 H.Kiehl Added group support.
 **   11.04.2017 H.Kiehl Added debug_fp parameter to print warnings and
 **                      errors.
 **   19.07.2019 H.Kiehl Allow simulate mode for host_status.
 **   29.11.2021 H.Kiehl Added protocol option bucketname in path.
 **   26.01.2022 H.Kiehl Added protocol options 2.
 **   02.08.2022 H.Kiehl Added protocol option TLS legacy renegotiation.
 **   08.02.2023 H.Kiehl For group entry always set host_status to
 **                      HOST_NOT_IN_DIR_CONFIG otherwise reread_host_config()
 **                      falsely thinks that there are changes.
 **   18.02.2023 H.Kiehl Added 'OPTS UTF8 ON' option for windows FTP
 **                      server.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>                  /* F_OK                            */
#include <stdlib.h>                  /* atoi(), exit(), free()          */
#include <errno.h>

/* External global variables. */
extern int                        no_of_hosts;
extern struct filetransfer_status *fsa;

#define OLD_FTP_PASSIVE_MODE      1024
#define OLD_SET_IDLE_TIME         2048
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
# define OLD_STAT_KEEPALIVE       4096
#endif


/*+++++++++++++++++++++++++ eval_host_config() ++++++++++++++++++++++++++*/
int
eval_host_config(int              *hosts_found,
                 char             *host_config_file,
                 struct host_list **hl,
                 unsigned int     *warn_counter,
                 FILE             *debug_fp,
                 int              first_time)
{
   int    i,
          error_flag = NO,
          host_counter;
   size_t new_size,
          offset;
   char   *ptr,
          *hostbase = NULL,
#ifdef WITH_DUP_CHECK
          number[MAX_LONG_LENGTH + 1];
#else
          number[MAX_INT_LENGTH + 1];
#endif

   /* Ensure that the HOST_CONFIG file does exist. */
   if (eaccess(host_config_file, F_OK) == -1)
   {
      /* The file does not exist. Don't care since we will create */
      /* when we return from eval_dir_config().                   */
      return(NO_ACCESS);
   }

   /* Read the contents of the HOST_CONFIG file into a buffer. */
   if (read_file_no_cr(host_config_file, &hostbase, YES, __FILE__, __LINE__) == INCORRECT)
   {
      exit(INCORRECT);
   }

   ptr = hostbase;

   /*
    * NOTE: We need the temporal storage host_counter, since *hosts_found
    *       is really no_of_hosts! And this is always reset by function
    *       fsa_attach() and thus can cause some very strang behaviour.
    */
   host_counter = 0;

   /*
    * Cut off any comments before the hostname come.
    */
   for (;;)
   {
      if ((*ptr != '\n') && (*ptr != '#') &&
          (*ptr != ' ') && (*ptr != '\t'))
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

      /* Check if buffer for host list structure is large enough. */
      if ((host_counter % HOST_BUF_SIZE) == 0)
      {
         /* Calculate new size for host list. */
         new_size = ((host_counter / HOST_BUF_SIZE) + 1) *
                    HOST_BUF_SIZE * sizeof(struct host_list);

         /* Now increase the space. */
         if ((*hl = realloc(*hl, new_size)) == NULL)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("Could not reallocate memory for host list : %s"),
                       strerror(errno));
            exit(INCORRECT);
         }

         /* Initialise the new memory area. */
         new_size = HOST_BUF_SIZE * sizeof(struct host_list);
         offset = (host_counter / HOST_BUF_SIZE) * new_size;
         (void)memset((char *)(*hl) + offset, 0, new_size);
      }

      /* Store host alias. */
      i = 0;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_HOSTNAME_LENGTH))
      {
         (*hl)[host_counter].host_alias[i] = *ptr;
         ptr++; i++;
      }
      if ((i == MAX_HOSTNAME_LENGTH) && (*ptr != ':') && (*ptr != '\n'))
      {
         error_flag = YES;
         update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                       _("Maximum length for host alias name %s exceeded in HOST_CONFIG. Will be truncated to %d characters."),
                       (*hl)[host_counter].host_alias, MAX_HOSTNAME_LENGTH);
         while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
      }
      (*hl)[host_counter].host_alias[i] = '\0';
      (*hl)[host_counter].fullname[0] = '\0';
      (*hl)[host_counter].in_dir_config = (char)NO;
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Mark as group. */
         (*hl)[host_counter].real_hostname[0][0] = 1;

         /* Initialise all other values with DEFAULTS. */
         (*hl)[host_counter].real_hostname[1][0] = '\0';
         (*hl)[host_counter].proxy_name[0]       = '\0';
         (*hl)[host_counter].host_toggle_str[0]  = '\0';
         (*hl)[host_counter].allowed_transfers   = MAX_NO_PARALLEL_JOBS;
         (*hl)[host_counter].max_errors          = 0;
         (*hl)[host_counter].retry_interval      = 0;
         (*hl)[host_counter].transfer_blksize    = 0;
         (*hl)[host_counter].successful_retries  = 0;
         (*hl)[host_counter].file_size_offset    = 0;
         (*hl)[host_counter].transfer_timeout    = 0;
         (*hl)[host_counter].number_of_no_bursts = 0;
         (*hl)[host_counter].host_status         = HOST_NOT_IN_DIR_CONFIG;
         (*hl)[host_counter].protocol_options    = DEFAULT_PROTOCOL_OPTIONS;
         (*hl)[host_counter].transfer_rate_limit = 0;
         (*hl)[host_counter].ttl                 = 0;
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Real hostname 1. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_REAL_HOSTNAME_LENGTH))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         (*hl)[host_counter].real_hostname[0][i] = *ptr;
         ptr++; i++;
      }
      if (i == MAX_REAL_HOSTNAME_LENGTH)
      {
         error_flag = YES;
         update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                       _("Maximum length for real hostname 1 for %s exceeded in HOST_CONFIG. Will be truncated to %d characters."),
                       (*hl)[host_counter].host_alias, MAX_REAL_HOSTNAME_LENGTH);
         while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
         {
            if (*ptr == '\\')
            {
               ptr++;
            }
            ptr++;
         }
         (*hl)[host_counter].real_hostname[0][i - 1] = '\0';
      }
      else
      {
         (*hl)[host_counter].real_hostname[0][i] = '\0';
      }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS. */
         (*hl)[host_counter].real_hostname[1][0] = '\0';
         (*hl)[host_counter].proxy_name[0]       = '\0';
         (*hl)[host_counter].host_toggle_str[0]  = '\0';
         (*hl)[host_counter].allowed_transfers   = DEFAULT_NO_PARALLEL_JOBS;
         (*hl)[host_counter].max_errors          = DEFAULT_MAX_ERRORS;
         (*hl)[host_counter].retry_interval      = DEFAULT_RETRY_INTERVAL;
         (*hl)[host_counter].transfer_blksize    = DEFAULT_TRANSFER_BLOCKSIZE;
         (*hl)[host_counter].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         (*hl)[host_counter].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         (*hl)[host_counter].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         (*hl)[host_counter].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         (*hl)[host_counter].host_status         = DEFAULT_FSA_HOST_STATUS;
         (*hl)[host_counter].protocol_options    = DEFAULT_PROTOCOL_OPTIONS;
         (*hl)[host_counter].transfer_rate_limit = 0;
         (*hl)[host_counter].ttl                 = 0;
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Real hostname 2. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_REAL_HOSTNAME_LENGTH))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         (*hl)[host_counter].real_hostname[1][i] = *ptr;
         ptr++; i++;
      }
      if (i == MAX_REAL_HOSTNAME_LENGTH)
      {
         error_flag = YES;
         update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                       _("Maximum length for real hostname 2 for %s exceeded in HOST_CONFIG. Will be truncated to %d characters."),
                       (*hl)[host_counter].host_alias, MAX_REAL_HOSTNAME_LENGTH);
         while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
         {
            if (*ptr == '\\')
            {
               ptr++;
            }
            ptr++;
         }
         (*hl)[host_counter].real_hostname[1][i - 1] = '\0';
      }
      else
      {
         (*hl)[host_counter].real_hostname[1][i] = '\0';
      }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS. */
         (*hl)[host_counter].proxy_name[0]       = '\0';
         (*hl)[host_counter].host_toggle_str[0]  = '\0';
         (*hl)[host_counter].allowed_transfers   = DEFAULT_NO_PARALLEL_JOBS;
         (*hl)[host_counter].max_errors          = DEFAULT_MAX_ERRORS;
         (*hl)[host_counter].retry_interval      = DEFAULT_RETRY_INTERVAL;
         (*hl)[host_counter].transfer_blksize    = DEFAULT_TRANSFER_BLOCKSIZE;
         (*hl)[host_counter].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         (*hl)[host_counter].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         (*hl)[host_counter].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         (*hl)[host_counter].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         (*hl)[host_counter].host_status         = DEFAULT_FSA_HOST_STATUS;
         (*hl)[host_counter].protocol_options    = DEFAULT_PROTOCOL_OPTIONS;
         (*hl)[host_counter].transfer_rate_limit = 0;
         (*hl)[host_counter].ttl                 = 0;
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Host Toggle. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_TOGGLE_STR_LENGTH))
      {
         (*hl)[host_counter].host_toggle_str[i] = *ptr;
         ptr++; i++;
      }
      if (i == MAX_TOGGLE_STR_LENGTH)
      {
         error_flag = YES;
         update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                       _("Maximum length for the host toggle string %s exceeded in HOST_CONFIG. Will be truncated to %d characters."),
                       (*hl)[host_counter].host_alias, MAX_TOGGLE_STR_LENGTH);
         while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
         i--;
      }
      else if ((i > 0) && (i != 4))
           {
              error_flag = YES;
              (*hl)[host_counter].host_toggle_str[i] = '\0';
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Host toggle string <%s> not four characters long for host %s in HOST_CONFIG. Will be ignored."),
                            (*hl)[host_counter].host_toggle_str,
                            (*hl)[host_counter].host_alias);
              i = 0;
           }
      (*hl)[host_counter].host_toggle_str[i] = '\0';
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS. */
         (*hl)[host_counter].proxy_name[0]       = '\0';
         (*hl)[host_counter].allowed_transfers   = DEFAULT_NO_PARALLEL_JOBS;
         (*hl)[host_counter].max_errors          = DEFAULT_MAX_ERRORS;
         (*hl)[host_counter].retry_interval      = DEFAULT_RETRY_INTERVAL;
         (*hl)[host_counter].transfer_blksize    = DEFAULT_TRANSFER_BLOCKSIZE;
         (*hl)[host_counter].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         (*hl)[host_counter].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         (*hl)[host_counter].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         (*hl)[host_counter].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         (*hl)[host_counter].host_status         = DEFAULT_FSA_HOST_STATUS;
         (*hl)[host_counter].protocol_options    = DEFAULT_PROTOCOL_OPTIONS;
         (*hl)[host_counter].transfer_rate_limit = 0;
         (*hl)[host_counter].ttl                 = 0;
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Proxy Name. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_PROXY_NAME_LENGTH))
      {
         (*hl)[host_counter].proxy_name[i] = *ptr;
         ptr++; i++;
      }
      if (i == MAX_PROXY_NAME_LENGTH)
      {
         error_flag = YES;
         update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                       _("Maximum length for proxy name for host %s exceeded in HOST_CONFIG. Will be truncated to %d characters."),
                       (*hl)[host_counter].host_alias, MAX_PROXY_NAME_LENGTH);
         while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
      }
      (*hl)[host_counter].proxy_name[i] = '\0';
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS. */
         (*hl)[host_counter].allowed_transfers   = DEFAULT_NO_PARALLEL_JOBS;
         (*hl)[host_counter].max_errors          = DEFAULT_MAX_ERRORS;
         (*hl)[host_counter].retry_interval      = DEFAULT_RETRY_INTERVAL;
         (*hl)[host_counter].transfer_blksize    = DEFAULT_TRANSFER_BLOCKSIZE;
         (*hl)[host_counter].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         (*hl)[host_counter].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         (*hl)[host_counter].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         (*hl)[host_counter].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         (*hl)[host_counter].host_status         = DEFAULT_FSA_HOST_STATUS;
         (*hl)[host_counter].protocol_options    = DEFAULT_PROTOCOL_OPTIONS;
         (*hl)[host_counter].transfer_rate_limit = 0;
         (*hl)[host_counter].ttl                 = 0;
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Allowed Transfers. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in allowed transfers field for host %s, using default %d."),
                          (int)*ptr, (*hl)[host_counter].host_alias,
                          DEFAULT_NO_PARALLEL_JOBS);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].allowed_transfers = DEFAULT_NO_PARALLEL_JOBS;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value allowed transfers to large (>%d characters) for host %s to store as integer. Setting to default %d."),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias,
                            DEFAULT_NO_PARALLEL_JOBS);
              (*hl)[host_counter].allowed_transfers = DEFAULT_NO_PARALLEL_JOBS;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              if (((*hl)[host_counter].allowed_transfers = atoi(number)) > MAX_NO_PARALLEL_JOBS)
              {
                 error_flag = YES;
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                               _("Maximum number of parallel (%d) transfers exceeded for %s. Value found in HOST_CONFIG %d. Setting to maximum %d."),
                               MAX_NO_PARALLEL_JOBS,
                               (*hl)[host_counter].host_alias,
                               (*hl)[host_counter].allowed_transfers,
                               MAX_NO_PARALLEL_JOBS);
                 (*hl)[host_counter].allowed_transfers = MAX_NO_PARALLEL_JOBS;
              }
           }

      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS. */
         (*hl)[host_counter].max_errors          = DEFAULT_MAX_ERRORS;
         (*hl)[host_counter].retry_interval      = DEFAULT_RETRY_INTERVAL;
         (*hl)[host_counter].transfer_blksize    = DEFAULT_TRANSFER_BLOCKSIZE;
         (*hl)[host_counter].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         (*hl)[host_counter].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         (*hl)[host_counter].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         (*hl)[host_counter].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         (*hl)[host_counter].host_status         = DEFAULT_FSA_HOST_STATUS;
         (*hl)[host_counter].protocol_options    = DEFAULT_PROTOCOL_OPTIONS;
         (*hl)[host_counter].transfer_rate_limit = 0;
         (*hl)[host_counter].ttl                 = 0;
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Max Errors. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in max error field for host %s, using default %d."),
                          (int)*ptr, (*hl)[host_counter].host_alias,
                          DEFAULT_MAX_ERRORS);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].max_errors = DEFAULT_MAX_ERRORS;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for max errors to large (>%d characters) for host %s to store as integer. Setting default %d."),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias,
                            DEFAULT_MAX_ERRORS);
              (*hl)[host_counter].max_errors = DEFAULT_MAX_ERRORS;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*hl)[host_counter].max_errors = atoi(number);
           }
           
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS. */
         (*hl)[host_counter].retry_interval      = DEFAULT_RETRY_INTERVAL;
         (*hl)[host_counter].transfer_blksize    = DEFAULT_TRANSFER_BLOCKSIZE;
         (*hl)[host_counter].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         (*hl)[host_counter].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         (*hl)[host_counter].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         (*hl)[host_counter].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         (*hl)[host_counter].host_status         = DEFAULT_FSA_HOST_STATUS;
         (*hl)[host_counter].protocol_options    = DEFAULT_PROTOCOL_OPTIONS;
         (*hl)[host_counter].transfer_rate_limit = 0;
         (*hl)[host_counter].ttl                 = 0;
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Retry Interval. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in retry interval field for host %s, using default %d."),
                          (int)*ptr, (*hl)[host_counter].host_alias,
                          DEFAULT_RETRY_INTERVAL);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].retry_interval = DEFAULT_RETRY_INTERVAL;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for retry interval to large (>%d characters) for host %s to store as integer. Setting default %d."),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias,
                            DEFAULT_RETRY_INTERVAL);
              (*hl)[host_counter].retry_interval = DEFAULT_RETRY_INTERVAL;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*hl)[host_counter].retry_interval = atoi(number);
           }

      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS. */
         (*hl)[host_counter].transfer_blksize    = DEFAULT_TRANSFER_BLOCKSIZE;
         (*hl)[host_counter].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         (*hl)[host_counter].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         (*hl)[host_counter].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         (*hl)[host_counter].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         (*hl)[host_counter].host_status         = DEFAULT_FSA_HOST_STATUS;
         (*hl)[host_counter].protocol_options    = DEFAULT_PROTOCOL_OPTIONS;
         (*hl)[host_counter].transfer_rate_limit = 0;
         (*hl)[host_counter].ttl                 = 0;
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Transfer Block size. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in transfer block size field for host %s, using default %d."),
                          (int)*ptr, (*hl)[host_counter].host_alias,
                          DEFAULT_TRANSFER_BLOCKSIZE);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].transfer_blksize = DEFAULT_TRANSFER_BLOCKSIZE;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for transfer block size to large (>%d characters) for host %s to store as integer. Setting default %d."),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias,
                            DEFAULT_TRANSFER_BLOCKSIZE);
              (*hl)[host_counter].transfer_blksize = DEFAULT_TRANSFER_BLOCKSIZE;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              if (((*hl)[host_counter].transfer_blksize = atoi(number)) > MAX_TRANSFER_BLOCKSIZE)
              {
                 error_flag = YES;
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                               _("Transfer block size for host %s to large (%d bytes) setting it to %d bytes."),
                               (*hl)[host_counter].host_alias,
                               (*hl)[host_counter].transfer_blksize, 
                               MAX_TRANSFER_BLOCKSIZE);
                 (*hl)[host_counter].transfer_blksize = MAX_TRANSFER_BLOCKSIZE;
              }
              else if ((*hl)[host_counter].transfer_blksize < MIN_TRANSFER_BLOCKSIZE)
                   {
                      error_flag = YES;
                      update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                                    _("Transfer block size for host %s to small (%d bytes) setting it to %d bytes."),
                                    (*hl)[host_counter].host_alias,
                                    (*hl)[host_counter].transfer_blksize, 
                                    MIN_TRANSFER_BLOCKSIZE);
                      (*hl)[host_counter].transfer_blksize = MIN_TRANSFER_BLOCKSIZE;
                   }
           }

      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS. */
         (*hl)[host_counter].successful_retries  = DEFAULT_SUCCESSFUL_RETRIES;
         (*hl)[host_counter].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         (*hl)[host_counter].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         (*hl)[host_counter].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         (*hl)[host_counter].host_status         = DEFAULT_FSA_HOST_STATUS;
         (*hl)[host_counter].protocol_options    = DEFAULT_PROTOCOL_OPTIONS;
         (*hl)[host_counter].transfer_rate_limit = 0;
         (*hl)[host_counter].ttl                 = 0;
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Successful Retries. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in successful retries field for host %s, using default %d."),
                          (int)*ptr, (*hl)[host_counter].host_alias,
                          DEFAULT_SUCCESSFUL_RETRIES);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].successful_retries = DEFAULT_SUCCESSFUL_RETRIES;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for successful retries to large (>%d characters) for host %s to store as integer. Setting default %d."),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias,
                            DEFAULT_SUCCESSFUL_RETRIES);
              (*hl)[host_counter].successful_retries = DEFAULT_SUCCESSFUL_RETRIES;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*hl)[host_counter].successful_retries = atoi(number);
           }

      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS. */
         (*hl)[host_counter].file_size_offset    = (char)DEFAULT_FILE_SIZE_OFFSET;
         (*hl)[host_counter].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         (*hl)[host_counter].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         (*hl)[host_counter].host_status         = DEFAULT_FSA_HOST_STATUS;
         (*hl)[host_counter].protocol_options    = DEFAULT_PROTOCOL_OPTIONS;
         (*hl)[host_counter].transfer_rate_limit = 0;
         (*hl)[host_counter].ttl                 = 0;
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store File Size Offset. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if ((isdigit((int)(*ptr))) || (*ptr == '-'))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in file size offset field for host %s, using default %d."),
                          (int)*ptr, (*hl)[host_counter].host_alias,
                          DEFAULT_FILE_SIZE_OFFSET);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].file_size_offset = (char)DEFAULT_FILE_SIZE_OFFSET;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for file size offset to large (>%d characters) for host %s to store as integer. Setting default %d."),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias,
                            DEFAULT_FILE_SIZE_OFFSET);
              (*hl)[host_counter].file_size_offset = (char)DEFAULT_FILE_SIZE_OFFSET;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*hl)[host_counter].file_size_offset = (char)atoi(number);
           }

      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise all other values with DEFAULTS. */
         (*hl)[host_counter].transfer_timeout    = DEFAULT_TRANSFER_TIMEOUT;
         (*hl)[host_counter].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         (*hl)[host_counter].host_status         = DEFAULT_FSA_HOST_STATUS;
         (*hl)[host_counter].protocol_options    = DEFAULT_PROTOCOL_OPTIONS;
         (*hl)[host_counter].transfer_rate_limit = 0;
         (*hl)[host_counter].ttl                 = 0;
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;
         error_flag = YES;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Transfer Timeout. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in transfer timeout field for host %s, using default %d."),
                          (int)*ptr, (*hl)[host_counter].host_alias,
                          DEFAULT_TRANSFER_TIMEOUT);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].transfer_timeout = DEFAULT_TRANSFER_TIMEOUT;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for transfer timeout to large (>%d characters) for host %s to store as integer. Setting default %d."),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias,
                            DEFAULT_TRANSFER_TIMEOUT);
              (*hl)[host_counter].transfer_timeout = DEFAULT_TRANSFER_TIMEOUT;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*hl)[host_counter].transfer_timeout = atoi(number);
           }

      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise rest with DEFAULTS. */
         (*hl)[host_counter].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
         (*hl)[host_counter].host_status         = DEFAULT_FSA_HOST_STATUS;
         (*hl)[host_counter].protocol_options    = DEFAULT_PROTOCOL_OPTIONS;
         (*hl)[host_counter].transfer_rate_limit = 0;
         (*hl)[host_counter].ttl                 = 0;
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Number of no Bursts. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in number of no bursts field for host %s, using default %d."),
                          (int)*ptr, (*hl)[host_counter].host_alias,
                          DEFAULT_NO_OF_NO_BURSTS);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for number of no bursts to large (>%d characters) for host %s to store as integer. Setting default %d."),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias,
                            DEFAULT_NO_OF_NO_BURSTS);
              (*hl)[host_counter].number_of_no_bursts = (unsigned char)DEFAULT_NO_OF_NO_BURSTS;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              if (((*hl)[host_counter].number_of_no_bursts = (unsigned char)atoi(number)) > (*hl)[host_counter].allowed_transfers)
              {
                 error_flag = YES;
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                               _("Number of no bursts for host %s is larger (%d) then allowed transfers. Setting it to %d."),
                               (*hl)[host_counter].host_alias,
                               (*hl)[host_counter].number_of_no_bursts,
                               (*hl)[host_counter].allowed_transfers);
                 (*hl)[host_counter].number_of_no_bursts = (*hl)[host_counter].allowed_transfers;
              }
           }

      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /*
          * This is an indication that this HOST_CONFIG is an older
          * version. The code that follows is for compatibility so
          * that hosts that are disabled or have transfer/queue stopped
          * suddenly have them enabled.
          */
         (*hl)[host_counter].host_status = DEFAULT_FSA_HOST_STATUS;
         if (first_time == NO)
         {
            if (fsa == NULL)
            {
               if (fsa_attach("eval_host_config") == SUCCESS)
               {
                  for (i = 0; i < no_of_hosts; i++)
                  {
                     if (CHECK_STRCMP(fsa[i].host_alias,
                                      (*hl)[host_counter].host_alias) == 0)
                     {
                        if (fsa[i].special_flag & HOST_DISABLED)
                        {
                           (*hl)[host_counter].host_status |= HOST_CONFIG_HOST_DISABLED;
                        }
                        if ((fsa[i].special_flag & HOST_IN_DIR_CONFIG) == 0)
                        {
                           (*hl)[host_counter].host_status |= HOST_NOT_IN_DIR_CONFIG;
                        }
                        if (fsa[i].host_status & STOP_TRANSFER_STAT)
                        {
                           (*hl)[host_counter].host_status |= STOP_TRANSFER_STAT;
                        }
                        if (fsa[i].host_status & PAUSE_QUEUE_STAT)
                        {
                           (*hl)[host_counter].host_status |= PAUSE_QUEUE_STAT;
                        }
                        break;
                     }
                  }
                  (void)fsa_detach(NO);
               }
            }
            else
            {
               for (i = 0; i < no_of_hosts; i++)
               {
                  if (CHECK_STRCMP(fsa[i].host_alias,
                                   (*hl)[host_counter].host_alias) == 0)
                  {
                     if (fsa[i].special_flag & HOST_DISABLED)
                     {
                        (*hl)[host_counter].host_status |= HOST_CONFIG_HOST_DISABLED;
                     }
                     if ((fsa[i].special_flag & HOST_IN_DIR_CONFIG) == 0)
                     {
                        (*hl)[host_counter].host_status |= HOST_NOT_IN_DIR_CONFIG;
                     }
                     if (fsa[i].host_status & STOP_TRANSFER_STAT)
                     {
                        (*hl)[host_counter].host_status |= STOP_TRANSFER_STAT;
                     }
                     if (fsa[i].host_status & PAUSE_QUEUE_STAT)
                     {
                        (*hl)[host_counter].host_status |= PAUSE_QUEUE_STAT;
                     }
                     break;
                  }
               }
            }
         }
         (*hl)[host_counter].protocol_options    = DEFAULT_PROTOCOL_OPTIONS;
         (*hl)[host_counter].transfer_rate_limit = 0;
         (*hl)[host_counter].ttl                 = 0;
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store the host status. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in host status field for host %s, using default %d."),
                          (int)*ptr, (*hl)[host_counter].host_alias,
                          DEFAULT_FSA_HOST_STATUS);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].host_status = DEFAULT_FSA_HOST_STATUS;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for host status to large (>%d characters) for host %s to store as integer. Setting to %d."),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias,
                            DEFAULT_FSA_HOST_STATUS);
              (*hl)[host_counter].host_status = DEFAULT_FSA_HOST_STATUS;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              if (((*hl)[host_counter].host_status = (unsigned int)atoi(number)) > (PAUSE_QUEUE_STAT|STOP_TRANSFER_STAT|HOST_ERROR_OFFLINE_STATIC|HOST_CONFIG_HOST_DISABLED|HOST_NOT_IN_DIR_CONFIG|HOST_TWO_FLAG|DO_NOT_DELETE_DATA|SIMULATE_SEND_MODE))
              {
                 error_flag = YES;
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                               _("Unknown host status <%d> for host %s, largest value is %d. Setting to %d."),
                               (*hl)[host_counter].host_status,
                               (*hl)[host_counter].host_alias,
                               (PAUSE_QUEUE_STAT|STOP_TRANSFER_STAT|HOST_ERROR_OFFLINE_STATIC|HOST_CONFIG_HOST_DISABLED|HOST_NOT_IN_DIR_CONFIG|HOST_TWO_FLAG|DO_NOT_DELETE_DATA|SIMULATE_SEND_MODE),
                               DEFAULT_FSA_HOST_STATUS);
                 (*hl)[host_counter].host_status = DEFAULT_FSA_HOST_STATUS;
              }
           }

      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /*
          * This is an indication that this HOST_CONFIG is an older
          * version. The code that follows is for compatibility so
          * host_status and protocol_options are set correctly.
          */
         if (error_flag != YES)
         {
            (*hl)[host_counter].protocol_options = (unsigned int)atoi(number);
            if (((*hl)[host_counter].protocol_options != 0) &&
                (((*hl)[host_counter].protocol_options > (SET_IDLE_TIME |
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
                                                          STAT_KEEPALIVE |
#endif
                                                          FTP_FAST_MOVE |
                                                          FTP_FAST_CD |
                                                          FTP_IGNORE_BIN |
                                                          FTP_PASSIVE_MODE)) ||
                 ((*hl)[host_counter].protocol_options < FTP_PASSIVE_MODE)))
            {
               (*hl)[host_counter].protocol_options = DEFAULT_PROTOCOL_OPTIONS;
            }
            else
            {
               (*hl)[host_counter].host_status = (unsigned int)(*hl)[host_counter].transfer_rate_limit;
               (*hl)[host_counter].transfer_rate_limit = 0;
            }
         }
         else
         {
            (*hl)[host_counter].protocol_options = DEFAULT_PROTOCOL_OPTIONS;
         }

         /* Initialise rest with DEFAULTS. */
         (*hl)[host_counter].transfer_rate_limit = 0;
         (*hl)[host_counter].ttl                 = 0;
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store the protocol options. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in protocol options field for host %s, using default %d."),
                          (int)*ptr, (*hl)[host_counter].host_alias,
                          DEFAULT_PROTOCOL_OPTIONS);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].protocol_options = DEFAULT_PROTOCOL_OPTIONS;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for protocol options to large (>%d characters) for host %s to store as integer, using default %d"),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias,
                            DEFAULT_PROTOCOL_OPTIONS);
              (*hl)[host_counter].protocol_options = DEFAULT_PROTOCOL_OPTIONS;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*hl)[host_counter].protocol_options = (unsigned int)atoi(number);
           }

      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise rest with DEFAULTS. */
         (*hl)[host_counter].transfer_rate_limit = 0;
         (*hl)[host_counter].ttl                 = 0;
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;

         /*
          * As of 1.3.0 SET_IDLE_TIME, STAT_KEEPALIVE and FTP_PASSIVE_MODE
          * have different values. So we must check here if this is the
          * case and adapt to the new values.
          */
         if (((*hl)[host_counter].protocol_options != 0) &&
             (((*hl)[host_counter].protocol_options <= (OLD_SET_IDLE_TIME |
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
                                                        OLD_STAT_KEEPALIVE |
#endif
                                                        OLD_FTP_PASSIVE_MODE)) &&
              ((*hl)[host_counter].protocol_options >= OLD_FTP_PASSIVE_MODE)))
         {
            int old_protocol_options = (*hl)[host_counter].protocol_options;

            (*hl)[host_counter].protocol_options = DEFAULT_PROTOCOL_OPTIONS;
            if (old_protocol_options & OLD_SET_IDLE_TIME)
            {
               (*hl)[host_counter].protocol_options |= SET_IDLE_TIME;
            }
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
            if (old_protocol_options & OLD_STAT_KEEPALIVE)
            {
               (*hl)[host_counter].protocol_options |= STAT_KEEPALIVE;
            }
#endif
            if (old_protocol_options & OLD_FTP_PASSIVE_MODE)
            {
               (*hl)[host_counter].protocol_options |= FTP_PASSIVE_MODE;
            }
         }

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /*
       * As of 1.3.0 SET_IDLE_TIME, STAT_KEEPALIVE and FTP_PASSIVE_MODE
       * have different values. So we must do this check here.
       */
      if (((*hl)[host_counter].protocol_options != 0) &&
          (((*hl)[host_counter].protocol_options > (TLS_LEGACY_RENEGOTIATION |
                                                    HTTP_BUCKETNAME_IN_PATH |
                                                    NO_EXPECT |
#ifdef _WITH_EXTRA_CHECK
                                                    USE_EXTRA_CHECK |
#endif
                                                    IMPLICIT_FTPS |
                                                    USE_STAT_LIST |
                                                    DISABLE_STRICT_HOST_KEY|
                                                    KEEP_CONNECTED_DISCONNECT |
                                                    FTP_DISABLE_MLST |
                                                    TLS_STRICT_VERIFY |
                                                    FTP_USE_LIST |
                                                    FTP_CCC_OPTION |
                                                    KEEP_CON_NO_SEND_2 |
                                                    KEEP_CON_NO_FETCH_2 |
                                                    TIMEOUT_TRANSFER |
                                                    CHECK_SIZE |
                                                    NO_AGEING_JOBS |
                                                    SORT_FILE_NAMES |
                                                    KEEP_TIME_STAMP |
                                                    ENABLE_COMPRESSION |
                                                    USE_SEQUENCE_LOCKING |
                                                    FILE_WHEN_LOCAL_FLAG |
                                                    FTP_ALLOW_DATA_REDIRECT |
#ifdef _WITH_BURST_2
                                                    DISABLE_BURSTING |
#endif
                                                    FTP_EXTENDED_MODE |
                                                    SET_IDLE_TIME |
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
                                                    STAT_KEEPALIVE |
                                                    AFD_TCP_KEEPALIVE |
#endif
                                                    FTP_FAST_MOVE |
                                                    FTP_FAST_CD |
                                                    FTP_IGNORE_BIN |
                                                    FTP_PASSIVE_MODE)) ||
           ((*hl)[host_counter].protocol_options < FTP_PASSIVE_MODE)))
      {
         error_flag = YES;
         update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                       _("Unknown protocol option <%d> for host %s, largest value is %d and smallest %d. Setting to %d."),
                       (*hl)[host_counter].protocol_options,
                       (*hl)[host_counter].host_alias,
                       (TLS_LEGACY_RENEGOTIATION |
                        HTTP_BUCKETNAME_IN_PATH |
                        NO_EXPECT |
#ifdef _WITH_EXTRA_CHECK
                        USE_EXTRA_CHECK |
#endif
                        IMPLICIT_FTPS |
                        USE_STAT_LIST |
                        DISABLE_STRICT_HOST_KEY |
                        KEEP_CONNECTED_DISCONNECT |
                        FTP_DISABLE_MLST |
                        TLS_STRICT_VERIFY |
                        FTP_USE_LIST |
                        FTP_CCC_OPTION |
                        KEEP_CON_NO_SEND_2 |
                        KEEP_CON_NO_FETCH_2 |
                        TIMEOUT_TRANSFER |
                        CHECK_SIZE |
                        NO_AGEING_JOBS |
                        SORT_FILE_NAMES |
                        KEEP_TIME_STAMP |
                        ENABLE_COMPRESSION |
                        USE_SEQUENCE_LOCKING |
                        FILE_WHEN_LOCAL_FLAG |
                        FTP_ALLOW_DATA_REDIRECT |
#ifdef _WITH_BURST_2
                        DISABLE_BURSTING |
#endif
                        FTP_EXTENDED_MODE |
                        SET_IDLE_TIME |
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
                        STAT_KEEPALIVE |
                        AFD_TCP_KEEPALIVE |
#endif
                        FTP_FAST_MOVE |
                        FTP_FAST_CD |
                        FTP_IGNORE_BIN |
                        FTP_PASSIVE_MODE),
                       FTP_PASSIVE_MODE, DEFAULT_PROTOCOL_OPTIONS);
         (*hl)[host_counter].protocol_options = DEFAULT_PROTOCOL_OPTIONS;
      }

      /* Store transfer rate limit. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in transfer rate limit field for host %s, using default 0."),
                          (int)*ptr, (*hl)[host_counter].host_alias);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].transfer_rate_limit = 0;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for transfer rate limit to large (>%d characters) for host %s to store as integer, using default 0"),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias);
              (*hl)[host_counter].transfer_rate_limit = 0;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*hl)[host_counter].transfer_rate_limit = atoi(number);
           }

      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise rest with DEFAULTS. */
         (*hl)[host_counter].ttl                 = 0;
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store TTL (time-to-live). */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in TTL field for host %s, using default 0."),
                          (int)*ptr, (*hl)[host_counter].host_alias);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].ttl = 0;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for TTL to large (>%d characters) for host %s to store as integer, using default 0."),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias);
              (*hl)[host_counter].ttl = 0;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*hl)[host_counter].ttl = atoi(number);
           }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise rest with DEFAULTS. */
         (*hl)[host_counter].socksnd_bufsize     = 0;
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Socket Send Buffer. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in SSB field for host %s, using default 0."),
                          (int)*ptr, (*hl)[host_counter].host_alias);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].socksnd_bufsize = 0;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for SSB to large (>%d characters) for host %s to store as integer, using default 0."),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias);
              (*hl)[host_counter].socksnd_bufsize = 0;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*hl)[host_counter].socksnd_bufsize = (unsigned int)strtoul(number, NULL, 10);
           }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise rest with DEFAULTS. */
         (*hl)[host_counter].sockrcv_bufsize     = 0;
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Socket Receive Buffer. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in SRB field for host %s, using default 0."),
                          (int)*ptr, (*hl)[host_counter].host_alias);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].sockrcv_bufsize = 0;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for SRB to large (>%d characters) for host %s to store as integer, using default 0."),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias);
              (*hl)[host_counter].sockrcv_bufsize = 0;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*hl)[host_counter].sockrcv_bufsize = (unsigned int)strtoul(number, NULL, 10);
           }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise rest with DEFAULTS. */
#ifdef WITH_DUP_CHECK
         (*hl)[host_counter].dup_check_timeout   = 0L;
         (*hl)[host_counter].dup_check_flag      = 0;
#endif
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

#ifdef WITH_DUP_CHECK
      /* Store Dupcheck Timeout. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_LONG_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in DT field for host %s, using default 0."),
                          (int)*ptr, (*hl)[host_counter].host_alias);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].dup_check_timeout = 0L;
      }
      else if (i == MAX_LONG_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for DT to large (>%d characters) for host %s to store as long integer, using default 0."),
                            MAX_LONG_LENGTH, (*hl)[host_counter].host_alias);
              (*hl)[host_counter].dup_check_timeout = 0L;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*hl)[host_counter].dup_check_timeout = (time_t)str2timet(number, NULL, 10);
           }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise rest with DEFAULTS. */
         (*hl)[host_counter].dup_check_flag      = 0;
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store Dupcheck Flag. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in DF field for host %s, using default 0."),
                          (int)*ptr, (*hl)[host_counter].host_alias);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].dup_check_flag = 0;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for DF to large (>%d characters) for host %s to store as integer, using default 0."),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias);
              (*hl)[host_counter].dup_check_flag = 0;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*hl)[host_counter].dup_check_flag = (unsigned int)strtoul(number, NULL, 10);
           }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise rest with DEFAULTS. */
         (*hl)[host_counter].keep_connected      = 0;
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }
#endif /* WITH_DUP_CHECK */

      /* Keep Connected. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in KC field for host %s, using default 0."),
                          (int)*ptr, (*hl)[host_counter].host_alias);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].keep_connected = 0;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for KC to large (>%d characters) for host %s to store as integer, using default 0."),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias);
              (*hl)[host_counter].keep_connected = 0;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*hl)[host_counter].keep_connected = (unsigned int)strtoul(number, NULL, 10);
           }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise rest with DEFAULTS. */
         (*hl)[host_counter].warn_time           = 0L;
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Warn time. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_LONG_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in WT field for host %s, using default 0."),
                          (int)*ptr, (*hl)[host_counter].host_alias);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].warn_time = 0L;
      }
      else if (i == MAX_LONG_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for WT to large (>%d characters) for host %s to store as integer, using default 0."),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias);
              (*hl)[host_counter].warn_time = 0L;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*hl)[host_counter].warn_time = (time_t)str2timet(number, NULL, 10);
           }
      if ((*ptr == '\n') || (*ptr == '\0'))
      {
         /* Initialise rest with DEFAULTS. */
         (*hl)[host_counter].protocol_options2   = DEFAULT_PROTOCOL_OPTIONS2;

         if (*ptr == '\n')
         {
            while (*ptr == '\n')
            {
               ptr++;
            }
            (host_counter)++;
            continue;
         }
         else
         {
            break;
         }
      }

      /* Store the protocol options 2. */
      i = 0; ptr++;
      while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0') &&
             (i < MAX_INT_LENGTH))
      {
         if (isdigit((int)(*ptr)))
         {
            number[i] = *ptr;
            ptr++; i++;
         }
         else
         {
            error_flag = YES;
            update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                          _("Non numeric character <%d> in protocol options 2 field for host %s, using default %d."),
                          (int)*ptr, (*hl)[host_counter].host_alias,
                          DEFAULT_PROTOCOL_OPTIONS2);

            /* Ignore this entry. */
            i = 0;
            while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
         }
      }
      number[i] = '\0';
      if (i == 0)
      {
         error_flag = YES;
         (*hl)[host_counter].protocol_options2 = DEFAULT_PROTOCOL_OPTIONS2;
      }
      else if (i == MAX_INT_LENGTH)
           {
              error_flag = YES;
              update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                            _("Numeric value for protocol options 2 to large (>%d characters) for host %s to store as integer, using default %d"),
                            MAX_INT_LENGTH, (*hl)[host_counter].host_alias,
                            DEFAULT_PROTOCOL_OPTIONS2);
              (*hl)[host_counter].protocol_options2 = DEFAULT_PROTOCOL_OPTIONS2;
              while ((*ptr != ':') && (*ptr != '\n') && (*ptr != '\0'))
              {
                 ptr++;
              }
           }
           else
           {
              (*hl)[host_counter].protocol_options2 = (unsigned int)atoi(number);
           }
      if ((*hl)[host_counter].protocol_options2 > FTP_SEND_UTF8_ON)
      {
         error_flag = YES;
         update_db_log(WARN_SIGN, __FILE__, __LINE__, debug_fp, warn_counter,
                       _("Unknown protocol option 2 <%d> for host %s, largest value is %d and smallest %d. Setting to %d."),
                       (*hl)[host_counter].protocol_options2,
                       (*hl)[host_counter].host_alias, FTP_SEND_UTF8_ON,
                       DEFAULT_PROTOCOL_OPTIONS2);
         (*hl)[host_counter].protocol_options2 = DEFAULT_PROTOCOL_OPTIONS2;
      }

      /* Ignore the rest of the line. We have everything we need. */
      while ((*ptr != '\n') && (*ptr != '\0'))
      {
         ptr++;
      }
      while (*ptr == '\n')
      {
         ptr++;
      }
      host_counter++;
   } while (*ptr != '\0');

   free(hostbase);
   *hosts_found = host_counter;

   return(error_flag);
}
