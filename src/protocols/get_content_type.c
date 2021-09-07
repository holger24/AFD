/*
 *  get_content_type.c - Part of AFD, an automatic file distribution
 *                       program.
 *  Copyright (c) 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_content_type - tries to determine content-type from filename
 **                      end
 **
 ** SYNOPSIS
 **   void get_content_type(char *filename,
 **                         char *content_type,
 **                         int is_attachment)
 **
 ** DESCRIPTION
 **   Tries to determine the content-type from filename extension.
 **
 ** RETURN VALUES
 **   Returns content type in variable content_type. If the extension
 **   is unknown it will insert APPLICATION/octet-stream if is_attachment
 **   is YES, otherwise TEXT/plain,
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.08.2021 H.Kiehl Cut out from smtpcmd.c
 */
DESCR__E_M3

#include <string.h>      /* strcpy()                                     */
#include "commondefs.h"


/*######################### get_content_type() ##########################*/
void
get_content_type(char *filename, char *content_type, int is_attachment)
{
   content_type[0] = '\0';
   if (filename != NULL)
   {
      char *ptr;

      ptr = filename + strlen(filename);
      while ((ptr > filename) && (*ptr != '.'))
      {
         ptr--;
      }
      if ((*ptr == '.') && (*(ptr + 1) != '\0'))
      {
         ptr++;
         if (((*ptr == 'p') || (*ptr == 'P')) &&
             ((*(ptr + 1) == 'n') || (*(ptr + 1) == 'N')) &&
             ((*(ptr + 2) == 'g') || (*(ptr + 2) == 'G')) &&
             (*(ptr + 3) == '\0'))
         {
            (void)strcpy(content_type, "IMAGE/png");
         }
         else if (((*ptr == 'j') || (*ptr == 'J')) &&
                  ((((*(ptr + 1) == 'p') || (*(ptr + 1) == 'P')) &&
                    (((*(ptr + 2) == 'g') || (*(ptr + 2) == 'G')) ||
                     ((*(ptr + 2) == 'e') || (*(ptr + 2) == 'E'))) &&
                    (*(ptr + 3) == '\0')) ||
                   (((*(ptr + 1) == 'e') || (*(ptr + 1) == 'E')) &&
                    ((*(ptr + 2) == 'p') || (*(ptr + 2) == 'P')) &&
                    ((*(ptr + 3) == 'g') || (*(ptr + 3) == 'G')) &&
                    (*(ptr + 4) == '\0'))))
              {
                 (void)strcpy(content_type, "IMAGE/jpeg");
              }
         else if (((*ptr == 't') || (*ptr == 'T')) &&
                  ((*(ptr + 1) == 'i') || (*(ptr + 1) == 'I')) &&
                  ((*(ptr + 2) == 'f') || (*(ptr + 2) == 'F')) &&
                  ((((*(ptr + 3) == 'f') || (*(ptr + 3) == 'F')) &&
                   (*(ptr + 4) == '\0')) ||
                   (*(ptr + 3) == '\0')))
              {
                 (void)strcpy(content_type, "IMAGE/tiff");
              }
         else if (((*ptr == 'g') || (*ptr == 'G')) &&
                  ((*(ptr + 1) == 'i') || (*(ptr + 1) == 'I')) &&
                  ((*(ptr + 2) == 'f') || (*(ptr + 2) == 'F')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "IMAGE/gif");
              }
         else if (((*ptr == 'j') || (*ptr == 'J')) &&
                  ((*(ptr + 1) == 's') || (*(ptr + 1) == 'S')) &&
                  (*(ptr + 2) == '\0'))
              {
                 (void)strcpy(content_type, "APPLICATION/javascript");
              }
         else if (((*ptr == 'm') || (*ptr == 'M')) &&
                  ((*(ptr + 1) == 'p') || (*(ptr + 1) == 'P')) &&
                  (*(ptr + 2) == '4') && (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "APPLICATION/mp4");
              }
         else if (((*ptr == 'd') || (*ptr == 'D')) &&
                  ((*(ptr + 1) == 'o') || (*(ptr + 1) == 'O')) &&
                  ((*(ptr + 2) == 'c') || (*(ptr + 2) == 'C')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "APPLICATION/msword");
              }
         else if (((*ptr == 'p') || (*ptr == 'P')) &&
                  ((*(ptr + 1) == 'd') || (*(ptr + 1) == 'D')) &&
                  ((*(ptr + 2) == 'f') || (*(ptr + 2) == 'F')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "APPLICATION/pdf");
              }
         else if (((*ptr == 'e') || (*ptr == 'E')) &&
                  ((*(ptr + 1) == 'p') || (*(ptr + 1) == 'P')) &&
                  ((*(ptr + 2) == 's') || (*(ptr + 2) == 'S')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "APPLICATION/postscript");
              }
         else if (((*ptr == 'x') || (*ptr == 'X')) &&
                  ((*(ptr + 1) == 'l') || (*(ptr + 1) == 'L')) &&
                  ((*(ptr + 2) == 's') || (*(ptr + 2) == 'S')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "APPLICATION/vnd.ms-excel");
              }
         else if (((*ptr == 'p') || (*ptr == 'P')) &&
                  ((*(ptr + 1) == 'p') || (*(ptr + 1) == 'P')) &&
                  ((*(ptr + 2) == 't') || (*(ptr + 2) == 'T')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "APPLICATION/vnd.ms-powerpoint");
              }
         else if (((*ptr == 'b') || (*ptr == 'B')) &&
                  ((*(ptr + 1) == 'z') || (*(ptr + 1) == 'Z')) &&
                  (*(ptr + 2) == '2') && (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "APPLICATION/x-bzip2");
              }
         else if ((((*ptr == 'g') || (*ptr == 'G')) &&
                   ((*(ptr + 1) == 'z') || (*(ptr + 1) == 'Z')) &&
                   (*(ptr + 2) == '\0')) ||
                  (((*ptr == 't') || (*ptr == 'T')) &&
                   ((*(ptr + 1) == 'g') || (*(ptr + 1) == 'G')) &&
                   ((*(ptr + 2) == 'z') || (*(ptr + 2) == 'Z')) &&
                   (*(ptr + 3) == '\0')))
              {
                 (void)strcpy(content_type, "APPLICATION/x-gzip");
              }
         else if (((*ptr == 's') || (*ptr == 'S')) &&
                  ((*(ptr + 1) == 'h') || (*(ptr + 1) == 'H')) &&
                  (*(ptr + 2) == '\0'))
              {
                 (void)strcpy(content_type, "APPLICATION/x-sh");
              }
         else if (((*ptr == 't') || (*ptr == 'T')) &&
                  ((*(ptr + 1) == 'a') || (*(ptr + 1) == 'A')) &&
                  ((*(ptr + 2) == 'r') || (*(ptr + 2) == 'R')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "APPLICATION/x-tar");
              }
         else if (((*ptr == 'z') || (*ptr == 'Z')) &&
                  ((*(ptr + 1) == 'i') || (*(ptr + 1) == 'I')) &&
                  ((*(ptr + 2) == 'p') || (*(ptr + 2) == 'P')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "APPLICATION/zip");
              }
         else if (((*ptr == 'm') || (*ptr == 'M')) &&
                  ((*(ptr + 1) == 'p') || (*(ptr + 1) == 'P')) &&
                  ((((*(ptr + 2) == 'g') || (*(ptr + 2) == 'G')) &&
                    ((*(ptr + 3) == 'a') || (*(ptr + 3) == 'A')) &&
                    (*(ptr + 4) == '\0')) ||
                   (((*(ptr + 2) == '2') || (*(ptr + 2) == '3')) &&
                    (*(ptr + 4) == '\0'))))
              {
                 (void)strcpy(content_type, "VIDEO/mpeg");
              }
         else if ((((*ptr == 'm') || (*ptr == 'M')) &&
                   ((*(ptr + 1) == 'o') || (*(ptr + 1) == 'O')) &&
                   ((*(ptr + 2) == 'v') || (*(ptr + 2) == 'V')) &&
                   (*(ptr + 3) == '\0')) ||
                  (((*ptr == 'q') || (*ptr == 'Q')) &&
                   ((*(ptr + 1) == 't') || (*(ptr + 1) == 'T')) &&
                   (*(ptr + 2) == '\0')))
              {
                 (void)strcpy(content_type, "VIDEO/quicktime");
              }
         else if (((((*ptr == 'a') || (*ptr == 'A')) &&
                    ((*(ptr + 1) == 's') || (*(ptr + 1) == 'S')) &&
                    ((*(ptr + 2) == 'c') || (*(ptr + 2) == 'C'))) ||
                   (((*ptr == 't') || (*ptr == 'T')) &&
                    ((*(ptr + 1) == 'x') || (*(ptr + 1) == 'X')) &&
                    ((*(ptr + 2) == 't') || (*(ptr + 2) == 'T')))) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "TEXT/plain");
              }
         else if (((*ptr == 'c') || (*ptr == 'C')) &&
                  ((*(ptr + 1) == 's') || (*(ptr + 1) == 'S')) &&
                  ((*(ptr + 2) == 'v') || (*(ptr + 2) == 'V')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "TEXT/csv");
              }
         else if (((*ptr == 'c') || (*ptr == 'C')) &&
                  ((*(ptr + 1) == 's') || (*(ptr + 1) == 'S')) &&
                  ((*(ptr + 2) == 's') || (*(ptr + 2) == 'S')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "TEXT/css");
              }
         else if (((*ptr == 'r') || (*ptr == 'R')) &&
                  ((*(ptr + 1) == 't') || (*(ptr + 1) == 'T')) &&
                  ((*(ptr + 2) == 'x') || (*(ptr + 2) == 'X')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "TEXT/richtext");
              }
         else if (((*ptr == 'r') || (*ptr == 'R')) &&
                  ((*(ptr + 1) == 't') || (*(ptr + 1) == 'T')) &&
                  ((*(ptr + 2) == 'f') || (*(ptr + 2) == 'F')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "TEXT/rtf");
              }
         else if (((*ptr == 'x') || (*ptr == 'X')) &&
                  ((*(ptr + 1) == 'm') || (*(ptr + 1) == 'M')) &&
                  ((*(ptr + 2) == 'l') || (*(ptr + 2) == 'L')) &&
                  (*(ptr + 3) == '\0'))
              {
                 (void)strcpy(content_type, "TEXT/xml");
              }
         else if (((*ptr == 'h') || (*ptr == 'H')) &&
                  ((*(ptr + 1) == 't') || (*(ptr + 1) == 'T')) &&
                  ((*(ptr + 2) == 'm') || (*(ptr + 2) == 'M')) &&
                  ((((*(ptr + 3) == 'l') || (*(ptr + 3) == 'L')) &&
                   (*(ptr + 4) == '\0')) ||
                   (*(ptr + 3) == '\0')))
              {
                 (void)strcpy(content_type, "TEXT/html");
              }
      }
   }
   if (content_type[0] == '\0')
   {
      if (is_attachment == YES)
      {
         (void)strcpy(content_type, "APPLICATION/octet-stream");
      }
      else
      {
         /*
          * Maybe it is better to insert "TEXT/plain" since this
          * is what mail servers do if they do not get the Content-Type
          * from the client.
          */
         (void)strcpy(content_type, "TEXT/plain");
      }
   }
   return;
}
