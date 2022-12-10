/*
 *  eval_bul_rep_config.c - Part of AFD, an automatic file distribution
 *                          program.
 *  Copyright (c) 2011 - 2022 Deutscher Wetterdienst (DWD),
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
 **   eval_bul_rep_config - 
 **
 ** SYNOPSIS
 **   void eval_bul_rep_config(void)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.03.2011 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>             /* fprintf()                              */
#include <string.h>            /* atoi(), strerror()                     */
#include <stdlib.h>            /* malloc(), calloc(), free()             */
#include <ctype.h>             /* isdigit()                              */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <unistd.h>            /* read()                                 */
#include <errno.h>
#include "amgdefs.h"

/* #define _DEBUG_BUL_REP */

/* External global variables. */
extern int                 no_of_bc_entries,
                           no_of_rc_entries;
extern struct wmo_bul_list *bcdb; /* Bulletin Configuration Database */
extern struct wmo_rep_list *rcdb; /* Report Configuration Database */

/* Local function prototypes. */
static void                get_report_data(char *);


/*######################### eval_bul_rep_config() #######################*/
void
eval_bul_rep_config(char *bul_file, char *rep_file, int verbose)
{
   static time_t last_read_bul = 0,
                 last_read_rep = 0;
   static int    first_time = YES;
#ifdef HAVE_STATX
   struct statx  stat_buf;

   if (statx(0, bul_file, AT_STATX_SYNC_AS_STAT,
             STATX_SIZE | STATX_MTIME, &stat_buf) == -1)
#else
   struct stat   stat_buf;

   if (stat(bul_file, &stat_buf) == -1)
#endif
   {
      if (errno == ENOENT)
      {
         /*
          * Only tell user once that the message specification file is
          * missing. Otherwise it is anoying to constantly receive this
          * message.
          */
         if (first_time == YES)
         {
            system_log(INFO_SIGN, __FILE__, __LINE__,
                       _("There is no message specification file `%s'"),
                       bul_file);
            first_time = NO;
         }
      }
      else
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
#ifdef HAVE_STATX
                    _("Failed to statx() `%s' : %s"),
#else
                    _("Failed to stat() `%s' : %s"),
#endif
                    bul_file, strerror(errno));
      }
   }
   else
   {
      char         *rep_buffer = NULL,
                   *rep_header_end;
#ifdef HAVE_STATX
      struct statx stat_buf2;

      if (statx(0, rep_file, AT_STATX_SYNC_AS_STAT,
                STATX_SIZE | STATX_MTIME, &stat_buf2) == -1)
#else
      struct stat  stat_buf2;

      if (stat(rep_file, &stat_buf2) == -1)
#endif
      {
         if (errno != ENOENT)
         {
            system_log(INFO_SIGN, __FILE__, __LINE__,
                       _("There is no report specification file `%s'"),
                       rep_file);
         }
      }
      else
      {
#ifdef HAVE_STATX
         if (stat_buf2.stx_mtime.tv_sec != last_read_rep)
#else
         if (stat_buf2.st_mtime != last_read_rep)
#endif
         {
            int fd;

            /* Allocate memory to store file. */
#ifdef HAVE_STATX
            if ((rep_buffer = malloc(stat_buf2.stx_size + 1)) == NULL)
#else
            if ((rep_buffer = malloc(stat_buf2.st_size + 1)) == NULL)
#endif
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("malloc() error : %s"), strerror(errno));
               exit(INCORRECT);
            }

            /* Open file. */
            if ((fd = open(rep_file, O_RDONLY)) == -1)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to open() `%s' : %s"),
                          rep_file, strerror(errno));
               free(rep_buffer);
               exit(INCORRECT);
            }

            /* Read file into buffer. */
#ifdef HAVE_STATX
            if (read(fd, &rep_buffer[0], stat_buf2.stx_size) != stat_buf2.stx_size)
#else
            if (read(fd, &rep_buffer[0], stat_buf2.st_size) != stat_buf2.st_size)
#endif
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("Failed to read() `%s' : %s"),
                          rep_file, strerror(errno));
               free(rep_buffer);
               (void)close(fd);
               exit(INCORRECT);
            }
            if (close(fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("close() error : %s"), strerror(errno));
            }
#ifdef HAVE_STATX
            rep_buffer[stat_buf2.stx_size] = '\0';
#else
            rep_buffer[stat_buf2.st_size] = '\0';
#endif
            rep_header_end = rep_buffer;
            while ((*rep_header_end != '\0') &&
                   (*rep_header_end != '\n') &&
                   (*rep_header_end != '\r'))
            {
               rep_header_end++;
            }
            while ((*rep_header_end == '\n') ||
                   (*rep_header_end == '\r'))
            {
               rep_header_end++;
            }
            get_report_data(rep_header_end);

            if (verbose == YES)
            {
               system_log(INFO_SIGN, NULL, 0,
                          _("Found %d report rules in `%s'."),
                          no_of_rc_entries, rep_file);
            }
#ifdef HAVE_STATX
            last_read_rep = stat_buf2.stx_mtime.tv_sec;
#else
            last_read_rep = stat_buf2.st_mtime;
#endif
         } /* if (stat_buf2.st_mtime != last_read_rep) */
      }

#ifdef HAVE_STATX
      if (stat_buf.stx_mtime.tv_sec != last_read_bul)
#else
      if (stat_buf.st_mtime != last_read_bul)
#endif
      {
         register int i;
         int          fd;
         char         *buffer,
                      *ptr;
#ifdef _DEBUG_BUL_REP
         FILE         *p_bul_rep_debug_file;
#endif

         if (first_time == YES)
         {
            first_time = NO;
         }
         else
         {
            if (verbose == YES)
            {
               system_log(INFO_SIGN, NULL, 0,
                          _("Rereading message specification file."));
            }
         }

         if (last_read_bul != 0)
         {
            /*
             * Since we are rereading the whole message specification file
             * again lets release the memory we stored for the previous
             * structure of wmo_bul_rep.
             */
            free(bcdb);
            no_of_bc_entries = 0;
         }

         /* Allocate memory to store file. */
#ifdef HAVE_STATX
         if ((buffer = malloc(stat_buf.stx_size + 1)) == NULL)
#else
         if ((buffer = malloc(stat_buf.st_size + 1)) == NULL)
#endif
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("malloc() error : %s"), strerror(errno));
            free(rep_buffer);
            exit(INCORRECT);
         }

         /* Open file. */
         if ((fd = open(bul_file, O_RDONLY)) == -1)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' : %s"),
                       bul_file, strerror(errno));
            free(buffer);
            free(rep_buffer);
            exit(INCORRECT);
         }

         /* Read file into buffer. */
#ifdef HAVE_STATX
         if (read(fd, &buffer[0], stat_buf.stx_size) != stat_buf.stx_size)
#else
         if (read(fd, &buffer[0], stat_buf.st_size) != stat_buf.st_size)
#endif
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       _("Failed to read() `%s' : %s"),
                       bul_file, strerror(errno));
            free(buffer);
            free(rep_buffer);
            (void)close(fd);
            exit(INCORRECT);
         }
         if (close(fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("close() error : %s"), strerror(errno));
         }
#ifdef HAVE_STATX
         buffer[stat_buf.stx_size] = '\0';
         last_read_bul = stat_buf.stx_mtime.tv_sec;
#else
         buffer[stat_buf.st_size] = '\0';
         last_read_bul = stat_buf.st_mtime;
#endif

         /*
          * Now that we have the contents in the buffer lets first see
          * how many entries there are in the buffer so we can allocate
          * memory for the entries.
          */
         ptr = buffer;
         while (*ptr != '\0')
         {
            while ((*ptr != '\n') && (*ptr != '\r'))
            {
               ptr++;
            }
            if ((*ptr == '\n') || (*ptr == '\r'))
            {
               no_of_bc_entries++;
               ptr++;
            }
            while ((*ptr == '\n') || (*ptr == '\r'))
            {
               ptr++;
            }
         }

         if (no_of_bc_entries > 0)
         {
            register int j, k;

            if ((bcdb = calloc(no_of_bc_entries,
                               sizeof(struct wmo_bul_list))) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          _("calloc() error : %s (%s %d)\n"), strerror(errno));
               free(rep_buffer);
               exit(INCORRECT);
            }
            ptr = buffer;

            for (i = 0; i < no_of_bc_entries; i++)
            {
               while ((*ptr == ' ') || (*ptr == '\t'))
               {
                  ptr++;
               }
               k = 0;
               while ((k < 6) && (*ptr != ';') && (*ptr != '\n') &&
                      (*ptr != '\0') && (*ptr != '\r'))
               {
                  bcdb[i].TTAAii[k] = *ptr;
                  k++; ptr++;
               }
               bcdb[i].TTAAii[k] = '\0';
               if (*ptr != ';')
               {
                  while ((*ptr != ';') && (*ptr != '\n') &&
                         (*ptr != '\0') && (*ptr != '\r'))
                  {
                     ptr++;
                  }
               }
               if (*ptr == ';')
               {
                  ptr++;
                  k = 0;
                  while ((k < 4) && (*ptr != ';') && (*ptr != '\n') &&
                         (*ptr != '\0') && (*ptr != '\r'))
                  {
                     bcdb[i].CCCC[k] = *ptr;
                     k++; ptr++;
                  }
                  bcdb[i].CCCC[k] = '\0';
                  if (*ptr != ';')
                  {
                     while ((*ptr != ';') && (*ptr != '\n') &&
                            (*ptr != '\0') && (*ptr != '\r'))
                     {
                        ptr++;
                     }
                  }
                  if (*ptr == ';')
                  {
                     ptr++;
                     if ((*ptr == 'i') && (*(ptr + 1) == 'n') &&
                         (*(ptr + 2) == 'p') && (*(ptr + 3) == ';'))
                     {
                        bcdb[i].type = BUL_TYPE_INP;
                        ptr += 3;
                     }
                     else if ((*ptr == 'i') && (*(ptr + 1) == 'g') &&
                              (*(ptr + 2) == 'n') && (*(ptr + 3) == ';'))
                          {
                             bcdb[i].type = BUL_TYPE_IGN;
                             ptr += 3;
                          }
                     else if ((*ptr == 'c') && (*(ptr + 1) == 'm') &&
                              (*(ptr + 2) == 'p') && (*(ptr + 3) == ';'))
                          {
                             bcdb[i].type = BUL_TYPE_CMP;
                             ptr += 3;
                          }
                          else
                          {
                             while ((*ptr != ';') && (*ptr != '\n') &&
                                    (*ptr != '\0') && (*ptr != '\r'))
                             {
                                ptr++;
                             }
                             bcdb[i].type = 0;
                          }

                     if (*ptr != ';')
                     {
                        while ((*ptr != ';') && (*ptr != '\n') &&
                               (*ptr != '\0') && (*ptr != '\r'))
                        {
                           ptr++;
                        }
                     }
                     if (*ptr == ';')
                     {
                        ptr++;
                        if (*ptr == 'D')
                        {
                           bcdb[i].spec = BUL_SPEC_DUP;
                           ptr++;
                        }
                        else
                        {
                           bcdb[i].spec = 0;
                        }
                        while ((*ptr != ';') && (*ptr != '\n') &&
                               (*ptr != '\0') && (*ptr != '\r'))
                        {
                           ptr++;
                        }

                        if (*ptr == ';')
                        {
                           ptr++;

                           /* Store BTIME. */
                           j = 0;
                           while ((*ptr != ';') && (*ptr != '\n') &&
                                  (*ptr != '\0') && (*ptr != '\r') &&
                                  (j < 8))
                           {
                              bcdb[i].BTIME[j] = *ptr;
                              ptr++; j++;
                           }
                           bcdb[i].BTIME[j] = '\0';

                           if (*ptr == ';')
                           {
                              ptr++;

                              /* Store ITIME */
                              j = 0;
                              while ((*ptr != ';') && (*ptr != '\n') &&
                                     (*ptr != '\0') && (*ptr != '\r') &&
                                     (j < 8))
                              {
                                 bcdb[i].ITIME[j] = *ptr;
                                 ptr++; j++;
                              }
                              bcdb[i].ITIME[j] = '\0';

                              if (*ptr == ';')
                              {
                                 ptr++;

                                 if ((*ptr == 'Y') && (*(ptr + 1) == ';') &&
                                     (isdigit((int)(*(ptr + 2)))))
                                 {
                                    char str_number[MAX_SHORT_LENGTH];

                                    str_number[0] = *(ptr + 2);
                                    ptr += 3;
                                    k = 1;
                                    while ((isdigit((int)(*ptr))) &&
                                           (k < (MAX_SHORT_LENGTH - 1)))
                                    {
                                       str_number[k] = *ptr;
                                       ptr++; k++;
                                    }
                                    str_number[k] = '\0';
                                    bcdb[i].rss = (unsigned short)atoi(str_number);
                                 }
                                 else
                                 {
                                    bcdb[i].rss = -1;
                                 }
                              }
                              else
                              {
                                 bcdb[i].rss = -1;
                              }
                           }
                           else
                           {
                              bcdb[i].rss = -1;
                           }
                        }
                        else
                        {
                           bcdb[i].rss = -1;
                        }
                     }
                     else
                     {
                        bcdb[i].spec = 0;
                        bcdb[i].rss = -1;
                     }
                  }
                  else
                  {
                     bcdb[i].type = 0;
                     bcdb[i].spec = 0;
                     bcdb[i].rss = -1;
                  }
               }
               else
               {
                  bcdb[i].CCCC[0] = '\0';
                  bcdb[i].type = 0;
                  bcdb[i].spec = 0;
                  bcdb[i].rss = -1;
               }
               while ((*ptr != '\n') && (*ptr != '\0') && (*ptr != '\r'))
               {
                  ptr++;
               }
               while ((*ptr == '\n') || (*ptr == '\r'))
               {
                  ptr++;
               }
            } /* for (i = 0; i < no_of_bc_entries; i++) */

            if (verbose == YES)
            {
               system_log(INFO_SIGN, NULL, 0,
                          _("Found %d bulletin rules in `%s'."),
                          no_of_bc_entries, bul_file);
            }
         } /* if (no_of_bc_entries > 0) */
         else
         {
            if (verbose == YES)
            {
               system_log(INFO_SIGN, NULL, 0,
                          _("No bulletin rules found in `%s'"), bul_file);
            }
         }

         /* The buffer holding the contents of the bulletin */
         /* file is no longer needed.                       */
         free(buffer);

#ifdef _DEBUG_BUL_REP
         if ((p_bul_rep_debug_file = fopen("bul_rep.debug", "w")) == NULL)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Could not fopen() bul_rep.debug : %s",
                       strerror(errno));
         }
         else
         {
            char *rtl[] =
                  {
                     "NOT_DEFINED",
                     "TEXT",
                     "ATEXT",
                     "CLIMAT",
                     "TAF",
                     "METAR",
                     "SPECIAL_01",
                     "SPECIAL_02",
                     "SPECIAL_03",
                     "SPECIAL_66",
                     "SYNOP",
                     "SYNOP_SHIP",
                     "SYNOP_MOBIL",
                     "UPPER_AIR"
                  };

            (void)fprintf(p_bul_rep_debug_file,
                          "pos:TTAAii;CCCC;type;spec;rss;BTIME;ITIME\n");
            for (i = 0; i < no_of_bc_entries; i++)
            {
               (void)fprintf(p_bul_rep_debug_file,
                             "%d:%s;%s;%d;%d;%d;%s;%s\n",
                             i, bcdb[i].TTAAii, bcdb[i].CCCC,
                             (int)bcdb[i].type, (int)bcdb[i].spec,
                             (int)bcdb[i].rss, bcdb[i].BTIME,
                             bcdb[i].ITIME);
            }
            (void)fprintf(p_bul_rep_debug_file,
                          "\npos:TT;rt;mimj;stid;rss;wid;BTIME;ITIME\n");
            for (i = 0; i < no_of_rc_entries; i++)
            {
               (void)fprintf(p_bul_rep_debug_file,
                             "%d:%c%c;%d->%s;%c%c;%d->%s;%d;%s;%s;%s\n",
                             i, rcdb[i].TT[0], rcdb[i].TT[1],
                             rcdb[i].rt, rtl[rcdb[i].rt], rcdb[i].mimj[0],
                             rcdb[i].mimj[1], rcdb[i].stid,
                             (rcdb[i].stid == 1) ? "IIiii" : "CCCC",
                             (int)rcdb[i].rss, rcdb[i].wid,
                             rcdb[i].BTIME, rcdb[i].ITIME);
            }
            (void)fclose(p_bul_rep_debug_file);
         }
#endif
      } /* if (stat_buf.st_mtime != last_read_bul) */

      free(rep_buffer);
   }

   return;
}


/*++++++++++++++++++++++++++ get_report_data() ++++++++++++++++++++++++++*/
static void
get_report_data(char *rep_buf)
{
   int  i,
        j;
   char *p_start_r,
        str_number[MAX_SHORT_LENGTH];

   /* First lets just count the number of entries. */
   p_start_r = rep_buf;
   no_of_rc_entries = 0;
   while (*p_start_r != '\0')
   {
      while ((*p_start_r != '\n') && (*p_start_r != '\r') &&
             (*p_start_r != '\0'))
      {
         p_start_r++;
      }
      while ((*p_start_r == '\n') || (*p_start_r == '\r'))
      {
         p_start_r++;
      }
      no_of_rc_entries++;
   }

   if (no_of_rc_entries == 0)
   {
      system_log(INFO_SIGN, __FILE__, __LINE__,
                 "No report specification entries found.");
      return;
   }

   free(rcdb);
   if ((rcdb = malloc((no_of_rc_entries * sizeof(struct wmo_rep_list)))) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "malloc() error : %s", strerror(errno));
      return;
   }

   /* Now, lets fill the structure with data. */
   p_start_r = rep_buf;
   no_of_rc_entries = 0;
   while (*p_start_r != '\0')
   {
      if ((isupper((int)(*p_start_r))) && (isupper((int)(*(p_start_r + 1)))) &&
          (*(p_start_r + 2) == ';'))
      {
         rcdb[no_of_rc_entries].TT[0] = *p_start_r;
         rcdb[no_of_rc_entries].TT[1] = *(p_start_r + 1);
         p_start_r += 3;
         i = 0;
         while ((i < (MAX_SHORT_LENGTH - 1)) && (isdigit((int)(*p_start_r))))
         {
            str_number[i] = *p_start_r;
            i++; p_start_r++;
         }
         str_number[i] = '\0';
         rcdb[no_of_rc_entries].rss = (short)atoi(str_number);
         while ((*p_start_r != ';') && (*p_start_r != '\n') &&
                (*p_start_r != '\0') && (*p_start_r != '\r'))
         {
            p_start_r++;
         }
         if (*p_start_r == ';')
         {
            p_start_r++;

            /* TEXT */
            if ((*p_start_r == 'T') && (*(p_start_r + 1) == 'E') &&
                (*(p_start_r + 2) == 'X') && (*(p_start_r + 3) == 'T') &&
                (*(p_start_r + 4) == ';'))
            {
               rcdb[no_of_rc_entries].rt = RT_TEXT;
               p_start_r += 4;
            }
                 /* ATEXT */
            else if ((*p_start_r == 'A') && (*(p_start_r + 1) == 'T') &&
                     (*(p_start_r + 2) == 'E') && (*(p_start_r + 3) == 'X') &&
                     (*(p_start_r + 4) == 'T') && (*(p_start_r + 5) == ';'))
                 {
                    rcdb[no_of_rc_entries].rt = RT_ATEXT;
                    p_start_r += 5;
                 }
                 /* CLIMAT */
            else if ((*p_start_r == 'C') && (*(p_start_r + 1) == 'L') &&
                     (*(p_start_r + 2) == 'I') &&
                     (*(p_start_r + 3) == 'M') &&
                     (*(p_start_r + 4) == 'A') &&
                     (*(p_start_r + 5) == 'T') &&
                     (*(p_start_r + 6) == ';'))
                 {
                    rcdb[no_of_rc_entries].rt = RT_CLIMAT;
                    p_start_r += 6;
                 }
                 /* TAF */
            else if ((*p_start_r == 'T') && (*(p_start_r + 1) == 'A') &&
                     (*(p_start_r + 2) == 'F') && (*(p_start_r + 3) == ';'))
                 {
                    rcdb[no_of_rc_entries].rt = RT_TAF;
                    p_start_r += 3;
                 }
                 /* METAR */
            else if ((*p_start_r == 'M') && (*(p_start_r + 1) == 'E') &&
                     (*(p_start_r + 2) == 'T') &&
                     (*(p_start_r + 3) == 'A') &&
                     (*(p_start_r + 4) == 'R') &&
                     (*(p_start_r + 5) == ';'))
                 {
                    rcdb[no_of_rc_entries].rt = RT_METAR;
                    p_start_r += 5;
                 }
                 /* SPECIAL 01 */
            else if ((*p_start_r == 'S') && (*(p_start_r + 1) == 'P') &&
                     (*(p_start_r + 2) == 'E') &&
                     (*(p_start_r + 3) == 'C') &&
                     (*(p_start_r + 4) == 'I') &&
                     (*(p_start_r + 5) == 'A') &&
                     (*(p_start_r + 6) == 'L') &&
                     (*(p_start_r + 7) == '-') &&
                     (*(p_start_r + 8) == '0') &&
                     (*(p_start_r + 9) == '1') &&
                     (*(p_start_r + 10) == ';'))
                 {
                    rcdb[no_of_rc_entries].rt = RT_SPECIAL_01;
                    p_start_r += 10;
                 }
                 /* SPECIAL 02 */
            else if ((*p_start_r == 'S') && (*(p_start_r + 1) == 'P') &&
                     (*(p_start_r + 2) == 'E') &&
                     (*(p_start_r + 3) == 'C') &&
                     (*(p_start_r + 4) == 'I') &&
                     (*(p_start_r + 5) == 'A') &&
                     (*(p_start_r + 6) == 'L') &&
                     (*(p_start_r + 7) == '-') &&
                     (*(p_start_r + 8) == '0') &&
                     (*(p_start_r + 9) == '2') &&
                     (*(p_start_r + 10) == ';'))
                 {
                    rcdb[no_of_rc_entries].rt = RT_SPECIAL_02;
                    p_start_r += 10;
                 }
                 /* SPECIAL 03 */
            else if ((*p_start_r == 'S') && (*(p_start_r + 1) == 'P') &&
                     (*(p_start_r + 2) == 'E') &&
                     (*(p_start_r + 3) == 'C') &&
                     (*(p_start_r + 4) == 'I') &&
                     (*(p_start_r + 5) == 'A') &&
                     (*(p_start_r + 6) == 'L') &&
                     (*(p_start_r + 7) == '-') &&
                     (*(p_start_r + 8) == '0') &&
                     (*(p_start_r + 9) == '3') &&
                     (*(p_start_r + 10) == ';'))
                 {
                    rcdb[no_of_rc_entries].rt = RT_SPECIAL_03;
                    p_start_r += 10;
                 }
                 /* SPECIAL 66 */
            else if ((*p_start_r == 'S') && (*(p_start_r + 1) == 'P') &&
                     (*(p_start_r + 2) == 'E') &&
                     (*(p_start_r + 3) == 'C') &&
                     (*(p_start_r + 4) == 'I') &&
                     (*(p_start_r + 5) == 'A') &&
                     (*(p_start_r + 6) == 'L') &&
                     (*(p_start_r + 7) == '-') &&
                     (*(p_start_r + 8) == '6') &&
                     (*(p_start_r + 9) == '6') &&
                     (*(p_start_r + 10) == ';'))
                 {
                    rcdb[no_of_rc_entries].rt = RT_SPECIAL_66;
                    p_start_r += 10;
                 }
                 /* SYNOP */
            else if ((*p_start_r == 'S') && (*(p_start_r + 1) == 'Y') &&
                     (*(p_start_r + 2) == 'N') &&
                     (*(p_start_r + 3) == 'O') &&
                     (*(p_start_r + 4) == 'P') &&
                     (*(p_start_r + 5) == ';'))
                 {
                    rcdb[no_of_rc_entries].rt = RT_SYNOP;
                    p_start_r += 5;
                 }
                 /* SYNOP SHIP */
            else if ((*p_start_r == 'S') && (*(p_start_r + 1) == 'Y') &&
                     (*(p_start_r + 2) == 'N') &&
                     (*(p_start_r + 3) == 'O') &&
                     (*(p_start_r + 4) == 'P') &&
                     (*(p_start_r + 5) == '-') &&
                     (*(p_start_r + 6) == 'S') &&
                     (*(p_start_r + 7) == 'H') &&
                     (*(p_start_r + 8) == 'I') &&
                     (*(p_start_r + 9) == 'P') &&
                     (*(p_start_r + 10) == ';'))
                 {
                    rcdb[no_of_rc_entries].rt = RT_SYNOP_SHIP;
                    p_start_r += 10;
                 }
                 /* SYNOP MOBIL */
            else if ((*p_start_r == 'S') && (*(p_start_r + 1) == 'Y') &&
                     (*(p_start_r + 2) == 'N') &&
                     (*(p_start_r + 3) == 'O') &&
                     (*(p_start_r + 4) == 'P') &&
                     (*(p_start_r + 5) == '-') &&
                     (*(p_start_r + 6) == 'M') &&
                     (*(p_start_r + 7) == 'O') &&
                     (*(p_start_r + 8) == 'B') &&
                     (*(p_start_r + 9) == 'I') &&
                     (*(p_start_r + 10) == 'L') &&
                     (*(p_start_r + 11) == ';'))
                 {
                    rcdb[no_of_rc_entries].rt = RT_SYNOP_MOBIL;
                    p_start_r += 11;
                 }
                 /* UPPER AIR */
            else if ((*p_start_r == 'U') && (*(p_start_r + 1) == 'P') &&
                     (*(p_start_r + 2) == 'P') &&
                     (*(p_start_r + 3) == 'E') &&
                     (*(p_start_r + 4) == 'R') &&
                     (*(p_start_r + 5) == '-') &&
                     (*(p_start_r + 6) == 'A') &&
                     (*(p_start_r + 7) == 'I') &&
                     (*(p_start_r + 8) == 'R') &&
                     (*(p_start_r + 9) == ';'))
                 {
                    rcdb[no_of_rc_entries].rt = RT_UPPER_AIR;
                    p_start_r += 9;
                 }
                 else
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "Unable to determine report type for %c%c",
                               rcdb[no_of_rc_entries].TT[0],
                               rcdb[no_of_rc_entries].TT[1]);
                    rcdb[no_of_rc_entries].rt = RT_NOT_DEFINED;
                    while ((*p_start_r != ';') && (*p_start_r != '\n') &&
                           (*p_start_r != '\0') && (*p_start_r != '\r'))
                    {
                       p_start_r++;
                    }
                 }

            if (*p_start_r == ';')
            {
               p_start_r++;
               if (*p_start_r == ';')
               {
                  rcdb[no_of_rc_entries].mimj[0] = '\0';
                  p_start_r++;
                  if (*p_start_r == 'D')
                  {
                     rcdb[no_of_rc_entries].stid = STID_IIiii;
                  }
                  else if (*p_start_r == 'L')
                       {
                          rcdb[no_of_rc_entries].stid = STID_CCCC;
                       }
                       else
                       {
                          rcdb[no_of_rc_entries].stid = 0;
                       }
                  while ((*p_start_r != ';') && (*p_start_r != '\n') &&
                         (*p_start_r != '\0') && (*p_start_r != '\r'))
                  {
                     p_start_r++;
                  }
                  if (*p_start_r == ';')
                  {
                     p_start_r++;
                     if (isdigit((int)(*p_start_r)))
                     {
                        rcdb[no_of_rc_entries].wid[0] = *p_start_r;
                        rcdb[no_of_rc_entries].wid[1] = '\0';
                     }
                     else
                     {
                        rcdb[no_of_rc_entries].wid[0] = '\0';
                     }
                     while ((*p_start_r != ';') && (*p_start_r != '\n') &&
                            (*p_start_r != '\0') && (*p_start_r != '\r'))
                     {
                        p_start_r++;
                     }
                     if (*p_start_r == ';')
                     {
                        /* Ignore MXSIZ */
                        p_start_r++;
                        while ((*p_start_r != ';') && (*p_start_r != '\n') &&
                               (*p_start_r != '\0') && (*p_start_r != '\r'))
                        {
                           p_start_r++;
                        }
                        if (*p_start_r == ';')
                        {
                           p_start_r++;

                           /* Store BTIME. */
                           j = 0;
                           while ((*p_start_r != ';') &&
                                  (*p_start_r != '\n') &&
                                  (*p_start_r != '\0') &&
                                  (*p_start_r != '\r') && (j < 5))
                           {
                              rcdb[no_of_rc_entries].BTIME[j] = *p_start_r;
                              p_start_r++; j++;
                           }
                           rcdb[no_of_rc_entries].BTIME[j] = '\0';
                           while ((*p_start_r != ';') && (*p_start_r != '\n') &&
                                  (*p_start_r != '\0') && (*p_start_r != '\r'))
                           {
                              p_start_r++;
                           }
                           if (*p_start_r == ';')
                           {
                              p_start_r++;

                              /* Store ITIME. */
                              j = 0;
                              while ((*p_start_r != ';') &&
                                     (*p_start_r != '\n') &&
                                     (*p_start_r != '\0') &&
                                     (*p_start_r != '\r') && (j < 5))
                              {
                                 rcdb[no_of_rc_entries].ITIME[j] = *p_start_r;
                                 p_start_r++; j++;
                              }
                              rcdb[no_of_rc_entries].ITIME[j] = '\0';
                           }
                           else
                           {
                              rcdb[no_of_rc_entries].ITIME[0] = '\0';
                           }
                        }
                        else
                        {
                           rcdb[no_of_rc_entries].BTIME[0] = '\0';
                           rcdb[no_of_rc_entries].ITIME[0] = '\0';
                        }
                     }
                  }
                  else
                  {
                     rcdb[no_of_rc_entries].wid[0] = '\0';
                     rcdb[no_of_rc_entries].BTIME[0] = '\0';
                     rcdb[no_of_rc_entries].ITIME[0] = '\0';
                  }
               }
               else
               {
                  rcdb[no_of_rc_entries].mimj[0] = *p_start_r;
                  if (*(p_start_r + 1) != ';')
                  {
                     rcdb[no_of_rc_entries].mimj[1] = *(p_start_r + 1);
                     p_start_r += 2;
                     while ((*p_start_r != ';') && (*p_start_r != '\n') &&
                            (*p_start_r != '\0') && (*p_start_r != '\r'))
                     {
                        p_start_r++;
                     }

                     if (*p_start_r == ';')
                     {
                        p_start_r++;
                        if (*p_start_r == 'D')
                        {
                           rcdb[no_of_rc_entries].stid = STID_IIiii;
                        }
                        else if (*p_start_r == 'L')
                             {
                                rcdb[no_of_rc_entries].stid = STID_CCCC;
                             }
                             else
                             {
                                rcdb[no_of_rc_entries].stid = 0;
                             }
                        while ((*p_start_r != ';') && (*p_start_r != '\n') &&
                               (*p_start_r != '\0') && (*p_start_r != '\r'))
                        {
                           p_start_r++;
                        }
                        if (*p_start_r == ';')
                        {
                           p_start_r++;
                           if (isdigit((int)(*p_start_r)))
                           {
                              rcdb[no_of_rc_entries].wid[0] = *p_start_r;
                              rcdb[no_of_rc_entries].wid[1] = '\0';
                           }
                           else
                           {
                              rcdb[no_of_rc_entries].wid[0] = '\0';
                           }
                           while ((*p_start_r != ';') && (*p_start_r != '\n') &&
                                  (*p_start_r != '\0') && (*p_start_r != '\r'))
                           {
                              p_start_r++;
                           }
                           if (*p_start_r == ';')
                           {
                              /* Ignore MXSIZ */
                              p_start_r++;
                              while ((*p_start_r != ';') && (*p_start_r != '\n') &&
                                     (*p_start_r != '\0') && (*p_start_r != '\r'))
                              {
                                 p_start_r++;
                              }
                              if (*p_start_r == ';')
                              {
                                 p_start_r++;

                                 /* Store BTIME. */
                                 j = 0;
                                 while ((*p_start_r != ';') &&
                                        (*p_start_r != '\n') &&
                                        (*p_start_r != '\0') &&
                                        (*p_start_r != '\r') && (j < 5))
                                 {
                                    rcdb[no_of_rc_entries].BTIME[j] = *p_start_r;
                                    p_start_r++; j++;
                                 }
                                 rcdb[no_of_rc_entries].BTIME[j] = '\0';
                                 while ((*p_start_r != ';') && (*p_start_r != '\n') &&
                                        (*p_start_r != '\0') && (*p_start_r != '\r'))
                                 {
                                    p_start_r++;
                                 }
                                 if (*p_start_r == ';')
                                 {
                                    p_start_r++;

                                    /* Store ITIME. */
                                    j = 0;
                                    while ((*p_start_r != ';') &&
                                           (*p_start_r != '\n') &&
                                           (*p_start_r != '\0') &&
                                           (*p_start_r != '\r') && (j < 5))
                                    {
                                       rcdb[no_of_rc_entries].ITIME[j] = *p_start_r;
                                       p_start_r++; j++;
                                    }
                                    rcdb[no_of_rc_entries].ITIME[j] = '\0';
                                 }
                                 else
                                 {
                                    rcdb[no_of_rc_entries].ITIME[0] = '\0';
                                 }
                              }
                              else
                              {
                                 rcdb[no_of_rc_entries].BTIME[0] = '\0';
                                 rcdb[no_of_rc_entries].ITIME[0] = '\0';
                              }
                           }
                        }
                        else
                        {
                           rcdb[no_of_rc_entries].stid = 0;
                           rcdb[no_of_rc_entries].wid[0] = '\0';
                           rcdb[no_of_rc_entries].BTIME[0] = '\0';
                           rcdb[no_of_rc_entries].ITIME[0] = '\0';
                        }
                     }
                     else
                     {
                        rcdb[no_of_rc_entries].stid = 0;
                        rcdb[no_of_rc_entries].wid[0] = '\0';
                        rcdb[no_of_rc_entries].BTIME[0] = '\0';
                        rcdb[no_of_rc_entries].ITIME[0] = '\0';
                     }
                  }
                  else
                  {
                     rcdb[no_of_rc_entries].mimj[0] = '\0';
                     rcdb[no_of_rc_entries].stid = 0;
                     rcdb[no_of_rc_entries].wid[0] = '\0';
                     rcdb[no_of_rc_entries].BTIME[0] = '\0';
                     rcdb[no_of_rc_entries].ITIME[0] = '\0';
                  }
               }
            }
            else
            {
               rcdb[no_of_rc_entries].mimj[0] = '\0';
               rcdb[no_of_rc_entries].stid = 0;
               rcdb[no_of_rc_entries].wid[0] = '\0';
               rcdb[no_of_rc_entries].BTIME[0] = '\0';
               rcdb[no_of_rc_entries].ITIME[0] = '\0';
            }
         }
         else
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Unable to determine report type for %c%c",
                       rcdb[no_of_rc_entries].TT[0],
                       rcdb[no_of_rc_entries].TT[1]);
            rcdb[no_of_rc_entries].rt = RT_NOT_DEFINED;
            rcdb[no_of_rc_entries].mimj[0] = '\0';
            rcdb[no_of_rc_entries].stid = 0;
            rcdb[no_of_rc_entries].wid[0] = '\0';
            rcdb[no_of_rc_entries].BTIME[0] = '\0';
            rcdb[no_of_rc_entries].ITIME[0] = '\0';
         }
         no_of_rc_entries++;
      }

      /* Lets continue with the next line. */
      while ((*p_start_r != '\n') && (*p_start_r != '\r') &&
             (*p_start_r != '\0'))
      {
         p_start_r++;
      }
      while ((*p_start_r == '\n') || (*p_start_r == '\r'))
      {
         p_start_r++;
      }
   }

   return;
}
