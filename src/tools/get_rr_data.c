/*
 *  get_rr_data.c - Part of AFD, an automatic file distribution program.
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
 **   get_rr_data - gets all data out of the rename.rule file(s)
 **
 ** SYNOPSIS
 **   get_rr_data [<rule alias 0> ... [<rule alias n>]]
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
 **   19.01.2021 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>        /* strcmp(), strlen()                         */
#include <stdlib.h>        /* exit()                                     */
#include <unistd.h>        /* STDERR_FILENO                              */
#include "version.h"

/* Global variables. */
int         no_of_rule_headers,
            sys_log_fd = STDERR_FILENO;
char        *p_work_dir;
struct rule *rule;
const char  *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void usage(FILE *, char *);
static int  check_rule_header(char *, int, char **);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int  no_of_search_rule_alias = 0,
        ret = SUCCESS;
   char **search_rule_alias = NULL,
        work_dir[MAX_PATH_LENGTH];

   CHECK_FOR_VERSION(argc, argv);
   p_work_dir = work_dir;

   if (get_afd_path(&argc, argv, p_work_dir) < 0)
   {
      (void)fprintf(stderr,
                    _("Failed to get working directory of AFD. (%s %d)\n"),
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef WITH_SETUID_PROGS
   set_afd_euid(p_work_dir);
#endif

   if ((get_arg(&argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(&argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(stdout, argv[0]);
      exit(0);
   }

   (void)get_arg_array_all(&argc, argv, &search_rule_alias,
                           &no_of_search_rule_alias);

   get_rename_rules(NO);

   if (no_of_rule_headers > 0)
   {
      int i, j,
          length,
          longest_filter_length = 0,
          headers_to_do = 0;

      /* First lets get the longest filter. */
      for (i = 0; i < no_of_rule_headers; i++)
      {
         if ((no_of_search_rule_alias == 0) ||
             (check_rule_header(rule[i].header,
                                no_of_search_rule_alias,
                                search_rule_alias) == YES))
         {
            for (j = 0; j < rule[i].no_of_rules; j++)
            {
               length = strlen(rule[i].filter[j]);
               if (length > longest_filter_length)
               {
                  longest_filter_length = length;
               }
            }
            headers_to_do++;
         }
      }

      /* Now show them. */
      if (headers_to_do > 0)
      {
         int show_alias;

         if (headers_to_do > 1)
         {
            show_alias = YES;
         }
         else
         {
            show_alias = NO;
         }
         for (i = 0; i < no_of_rule_headers; i++)
         {
            if ((no_of_search_rule_alias == 0) ||
                (check_rule_header(rule[i].header,
                                   no_of_search_rule_alias,
                                   search_rule_alias) == YES))
            {
               if (show_alias == YES)
               {
                  (void)fprintf(stdout, "[%s]\n", rule[i].header);
               }
               for (j = 0; j < rule[i].no_of_rules; j++)
               {
                  (void)fprintf(stdout, "%-*s %s\n", longest_filter_length,
                                rule[i].filter[j], rule[i].rename_to[j]);
               }
               headers_to_do--;

               if (headers_to_do > 0)
               {
                  (void)fprintf(stdout, "\n");
               }
            }
         }
      }
      else
      {
         (void)fprintf(stdout, "No such header(s) in rename.rule(s)\n");
         ret = INCORRECT;
      }
   }
   else
   {
      (void)fprintf(stdout, "Rename rules are empty\n");
      ret = INCORRECT;
   }

   exit(ret);
}


/*+++++++++++++++++++++++++ check_rule_header() ++++++++++++++++++++++++*/
static int
check_rule_header(char *rule_header,
                  int  no_of_search_rule_alias,
                  char **search_rule_alias)
{
   int i;

   for (i = 0; i < no_of_search_rule_alias; i++)
   {
      if (strcmp(rule_header, search_rule_alias[i]) == 0)
      {
         return(YES);
      }
   }

   return(NO);
}


/*++++++++++++++++++++++++++++ usage() +++++++++++++++++++++++++++++++++*/
static void
usage(FILE *stream, char *progname)
{
   (void)fprintf(stream, _("Usage: %s [<rule alias 0> [.. <rule alias n>]]\n"),
                 progname);

   return;
}
