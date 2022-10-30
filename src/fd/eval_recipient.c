/*
 *  eval_recipient.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2022 Deutscher Wetterdienst (DWD),
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
 **   eval_recipient - reads the recipient string
 **
 ** SYNOPSIS
 **   int eval_recipient(char       *recipient,
 **                      struct job *p_db,
 **                      char       *full_msg_path,
 **                      time_t     next_check_time)
 **
 ** DESCRIPTION
 **   Here we evaluate the recipient string which must have the
 **   URL (Uniform Resource Locators) format:
 **
 **   <sheme>://[<user>][;fingerprint=][:<password>]@<host>[:<port>][/<url-path>][;type=i|a|d]
 **                                                                              [;server=<server-name>]
 **                                                                              [;protocol=<protocol number>]
 **
 ** RETURN VALUES
 **   struct job *p_db   - The structure in which we store the
 **                        following values:
 **
 **                             - user
 **                             - password
 **                             - hostname
 **                             - port
 **                             - directory
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.11.1995 H.Kiehl   Created
 **   15.12.1996 H.Kiehl   Changed to URL format.
 **   26.03.1998 H.Kiehl   Remove host switching information, this is now
 **                        completely handled by the AMG.
 **   12.04.1998 H.Kiehl   Added DOS binary mode.
 **   03.06.1998 H.Kiehl   Allow user to add special characters (@, :, etc)
 **                        in password.
 **   15.12.2001 H.Kiehl   When server= is set take this as hostname.
 **   19.02.2002 H.Kiehl   Added the ability to read recipient group file.
 **   21.03.2003 H.Kiehl   Support to only map to the required part of FSA.
 **   09.04.2004 H.Kiehl   Support for TLS/SSL.
 **   28.01.2005 H.Kiehl   Don't keep the error time indefinite long, for
 **                        retrieving remote files.
 **   21.11.2005 H.Kiehl   Added time modifier when evaluating the directory.
 **   12.02.2006 H.Kiehl   Added transfer_mode N for none for FTP.
 **   28.03.2006 H.Kiehl   Added %h directory modifier for inserting local
 **                        hostname.
 **   15.04.2008 H.Kiehl   Accept url's without @ sign such as http://idefix.
 **                        For FTP assume assume anonymous login if no user
 **                        is given.
 **   10.03.2012 H.Kiehl   We must pass struct job *p_db also to
 **                        get_group_list(), otherwise we loose those values
 **                        during a burst.
 **   24.04.2012 S.Knudsen Scheme ftpS was ignored.
 **   21.04.2013 H.Kiehl   Handle case when we rename host_alias in message.
 **   03.09.2020 H.Kiehl   For HTTP do not set a default user, otherwise
 **                        we always send Basic Authorization for nothing.
 **
 */
DESCR__E_M3

#include <stdio.h>                   /* NULL                             */
#include <string.h>                  /* strcpy(), strcat(), memmove()    */
#include <errno.h>
#include "fddefs.h"

/* Global variables. */
extern int                        no_of_hosts;
extern struct filetransfer_status *fsa;


/*########################## eval_recipient() ###########################*/
int
eval_recipient(char       *recipient,
               struct job *p_db,
               char       *full_msg_path,
               time_t     next_check_time)
{
   unsigned int error_mask,
                scheme;
   int          port;
   time_t       time_buf;
   char         server[MAX_REAL_HOSTNAME_LENGTH];

   if ((next_check_time > 0) && (fsa->error_counter > 0) &&
       (fsa->error_counter < fsa->max_errors))
   {
      time_buf = next_check_time;
   }
   else
   {
      time_buf = 0L;
   }

#ifdef WITH_DUP_CHECK
   if (p_db->dup_check_flag & USE_RECIPIENT_ID)
   {
      p_db->crc_id = get_str_checksum(recipient);
   }
   else
   {
      p_db->crc_id = fsa->host_id;
   }
#endif
   if ((error_mask = url_evaluate(recipient, &scheme, p_db->user,
                                  &p_db->smtp_auth, p_db->smtp_user,
#ifdef WITH_SSH_FINGERPRINT
                                  p_db->ssh_fingerprint, &p_db->key_type,
#endif
                                  p_db->password, NO, p_db->hostname, &port,
                                  p_db->target_dir, NULL, &time_buf,
                                  &p_db->transfer_mode, &p_db->ssh_protocol,
                                  &p_db->auth, p_db->region, &p_db->service,
                                  server)) < 4)
   {
      if (error_mask & TARGET_DIR_CAN_CHANGE)
      {
         p_db->special_flag |= PATH_MAY_CHANGE;
      }
#ifdef _WITH_DE_MAIL_SUPPORT
      if ((p_db->protocol & SMTP_FLAG) && (scheme & DE_MAIL_FLAG))
      {
         p_db->protocol &= ~SMTP_FLAG;    /* Unset flag. */
         p_db->protocol |= DE_MAIL_FLAG;  /* Set flag.   */
      }
#endif
      if (p_db->protocol & EXEC_FLAG)
      {
         p_db->exec_cmd = p_db->target_dir;
      }

      if (port != -1)
      {
         p_db->port = port;
      }
      if (server[0] != '\0')
      {
#ifdef _WITH_DE_MAIL_SUPPORT
         if ((scheme & SMTP_FLAG) || (scheme & DE_MAIL_FLAG))
#else
         if (scheme & SMTP_FLAG)
#endif
         {
            p_db->special_flag |= SMTP_SERVER_NAME_IN_MESSAGE;
            (void)strcpy(p_db->smtp_server, server);
         }
         if (scheme & HTTP_FLAG)
         {
            (void)strcpy(p_db->http_proxy, server);
         }
      }
#ifdef WITH_SSL
# ifdef WITH_PROPER_PROXY_SUPPORT
      if (scheme & SSL_FLAG)
      {
         if (scheme & FTP_FLAG)
         {
            if (recipient[3] == 'S')
            {
               p_db->tls_auth = BOTH;
            }
            else
            {
               p_db->tls_auth = YES;
            }
         }
         else if (scheme & HTTP_FLAG)
              {
                 if (p_db->http_proxy[0] == '\0')
                 {
                    p_db->tls_auth = YES;
                 }
              }
              else
              {
                 p_db->tls_auth = YES;
              }
      }
# else
      if (scheme & SSL_FLAG)
      {
         if (recipient[3] == 'S')
         {
            p_db->tls_auth = BOTH;
         }
         else
         {
            p_db->tls_auth = YES;
         }
      }
# endif
#endif

      if (p_db->user[0] == MAIL_GROUP_IDENTIFIER)
      {
         get_group_list(&p_db->user[1], p_db);
      }
      else if (p_db->user[0] == '\0')
           {
              if (((p_db->protocol & LOC_FLAG) == 0) &&
                  ((p_db->protocol & EXEC_FLAG) == 0) &&
                  ((p_db->protocol & HTTP_FLAG) == 0)) 
              {
                 p_db->user[0] = 'a'; p_db->user[1] = 'n';
                 p_db->user[2] = 'o'; p_db->user[3] = 'n';
                 p_db->user[4] = 'y'; p_db->user[5] = 'm';
                 p_db->user[6] = 'o'; p_db->user[7] = 'u';
                 p_db->user[8] = 's'; p_db->user[9] = '\0';
              }
              if (p_db->hostname[0] == MAIL_GROUP_IDENTIFIER)
              {
                 get_group_list(&p_db->hostname[1], p_db);
                 (void)memmove(p_db->hostname, &p_db->hostname[1],
                               strlen(&p_db->hostname[1]) + 1);
              }
              else
              {
                 if (p_db->protocol & FTP_FLAG)
                 {
                    /* Assume anonymous login. */
                    p_db->password[0] = 'a'; p_db->password[1] = 'f';
                    p_db->password[2] = 'd'; p_db->password[3] = '@';
                    p_db->password[4] = 'h'; p_db->password[5] = 'o';
                    p_db->password[6] = 's'; p_db->password[7] = 't';
                    p_db->password[8] = '\0';
                 }
              }
           }

      if (p_db->hostname[0] == MAIL_GROUP_IDENTIFIER)
      {
         (void)memmove(p_db->hostname, &p_db->hostname[1],
                       strlen(&p_db->hostname[1]) + 1);
      }

#ifndef WITH_PASSWD_IN_MSG
      if (p_db->password[0] == '\0')
      {
         if ((p_db->protocol & LOC_FLAG) ||
# ifdef _WITH_DE_MAIL_SUPPORT
             (((p_db->protocol & SMTP_FLAG) ||
               (p_db->protocol & DE_MAIL_FLAG)) &&
              (p_db->smtp_auth == SMTP_AUTH_NONE)) ||
# else
             ((p_db->protocol & SMTP_FLAG) &&
              (p_db->smtp_auth == SMTP_AUTH_NONE)) ||
# endif
# ifdef _WITH_WMO_SUPPORT
             (p_db->protocol & WMO_FLAG) ||
# endif
# ifdef _WITH_MAP_SUPPORT
             (p_db->protocol & MAP_FLAG) ||
# endif
# ifdef _WITH_DFAX_SUPPORT
             (p_db->protocol & DFAX_FLAG) ||
# endif
             (p_db->protocol & EXEC_FLAG))
         {
            /* No need to lookup a passwd. */;
         }
         else
         {
            char uh_name[MAX_USER_NAME_LENGTH + MAX_REAL_HOSTNAME_LENGTH + 1];

# ifdef _WITH_DE_MAIL_SUPPORT
            if (((p_db->protocol & SMTP_FLAG) ||
                 (p_db->protocol & DE_MAIL_FLAG)) &&
                (p_db->smtp_auth != SMTP_AUTH_NONE))
# else
            if ((p_db->protocol & SMTP_FLAG) &&
                (p_db->smtp_auth != SMTP_AUTH_NONE))
# endif
            {
               (void)strcpy(uh_name, p_db->smtp_user);
               if (server[0] == '\0')
               {
                  (void)strcat(uh_name, p_db->hostname);
               }
               else
               {
                  (void)strcat(uh_name, server);
               }
            }
            else
            {
               if (p_db->user[0] != '\0')
               {
                  (void)strcpy(uh_name, p_db->user);
                  (void)strcat(uh_name, p_db->hostname);
               }
               else
               {
                  (void)strcpy(uh_name, p_db->hostname);
               }
            }
            if (get_pw(uh_name, p_db->password, YES) == INCORRECT)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Unable to get password.");
               return(INCORRECT);
            }
         }
      }
#endif

      if (p_db->protocol & HTTP_FLAG)
      {
         if (p_db->target_dir[0] == '\0')
         {
            p_db->target_dir[0] = '/';
            p_db->target_dir[1] = '\0';
         }
         else
         {
            port = strlen(p_db->target_dir) - 1;
            if (p_db->index_file == NULL)
            {
               if (p_db->target_dir[port] != '/')
               {
                  p_db->target_dir[port + 1] = '/';
                  p_db->target_dir[port + 2] = '\0';
               }
            }
            else
            {
               int length = 0;

               if (p_db->target_dir[port] == '/')
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Directory option '%s' set, but no file name in path '%s'.",
                             URL_WITH_INDEX_FILE_NAME_ID, p_db->target_dir);
                  return(INCORRECT);
               }
               while (((port - length) > 0) &&
                      (p_db->target_dir[port - length] != '/'))
               {
                  length++;
               }
               if ((length > 0) &&
                   (p_db->target_dir[port - length] == '/'))
               {
                  length--;
                  (void)memcpy(p_db->index_file,
                               &p_db->target_dir[port - length], length + 1);
                  p_db->index_file[length + 1] = '\0';
                  p_db->target_dir[port - length] = '\0';
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Directory option '%s' set, but no file name in path '%s'.",
                             URL_WITH_INDEX_FILE_NAME_ID, p_db->target_dir);
                  return(INCORRECT);
               }
            }
         }
      }

      if ((p_db->protocol & LOC_FLAG) && (p_db->target_dir[0] != '/'))
      {
         if (expand_path(p_db->user, p_db->target_dir) == INCORRECT)
         {
            return(INCORRECT);
         }
      }

      /*
       * Find position of this hostname in FSA.
       */
      if ((p_db->smtp_server[0] == '\0') ||
          ((p_db->special_flag & SMTP_SERVER_NAME_IN_AFD_CONFIG) &&
           ((p_db->special_flag & SMTP_SERVER_NAME_IN_MESSAGE) == 0)))
      {
         t_hostname(p_db->hostname, p_db->host_alias);
      }
      else
      {
         t_hostname(p_db->smtp_server, p_db->host_alias);
      }
      if (CHECK_STRCMP(p_db->host_alias, fsa->host_alias) != 0)
      {
         /*
          * Uups. What do we do now? Host not in the FSA!
          * Hmm. Maybe somebody has a better idea?
          */
         if (((port = gsf_check_fsa(p_db)) == NO) || (port == NEITHER) ||
             ((port == YES) && (p_db->fsa_pos == INCORRECT)) ||
             ((port == YES) && (p_db->fsa_pos != INCORRECT) &&
              (CHECK_STRCMP(p_db->host_alias, fsa->host_alias))))
         {
            /*
             * Maybe the host alias has been renamed in message. So
             * lets see if this is the case.
             */
            if ((port == NO) && (p_db->fsa_pos != INCORRECT) &&
                (CHECK_STRCMP(p_db->host_alias, fsa->host_alias)))
            {
               fsa_detach_pos(p_db->fsa_pos);
               if (fsa_attach("sf/gf_xxx") == SUCCESS)
               {
                  p_db->fsa_pos = get_host_position(fsa, p_db->host_alias,
                                                    no_of_hosts);
                  (void)fsa_detach(NO);
                  if (p_db->fsa_pos != INCORRECT)
                  {
                     if ((port = fsa_attach_pos(p_db->fsa_pos)) != SUCCESS)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to attach to FSA position %d (%d).",
                                   p_db->fsa_pos, port);
                        p_db->fsa_pos = INCORRECT;
                        port = INCORRECT;
                     }
                     else
                     {
                        p_db->lock_offset = AFD_WORD_OFFSET +
                                            (p_db->fsa_pos * sizeof(struct filetransfer_status));
#ifdef WITH_DUP_CHECK
                        if ((p_db->dup_check_flag & USE_RECIPIENT_ID) == 0)
                        {
                           p_db->crc_id = fsa->host_id;
                        }
#endif
                        port = YES;
                     }
                  }
                  else
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "Function get_host_position() failed to locate alias %s (%d).",
                                p_db->host_alias, p_db->fsa_pos);
                     port = INCORRECT;
                  }
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "fsa_attach() failed.");
                  p_db->fsa_pos = INCORRECT;
                  port = INCORRECT;
               }
            }
            else
            {
               if (full_msg_path != NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "The message %s contains a hostname (%s) that is not in the FSA.",
                             full_msg_path, p_db->host_alias);
               }
               else
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to locate host %s in the FSA.",
                             p_db->host_alias);
               }
               port = INCORRECT;
            }
         }
         else
         {
            if (p_db->smtp_server[0] != '\0')
            {
               (void)strcpy(p_db->smtp_server,
                            fsa->real_hostname[(int)(fsa->host_toggle - 1)]);
            }
            port = SUCCESS;
         }
      }
      else
      {
         port = SUCCESS;
      }
   }
   else
   {
      char error_msg[MAX_URL_ERROR_MSG];

      url_get_error(error_mask, error_msg, MAX_URL_ERROR_MSG);
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Incorrect url `%s'. Error is: %s.", recipient, error_msg);
      port = INCORRECT;
   }

   return(port);
}
