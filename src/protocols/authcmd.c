/*
 *  authcmd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2021, 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   authcmd - set of functions to handle differnet types of authorization
 **
 ** SYNOPSIS
 **   int basic_authentication(struct http_message_reply *p_hmr)
 **
 ** DESCRIPTION
 **   authcmd provides a set of functions to generate the authorization
 **   line. Currently it supports basic and AWS4-HMAC-SHA256.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when successful. When an error has occurred
 **   it will either return INCORRECT or 
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.08.2021 H.Kiehl Created
 **   06.01.2022 H.Kiehl Catch double / at start of target directory.
 */
DESCR__E_M3

#include <stdio.h>             /* snprintf()                             */
#include <stdlib.h>            /* malloc(), free()                       */
#include <string.h>            /* memcpy(), strlen()                     */
#include <time.h>              /* strftime(), time()                     */
#ifdef WITH_SSL
# include <openssl/opensslv.h> /* OPENSSL_VERSION_MAJOR                  */
# include <openssl/hmac.h>     /* HMAC()                                 */
# include <openssl/evp.h>      /* EVP_sha256()                           */
# include <openssl/sha.h>      /* SHA256_Init(), SHA256_Update(),        */
                               /* SHA256_Final()                         */
#endif
#include <errno.h>
#include "fddefs.h"
#include "commondefs.h"
#include "httpdefs.h"

/* External global variables. */
extern char msg_str[];

#ifdef WITH_SSL
/* Local function prototypes. */
static int  aws4_cmd_authentication(char *, char *, char *, char *,
                                    struct http_message_reply *),
            aws_cmd_no_sign_request(struct http_message_reply *),
            sha256_file(char *, char *);
static void hash_2_hex(const unsigned char *, const int, char *),
            sha256_string(char *, char *);
#endif


/*######################## basic_authentication() #######################*/
int
basic_authentication(struct http_message_reply *p_hmr)
{
   size_t length;
   char   *dst_ptr,
          *src_ptr,
          base_64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
          userpasswd[MAX_USER_NAME_LENGTH + MAX_USER_NAME_LENGTH];

   /*
    * Let us first construct the authorization string from user name
    * and passwd:  <user>:<passwd>
    * And then encode this to using base-64 encoding.
    */
   length = snprintf(userpasswd, MAX_USER_NAME_LENGTH + MAX_USER_NAME_LENGTH,
                     "%s:%s", p_hmr->user, p_hmr->passwd);
   if (length > (MAX_USER_NAME_LENGTH + MAX_USER_NAME_LENGTH))
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "basic_authentication", NULL,
#if SIZEOF_SIZE_T == 4
                _("Buffer length to store user+passwd not long enough, needs %d bytes"),
#else
                _("Buffer length to store user+passwd not long enough, needs %lld bytes"),
#endif
               (pri_size_t)length);
      return(INCORRECT);
   }
   free(p_hmr->authorization);
   if ((p_hmr->authorization = malloc(21 + length + (length / 3) + 4 + 2 + 1)) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "basic_authentication", NULL,
                _("malloc() error : %s"), strerror(errno));
      return(INCORRECT);
   }
   dst_ptr = p_hmr->authorization;
   *dst_ptr = 'A'; *(dst_ptr + 1) = 'u'; *(dst_ptr + 2) = 't';
   *(dst_ptr + 3) = 'h'; *(dst_ptr + 4) = 'o'; *(dst_ptr + 5) = 'r';
   *(dst_ptr + 6) = 'i'; *(dst_ptr + 7) = 'z'; *(dst_ptr + 8) = 'a';
   *(dst_ptr + 9) = 't'; *(dst_ptr + 10) = 'i'; *(dst_ptr + 11) = 'o';
   *(dst_ptr + 12) = 'n'; *(dst_ptr + 13) = ':'; *(dst_ptr + 14) = ' ';
   *(dst_ptr + 15) = 'B'; *(dst_ptr + 16) = 'a'; *(dst_ptr + 17) = 's';
   *(dst_ptr + 18) = 'i'; *(dst_ptr + 19) = 'c'; *(dst_ptr + 20) = ' ';
   dst_ptr += 21;
   src_ptr = userpasswd;
   while (length > 2)
   {
      *dst_ptr = base_64[(int)(*src_ptr) >> 2];
      *(dst_ptr + 1) = base_64[((((int)(*src_ptr) & 0x3)) << 4) | (((int)(*(src_ptr + 1)) & 0xF0) >> 4)];
      *(dst_ptr + 2) = base_64[((((int)(*(src_ptr + 1))) & 0xF) << 2) | ((((int)(*(src_ptr + 2))) & 0xC0) >> 6)];
      *(dst_ptr + 3) = base_64[((int)(*(src_ptr + 2))) & 0x3F];
      src_ptr += 3;
      length -= 3;
      dst_ptr += 4;
   }
   if (length == 2)
   {
      *dst_ptr = base_64[(int)(*src_ptr) >> 2];
      *(dst_ptr + 1) = base_64[((((int)(*src_ptr) & 0x3)) << 4) | (((int)(*(src_ptr + 1)) & 0xF0) >> 4)];
      *(dst_ptr + 2) = base_64[(((int)(*(src_ptr + 1))) & 0xF) << 2];
      *(dst_ptr + 3) = '=';
      dst_ptr += 4;
   }
   else if (length == 1)
        {
           *dst_ptr = base_64[(int)(*src_ptr) >> 2];
           *(dst_ptr + 1) = base_64[(((int)(*src_ptr) & 0x3)) << 4];
           *(dst_ptr + 2) = '=';
           *(dst_ptr + 3) = '=';
           dst_ptr += 4;
        }
   *dst_ptr = '\r';
   *(dst_ptr + 1) = '\n';
   *(dst_ptr + 2) = '\0';

   return(SUCCESS);
}


#ifdef WITH_SSL
/*############################# aws_cmd() ###############################*/
int
aws_cmd(char                      *cmd,
        char                      *file_name,
        char                      *target_dir,
        char                      *parameter,
        struct http_message_reply *p_hmr)
{
   int ret;

   if (p_hmr->auth_type == AUTH_AWS4_HMAC_SHA256)
   {
      ret = aws4_cmd_authentication(cmd, file_name, target_dir,
                                    parameter, p_hmr);
   }
   else if (p_hmr->auth_type == AUTH_AWS_NO_SIGN_REQUEST)
        {
           ret = aws_cmd_no_sign_request(p_hmr);
        }
        else
        {
           trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws_cmd", NULL,
                     "Unknown auth_type (%d)",
                     (int)p_hmr->auth_type);
           ret = INCORRECT;
        }

   return(ret);
}


/*                                    cmd     target_dir                       filename                        parameter                                                             host:                              x-amz-content-sha256:                                  host;x-amz-content-sha256;x-amz-date */
#define CANONICAL_REQUEST_CMD_LENGTH (8 + 1 + (3 * MAX_RECIPIENT_LENGTH) + 1 + (3 * MAX_FILENAME_LENGTH) + 1 + MAX_AWS4_PARAMETER_LENGTH + MAX_INT_LENGTH + MAX_FILENAME_LENGTH + 1 + 5 + MAX_REAL_HOSTNAME_LENGTH + 1 + 21 + SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + 1 + 11 + 16 + 2 + 52 + SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + 1 + 1)
#define STRING_2_SIGN_CMD_LENGTH     (16 + 1 + 16 + 1 + 8 + 1 + MAX_REAL_HOSTNAME_LENGTH + 1 + 15 + 1 + SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + 1)
/*++++++++++++++++++++++ aws4_cmd_authentication() ++++++++++++++++++++++*/
static int
aws4_cmd_authentication(char                      *cmd,
                        char                      *file_name,
                        char                      *target_dir,
                        char                      *parameter,
                        struct http_message_reply *p_hmr)
{
   int    ret;
   char   date_long[17];
   time_t now;

   if (p_hmr->authorization != NULL)
   {
      free(p_hmr->authorization);
      p_hmr->authorization = NULL;
   }

   now = time(NULL);
   if (strftime(date_long, 17, "%Y%m%dT%H%M%SZ", gmtime(&now)) == 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_cmd_authentication", NULL,
                _("strftime() error : %s"), strerror(errno));
      ret = INCORRECT;
   }
   else
   {
      int           key_len,
                    string_2_sign_length;
      char          canonical_request[CANONICAL_REQUEST_CMD_LENGTH],
                    canonical_request_hash_hex[SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + 1],
                    date_short[9],
                    key[4 + MAX_USER_NAME_LENGTH + 1],
                    string_2_sign[STRING_2_SIGN_CMD_LENGTH];
      unsigned char result[SHA256_DIGEST_LENGTH];
      const EVP_MD  *sha256evp = EVP_sha256();

      (void)memcpy(date_short, date_long, 8);
      date_short[8] = '\0';
      if ((file_name[0] == '\0') && (target_dir[0] == '/') &&
          (target_dir[1] == '\0'))
      {
         ret = snprintf(canonical_request, CANONICAL_REQUEST_CMD_LENGTH,
                        "%s\n/\n%s\nhost:%s\nx-amz-content-sha256:%s\nx-amz-date:%s\n\nhost;x-amz-content-sha256;x-amz-date\n%s",
                        cmd, parameter, p_hmr->hostname, SHA256_EMPTY_PAYLOAD,
                        date_long, SHA256_EMPTY_PAYLOAD);
      }
      else
      {
         char file_name_encoded[(3 * MAX_FILENAME_LENGTH)],
              target_dir_encoded[(3 * MAX_RECIPIENT_LENGTH)];

         url_path_encode(target_dir, target_dir_encoded);
         url_path_encode(file_name, file_name_encoded);
         ret = snprintf(canonical_request, CANONICAL_REQUEST_CMD_LENGTH,
                        "%s\n%s%s%s\n%s\nhost:%s\nx-amz-content-sha256:%s\nx-amz-date:%s\n\nhost;x-amz-content-sha256;x-amz-date\n%s",
                        cmd, (*target_dir_encoded != '/') ? "/" : "",
                        target_dir_encoded, file_name_encoded, parameter,
                        p_hmr->hostname, SHA256_EMPTY_PAYLOAD, date_long,
                        SHA256_EMPTY_PAYLOAD);
      }
      if (ret >= CANONICAL_REQUEST_CMD_LENGTH)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_cmd_authentication", NULL,
                   "snprintf() canonical_request output truncated (>=%d).",
                   CANONICAL_REQUEST_CMD_LENGTH);
         return(INCORRECT);
      }

#ifdef WITH_TRACE
      trace_log(NULL, 0, C_TRACE, "------------ canonical_request ------------", 43, NULL);
      trace_log(NULL, 0, CRLF_C_TRACE, canonical_request, strlen(canonical_request), NULL);
      trace_log(NULL, 0, C_TRACE, "-------------------------------------------", 43, NULL);
#endif
      sha256_string(canonical_request, canonical_request_hash_hex);
      string_2_sign_length = snprintf(string_2_sign, STRING_2_SIGN_CMD_LENGTH,
                                      "AWS4-HMAC-SHA256\n%s\n%s/%s/%s/aws4_request\n%s",
                                      date_long, date_short, p_hmr->region,
                                      p_hmr->service,
                                      canonical_request_hash_hex);
      if (string_2_sign_length >= STRING_2_SIGN_CMD_LENGTH)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_cmd_authentication", NULL,
                   "snprintf() string_2_sign output truncated (>=%d).",
                   STRING_2_SIGN_CMD_LENGTH);
         return(INCORRECT);
      }
#ifdef WITH_TRACE
      trace_log(NULL, 0, C_TRACE, "-------------- string_2_sign --------------", 43, NULL);
      trace_log(NULL, 0, CRLF_C_TRACE, string_2_sign, string_2_sign_length, NULL);
      trace_log(NULL, 0, C_TRACE, "-------------------------------------------", 43, NULL);
#endif

      key_len = snprintf(key, 4 + MAX_USER_NAME_LENGTH + 1,
                         "AWS4%s", p_hmr->passwd);
      if (HMAC(sha256evp, key, key_len, (const unsigned char *)date_short,
               8, result, NULL) != NULL)
      {
         size_t        bucket_location_length;
         unsigned char signature[SHA256_DIGEST_LENGTH];
#ifdef WITH_TRACE
         int           trace_length;
         char          tmp_hash_hex[SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + 1];

         hash_2_hex(result, SHA256_DIGEST_LENGTH, tmp_hash_hex);
         trace_length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                 "date key=%s", tmp_hash_hex);
         trace_log(NULL, 0, C_TRACE, msg_str, trace_length, NULL);
#endif

         (void)memcpy(signature, result, SHA256_DIGEST_LENGTH);
         bucket_location_length = strlen(p_hmr->region);
         if (HMAC(sha256evp, signature, SHA256_DIGEST_LENGTH,
                  (const unsigned char *)p_hmr->region,
                  bucket_location_length, result, NULL) != NULL)
         {
            size_t service_length;

#ifdef WITH_TRACE
            hash_2_hex(result, SHA256_DIGEST_LENGTH, tmp_hash_hex);
            trace_length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                    "date region key=%s", tmp_hash_hex);
            trace_log(NULL, 0, C_TRACE, msg_str, trace_length, NULL);
#endif
            service_length = strlen(p_hmr->service);
            (void)memcpy(signature, result, SHA256_DIGEST_LENGTH);
            if (HMAC(sha256evp, signature, SHA256_DIGEST_LENGTH,
                     (const unsigned char *)p_hmr->service,
                     service_length, result, NULL) != NULL)
            {
#ifdef WITH_TRACE
               hash_2_hex(result, SHA256_DIGEST_LENGTH, tmp_hash_hex);
               trace_length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                       "date region service key=%s",
                                       tmp_hash_hex);
               trace_log(NULL, 0, C_TRACE, msg_str, trace_length, NULL);
#endif
               (void)memcpy(signature, result, SHA256_DIGEST_LENGTH);
               if (HMAC(sha256evp, signature, SHA256_DIGEST_LENGTH,
                        (const unsigned char *)"aws4_request", 12,
                        result, NULL) != NULL)
               {
#ifdef WITH_TRACE
                  hash_2_hex(result, SHA256_DIGEST_LENGTH, tmp_hash_hex);
                  trace_length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                          "signing key=%s",
                                          tmp_hash_hex);
                  trace_log(NULL, 0, C_TRACE, msg_str, trace_length, NULL);
#endif
                  (void)memcpy(signature, result, SHA256_DIGEST_LENGTH);
                  if (HMAC(sha256evp, signature, SHA256_DIGEST_LENGTH,
                           (const unsigned char *)string_2_sign,
                           string_2_sign_length, result, NULL) != NULL)
                  {
                     size_t authorization_length,
                            user_length;
                     char   signature_hex[SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + 1];

                     hash_2_hex(result, SHA256_DIGEST_LENGTH, signature_hex);
#ifdef WITH_TRACE
                     trace_length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                             "signature=%s",
                                             signature_hex);
                     trace_log(NULL, 0, C_TRACE, msg_str, trace_length, NULL);
#endif
                     user_length = strlen(p_hmr->user);
                     authorization_length = 28 + /* x-amz-date: */
                                            2 +  /* CRLF */
                                            22 + SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + /* x-amz-content-sha256: */
                                            2 +  /* CRLF */
                                            43 + /* Authorization: AWS4-HMAC-SHA256 Credential= */
                                            user_length +
                                            1 +
                                            8 +  /* date_short */
                                            1 +
                                            bucket_location_length +
                                            1 +
                                            service_length +
                                            77 + /* /aws4_request, SignedHeaders=host;x-amz-content-sha256;x-amz-date, Signature= */
                                            SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH +
                                            2 +  /* CRLF */
                                            1;   /* \0 */
                     if ((p_hmr->authorization = malloc(authorization_length)) != NULL)
                     {
                        ret = snprintf(p_hmr->authorization,
                                       authorization_length,
                                       "x-amz-date: %s\r\nx-amz-content-sha256: %s\r\nAuthorization: AWS4-HMAC-SHA256 Credential=%s/%s/%s/%s/aws4_request, SignedHeaders=host;x-amz-content-sha256;x-amz-date, Signature=%s\r\n",
                                       date_long, SHA256_EMPTY_PAYLOAD,
                                       p_hmr->user, date_short, p_hmr->region,
                                       p_hmr->service, signature_hex);
                        if (ret < authorization_length)
                        {
#ifdef WITH_TRACE
                           trace_log(NULL, 0, CRLF_C_TRACE,
                                     p_hmr->authorization, ret, NULL);
#endif
                           ret = SUCCESS;
                        }
                        else
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_cmd_authentication", NULL,
                                     "snprintf() authorization output truncated (%d >= %d).",
                                     ret, authorization_length);
                           free(p_hmr->authorization);
                           p_hmr->authorization = NULL;
                           ret = INCORRECT;
                        }
                     }
                     else
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_cmd_authentication", NULL,
                                  _("malloc() error : %s"), strerror(errno));
                        ret = INCORRECT;
                     }
                  }
                  else
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_cmd_authentication", NULL,
                               _("HMAC() error for final signature."));
                     ret = INCORRECT;
                  }
               }
               else
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_cmd_authentication", NULL,
                            _("HMAC() error for signing key."));
                  ret = INCORRECT;
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_cmd_authentication", NULL,
                         _("HMAC() error for date+region+service key."));
               ret = INCORRECT;
            }
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_cmd_authentication", NULL,
                      _("HMAC() error for date+region key."));
            ret = INCORRECT;
         }
#ifdef WITH_TRACE
         msg_str[0] = '\0';
#endif
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_cmd_authentication", NULL,
                   _("HMAC() error for date key."));
         ret = INCORRECT;
      }
   }

   return(ret);
}


/*++++++++++++++++++++++ aws_cmd_no_sign_request() ++++++++++++++++++++++*/
static int
aws_cmd_no_sign_request(struct http_message_reply *p_hmr)
{
   int    ret;
   char   date_long[17];
   time_t now;

   if (p_hmr->authorization != NULL)
   {
      free(p_hmr->authorization);
      p_hmr->authorization = NULL;
   }

   now = time(NULL);
   if (strftime(date_long, 17, "%Y%m%dT%H%M%SZ", gmtime(&now)) == 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws_cmd_no_sign_request", NULL,
                _("strftime() error : %s"), strerror(errno));
      ret = INCORRECT;
   }
   else
   {
      size_t authorization_length;

      authorization_length = 28 + /* x-amz-date: */
                             2 +  /* CRLF */
                             1;   /* \0 */
      if ((p_hmr->authorization = malloc(authorization_length)) != NULL)
      {
         ret = snprintf(p_hmr->authorization, authorization_length,
                        "x-amz-date: %s\r\n", date_long);
         if (ret < authorization_length)
         {
            ret = SUCCESS;
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws_cmd_no_sign_request", NULL,
                      "snprintf() authorization output truncated (%d >= %d).",
                      ret, authorization_length);
            free(p_hmr->authorization);
            p_hmr->authorization = NULL;
            ret = INCORRECT;
         }
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws_cmd_no_sign_request", NULL,
                   _("malloc() error : %s"), strerror(errno));
         ret = INCORRECT;
      }
   }

   return(ret);
}


/*##################### aws4_put_authentication() #######################*/
int
aws4_put_authentication(char                      *file_name,
                        char                      *fullname,
                        off_t                     file_size,
                        char                      *target_dir,
                        char                      *file_content_hash_hex,
                        struct http_message_reply *p_hmr)
{
   int    free_file_content_hash_hex,
          ret;
   char   date_long[17];
   time_t now;

   if (p_hmr->authorization != NULL)
   {
      free(p_hmr->authorization);
      p_hmr->authorization = NULL;
   }

   if (file_content_hash_hex == NULL)
   {
      if (fullname == NULL)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_put_authentication", NULL,
                   "Wrong usage of function. If file_content_hash_hex is NULL, fullname may not be NULL.");
         return(INCORRECT);
      }
      if ((file_content_hash_hex = malloc(SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + 1)) == NULL)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_put_authentication", NULL,
                   _("malloc() error : %s"), strerror(errno));
         return(INCORRECT);
      }
      if (sha256_file(fullname, file_content_hash_hex) != SUCCESS)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_put_authentication", NULL,
                    _("sha256_file() error"));
         free(file_content_hash_hex);
         file_content_hash_hex = NULL;
         return(INCORRECT);
      }
      free_file_content_hash_hex = YES;
   }
   else
   {
      free_file_content_hash_hex = NO;
   }

   now = time(NULL);
   if (strftime(date_long, 17, "%Y%m%dT%H%M%SZ", gmtime(&now)) == 0)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_put_authentication", NULL,
                _("strftime() error : %s"), strerror(errno));
      ret = INCORRECT;
   }
   else
   {
      int           key_len,
                    string_2_sign_length;
      char          canonical_request[4 + 1 +                     /* PUT */
                                      (3 * MAX_RECIPIENT_LENGTH) + 1 +  /* target_dir */
                                      (3 * MAX_FILENAME_LENGTH) + 2 +   /* file_name */
                                      15 + MAX_OFF_T_LENGTH + 1 + /* content-length: */
                                      5 + MAX_REAL_HOSTNAME_LENGTH + 1 +  /* host: */
                                      21 + SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + 1 + /* x-amz-content-sha256: */
                                      11 + 16 + 2 + 52 +          /* content-length;host;x-amz-content-sha256;x-amz-date */
                                      SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + /* SHA256 sum */
                                      1],                         /* \0 */
                    canonical_request_hash_hex[SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + 1],
                    date_short[9],
                    file_name_encoded[(3 * MAX_FILENAME_LENGTH)],
                    key[4 + MAX_USER_NAME_LENGTH + 1],
                    string_2_sign[16 + 1 + 16 + 1 + 8 + 1 + MAX_REAL_HOSTNAME_LENGTH + 1 + 15 + 1 + SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + 1],
                    target_dir_encoded[(3 * MAX_RECIPIENT_LENGTH)];
      unsigned char result[SHA256_DIGEST_LENGTH];
      const EVP_MD  *sha256evp = EVP_sha256();

      (void)memcpy(date_short, date_long, 8);
      date_short[8] = '\0';
      url_path_encode(target_dir, target_dir_encoded);
      url_path_encode(file_name, file_name_encoded);
      (void)snprintf(canonical_request,
                     4 + 1 + (3 * MAX_RECIPIENT_LENGTH) + 1 + (3 * MAX_FILENAME_LENGTH) + 2 + 15 + MAX_OFF_T_LENGTH + 1 + 5 + MAX_REAL_HOSTNAME_LENGTH + 1 + 21 + SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + 1 + 11 + 16 + 2 + 52 + SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + 1,
# if SIZEOF_OFF_T == 4
                     "PUT\n%s%s%s\n\ncontent-length:%ld\nhost:%s\nx-amz-content-sha256:%s\nx-amz-date:%s\n\ncontent-length;host;x-amz-content-sha256;x-amz-date\n%s",
# else
                     "PUT\n%s%s%s\n\ncontent-length:%lld\nhost:%s\nx-amz-content-sha256:%s\nx-amz-date:%s\n\ncontent-length;host;x-amz-content-sha256;x-amz-date\n%s",
# endif
                     (*target_dir_encoded != '/') ? "/" : "",
                     target_dir_encoded, file_name_encoded,
                     (pri_off_t)file_size, p_hmr->hostname,
                     file_content_hash_hex, date_long, file_content_hash_hex);

#ifdef WITH_TRACE
       trace_log(NULL, 0, C_TRACE, "------------ canonical_request ------------", 43, NULL);
       trace_log(NULL, 0, CRLF_C_TRACE, canonical_request, strlen(canonical_request), NULL);
       trace_log(NULL, 0, C_TRACE, "-------------------------------------------", 43, NULL);
#endif
      sha256_string(canonical_request, canonical_request_hash_hex);
      string_2_sign_length = snprintf(string_2_sign,
                                      16 + 1 + 16 + 1 + 8 + 1 + MAX_REAL_HOSTNAME_LENGTH + 1 + 15 + 1 + SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + 1,
                                      "AWS4-HMAC-SHA256\n%s\n%s/%s/%s/aws4_request\n%s",
                                      date_long, date_short, p_hmr->region,
                                      p_hmr->service,
                                      canonical_request_hash_hex);
#ifdef WITH_TRACE
       trace_log(NULL, 0, C_TRACE, "-------------- string_2_sign --------------", 43, NULL);
       trace_log(NULL, 0, CRLF_C_TRACE, string_2_sign, string_2_sign_length, NULL);
       trace_log(NULL, 0, C_TRACE, "-------------------------------------------", 43, NULL);
#endif

      key_len = snprintf(key, 4 + MAX_USER_NAME_LENGTH + 1,
                         "AWS4%s", p_hmr->passwd);
      if (HMAC(sha256evp, key, key_len, (const unsigned char *)date_short,
               8, result, NULL) != NULL)
      {
         size_t        bucket_location_length;
         unsigned char signature[SHA256_DIGEST_LENGTH];
#ifdef WITH_TRACE
         int           trace_length;
         char          tmp_hash_hex[SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + 1];

         hash_2_hex(result, SHA256_DIGEST_LENGTH, tmp_hash_hex);
         trace_length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                 "date key=%s", tmp_hash_hex);
         trace_log(NULL, 0, C_TRACE, msg_str, trace_length, NULL);
#endif

         (void)memcpy(signature, result, SHA256_DIGEST_LENGTH);
         bucket_location_length = strlen(p_hmr->region);
         if (HMAC(sha256evp, signature, SHA256_DIGEST_LENGTH,
                  (const unsigned char *)p_hmr->region,
                  bucket_location_length, result, NULL) != NULL)
         {
            size_t service_length;

#ifdef WITH_TRACE
            hash_2_hex(result, SHA256_DIGEST_LENGTH, tmp_hash_hex);
            trace_length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                    "date region key=%s", tmp_hash_hex);
            trace_log(NULL, 0, C_TRACE, msg_str, trace_length, NULL);
#endif
            service_length = strlen(p_hmr->service);
            (void)memcpy(signature, result, SHA256_DIGEST_LENGTH);
            if (HMAC(sha256evp, signature, SHA256_DIGEST_LENGTH,
                     (const unsigned char *)p_hmr->service,
                     service_length, result, NULL) != NULL)
            {
#ifdef WITH_TRACE
               hash_2_hex(result, SHA256_DIGEST_LENGTH, tmp_hash_hex);
               trace_length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                       "date region service key=%s",
                                       tmp_hash_hex);
               trace_log(NULL, 0, C_TRACE, msg_str, trace_length, NULL);
#endif
               (void)memcpy(signature, result, SHA256_DIGEST_LENGTH);
               if (HMAC(sha256evp, signature, SHA256_DIGEST_LENGTH,
                        (const unsigned char *)"aws4_request", 12,
                        result, NULL) != NULL)
               {
#ifdef WITH_TRACE
                  hash_2_hex(result, SHA256_DIGEST_LENGTH, tmp_hash_hex);
                  trace_length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                          "signing key=%s",
                                          tmp_hash_hex);
                  trace_log(NULL, 0, C_TRACE, msg_str, trace_length, NULL);
#endif
                  (void)memcpy(signature, result, SHA256_DIGEST_LENGTH);
                  if (HMAC(sha256evp, signature, SHA256_DIGEST_LENGTH,
                           (const unsigned char *)string_2_sign,
                           string_2_sign_length, result, NULL) != NULL)
                  {
                     size_t authorization_length,
                            user_length;
                     char   signature_hex[SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + 1];

                     hash_2_hex(result, SHA256_DIGEST_LENGTH, signature_hex);
#ifdef WITH_TRACE
                     trace_length = snprintf(msg_str, MAX_RET_MSG_LENGTH,
                                             "signature=%s",
                                             signature_hex);
                     trace_log(NULL, 0, C_TRACE, msg_str, trace_length, NULL);
#endif
                     user_length = strlen(p_hmr->user);
                     authorization_length = 28 + /* x-amz-date: */
                                            2 +  /* CRLF */
                                            22 + SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH + /* x-amz-content-sha256: */
                                            2 +  /* CRLF */
                                            43 + /* Authorization: AWS4-HMAC-SHA256 Credential= */
                                            user_length +
                                            1 +
                                            8 +  /* date_short */
                                            1 +
                                            bucket_location_length +
                                            1 +
                                            service_length +
                                            92 + /* /aws4_request, SignedHeaders=content-length;host;x-amz-content-sha256;x-amz-date, Signature= */
                                            SHA256_DIGEST_LENGTH + SHA256_DIGEST_LENGTH +
                                            2 +  /* CRLF */
                                            1;   /* \0 */
                     if ((p_hmr->authorization = malloc(authorization_length)) != NULL)
                     {
                        ret = snprintf(p_hmr->authorization, authorization_length,
                                       "x-amz-date: %s\r\nx-amz-content-sha256: %s\r\nAuthorization: AWS4-HMAC-SHA256 Credential=%s/%s/%s/%s/aws4_request, SignedHeaders=content-length;host;x-amz-content-sha256;x-amz-date, Signature=%s\r\n",
                                       date_long, file_content_hash_hex,
                                       p_hmr->user, date_short, p_hmr->region,
                                       p_hmr->service, signature_hex);
                        if (ret < authorization_length)
                        {
#ifdef WITH_TRACE
                           trace_log(NULL, 0, CRLF_C_TRACE,
                                     p_hmr->authorization, ret, NULL);
#endif
                           ret = SUCCESS;
                        }
                        else
                        {
                           trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_put_authentication", NULL,
                                     "snprintf() authorization output truncated (%d >= %d).",
                                     ret, authorization_length);
                           free(p_hmr->authorization);
                           p_hmr->authorization = NULL;
                           ret = INCORRECT;
                        }
                     }
                     else
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_put_authentication", NULL,
                                  _("malloc() error : %s"), strerror(errno));
                        ret = INCORRECT;
                     }
                  }
                  else
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_put_authentication", NULL,
                               _("HMAC() error for final signature."));
                     ret = INCORRECT;
                  }
               }
               else
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_put_authentication", NULL,
                            _("HMAC() error for signing key."));
                  ret = INCORRECT;
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_put_authentication", NULL,
                         _("HMAC() error for date+region+service key."));
               ret = INCORRECT;
            }
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_put_authentication", NULL,
                      _("HMAC() error for date+region key."));
            ret = INCORRECT;
         }
#ifdef WITH_TRACE
         msg_str[0] = '\0';
#endif
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "aws4_put_authentication", NULL,
                   _("HMAC() error for date key."));
         ret = INCORRECT;
      }
   }

   if (free_file_content_hash_hex == YES)
   {
      free(file_content_hash_hex);
      file_content_hash_hex = NULL;
   }

   return(ret);
}


/*++++++++++++++++++++++++++++ sha256_file() ++++++++++++++++++++++++++++*/
static int
sha256_file(char *file, char *sha256_hash_hex)
{
   int  ret;
   FILE *fp;

   if ((fp = fopen(file, "rb")) == NULL)
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_file", NULL,
                "Failed to fopen() %s : %s", file, strerror(errno));
      ret = INCORRECT;
   }
   else
   {
#if OPENSSL_VERSION_MAJOR > 2
      EVP_MD_CTX *sha256 = NULL;

      if ((sha256 = EVP_MD_CTX_create()) != NULL)
      {
         if ((ret = EVP_DigestInit_ex(sha256, EVP_sha256(), NULL)) == 1)
         {
            char *buffer;

            if ((buffer = malloc(32768)) == NULL)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_file", NULL,
                         "Failed to malloc() 32768 bytes : %s",
                         strerror(errno));
               ret = INCORRECT;
            }
            else
            {
               size_t bytes_read;

               while ((bytes_read = fread(buffer, 1, 32768, fp)))
               {
                  if ((ret = EVP_DigestUpdate(sha256, buffer, bytes_read)) != 1)
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_file", NULL,
                               "EVP_DigestUpdate() failed (%d)", ret);
                     ret = INCORRECT;
                  }
               }
               if (ret != INCORRECT)
               {
                  unsigned int  ret_size;
                  unsigned char hash[EVP_MAX_MD_SIZE];

                  if ((ret = EVP_DigestFinal(sha256, hash, &ret_size)) == 1)
                  {
                     hash_2_hex(hash, ret_size, sha256_hash_hex);
                     ret = SUCCESS;
                  }
                  else
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_file", NULL,
                               "EVP_DigestFinal() failed (%d)", ret);
                     ret = INCORRECT;
                  }
               }
               free(buffer);
            }
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_file", NULL,
                      "EVP_DigestInit_ex() failed (%d)", ret);
            ret = INCORRECT;
         }
         EVP_MD_CTX_destroy(sha256);
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_file", NULL,
                   "EVP_MD_CTX_create() failed.");
         ret = INCORRECT;
      }
#else
      SHA256_CTX sha256;

      if ((ret = SHA256_Init(&sha256)) == 1)
      {
         char *buffer;

         if ((buffer = malloc(32768)) == NULL)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_file", NULL,
                      "Failed to malloc() 32768 bytes : %s", strerror(errno));
            ret = INCORRECT;
         }
         else
         {
            size_t bytes_read;

            while ((bytes_read = fread(buffer, 1, 32768, fp)))
            {
               if ((ret = SHA256_Update(&sha256, buffer, bytes_read)) != 1)
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_file", NULL,
                            "SHA256_Update() failed (%d)", ret);
                  ret = INCORRECT;
               }
            }
            if (ret != INCORRECT)
            {
               unsigned char hash[SHA256_DIGEST_LENGTH];

               if ((ret = SHA256_Final(hash, &sha256)) == 1)
               {
                  hash_2_hex(hash, SHA256_DIGEST_LENGTH, sha256_hash_hex);
                  ret = SUCCESS;
               }
               else
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_file", NULL,
                            "SHA256_Final() failed (%d)", ret);
                  ret = INCORRECT;
               }
            }
            free(buffer);
         }
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_file", NULL,
                   "SHA256_Init() failed (%d)", ret);
         ret = INCORRECT;
      }
#endif
      if (fclose(fp) == EOF)
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_file", NULL,
                   "Failed to fclose() %s : %s", file, strerror(errno));
      }
   }

   return(ret);
}


/*+++++++++++++++++++++++++++ sha256_string() +++++++++++++++++++++++++++*/
static void
sha256_string(char *string, char *sha256_hash_hex)
{
   int        i;
#if OPENSSL_VERSION_MAJOR > 2
   EVP_MD_CTX *sha256;

   if ((sha256 = EVP_MD_CTX_create()) != NULL)
   {
      if ((i = EVP_DigestInit_ex(sha256, EVP_sha256(), NULL)) == 1)
      {
         if ((i = EVP_DigestUpdate(sha256, string, strlen(string))) == 1)
         {
            unsigned int  ret_size;
            unsigned char hash[EVP_MAX_MD_SIZE];

            if ((i = EVP_DigestFinal(sha256, hash, &ret_size)) == 1)
            {
               int  wpos = 0;
               char *hex = "0123456789abcdef";

               for (i = 0; i < ret_size; i++)
               {
                  if (hash[i] > 15)
                  {
                     sha256_hash_hex[wpos] = hex[hash[i] >> 4];
                     sha256_hash_hex[wpos + 1] = hex[hash[i] & 0x0F];
                  }
                  else
                  {
                     sha256_hash_hex[wpos] = '0';
                     sha256_hash_hex[wpos + 1] = hex[hash[i]];
                  }
                  wpos += 2;
               }
               i = 64;
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_string", NULL,
                         "EVP_DigestFinal() failed (%d)", i);
               i = 0;
            }
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_string", NULL,
                      "EVP_DigestUpdate() failed (%d)", i);
            i = 0;
         }
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_string", NULL,
                   "EVP_DigestInit_ex() failed (%d)", i);
         i = 0;
      }
   }
   else
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_string", NULL,
                "EVP_MD_CTX_create() failed.");
      i = 0;
   }
#else
   SHA256_CTX sha256;

   if ((i = SHA256_Init(&sha256)) == 1)
   {
      if ((i = SHA256_Update(&sha256, string, strlen(string))) == 1)
      {
         unsigned char hash[SHA256_DIGEST_LENGTH];

         if ((i = SHA256_Final(hash, &sha256)) == 1)
         {
            int  wpos = 0;
            char *hex = "0123456789abcdef";

            for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
            {
               if (hash[i] > 15)
               {
                  sha256_hash_hex[wpos] = hex[hash[i] >> 4];
                  sha256_hash_hex[wpos + 1] = hex[hash[i] & 0x0F];
               }
               else
               {
                  sha256_hash_hex[wpos] = '0';
                  sha256_hash_hex[wpos + 1] = hex[hash[i]];
               }
               wpos += 2;
            }
            i = 64;
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_string", NULL,
                      "SHA256_Final() failed (%d)", i);
            i = 0;
         }
      }
      else
      {
         trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_string", NULL,
                   "SHA256_Update() failed (%d)", i);
         i = 0;
      }
   }
   else
   {
      trans_log(ERROR_SIGN, __FILE__, __LINE__, "sha256_string", NULL,
                "SHA256_Init() failed (%d)", i);
      i = 0;
   }
#endif
   sha256_hash_hex[i] = '\0';

   return;
}


/*----------------------------- hash_2_hex() ----------------------------*/
static void
hash_2_hex(const unsigned char *hash, const int length, char *hash_hex)
{
   int  i,
        wpos = 0;
   char *hex = "0123456789abcdef";

   for (i = 0; i < length; i++)
   {
      if (hash[i] > 15)
      {
         hash_hex[wpos] = hex[hash[i] >> 4];
         hash_hex[wpos + 1] = hex[hash[i] & 0x0F];
      }
      else
      {
         hash_hex[wpos] = '0';
         hash_hex[wpos + 1] = hex[hash[i]];
      }
      wpos += 2;
   }
   hash_hex[64] = '\0';

   return;
}
#endif /* WITH_SSL */
