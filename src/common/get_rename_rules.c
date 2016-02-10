/*
 *  get_rename_rules.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_rename_rules - reads rename rules from a file and stores them
 **                      in a shared memory area
 **
 ** SYNOPSIS
 **   void get_rename_rules(int verbose)
 **
 ** DESCRIPTION
 **   This function reads the rule file and stores the contents in the
 **   global structure rule. The contents of the rule file looks as
 **   follows:
 **
 **      [T4-charts]
 **      *PGAH??_EGRR*     *waf_egr_nat_000_000*
 **      *PGCX??_EGRR*     *waf_egr_gaf_000_900*
 **
 **   Where [T4-charts] is here defined as a rule header. *PGAH??_EGRR*
 **   and *PGCX??_EGRR* are the filter/file-mask, while the rest is the
 **   part to which we want to rename.
 **   The  number of rule headers and rules is not limited.
 **
 **   The caller is responsible for freeing the memory for rule.filter
 **   and rule.rename_to.
 **
 ** RETURN VALUES
 **   None. It will exit with INCORRECT if it fails to allocate memory
 **   or fails to open the rule_file.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.02.1997 H.Kiehl Created
 **   01.12.2002 H.Kiehl No need for two newlines before each rule header.
 **                      Added verbose flag and more info when set.
 **   12.05.2006 H.Kiehl Handle case the rename rule starts without a
 **                      newline as first line.
 **   08.11.2006 H.Kiehl Allow for spaces in the rule and rename_to part.
 **                      The space must then be preceeded by a \.
 **   07.11.2008 H.Kiehl Accept DOS-style rename rule files.
 **   01.04.2012 H.Kiehl Added supoort for reading multiple rename rule
 **                      files.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>          /* strcpy(), strerror()                     */
#include <stdlib.h>          /* calloc(), free()                         */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>          /* read(), close()                          */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int         no_of_rule_headers;
extern char        *p_work_dir;
extern struct rule *rule;


/*########################## get_rename_rules() #########################*/
void
get_rename_rules(int verbose)
{
   static time_t last_read_times = 0,
                 last_afd_config_read = 0;
   static int    first_time = YES,
                 no_of_rename_rule_files = 0;
   static char   *config_file = NULL,
                 *rule_file[MAX_RENAME_RULE_FILES];
   int           count,
                 read_size = 1024,
                 total_no_of_rules = 0;
   time_t        current_times = 0;
   off_t         max_size = 0;
   struct stat   stat_buf;

   if (config_file == NULL)
   {
      count = strlen(p_work_dir) + ETC_DIR_LENGTH + AFD_CONFIG_FILE_LENGTH + 1;
      if ((config_file = malloc(count)) == NULL)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to malloc() %d bytes : %s"),
                    (strlen(p_work_dir) + ETC_DIR_LENGTH + AFD_CONFIG_FILE_LENGTH + 1),
                    strerror(errno));
         return;
      }
      (void)snprintf(config_file, count, "%s%s%s",
                     p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   }
   if (stat(config_file, &stat_buf) == -1)
   {
      if (errno != ENOENT)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to stat() `%s' : %s"),
                    config_file, strerror(errno));
      }
   }
   else
   {
      if (stat_buf.st_mtime != last_afd_config_read)
      {
         char *buffer;

         /* Discard what we have up to now. */
         if (last_read_times != 0)
         {
            int i;

            /*
             * Since we are rereading the whole rules file again
             * lets release all the memory we stored for the previous
             * rename rules.
             */
            for (i = 0; i < no_of_rename_rule_files; i++)
            {
               free(rule_file[i]);
            }
            no_of_rename_rule_files = 0;

            for (i = 0; i < no_of_rule_headers; i++)
            {
               if (rule[i].filter != NULL)
               {
                  FREE_RT_ARRAY(rule[i].filter);
               }
               if (rule[i].rename_to != NULL)
               {
                  FREE_RT_ARRAY(rule[i].rename_to);
               }
            }
            free(rule);
            no_of_rule_headers = 0;
         }
         last_read_times = 0;

         if ((eaccess(config_file, F_OK) == 0) &&
             (read_file_no_cr(config_file, &buffer, YES, __FILE__, __LINE__) != INCORRECT))
         {
            int  length;
            char *ptr,
                 value[MAX_PATH_LENGTH];

            ptr = buffer;
            while ((ptr = get_definition(ptr, RENAME_RULE_NAME_DEF,
                                         value, MAX_PATH_LENGTH)) != NULL)
            {
               if (value[0] != '/')
               {
                  char *p_path,
                       user[MAX_USER_NAME_LENGTH + 1];

                  if (value[0] == '~')
                  {
                     if (value[1] == '/')
                     {
                        p_path = &value[2];
                        user[0] = '\0';
                     }
                     else
                     {
                        int j = 0;

                        p_path = &value[1];
                        while ((*(p_path + j) != '/') && (*(p_path + j) != '\0') &&
                               (j < MAX_USER_NAME_LENGTH))
                        {
                           user[j] = *(p_path + j);
                           j++;
                        }
                        if (j >= MAX_USER_NAME_LENGTH)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      _("User name to long for %s definition %s. User name may be %d bytes long."),
                                      RENAME_RULE_NAME_DEF, value,
                                      MAX_USER_NAME_LENGTH);
                        }
                        user[j] = '\0';
                     }
                     (void)expand_path(user, p_path);
                     length = strlen(p_path) + 1;
                     (void)memmove(value, p_path, length);
                  }
                  else
                  {
                     char tmp_value[MAX_PATH_LENGTH];

                     (void)my_strncpy(tmp_value, value, MAX_PATH_LENGTH);
                     length = snprintf(value, MAX_PATH_LENGTH, "%s%s/%s",
                                       p_work_dir, ETC_DIR, tmp_value) + 1;
                  }
               }
               else
               {
                  length = strlen(value) + 1;
               }

               if ((rule_file[no_of_rename_rule_files] = malloc(length)) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to malloc() %d bytes : %s"),
                             length, strerror(errno));
                  return;
               }
               (void)strcpy(rule_file[no_of_rename_rule_files], value);
               no_of_rename_rule_files++;
               if (no_of_rename_rule_files >= MAX_RENAME_RULE_FILES)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Only %d rename rule files possible.",
                             MAX_RENAME_RULE_FILES);
                  break;
               }
            }

            free(buffer);
            last_afd_config_read = stat_buf.st_mtime;
         }
      }
   }

   if (no_of_rename_rule_files == 0)
   {
      count = strlen(p_work_dir) + ETC_DIR_LENGTH + RENAME_RULE_FILE_LENGTH + 1;
      if ((rule_file[0] = malloc(count)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Failed to malloc() %d bytes : %s"),
                    (strlen(p_work_dir) + ETC_DIR_LENGTH + RENAME_RULE_FILE_LENGTH + 1),
                    strerror(errno));
         return;
      }
      (void)snprintf(rule_file[0], count, "%s%s%s",
                     p_work_dir, ETC_DIR, RENAME_RULE_FILE);
      no_of_rename_rule_files++;
   }

   for (count = 0; count < no_of_rename_rule_files; count++)
   {
      if (stat(rule_file[count], &stat_buf) == -1)
      {
         if (errno == ENOENT)
         {
            /*
             * Only tell user once that the rules file is missing. Otherwise
             * it is anoying to constantly receive this message.
             */
            if (first_time == YES)
            {
               if (verbose == YES)
               {
                  system_log(INFO_SIGN, __FILE__, __LINE__,
                             _("There is no renaming rules file `%s'"),
                             rule_file[count]);
               }
               if ((count + 1) == no_of_rename_rule_files)
               {
                  first_time = NO;
               }
            }
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Failed to stat() `%s' : %s"),
                       rule_file[count], strerror(errno));
         }
      }
      else
      {
         max_size += stat_buf.st_size;
         if (stat_buf.st_blksize > read_size)
         {
            read_size = stat_buf.st_blksize;
         }
         current_times += stat_buf.st_mtime;
      }
   }

   if (last_read_times != current_times)
   {
      register int i;
      int          bytes_read,
                   fd;
      off_t        bytes_buffered;
      char         *last_ptr,
                   *ptr,
                   *buffer;

      if (first_time == YES)
      {
         first_time = NO;
      }
      else
      {
         if (verbose == YES)
         {
            system_log(INFO_SIGN, NULL, 0,
                       _("Rereading %d renaming rules file."),
                       no_of_rename_rule_files);
         }
      }

      if (last_read_times != 0)
      {
         /*
          * Since we are rereading the whole rules file again
          * lets release the memory we stored for the previous
          * structure of rule.
          */
         for (i = 0; i < no_of_rule_headers; i++)
         {
            if (rule[i].filter != NULL)
            {
               FREE_RT_ARRAY(rule[i].filter);
            }
            if (rule[i].rename_to != NULL)
            {
               FREE_RT_ARRAY(rule[i].rename_to);
            }
         }
         free(rule);
         no_of_rule_headers = 0;
      }
      last_read_times = current_times;

      /* Allocate memory to store file. */
      if ((buffer = malloc(1 + max_size + 1 + read_size)) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("malloc() error : %s"), strerror(errno));
         return;
      }

      buffer[0] = '\n';
      bytes_buffered = 0;
      for (count = 0; count < no_of_rename_rule_files; count++)
      {
         /* Open file. */
         if ((fd = open(rule_file[count], O_RDONLY)) == -1)
         {
            system_log((errno == ENOENT) ? INFO_SIGN : WARN_SIGN, __FILE__, __LINE__,
                       _("Failed to open() `%s' : %s"),
                       rule_file[count], strerror(errno));
         }
         else
         {
            /* Read file into buffer. */
            do
            {
               if ((bytes_read = read(fd, &buffer[1] + bytes_buffered,
                                      read_size)) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to read() `%s' : %s"),
                             rule_file[count], strerror(errno));
                  break;
               }
               bytes_buffered += bytes_read;
            } while (bytes_read == read_size);
            buffer[bytes_buffered] = '\n';

            if (close(fd) == -1)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("close() error : %s"), strerror(errno));
            }
         }
      } /* for (count = 0; count < no_of_rename_rule_files; count++) */
      buffer[bytes_buffered] = '\0';

      if (bytes_buffered > 0)
      {
         /*
          * Now that we have the contents in the buffer lets first see
          * how many rules there are in the buffer so we can allocate
          * memory for the rules.
          */
         ptr = buffer;
         last_ptr = buffer + bytes_buffered;
         if (buffer[0] == '[')
         {
            no_of_rule_headers++;
         }
         while ((ptr = lposi(ptr, "\n[", 2)) != NULL)
         {
            no_of_rule_headers++;
         }

         if (no_of_rule_headers > 0)
         {
            register int j, k;
            int          no_of_rules,
                         max_filter_length,
                         max_rule_length;
            char         *end_ptr,
                         *search_ptr;

            if ((rule = calloc(no_of_rule_headers,
                               sizeof(struct rule))) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("calloc() error : %s (%s %d)\n"), strerror(errno));
               no_of_rule_headers = 0;
               return;
            }
            ptr = buffer;

            for (i = 0; i < no_of_rule_headers; i++)
            {
               if (((ptr == buffer) && (buffer[0] == '[')) ||
                   ((ptr = lposi(ptr, "\n[", 2)) != NULL))
               {
                  /*
                   * Lets first determine how many rules there are.
                   * This is simply done by counting the \n characters.
                   * While doing this, lets also find the longest rule or
                   * rename_to string.
                   */
                  if ((ptr == buffer) && (buffer[0] == '['))
                  {
                     ptr++;
                  }
                  no_of_rules = max_filter_length = max_rule_length = 0;
                  search_ptr = ptr;
                  while ((*search_ptr != '\n') && (*search_ptr != '\r'))
                  {
                     search_ptr++;
                  }
                  do
                  {
                     if (*search_ptr == '\r')
                     {
                        search_ptr++;
                     }
                     search_ptr++;

                     /* Ignore any comments. */
                     if ((*search_ptr == '#') &&
                         (search_ptr > ptr) && (*(search_ptr - 1) != '\\'))
                     {
                        while ((*search_ptr != '\n') && (*search_ptr != '\0') &&
                               (*search_ptr != '\r'))
                        {
                           search_ptr++;
                        }
                     }
                     else
                     {
                        count = 0;
                        while ((*search_ptr != ' ') && (*search_ptr != '\t') &&
                               (search_ptr < last_ptr))
                        {
                           if ((*search_ptr == '\\') &&
                               ((*(search_ptr + 1) == ' ') ||
                                 (*(search_ptr + 1) == '#')))
                           {
                              count++; search_ptr++;
                           }
                           count++; search_ptr++;
                        }
                        if (search_ptr == last_ptr)
                        {
                           break;
                        }
                        if (count > max_filter_length)
                        {
                           max_filter_length = count;
                        }
                        while ((*search_ptr == ' ') || (*search_ptr == '\t'))
                        {
                           search_ptr++;
                        }
                        count = 0;
                        while ((*search_ptr != '\n') && (*search_ptr != '\0') &&
                               (*search_ptr != '\r'))
                        {
                           count++; search_ptr++;
                        }
                        if (count > max_rule_length)
                        {
                           max_rule_length = count;
                        }
                        no_of_rules++;
                     }
                  } while (((*search_ptr == '\r') &&
                            (*(search_ptr + 2) != '\r') &&
                            (*(search_ptr + 2) != '[') &&
                            (*search_ptr != '\0') &&
                            (*(search_ptr + 2) != '\0')) ||
                           ((*search_ptr != '\r') &&
                            (*(search_ptr + 1) != '\n') &&
                            (*(search_ptr + 1) != '[') &&
                            (*search_ptr != '\0') &&
                            (*(search_ptr + 1) != '\0')));

                  /* Allocate memory for filter and rename_to */
                  /* part of struct rule.                     */
                  rule[i].filter = NULL;
                  RT_ARRAY(rule[i].filter, no_of_rules,
                           max_filter_length + 1, char);
                  rule[i].rename_to = NULL;
                  RT_ARRAY(rule[i].rename_to, no_of_rules,
                           max_rule_length + 1, char);

                  ptr--;
                  end_ptr = ptr;
                  while ((*end_ptr != ']') && (*end_ptr != '\n') &&
                         (*end_ptr != '\0') && (*end_ptr != '\r'))
                  {
                     end_ptr++;
                  }
                  if ((end_ptr - ptr) <= MAX_RULE_HEADER_LENGTH)
                  {
                     if (*end_ptr == ']')
                     {
                        *end_ptr = '\0';
                        (void)strcpy(rule[i].header, ptr);
                        ptr = end_ptr + 1;

                        while ((*ptr != '\n') && (*ptr != '\0') &&
                               (*ptr != '\r'))
                        {
                           ptr++;
                        }
                        if ((*ptr == '\n') || (*ptr == '\r'))
                        {
                           j = 0;

                           do
                           {
                              if (*ptr == '\r')
                              {
                                 ptr++;
                              }
                              ptr++;

                              /* Ignore any comments. */
                              if ((*ptr == '#') && (*(ptr - 1) != '\\'))
                              {
                                 while ((*ptr != '\n') && (*ptr != '\0') &&
                                        (*ptr != '\r'))
                                 {
                                    ptr++;
                                 }
                              }
                              else
                              {
                                 /*
                                  * Store the filter part.
                                  */
                                 k = 0;
                                 end_ptr = ptr;
                                 while ((*end_ptr != ' ') &&
                                        (*end_ptr != '\t') &&
                                        (*end_ptr != '\n') &&
                                        (*end_ptr != '\r') &&
                                        (*end_ptr != '\0'))
                                 {
                                    if ((*end_ptr == '\\') &&
                                        ((*(end_ptr + 1) == ' ') ||
                                         (*(end_ptr + 1) == '#')))
                                    {
                                       end_ptr++;
                                    }
                                    rule[i].filter[j][k] = *end_ptr;
                                    end_ptr++; k++;
                                 }
                                 if ((*end_ptr == ' ') || (*end_ptr == '\t'))
                                 {
                                    rule[i].filter[j][k] = '\0';
                                    end_ptr++;
                                    while ((*end_ptr == ' ') ||
                                           (*end_ptr == '\t'))
                                    {
                                       end_ptr++;
                                    }

                                    /*
                                     * Store the renaming part.
                                     */
                                    k = 0;
                                    while ((*end_ptr != ' ') &&
                                           (*end_ptr != '\t') &&
                                           (*end_ptr != '\n') &&
                                           (*end_ptr != '\r') &&
                                           (*end_ptr != '\0'))
                                    {
                                       if ((*end_ptr == '\\') &&
                                           ((*(end_ptr + 1) == ' ') ||
                                            (*(end_ptr + 1) == '#')))
                                       {
                                          end_ptr++;
                                       }
                                       rule[i].rename_to[j][k] = *end_ptr;
                                       end_ptr++; k++;
                                    }
                                    rule[i].rename_to[j][k] = '\0';
                                    if ((*end_ptr == ' ') || (*end_ptr == '\t'))
                                    {
                                       int more_data = NO;

                                       end_ptr++;
                                       while ((*end_ptr != '\n') &&
                                              (*end_ptr != '\0') &&
                                              (*end_ptr != '\r'))
                                       {
                                          if ((more_data == NO) &&
                                              (*end_ptr != ' ') &&
                                              (*end_ptr != '\t'))
                                          {
                                             more_data = YES;
                                          }
                                          end_ptr++;
                                       }
                                       if (more_data == YES)
                                       {
                                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                                     _("In rule [%s] the rule %s %s has data after the rename-to-part. Ignoring it!"),
                                                     rule[i].header,
                                                     rule[i].filter[j],
                                                     rule[i].rename_to[j]);
                                       }
                                    }
                                    ptr = end_ptr;
                                    j++;
                                 }
                                 else
                                 {
                                    if (end_ptr != ptr)
                                    {
                                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                                  _("A filter is specified for the rule header %s but not a rule."),
                                                  rule[i].header);
                                       ptr = end_ptr;
                                    }
                                    else if (*ptr == '\n')
                                         {
                                            ptr++;
                                         }
                                    else if (*ptr == '\r')
                                         {
                                            ptr++;
                                            if (*ptr == '\n')
                                            {
                                               ptr++;
                                            }
                                         }
                                 }
                              }
                           } while (((*ptr == '\n') || (*ptr == '\r')) &&
                                    (j < no_of_rules));

                           rule[i].no_of_rules = j;
                           total_no_of_rules += j;
                        }
                        else
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      _("Rule header %s specified, but could not find any rules."),
                                      rule[i].header);
                        }
                     }
                     else
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   _("Failed to determine the end of the rule header."));
                     }
                  }
                  else
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                _("Rule header to long. May not be longer then %d bytes [MAX_RULE_HEADER_LENGTH]."),
                                MAX_RULE_HEADER_LENGTH);
                  }
               }
               else
               {
                  /* Impossible! We just did find it and now it's gone?!? */
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             _("Could not get start of rule header %d [%d]."),
                             i, no_of_rule_headers);
                  rule[i].filter = NULL;
                  rule[i].rename_to = NULL;
                  break;
               }
            } /* for (i = 0; i < no_of_rule_headers; i++) */
         } /* if (no_of_rule_headers > 0) */
      }

      if (verbose == YES)
      {
         if (no_of_rule_headers > 0)
         {
            system_log(INFO_SIGN, NULL, 0,
                       _("Found %d rename rule headers with %d rules."),
                       no_of_rule_headers, total_no_of_rules);
         }
         else
         {
            system_log(INFO_SIGN, NULL, 0,
                       _("No rename rules found in %s"), rule_file);
         }
      }

      /* The buffer holding the contents of the rule file */
      /* is no longer needed.                             */
      free(buffer);

#ifdef _DEBUG_RULES
      {
         register int j;

         for (i = 0; i < no_of_rule_headers; i++)
         {
            system_log(DEBUG_SIGN, NULL, 0, "[%s]", rule[i].header);
            for (j = 0; j < rule[i].no_of_rules; j++)
            {
               system_log(DEBUG_SIGN, NULL, 0, "%s  %s",
                          rule[i].filter[j], rule[i].rename_to[j]);
            }
         }
      }
#endif
   }

   return;
}
