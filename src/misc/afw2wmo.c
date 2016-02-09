/*
 *  afw2wmo.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2014 Deutscher Wetterdienst (DWD),
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
 **   afw2wmo - checks the format of the message and creates
 **                      a WMO-message
 **
 ** SYNOPSIS
 **   int afw2wmo(char *msg,
 **               int  *msg_length,
 **               char **wmo_buffer,
 **               char *msg_name)
 **
 ** DESCRIPTION
 **   This function checks the syntax of message <msg>. The following
 **   AFW headers are detected:
 **   a) TT[ii][C] YYGGgg IIiii
 **       |  |  |   | | |  |
 **       |  |  |   | | |  +---------- Numeric station indicator
 **       |  |  |   | | +------------- Minutes of DTG [0 - 59]
 **       |  |  |   | +--------------- Hour of DTG    [0 - 23]
 **       |  |  |   +----------------- Day of DTG     [0 - 31]
 **       |  |  +--------------------- This message is a correction
 **       |  +------------------------ ii-Group
 **       +--------------------------- Data Type Designator
 **
 **   b) TT[ii][C] YYGGgg CCCC
 **                        |
 **                        +---------- Alphanumeric station indicator
 **
 **   The end of message is defined as "=<CR><CR><LF><ETX>". If this
 **   message is correct it gets storred to the wmo_buffer in WMO
 **   format where the MD (Message Distributor) can fetch it and
 **   send it via FTP to its destination. The WMO format of the
 **   message will be as follows (respectivly to those shown above):
 **   a) TTDL[ii] DAFW YYGGgg[ COR]<LF>
 **      IIiii <Message body><LF><ETX>
 **
 **   b) TTDL[ii] CCCC YYGGgg[ COR]<LF>
 **      <Message body><LF><ETX>
 **
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.07.1999 H.Kiehl Created (from evaluate_message() in MCS)
 **   24.02.2000 H.Kiehl Convert BM?? -> BMBB??
 **                      Convert XX?? -> BMBB??
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>             /* strerror(), strlen(), memcpy()        */
#include <stdlib.h>             /* malloc(), free()                      */
#include <ctype.h>              /* isalpha(), isdigit()                  */
#include <errno.h>
#include "amgdefs.h"

#ifdef _WITH_AFW2WMO
/* Local function prototypes */
static void show_error(char *, char *);

#define SOH                    1
#define ETX                    3
#define AFW_IDENTIFIER         "DAFW"
#define AFW_IDENTIFIER_SPECIAL "DWWW"
#define AFW_IDENTIFIER_LENGTH  4
#define COR_IDENTIFIER         " COR"
#define COR_IDENTIFIER_LENGTH  4
#endif /* _WITH_AFW2WMO */


/*############################## afw2wmo() ##############################*/
int
afw2wmo(char *msg, int *msg_length, char **wmo_buffer, char *msg_name)
{
#ifdef _WITH_AFW2WMO
   int  garbage = 0,
        length_done = 0,
        correction = NO,
        original_length = *msg_length;
   char *iiiii_ptr = NULL,
        *dtg_ptr,
        *write_ptr,
        *tmp_write_ptr = NULL,
        *p_start_afw_header,
        *p_start_ansi = NULL,   /* alpha nummeric station indicator */
        *read_ptr,
        *msg_header_ptr,
        msg_header[28];

   /* Ignore any garbage before ZCZC */
   read_ptr = msg;
   while ((*read_ptr <= ' ') && (length_done < *msg_length))
   {
      read_ptr++; garbage++;
   }
   *msg_length -= garbage;

   /* It does not make sence to check the message if it's less */
   /* then 20 Bytes.                                           */
   if (*msg_length < 20)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "afw2wmo(): Message %s to short [%d]", msg_name, *msg_length);
      return(INCORRECT);
   }

   if (((*read_ptr != 'Z') && (*read_ptr != 'z')) ||
       ((*(read_ptr + 1) != 'C') && (*(read_ptr + 1) != 'c')) ||
       ((*(read_ptr + 2) != 'Z') && (*(read_ptr + 2) != 'z')) ||
       ((*(read_ptr + 3) != 'C') && (*(read_ptr + 3) != 'c')))
   {
      *wmo_buffer = NULL;
      if (*msg == SOH)
      {
         *msg_length = original_length;
         return(WMO_MESSAGE);
      }
      else
      {
         receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                     "afw2wmo(): %s is contains an unknown message type!",
                     msg_name);
         return(INCORRECT);
      }
   }

   /* It does not make sence to check the message if it's more */
   /* then 1 Megabytes.                                        */
   if (*msg_length > 1048576)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "afw2wmo(): Message %s to long [%d]", msg_name, *msg_length);
      return(INCORRECT);
   }

   read_ptr += 4;
   length_done += 4;

   while ((*read_ptr <= ' ') && (length_done < *msg_length))
   {
      read_ptr++; length_done++;
   }

   /* Check for TT */
   if ((isalpha((int)(*read_ptr)) == 0) ||
       (isalpha((int)(*(read_ptr + 1)) == 0)))
   {
      show_error(read_ptr, "TT");
/*
      show_buffer(receive_log_fd, msg, *msg_length);
*/
      return(INCORRECT);
   }
   msg_header_ptr = msg_header;
   *msg_header_ptr = *read_ptr;
   *(msg_header_ptr + 1) = *(read_ptr + 1);
   msg_header_ptr += 2;

   /* Allocate memory for the wmo_buffer. */
   if ((*wmo_buffer = malloc((2 * *msg_length))) == NULL)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "afw2wmo(): Failed to malloc() memory : %s", strerror(errno));
      exit(INCORRECT);
   }
   write_ptr = *wmo_buffer;

   /* Write SOH and LF */
   *write_ptr       = SOH;
   *(write_ptr + 1) = '\n';

   /* Write TTDL */
   p_start_afw_header = read_ptr;
   if (((*read_ptr == 'X') || (*read_ptr == 'x')) &&
       ((*(read_ptr + 1) == 'X') || (*(read_ptr + 1) == 'x')) &&
       !((*(read_ptr + 2) == '3') && (*(read_ptr + 3) == '5')))
   {
      *(write_ptr + 2) = 'B';
      *(write_ptr + 3) = 'M';
   }
   else
   {
      *(write_ptr + 2) = toupper((int)(*read_ptr));
      *(write_ptr + 3) = toupper((int)(*(read_ptr + 1)));
   }
   if ((*(write_ptr + 2) == 'S') && (*(write_ptr + 3) == 'N') &&
       (*(read_ptr + 2) == '4') && (*(read_ptr + 3) == '0'))
   {
      *(write_ptr + 4) = 'X';
      *(write_ptr + 5) = 'X';
   }
   else
   {
      if ((*(write_ptr + 2 ) == 'B') && (*(write_ptr + 3) == 'M'))
      {
         *(write_ptr + 4) = 'B';
         *(write_ptr + 5) = 'B';
      }
      else
      {
         *(write_ptr + 4) = 'D';
         *(write_ptr + 5) = 'L';
      }
   }
   write_ptr += 6;

   /* Is it a correction? */
   if ((*(read_ptr + 2) == 'C') || (*(read_ptr + 2) == 'c'))
   {
      *(msg_header_ptr++) = *(read_ptr + 2);
      length_done += 3;
      read_ptr    += 3;
      correction = YES;
   }
   else
   {
      read_ptr += 2;

      /* Check for ii */
      if (isdigit((int)(*read_ptr)) && isdigit((int)(*(read_ptr + 1))))
      {
         *write_ptr = *read_ptr;
         *(write_ptr + 1) = *(read_ptr + 1);
         write_ptr += 2;
         *msg_header_ptr = *read_ptr;
         *(msg_header_ptr + 1) = *(read_ptr + 1);

         /* Again. Check if it's a correction! */
         if ((*(read_ptr + 2) == 'C') || (*(read_ptr + 2) == 'c'))
         {
            *(msg_header_ptr + 2) = *(read_ptr + 2);
            length_done    += 5;
            read_ptr       += 3;
            msg_header_ptr += 3;
            correction      = YES;
         }
         else
         {
            length_done    += 4;
            read_ptr       += 2;
            msg_header_ptr += 2;
         }
      }
      else
      {
         if (((*p_start_afw_header == 'S') || (*p_start_afw_header == 's')) &&
             ((*(p_start_afw_header + 1) == 'P') || (*(p_start_afw_header + 1) == 'p')))
         {
            tmp_write_ptr = write_ptr;
         }
         length_done += 2;
      }
   }

   /* Ignore any blank after TT[ii][C] */
   while ((*read_ptr <= ' ') && (length_done < *msg_length))
   {
      read_ptr++; length_done++;
   }
   *(msg_header_ptr++) = ' ';

   /* Now lets check YY of DTG (YYGGgg) */
   if ((*read_ptr < '0') || (*read_ptr > '3') ||
       (*(read_ptr + 1) < '0') || (*(read_ptr + 1) > '9') ||
       ((*read_ptr == '3') && (*(read_ptr + 1) > '1')))
   {
      show_error(read_ptr, "YY");
/*
      show_buffer(receive_log_fd, msg, *msg_length);
*/
      return(INCORRECT);
   }
   dtg_ptr = read_ptr;
   read_ptr += 2;

   /* Check GG of DTG (YYGGgg) */
   if ((*read_ptr < '0') || (*read_ptr > '2') ||
       (*(read_ptr + 1) < '0') || (*(read_ptr + 1) > '9') ||
       ((*read_ptr == '2') && (*(read_ptr + 1) > '3')))
   {
      show_error(read_ptr, "GG");
/*
      show_buffer(receive_log_fd, msg, *msg_length);
*/
      return(INCORRECT);
   }
   read_ptr += 2;

   /* Check gg of DTG (YYGGgg) */
   if ((*read_ptr < '0') || (*read_ptr > '5') ||
       (*(read_ptr + 1) < '0') || (*(read_ptr + 1) > '9'))
   {
      show_error(read_ptr, "gg");
/*
      show_buffer(receive_log_fd, msg, *msg_length);
*/
      return(INCORRECT);
   }
   length_done += 6;
   read_ptr    += 2;

   (void)memcpy(msg_header_ptr, dtg_ptr, 6);
   msg_header_ptr += 6;

   /* Ignore any blank after YYGGgg */
   while ((*read_ptr <= ' ') && (length_done < *msg_length))
   {
      read_ptr++; length_done++;
   }
   *(msg_header_ptr++) = ' ';

   *write_ptr = ' ';
   write_ptr++;

   /*
    * Either it is a numeric IIiii station indicator or a
    * an alphanumeric CCCC indicator.
    */
   if (isdigit((int)(*read_ptr)))
   {
      int cccc_gotcha = NO;

      if ((isdigit((int)(*(read_ptr + 1))) == 0) ||
          (isdigit((int)(*(read_ptr + 2))) == 0) ||
          (isdigit((int)(*(read_ptr + 3))) == 0) ||
          (isdigit((int)(*(read_ptr + 4))) == 0))
      {
         show_error(read_ptr, "IIiii");
/*
         show_buffer(receive_log_fd, msg, *msg_length);
*/
         return(INCORRECT);
      }
      (void)memcpy(msg_header_ptr, read_ptr, 5);
      msg_header_ptr += 5;

      if (((*p_start_afw_header == 'U') || (*p_start_afw_header == 'u')) &&
          ((*(p_start_afw_header + 1) == 'X') || (*(p_start_afw_header + 1) == 'x')) &&
          ((*(p_start_afw_header + 2) == '1') || (*(p_start_afw_header + 2) == '4')) &&
          (*(p_start_afw_header + 3) == '1') &&
          (*read_ptr == '1') && (*(read_ptr + 1) == '0'))
      {
         if ((*(read_ptr + 2) == '0') && (*(read_ptr + 3) == '3') &&
             (*(read_ptr + 4) == '5'))
         {
            *write_ptr = 'D'; *(write_ptr + 1) = 'W';
            *(write_ptr + 2) = 'S'; *(write_ptr + 3) = 'G';
            cccc_gotcha = YES;
         }
         else if ((*(read_ptr + 2) == '1') && (*(read_ptr + 3) == '8') &&
                  (*(read_ptr + 4) == '4'))
              {
                 *write_ptr = 'D'; *(write_ptr + 1) = 'W';
                 *(write_ptr + 2) = 'G'; *(write_ptr + 3) = 'W';
                 cccc_gotcha = YES;
              }
         else if ((*(read_ptr + 2) == '2') && (*(read_ptr + 3) == '0') &&
                  (*(read_ptr + 4) == '0'))
              {
                 *write_ptr = 'D'; *(write_ptr + 1) = 'W';
                 *(write_ptr + 2) = 'E'; *(write_ptr + 3) = 'D';
                 cccc_gotcha = YES;
              }
         else if ((*(read_ptr + 2) == '3') && (*(read_ptr + 3) == '9') &&
                  (*(read_ptr + 4) == '3'))
              {
                 *write_ptr = 'D'; *(write_ptr + 1) = 'W';
                 *(write_ptr + 2) = 'L'; *(write_ptr + 3) = 'G';
                 cccc_gotcha = YES;
              }
         else if ((*(read_ptr + 2) == '4') && (*(read_ptr + 3) == '1') &&
                  (*(read_ptr + 4) == '0'))
              {
                 *write_ptr = 'D'; *(write_ptr + 1) = 'W';
                 *(write_ptr + 2) = 'E'; *(write_ptr + 3) = 'M';
                 cccc_gotcha = YES;
              }
         else if ((*(read_ptr + 2) == '4') && (*(read_ptr + 3) == '8') &&
                  (*(read_ptr + 4) == '6'))
              {
                 *write_ptr = 'D'; *(write_ptr + 1) = 'W';
                 *(write_ptr + 2) = 'D'; *(write_ptr + 3) = 'R';
                 cccc_gotcha = YES;
              }
         else if ((*(read_ptr + 2) == '5') && (*(read_ptr + 3) == '4') &&
                  (*(read_ptr + 4) == '8'))
              {
                 *write_ptr = 'D'; *(write_ptr + 1) = 'W';
                 *(write_ptr + 2) = 'M'; *(write_ptr + 3) = 'E';
                 cccc_gotcha = YES;
              }
         else if ((*(read_ptr + 2) == '7') && (*(read_ptr + 3) == '3') &&
                  (*(read_ptr + 4) == '9'))
              {
                 *write_ptr = 'D'; *(write_ptr + 1) = 'W';
                 *(write_ptr + 2) = 'S'; *(write_ptr + 3) = 'U';
                 cccc_gotcha = YES;
              }
         else if ((*(read_ptr + 2) == '8') && (*(read_ptr + 3) == '6') &&
                  (*(read_ptr + 4) == '8'))
              {
                 *write_ptr = 'D'; *(write_ptr + 1) = 'W';
                 *(write_ptr + 2) = 'M'; *(write_ptr + 3) = 'O';
                 cccc_gotcha = YES;
              }
      }
      if (cccc_gotcha == NO)
      {
         (void)memcpy(write_ptr, AFW_IDENTIFIER_SPECIAL, AFW_IDENTIFIER_LENGTH);
      }
      else
      {
         /* Need to convert UXDL11 to UXDL41 */
         if (*(p_start_afw_header + 2) == '1')
         {
            *(write_ptr - 3) = '4';
         }
      }

      iiiii_ptr    = read_ptr;
      write_ptr   += AFW_IDENTIFIER_LENGTH;
      read_ptr    += 5;

      /* Handle special case when we receive SP[C] YYGGgg 10?? */
      if ((tmp_write_ptr != NULL) && (*(read_ptr - 5) == '1') &&
          (*(read_ptr - 4) == '0'))
      {
         (void)memmove(tmp_write_ptr + 2, tmp_write_ptr, write_ptr - tmp_write_ptr);
         *tmp_write_ptr = '4';
         *(tmp_write_ptr + 1) = '1';
         write_ptr += 2;
      }
   }
   else if (isalpha((int)(*read_ptr)))
        {
           int is_slash_slash = NO;

           if ((isalpha((int)(*(read_ptr + 1))) == 0) ||
               (isalpha((int)(*(read_ptr + 2))) == 0) ||
               (isalpha((int)(*(read_ptr + 3))) == 0))
           {
              if (((*read_ptr == 'D') || (*read_ptr == 'd')) &&
                  ((*(read_ptr + 1) == 'W') || (*(read_ptr + 1) == 'w')) &&
                  (*(read_ptr + 2) == '/') && (*(read_ptr + 3) == '/'))
              {
                 is_slash_slash = YES;
              }
              else
              {
                 receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                             "afw2wmo(): Error in message %s", msg_name);
                 show_error(read_ptr, "CCCC");
/*
                 show_buffer(receive_log_fd, msg, *msg_length);
*/
                 return(INCORRECT);
              }
           }
           (void)memcpy(msg_header_ptr, read_ptr, 4);
           msg_header_ptr += 4;
           p_start_ansi = write_ptr;
           if (is_slash_slash == YES)
           {
              *write_ptr = 'D';
              *(write_ptr + 1) = 'W';
              *(write_ptr + 2) = 'W';
              *(write_ptr + 3) = 'W';
           }
           else
           {
              *write_ptr = toupper((int)(*read_ptr));
              *(write_ptr + 1) = toupper((int)(*(read_ptr + 1)));
              *(write_ptr + 2) = toupper((int)(*(read_ptr + 2)));
              *(write_ptr + 3) = toupper((int)(*(read_ptr + 3)));
           }
           write_ptr   += 4;
           read_ptr    += 4;

           /* Handle special case when we receive SP[C] YYGGgg ED?? */
           if ((tmp_write_ptr != NULL) &&
               ((*(read_ptr - 4) == 'E') || (*(read_ptr - 4) == 'e')) &&
               ((*(read_ptr - 3) == 'D') || (*(read_ptr - 3) == 'd')))
           {
              (void)memmove(tmp_write_ptr + 2, tmp_write_ptr, write_ptr - tmp_write_ptr);
              *tmp_write_ptr = '4';
              *(tmp_write_ptr + 1) = '0';
              write_ptr += 2;
           }
        }
        else
        {
           /* Impossible! Should never come here. */
           receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                       "afw2wmo(): Hmmm. This should never happen!");
           return(INCORRECT);
        }

   /* No lets write YYGGgg to buffer */
   *write_ptr = ' ';
   (void)memcpy((write_ptr + 1), dtg_ptr, 6);
   write_ptr += 7;

   /* Is this message a correction */
   if (correction == YES)
   {
      (void)memcpy(write_ptr, COR_IDENTIFIER, COR_IDENTIFIER_LENGTH);
      write_ptr += COR_IDENTIFIER_LENGTH;
   }

   /* Write LF */
   *write_ptr = '\n';
   write_ptr++;

   /* Write numeric station indicator */
   if (iiiii_ptr != NULL)
   {
      /* if (((*p_start_afw_header != 'U') && (*p_start_afw_header != 'u')) && */
      /* Do not write nummeric station indicator, when its an */
      /* upper air bulletin. Since it is already contained in */
      /* the message itself.                                  */
      if (((*p_start_afw_header == 'U') || (*p_start_afw_header == 'u')) ||
          (((*p_start_afw_header == 'C') || (*p_start_afw_header == 'c')) &&
           ((*(p_start_afw_header + 1) == 'S') ||
            (*(p_start_afw_header + 1) == 's'))) ||
          (((*p_start_afw_header == 'S') || (*p_start_afw_header == 's')) &&
           ((*(p_start_afw_header + 1) == 'N') || (*(p_start_afw_header + 1) == 'n')) &&
           (*(p_start_afw_header + 2) == '4') && (*(p_start_afw_header + 3) == '0')))
      {
         /* Do NOT write numeric station indicator! */;
      }
      else
      {
         (void)memcpy(write_ptr, iiiii_ptr, 5);
         *(write_ptr + 5) = ' ';
         write_ptr += 6;
      }

      /* Ignore any <CR> and <LF> at begining of message */
      while ((*read_ptr <= ' ') && (length_done < *msg_length))
      {
         read_ptr++; length_done++;
      }
   }
   else if ((p_start_ansi != NULL) &&
            ((((*p_start_afw_header == 'S') || (*p_start_afw_header == 's')) &&
              ((*(p_start_afw_header + 1) == 'A') || (*(p_start_afw_header + 1) == 'a')) &&
              (*p_start_ansi == 'E') &&
              (*(p_start_ansi + 1) == 'D')) ||
             (((*p_start_afw_header == 'S') || (*p_start_afw_header == 's')) &&
              ((*(p_start_afw_header + 1) == 'H') || (*(p_start_afw_header + 1) == 'h')) &&
              (*(p_start_afw_header + 2) == ' ')) ||
             (((*p_start_afw_header == 'X') || (*p_start_afw_header == 'x')) &&
              ((*(p_start_afw_header + 1) == 'X') || (*(p_start_afw_header + 1) == 'x')) &&
              (*(p_start_afw_header + 2) == '0') &&
              ((*(p_start_afw_header + 3) == '1') || (*(p_start_afw_header + 3) == '2')))))
        {
            (void)memcpy(write_ptr, p_start_ansi, 4);
            *(write_ptr + 4) = ' ';
            write_ptr += 5;

            /* Overwrite CCCC with AFW_IDENTIFIER_SPECIAL */
            (void)memcpy(p_start_ansi, AFW_IDENTIFIER_SPECIAL, AFW_IDENTIFIER_LENGTH);

            /* Ignore any <CR> and <LF> at begining of message */
            while ((*read_ptr <= ' ') && (length_done < *msg_length))
            {
               read_ptr++; length_done++;
            }
        }
   else if ((p_start_ansi != NULL) &&
            ((*p_start_afw_header == 'W') || (*p_start_afw_header == 'w')) &&
            ((*(p_start_afw_header + 1) == 'O') || (*(p_start_afw_header + 1) == 'o')) &&
            (*(p_start_afw_header + 2) == '5') &&
            (*(p_start_afw_header + 3) == '0') &&
            (*p_start_ansi == 'D') &&
            (*(p_start_ansi + 1) == 'W'))
        {
            *p_start_ansi = 'E';
            *(p_start_ansi + 1) = 'D';
            *(p_start_ansi + 2) = 'Z';
            *(p_start_ansi + 3) = 'W';
        }

   /* AAaaaahhhhh. Now we may write the message! */
   *msg_length = *msg_length + garbage - (read_ptr - msg);
   if (*msg_length < 1)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "afw2wmo(): Premature end of message %s!", msg_name);
/*
      show_buffer(receive_log_fd, msg, read_ptr - msg);
*/
      return(INCORRECT);
   }
   (void)memcpy(write_ptr, read_ptr, *msg_length);
   write_ptr += *msg_length;

   if (*(write_ptr - 1) != ETX)
   {
      /* Don't forget the LF and ETX */
      *write_ptr = '\n';
      *(write_ptr + 1) = ETX;
      write_ptr += 2;
   }
   *msg_length = write_ptr - *wmo_buffer;

   /* Tell user we have converted a message. */
   *msg_header_ptr = '\0';
   receive_log(INFO_SIGN, __FILE__, __LINE__, 0L,
               "Converted %s [%d]", msg_header, *msg_length);
#endif /* _WITH_AFW2WMO */

   return(SUCCESS);
}


#ifdef _WITH_AFW2WMO
/*+++++++++++++++++++++++++++++ show_error() ++++++++++++++++++++++++++++*/
static void
show_error(char *is, char *expect)
{
   int    i,
          buf_length = 0;
   size_t length,
          show_length;
   char   *tmp_buf;

   show_length = strlen(expect);
   length = (MAX_INT_LENGTH + 2) * show_length;
   if ((tmp_buf = malloc(length + 1)) == NULL)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, 0L,
                  "afw2wmo(): malloc() error : %s", strerror(errno));
      exit(INCORRECT);
   }

   for (i = 0; i < show_length; i++)
   {
      if (is[i] < ' ')
      {
         buf_length += snprintf(&tmp_buf[buf_length], length + 1 - buf_length,
                                "<%d>", (int)is[i]);
      }
      else
      {
         tmp_buf[buf_length] = is[i];
         buf_length++;
      }
   }
   tmp_buf[buf_length] = '\0';
   receive_log(WARN_SIGN, __FILE__, __LINE__, 0L,
               "afw2wmo(): Received <%s> instead of <%s>", tmp_buf, expect);

   free(tmp_buf);

   return;
}
#endif /* _WITH_AFW2WMO */
