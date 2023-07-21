/*
 *  check_option.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_option - checks if the syntax of the option is correct
 **
 ** SYNOPSIS
 **   int check_option(char *option, FILE *cmd_fp)
 **
 ** DESCRIPTION
 **   This function checks if the syntax of the option is correct.
 **   In some cases it also checks if the content of the option
 **   is correct.
 **
 ** RETURN VALUES
 **   Returns SUCCESS if syntax is correct, otherwise INCORRECT is
 **   returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.05.2007 H.Kiehl Created
 **   07.01.2008 H.Kiehl Added check if subject or mail header file
 **                      is readable.
 **   11.05.2017 H.Kiehl Added parameter cmd_fp, so we can return warnings/
 **                      errors to command line tools.
 **   07.07.2019 H.Kiehl Added trans_srename.
 **   31.05.2023 H.Kiehl Added hardlink and symlink.
 **   27.05.2023 H.Kiehl Added ageing.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>     /* strncmp()                                     */
#include <ctype.h>      /* isascii()                                     */
#include <unistd.h>     /* access()                                      */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int         no_of_rule_headers;
extern char        *p_work_dir;
extern struct rule *rule;

/* Local function prototypes. */
static int         check_rule(char *, FILE *);


/*############################ check_option() ###########################*/
int
check_option(char *option, FILE *cmd_fp)
{
   char *ptr;

   ptr = option;
   if ((CHECK_STRNCMP(option, PRIORITY_ID, PRIORITY_ID_LENGTH) == 0) &&
       ((*(option + PRIORITY_ID_LENGTH) == ' ') ||
        (*(option + PRIORITY_ID_LENGTH) == '\t')))
   {
      ptr += PRIORITY_ID_LENGTH + 1;
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      if ((*ptr < '0') || (*ptr > '9') ||
          ((*(ptr + 1) != '\0') && (*(ptr + 1) != ' ')))
      {
         update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                    "Unknown priority, setting to default %c.",
                    DEFAULT_PRIORITY);
         return(INCORRECT);
      }
   }
   else if ((CHECK_STRNCMP(option, ARCHIVE_ID, ARCHIVE_ID_LENGTH) == 0) &&
            ((*(option + ARCHIVE_ID_LENGTH) == ' ') ||
             (*(option + ARCHIVE_ID_LENGTH) == '\t')))
        {
           ptr += ARCHIVE_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No %s time specified.", ARCHIVE_ID);
              return(INCORRECT);
           }
           else
           {
              int i;

              i = 0;
              while ((isdigit((int)(*(ptr + i)))) && (i < MAX_INT_LENGTH))
              {
                 i++;
              }
              if (i == MAX_INT_LENGTH)
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Value for %s option to large.", ARCHIVE_ID);
                 return(INCORRECT);
              }
              if (i == 0)
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Invalid (%s) %s time specified.",
                               ptr, ARCHIVE_ID);
                 return(INCORRECT);
              }
              switch (*(ptr + i))
              {
                 case '\0' : /* Default unit */
                 case 'd'  : /* Days */
                 case 'h'  : /* Hours */
                 case 'm'  : /* Minutes */
                 case 's'  : /* Seconds */
                 case ' '  : /* Default unit */
                 case '\t' : /* Default unit */
                    /* OK */;
                    break;
                 default   : /* Incorrect */
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Unknown %s unit %c (%d).",
                                  ARCHIVE_ID, *(ptr + i), (int)(*(ptr + i)));
                    return(INCORRECT);
              }
           }
        }
   else if ((CHECK_STRNCMP(option, LOCK_ID, LOCK_ID_LENGTH) == 0) &&
            ((*(option + LOCK_ID_LENGTH) == ' ') ||
             (*(option + LOCK_ID_LENGTH) == '\t')))
        {
           ptr += LOCK_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No %s type specified.", LOCK_ID);
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, RENAME_ID, RENAME_ID_LENGTH) == 0) &&
            ((*(option + RENAME_ID_LENGTH) == ' ') ||
             (*(option + RENAME_ID_LENGTH) == '\t')))
        {
           ptr += RENAME_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (check_rule(ptr, cmd_fp) == INCORRECT)
           {
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, SRENAME_ID, SRENAME_ID_LENGTH) == 0) &&
            ((*(option + SRENAME_ID_LENGTH) == ' ') ||
             (*(option + SRENAME_ID_LENGTH) == '\t')))
        {
           int k = 0;

           ptr += SRENAME_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No filter and 'rename to' specified for %s",
                            SRENAME_ID);
              return(INCORRECT);
           }
           while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
                  (*ptr != '\0') && (k < MAX_FILENAME_LENGTH))
           {
              if ((*ptr == '\\') &&
                  ((*(ptr + 1) == ' ') || (*(ptr + 1) == '#') ||
                   (*(ptr + 1) == '\t')))
              {
                 ptr++;
              }
              ptr++; k++;
           }
           if ((*ptr != ' ') && (*ptr != '\t'))
           {
              if (k == MAX_FILENAME_LENGTH)
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "The filter for option %s is to long (%d)",
                               SRENAME_ID, MAX_FILENAME_LENGTH);
              }
              else
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "No filter specified for %s", SRENAME_ID);
              }
              return(INCORRECT);
           }
           else
           {
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              if (*ptr == '\0')
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "No 'rename to' part specified for option %s.",
                               SRENAME_ID);
                 return(INCORRECT);
              }
              else
              {
                 k = 0;
                 while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
                        (*ptr != '\0') && (k < MAX_FILENAME_LENGTH))
                 {
                    if ((*ptr == '\\') &&
                        ((*(ptr + 1) == ' ') || (*(ptr + 1) == '#') ||
                         (*(ptr + 1) == '\t')))
                    {
                       ptr++;
                    }
                    ptr++; k++;
                 }
                 if (k == MAX_FILENAME_LENGTH)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "The 'rename to' part for option %s is to long (%d)",
                                  SRENAME_ID, MAX_FILENAME_LENGTH);
                    return(INCORRECT);
                 }
              }
           }
        }
   else if ((CHECK_STRNCMP(option, AGE_LIMIT_ID, AGE_LIMIT_ID_LENGTH) == 0) &&
            ((*(option + AGE_LIMIT_ID_LENGTH) == ' ') ||
             (*(option + AGE_LIMIT_ID_LENGTH) == '\t')))
        {
           ptr += AGE_LIMIT_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No age limit for option %s specified.",
                            AGE_LIMIT_ID);
              return(INCORRECT);
           }
           else
           {
              int i;

              i = 0;
              while ((isdigit((int)(*(ptr + i)))) && (i < MAX_INT_LENGTH))
              {
                 i++;
              }
              if (i == MAX_INT_LENGTH)
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Value for %s option to large.", AGE_LIMIT_ID);
                 return(INCORRECT);
              }
              if (i == 0)
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Invalid (%s) age limit specified.", ptr);
                 return(INCORRECT);
              }
              switch (*(ptr + i))
              {
                 case '\0' :
                 case ' '  :
                 case '\t' :
                    /* OK */;
                    break;
                 default   : /* Incorrect */
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Invalid age limit specified.");
                    return(INCORRECT);
              }
           }
        }
   else if ((CHECK_STRNCMP(option, AGEING_ID, AGEING_ID_LENGTH) == 0) &&
            ((*(option + AGEING_ID_LENGTH) == ' ') ||
             (*(option + AGEING_ID_LENGTH) == '\t')))
        {
           ptr += AGEING_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if ((*ptr < '0') || (*ptr > '9') ||
               ((*(ptr + 1) != '\0') && (*(ptr + 1) != ' ')))
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                         "Invalid %s value.", AGEING_ID);
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, ULOCK_ID, ULOCK_ID_LENGTH) == 0) &&
            ((*(option + ULOCK_ID_LENGTH) == ' ') ||
             (*(option + ULOCK_ID_LENGTH) == '\t')))
        {
           ptr += ULOCK_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No %s type specified.", ULOCK_ID);
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, TRANS_SRENAME_ID,
                           TRANS_SRENAME_ID_LENGTH) == 0) &&
            ((*(option + TRANS_SRENAME_ID_LENGTH) == ' ') ||
             (*(option + TRANS_SRENAME_ID_LENGTH) == '\t')))
        {
           int k = 0;

           ptr += TRANS_SRENAME_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No filter and 'rename to' specified for %s",
                            TRANS_SRENAME_ID);
              return(INCORRECT);
           }
           while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
                  (*ptr != '\0') && (k < MAX_FILENAME_LENGTH))
           {
              if ((*ptr == '\\') &&
                  ((*(ptr + 1) == ' ') || (*(ptr + 1) == '#') ||
                   (*(ptr + 1) == '\t')))
              {
                 ptr++;
              }
              ptr++; k++;
           }
           if ((*ptr != ' ') && (*ptr != '\t'))
           {
              if (k == MAX_FILENAME_LENGTH)
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "The filter for option %s is to long (%d)",
                               TRANS_SRENAME_ID, MAX_FILENAME_LENGTH);
              }
              else
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "No filter specified for %s", TRANS_SRENAME_ID);
              }
              return(INCORRECT);
           }
           else
           {
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              if (*ptr == '\0')
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "No 'rename to' part specified for option %s.",
                               TRANS_SRENAME_ID);
                 return(INCORRECT);
              }
              else
              {
                 k = 0;
                 while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\n') &&
                        (*ptr != '\0') && (k < MAX_FILENAME_LENGTH))
                 {
                    if ((*ptr == '\\') &&
                        ((*(ptr + 1) == ' ') || (*(ptr + 1) == '#') ||
                         (*(ptr + 1) == '\t')))
                    {
                       ptr++;
                    }
                    ptr++; k++;
                 }
                 if (k == MAX_FILENAME_LENGTH)
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "The 'rename to' part for option %s is to long (%d)",
                                  TRANS_SRENAME_ID, MAX_FILENAME_LENGTH);
                    return(INCORRECT);
                 }

                 /* primary_only/secondary_only/dupcheck */
                 if ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    /* primary_only/secondary_only */
                    if ((*ptr == '\0') ||
                        ((*ptr == 'p') && (*(ptr + 1) == 'r') && (*(ptr + 2) == 'i') &&
                         (*(ptr + 3) == 'm') && (*(ptr + 4) == 'a') &&
                         (*(ptr + 5) == 'r') && (*(ptr + 6) == 'y') &&
                         (*(ptr + 7) == '_') && (*(ptr + 8) == 'o') &&
                         (*(ptr + 9) == 'n') && (*(ptr + 10) == 'l') &&
                         (*(ptr + 11) == 'y') && (*(ptr + 12) == '\0')) ||
                        ((*ptr == 's') && (*(ptr + 1) == 'e') && (*(ptr + 2) == 'c') &&
                         (*(ptr + 3) == 'o') && (*(ptr + 4) == 'n') &&
                         (*(ptr + 5) == 'd') && (*(ptr + 6) == 'a') &&
                         (*(ptr + 7) == 'r') && (*(ptr + 8) == 'y') &&
                         (*(ptr + 9) == '_') && (*(ptr + 10) == 'o') &&
                         (*(ptr + 11) == 'n') && (*(ptr + 12) == 'l') &&
                         (*(ptr + 13) == 'y') && (*(ptr + 14) == '\0'))
#ifdef WITH_DUP_CHECK
                         || (CHECK_STRNCMP(ptr, DUPCHECK_ID, DUPCHECK_ID_LENGTH) == 0))
#else
                         )
#endif
                    {
                       /* OK */;
                    }
                    else
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     "Unknown data behind option %s.",
                                     TRANS_RENAME_ID);
                       return(INCORRECT);
                    }
                 }
              }
           }
        }
   else if ((CHECK_STRNCMP(option, TRANS_RENAME_ID,
                           TRANS_RENAME_ID_LENGTH) == 0) &&
            ((*(option + TRANS_RENAME_ID_LENGTH) == ' ') ||
             (*(option + TRANS_RENAME_ID_LENGTH) == '\t')))
        {
           char *p_rename_rule,
                *tmp_ptr;

           ptr += TRANS_RENAME_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           p_rename_rule = ptr;
           while ((*ptr != '\0') && (*ptr != ' ') && (*ptr != '\t'))
           {
              ptr++;
           }
           if ((*ptr == ' ') || (*ptr == '\t'))
           {
              *ptr = '\0';
              tmp_ptr = ptr;
              ptr++;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              /* primary_only/secondary_only */
              if ((*ptr == '\0') ||
                  ((*ptr == 'p') && (*(ptr + 1) == 'r') && (*(ptr + 2) == 'i') &&
                   (*(ptr + 3) == 'm') && (*(ptr + 4) == 'a') &&
                   (*(ptr + 5) == 'r') && (*(ptr + 6) == 'y') &&
                   (*(ptr + 7) == '_') && (*(ptr + 8) == 'o') &&
                   (*(ptr + 9) == 'n') && (*(ptr + 10) == 'l') &&
                   (*(ptr + 11) == 'y') && (*(ptr + 12) == '\0')) ||
                  ((*ptr == 's') && (*(ptr + 1) == 'e') && (*(ptr + 2) == 'c') &&
                   (*(ptr + 3) == 'o') && (*(ptr + 4) == 'n') &&
                   (*(ptr + 5) == 'd') && (*(ptr + 6) == 'a') &&
                   (*(ptr + 7) == 'r') && (*(ptr + 8) == 'y') &&
                   (*(ptr + 9) == '_') && (*(ptr + 10) == 'o') &&
                   (*(ptr + 11) == 'n') && (*(ptr + 12) == 'l') &&
                   (*(ptr + 13) == 'y') && (*(ptr + 14) == '\0'))
#ifdef WITH_DUP_CHECK
                   || (CHECK_STRNCMP(ptr, DUPCHECK_ID, DUPCHECK_ID_LENGTH) == 0))
#else
                   )
#endif
              {
                 /* OK */;
              }
              else
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Unknown data behind option %s.",
                               TRANS_RENAME_ID);
                 return(INCORRECT);
              }
           }
           else
           {
              tmp_ptr = NULL;
           }
           if (check_rule(p_rename_rule, cmd_fp) == INCORRECT)
           {
              return(INCORRECT);
           }
           if (tmp_ptr != NULL)
           {
              *tmp_ptr = ' ';
           }
        }
   else if (CHECK_STRNCMP(option, EXEC_ID, EXEC_ID_LENGTH) == 0)
        {
           ptr += EXEC_ID_LENGTH;
           if ((*ptr == 'd') || (*ptr == 'D'))
           {
              ptr++;
              if ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 if (*ptr == '\0')
                 {
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Nothing to execute.");
                    return(INCORRECT);
                 }
              }
              else
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Unknown option.");
                 return(INCORRECT);
              }
           }
           else
           {
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
                 while ((*ptr == ' ') || (*ptr == '\t'))
                 {
                    ptr++;
                 }
                 if (*ptr == '-')
                 {
                    switch (*(ptr + 1))
                    {
                       case 'd' :
                       case 'D' :
                       case 'l' :
                       case 'L' :
                       case 's' :
                          ptr += 2;
                          if ((*ptr != ' ') && (*ptr != '\t'))
                          {
                             update_db_log(WARN_SIGN, __FILE__, __LINE__,
                                           cmd_fp, NULL,
                                           "Unknown parameter `%s' in %s option.",
                                           ptr - 2, EXEC_ID);
                             return(INCORRECT);
                          }
                          break;

                       case 't' :
                          ptr += 2;
                          if ((*ptr == ' ') || (*ptr == '\t'))
                          {
                             int i;

                             ptr++;
                             i = 0;
                             while ((isdigit((int)(*(ptr + i)))) &&
                                    (i < MAX_INT_LENGTH))
                             {
                                i++;
                             }
                             if (i > 0)
                             {
                                if (i < MAX_INT_LENGTH)
                                {
                                   ptr += i;
                                   if ((*ptr != ' ') && (*ptr != '\t'))
                                   {
                                      update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                                    "Nothing to execute.");
                                      return(INCORRECT);
                                   }
                                }
                                else
                                {
                                   update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                                 "Time specified to long, may only be %d bytes long.",
                                                 MAX_INT_LENGTH - 1);
                                   return(INCORRECT);
                                }
                             }
                             else
                             {
                                update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                              "No time specified.");
                                return(INCORRECT);
                             }
                          }
                          else
                          {
                             update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                           "No time specified.");
                             return(INCORRECT);
                          }
                          break;

                       default: /* Unknown option. */
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        "Unknown %s parameter -%c",
                                        EXEC_ID, *(ptr + 1));
                          return(INCORRECT);
                    }
                 }
                 else if (*ptr == '\0')
                      {
                         update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                       "Nothing to execute.");
                         return(INCORRECT);
                      }
              }

              if (*ptr == '\0')
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Nothing to execute.");
                 return(INCORRECT);
              }
           }
        }
   else if ((CHECK_STRNCMP(option, TIME_NO_COLLECT_ID,
                           TIME_NO_COLLECT_ID_LENGTH) == 0) &&
            ((*(option + TIME_NO_COLLECT_ID_LENGTH) == ' ') ||
             (*(option + TIME_NO_COLLECT_ID_LENGTH) == '\t')))
        {
           ptr += TIME_NO_COLLECT_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (check_time_str(ptr, cmd_fp) == INCORRECT)
           {
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, TIME_ID, TIME_ID_LENGTH) == 0) &&
            ((*(option + TIME_ID_LENGTH) == ' ') ||
             (*(option + TIME_ID_LENGTH) == '\t')))
        {
           ptr += TIME_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (check_time_str(ptr, cmd_fp) == INCORRECT)
           {
              return(INCORRECT);
           }
        }
#ifdef WITH_TIMEZONE
   else if ((CHECK_STRNCMP(option, TIMEZONE_ID, TIMEZONE_ID_LENGTH) == 0) &&
            ((*(option + TIMEZONE_ID_LENGTH) == ' ') ||
             (*(option + TIMEZONE_ID_LENGTH) == '\t')))
        {
           int  i;
# ifdef TZDIR
           char timezone_name[MAX_TIMEZONE_LENGTH + 1];
# endif

           ptr += TIMEZONE_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }

           i = 0;
           while ((*(ptr + i) != '\0') && (isascii((int)(*(ptr + i)))) &&
                  (i < MAX_TIMEZONE_LENGTH))
           {
# ifdef TZDIR
              timezone_name[i] = *(ptr + i);
# endif
              i++;
           }
           if (i == MAX_TIMEZONE_LENGTH)
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "Value for %s option to large.", TIMEZONE_ID);
              return(INCORRECT);
           }
           if (i == 0)
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "Invalid (%s) %s specified.", ptr, TIMEZONE_ID);
              return(INCORRECT);
           }
# ifdef TZDIR
           timezone_name[i] = '\0';
           if (timezone_name_check(timezone_name) == INCORRECT)
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "Unable to find specified timezone (%s) in %s",
                            timezone_name, TZDIR);
           }
# endif
        }
#endif
#ifdef _WITH_TRANS_EXEC
   else if ((CHECK_STRNCMP(option, TRANS_EXEC_ID, TRANS_EXEC_ID_LENGTH) == 0) &&
            ((*(option + TRANS_EXEC_ID_LENGTH) == ' ') ||
             (*(option + TRANS_EXEC_ID_LENGTH) == '\t')))
        {
           ptr += TRANS_EXEC_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              if (*ptr == '-')
              {
                 switch (*(ptr + 1))
                 {
                    case 'l' :
                    case 'L' :
                       ptr += 2;
                       if ((*ptr != ' ') && (*ptr != '\t'))
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        "Unknown paramter `%s' in %s option.",
                                        ptr - 2, TRANS_EXEC_ID);
                          return(INCORRECT);
                       }
                       break;

                    case 't' :
                       ptr += 2;
                       if ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          int i;

                          ptr++;
                          i = 0;
                          while ((isdigit((int)(*(ptr + i)))) &&
                                 (i < MAX_INT_LENGTH))
                          {
                             i++;
                          }
                          if (i > 0)
                          {
                             if (i < MAX_INT_LENGTH)
                             {
                                ptr += i;
                                if ((*ptr != ' ') && (*ptr != '\t'))
                                {
                                   update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                                 "Nothing to execute.");
                                   return(INCORRECT);
                                }
                             }
                             else
                             {
                                update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                              "Time specified to long, may only be %d bytes long.",
                                              MAX_INT_LENGTH - 1);
                                return(INCORRECT);
                             }
                          }
                          else
                          {
                             update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                           "No time specified.");
                             return(INCORRECT);
                          }
                       }
                       else
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        "No time specified.");
                          return(INCORRECT);
                       }
                       break;

                    default: /* Unknown option. */
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     "Unknown %s parameter -%c",
                                     TRANS_EXEC_ID, *(ptr + 1));
                       return(INCORRECT);
                 }
              }
              else if (*ptr == '\0')
                   {
                      update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                    "Nothing to execute.");
                      return(INCORRECT);
                   }
           }

           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "Nothing to execute.");
              return(INCORRECT);
           }
        }
#endif /* _WITH_TRANS_EXEC */
   else if ((CHECK_STRNCMP(option, ADD_PREFIX_ID, ADD_PREFIX_ID_LENGTH) == 0) &&
            ((*(option + ADD_PREFIX_ID_LENGTH) == ' ') ||
             (*(option + ADD_PREFIX_ID_LENGTH) == '\t')))
        {
           ptr += ADD_PREFIX_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No prefix to add found.");
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, DEL_PREFIX_ID, DEL_PREFIX_ID_LENGTH) == 0) &&
            ((*(option + DEL_PREFIX_ID_LENGTH) == ' ') ||
             (*(option + DEL_PREFIX_ID_LENGTH) == '\t')))
        {
           ptr += DEL_PREFIX_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No prefix to delete found.");
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, FILE_NAME_IS_USER_ID,
                           FILE_NAME_IS_USER_ID_LENGTH) == 0) &&
            ((*(option + FILE_NAME_IS_USER_ID_LENGTH) == '\0') ||
             (*(option + FILE_NAME_IS_USER_ID_LENGTH) == ' ') ||
             (*(option + FILE_NAME_IS_USER_ID_LENGTH) == '\t')))
        {
           if (*(option + FILE_NAME_IS_USER_ID_LENGTH) != '\0')
           {
              ptr += FILE_NAME_IS_USER_ID_LENGTH + 1;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              if (check_rule(ptr, cmd_fp) == INCORRECT)
              {
                 return(INCORRECT);
              }
           }
        }
   else if ((CHECK_STRNCMP(option, FILE_NAME_IS_TARGET_ID,
                           FILE_NAME_IS_TARGET_ID_LENGTH) == 0) &&
            ((*(option + FILE_NAME_IS_TARGET_ID_LENGTH) == '\0') ||
             (*(option + FILE_NAME_IS_TARGET_ID_LENGTH) == ' ') ||
             (*(option + FILE_NAME_IS_TARGET_ID_LENGTH) == '\t')))
        {
           if (*(option + FILE_NAME_IS_TARGET_ID_LENGTH) != '\0')
           {
              ptr += FILE_NAME_IS_TARGET_ID_LENGTH + 1;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              if (check_rule(ptr, cmd_fp) == INCORRECT)
              {
                 return(INCORRECT);
              }
           }
        }
   else if ((CHECK_STRNCMP(option, GRIB2WMO_ID, GRIB2WMO_ID_LENGTH) == 0) &&
            ((*(option + GRIB2WMO_ID_LENGTH) == ' ') ||
             (*(option + GRIB2WMO_ID_LENGTH) == '\0') ||
             (*(option + GRIB2WMO_ID_LENGTH) == '\t')))
        {
           if (*(option + GRIB2WMO_ID_LENGTH) != '\0')
           {
              ptr += GRIB2WMO_ID_LENGTH + 1;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              if ((isalpha((int)(*ptr))) && (isalpha((int)(*(ptr + 1)))) &&
                  (isalpha((int)(*(ptr + 2)))) && (isalpha((int)(*(ptr + 3)))) &&
                  ((*(ptr + 4) == '\0') || (*(ptr + 4) == ' ') ||
                   (*(ptr + 4) == '\t')))
              {
                 /* OK */;
              }
              else
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Not a valid CCCC `%s' for %s.", ptr, GRIB2WMO_ID);
                 return(INCORRECT);
              }
           }
        }
   else if ((CHECK_STRNCMP(option, ASSEMBLE_ID, ASSEMBLE_ID_LENGTH) == 0) &&
            ((*(option + ASSEMBLE_ID_LENGTH) == ' ') ||
             (*(option + ASSEMBLE_ID_LENGTH) == '\t')))
        {
           ptr += ASSEMBLE_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }

           /* VAX, LBF, HBF, MSS, DWD, WMO and ASCII. */
           if (((((*ptr == 'V') && (*(ptr + 1) == 'A') && (*(ptr + 2) == 'X')) ||
                 (((*ptr == 'L') || (*ptr == 'H')) && (*(ptr + 1) == 'B') && (*(ptr + 2) == 'F')) ||
                 ((*ptr == 'M') && (*(ptr + 1) == 'S') && (*(ptr + 2) == 'S')) ||
                 ((*ptr == 'D') && (*(ptr + 1) == 'W') && (*(ptr + 2) == 'D')) ||
                 ((*ptr == 'W') && (*(ptr + 1) == 'M') && (*(ptr + 2) == 'O'))) &&
                ((*(ptr + 3) == ' ') || (*(ptr + 3) == '+') || (*(ptr + 3) == '\t') || (*(ptr + 3) == '\0'))) ||
               ((*ptr == 'A') && (*(ptr + 1) == 'S') && (*(ptr + 2) == 'C') &&
                (*(ptr + 3) == 'I') && (*(ptr + 4) == 'I') &&
                ((*(ptr + 5) == ' ') || (*(ptr + 5) == '+') || (*(ptr + 5) == '\t') || (*(ptr + 5) == '\0'))) ||
               ((*ptr == 'W') && (*(ptr + 1) == 'M') && (*(ptr + 2) == 'O') &&
                (*(ptr + 3) == '+') && (*(ptr + 4) == 'D') &&
                (*(ptr + 5) == 'U') && (*(ptr + 6) == 'M') &&
                (*(ptr + 7) == 'M') && (*(ptr + 8) == 'Y') &&
                ((*(ptr + 9) == ' ') || (*(ptr + 9) == '+') || (*(ptr + 9) == '\t') || (*(ptr + 9) == '\0'))))
           {
              /* OK */;
           }
           else
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "Unknown %s type `%s'.", ASSEMBLE_ID, ptr);
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, CONVERT_ID, CONVERT_ID_LENGTH) == 0) &&
            ((*(option + CONVERT_ID_LENGTH) == ' ') ||
             (*(option + CONVERT_ID_LENGTH) == '\t')))
        {
           ptr += CONVERT_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }

           /*
            * sohetx, sohetxwmo, wmo, sohetx2wmo0, sohetx2wmo1
            * mrz2wmo, iso8859_2ascii, unix2dos, dos2unix,
            * lf2crcrlf and crcrlf2lf.
            */
           if (((*ptr == 's') && (*(ptr + 1) == 'o') && (*(ptr + 2) == 'h') &&
                (*(ptr + 3) == 'e') && (*(ptr + 4) == 't') &&
                (*(ptr + 5) == 'x') &&
                ((*(ptr + 6) == '\0') || (*(ptr + 6) == ' ') ||
                 (*(ptr + 6) == '\t') || (*(ptr + 6) == '+'))) ||
               ((*ptr == 's') && (*(ptr + 1) == 'o') && (*(ptr + 2) == 'h') &&
                (*(ptr + 3) == 'e') && (*(ptr + 4) == 't') &&
                (*(ptr + 5) == 'x') && (*(ptr + 6) == 'w') &&
                (*(ptr + 7) == 'm') && (*(ptr + 8) == 'o') &&
                ((*(ptr + 9) == '\0') || (*(ptr + 9) == ' ') ||
                 (*(ptr + 9) == '\t') || (*(ptr + 9) == '+'))) ||
               ((*ptr == 'w') && (*(ptr + 1) == 'm') && (*(ptr + 2) == 'o') &&
                ((*(ptr + 3) == '\0') || (*(ptr + 3) == ' ') ||
                 (*(ptr + 3) == '\t') || (*(ptr + 3) == '+'))) ||
               ((*ptr == 's') && (*(ptr + 1) == 'o') && (*(ptr + 2) == 'h') &&
                (*(ptr + 3) == 'e') && (*(ptr + 4) == 't') &&
                (*(ptr + 5) == 'x') && (*(ptr + 6) == '2') &&
                (*(ptr + 7) == 'w') && (*(ptr + 8) == 'm') &&
                (*(ptr + 9) == 'o') &&
                ((*(ptr + 10) == '0') || (*(ptr + 10) == '1')) &&
                ((*(ptr + 11) == '\0') || (*(ptr + 11) == ' ') ||
                 (*(ptr + 11) == '\t') || (*(ptr + 11) == '+'))) ||
               ((*ptr == 'm') && (*(ptr + 1) == 'r') && (*(ptr + 2) == 'z') &&
                (*(ptr + 3) == '2') && (*(ptr + 4) == 'w') &&
                (*(ptr + 5) == 'm') && (*(ptr + 6) == 'o') &&
                ((*(ptr + 7) == '\0') || (*(ptr + 7) == ' ') ||
                 (*(ptr + 7) == '\t'))) ||
               /* iso8859_2ascii */
               ((*ptr == 'i') && (*(ptr + 1) == 's') && (*(ptr + 2) == 'o') &&
                (*(ptr + 3) == '8') && (*(ptr + 4) == '8') &&
                (*(ptr + 5) == '5') && (*(ptr + 6) == '9') &&
                (*(ptr + 7) == '_') && (*(ptr + 8) == '2') &&
                (*(ptr + 9) == 'a') && (*(ptr + 10) == 's') &&
                (*(ptr + 11) == 'c') && (*(ptr + 12) == 'i') &&
                (*(ptr + 13) == 'i') &&
                ((*(ptr + 14) == '\0') || (*(ptr + 14) == ' ') ||
                 (*(ptr + 14) == '\t'))) ||
               ((*ptr == 'u') && (*(ptr + 1) == 'n') && (*(ptr + 2) == 'i') &&
                (*(ptr + 3) == 'x') && (*(ptr + 4) == '2') &&
                (*(ptr + 5) == 'd') && (*(ptr + 6) == 'o') &&
                (*(ptr + 7) == 's') &&
                ((*(ptr + 8) == '\0') || (*(ptr + 8) == ' ') ||
                 (*(ptr + 8) == '\t'))) ||
               ((*ptr == 'd') && (*(ptr + 1) == 'o') && (*(ptr + 2) == 's') &&
                (*(ptr + 3) == '2') && (*(ptr + 4) == 'u') &&
                (*(ptr + 5) == 'n') && (*(ptr + 6) == 'i') &&
                (*(ptr + 7) == 'x') &&
                ((*(ptr + 8) == '\0') || (*(ptr + 8) == ' ') ||
                 (*(ptr + 8) == '\t'))) ||
               ((*ptr == 'l') && (*(ptr + 1) == 'f') && (*(ptr + 2) == '2') &&
                (*(ptr + 3) == 'c') && (*(ptr + 4) == 'r') &&
                (*(ptr + 5) == 'c') && (*(ptr + 6) == 'r') &&
                (*(ptr + 7) == 'l') && (*(ptr + 8) == 'f') &&
                ((*(ptr + 9) == '\0') || (*(ptr + 9) == ' ') ||
                 (*(ptr + 9) == '\t'))) ||
               ((*ptr == 'c') && (*(ptr + 1) == 'r') && (*(ptr + 2) == 'c') &&
                (*(ptr + 3) == 'r') && (*(ptr + 4) == 'l') &&
                (*(ptr + 5) == 'f') && (*(ptr + 6) == '2') &&
                (*(ptr + 7) == 'l') && (*(ptr + 8) == 'f') &&
                ((*(ptr + 9) == '\0') || (*(ptr + 9) == ' ') ||
                 (*(ptr + 9) == '\t'))))
           {
              /* OK */;
           }
           else
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "Unknown %s type `%s'.", CONVERT_ID, ptr);
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, EXTRACT_ID, EXTRACT_ID_LENGTH) == 0) &&
            ((*(option + EXTRACT_ID_LENGTH) == ' ') ||
             (*(option + EXTRACT_ID_LENGTH) == '\t')))
        {
           ptr += EXTRACT_ID_LENGTH;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              if (*ptr == '-')
              {
                 switch (*(ptr + 1))
                 {
                    case 'a' :
                    case 'A' :
                    case 'b' :
                    case 'B' :
                    case 'c' :
                    case 'C' :
                    case 'd' :
                    case 'D' :
                    case 'e' :
                    case 'E' :
                    case 'f' :
                    case 'F' :
                    case 'H' :
                    case 'n' :
                    case 'N' :
                    case 'r' :
                    case 'R' :
                    case 's' :
                    case 'S' :
                    case 't' :
                    case 'T' :
                       ptr += 2;
                       if ((*ptr != ' ') && (*ptr != '\t'))
                       {
                          update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                        "No %s type specified.", EXTRACT_ID);
                          return(INCORRECT);
                       }
                       break;

                    default: /* Unknown option. */
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     "Unknown %s parameter -%c",
                                     EXTRACT_ID, *(ptr + 1));
                       return(INCORRECT);
                 }
              }
              else if (*ptr == '\0')
                   {
                      update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                    "No %s type specified.", EXTRACT_ID);
                      return(INCORRECT);
                   }
           }

           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No %s type specified.", EXTRACT_ID);
              return(INCORRECT);
           }
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }

           /* VAX, LBF, HBF, MRZ, MSS, WMO, ASCII, BINARY, ZCZC, GRIB, */
           /* WMO+CHK and SP_CHAR                                      */
           if (((((*ptr == 'V') && (*(ptr + 1) == 'A') && (*(ptr + 2) == 'X')) ||
                 (((*ptr == 'L') || (*ptr == 'H')) && (*(ptr + 1) == 'B') && (*(ptr + 2) == 'F')) ||
                 ((*ptr == 'M') && (*(ptr + 1) == 'R') && (*(ptr + 2) == 'Z')) ||
                 ((*ptr == 'M') && (*(ptr + 1) == 'S') && (*(ptr + 2) == 'S')) ||
                 ((*ptr == 'W') && (*(ptr + 1) == 'M') && (*(ptr + 2) == 'O'))) &&
                ((*(ptr + 3) == ' ') || (*(ptr + 3) == '\t') || (*(ptr + 3) == '\0'))) ||
               ((*ptr == 'A') && (*(ptr + 1) == 'S') && (*(ptr + 2) == 'C') &&
                (*(ptr + 3) == 'I') && (*(ptr + 4) == 'I') &&
                ((*(ptr + 5) == ' ') || (*(ptr + 5) == '\t') || (*(ptr + 5) == '\0'))) ||
               ((*ptr == 'B') && (*(ptr + 1) == 'I') && (*(ptr + 2) == 'N') &&
                (*(ptr + 3) == 'A') && (*(ptr + 4) == 'R') && (*(ptr + 5) == 'Y') &&
                ((*(ptr + 6) == ' ') || (*(ptr + 6) == '\t') || (*(ptr + 6) == '\0'))) ||
               ((((*ptr == 'Z') && (*(ptr + 1) == 'C') && (*(ptr + 2) == 'Z') &&
                  (*(ptr + 3) == 'C')) ||
                 ((*ptr == 'G') && (*(ptr + 1) == 'R') && (*(ptr + 2) == 'I') &&
                  (*(ptr + 3) == 'B'))) &&
                ((*(ptr + 4) == ' ') || (*(ptr + 4) == '\t') || (*(ptr + 4) == '\0'))) ||
               ((*ptr == 'W') && (*(ptr + 1) == 'M') && (*(ptr + 2) == 'O') &&
                (*(ptr + 3) == '+') && (*(ptr + 4) == 'C') &&
                (*(ptr + 5) == 'H') && (*(ptr + 6) == 'K') &&
                ((*(ptr + 7) == ' ') || (*(ptr + 7) == '\t') || (*(ptr + 7) == '\0'))) ||
               ((*ptr == 'S') && (*(ptr + 1) == 'P') && (*(ptr + 2) == '_') &&
                (*(ptr + 3) == 'C') && (*(ptr + 4) == 'H') &&
                (*(ptr + 5) == 'A') && (*(ptr + 6) == 'R') &&
                ((*(ptr + 7) == ' ') || (*(ptr + 7) == '\t') || (*(ptr + 7) == '\0'))))
           {
              /* OK */;
           }
           else
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "Unknown %s type `%s'.", EXTRACT_ID, ptr);
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, LCHMOD_ID, LCHMOD_ID_LENGTH) == 0) &&
            ((*(option + LCHMOD_ID_LENGTH) == ' ') ||
             (*(option + LCHMOD_ID_LENGTH) == '\t')))
        {
           ptr += LCHMOD_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No mode specified for option %s.", LCHMOD_ID);
              return(INCORRECT);
           }
           else
           {
              if (((*ptr >= '0') && (*ptr < '8')) &&
                  ((*(ptr + 1) >= '0') && (*(ptr + 1) < '8')) &&
                  ((*(ptr + 2) >= '0') && (*(ptr + 2) < '8')) &&
                  (((*(ptr + 3) >= '0') && (*(ptr + 3) < '8') &&
                   (*(ptr + 4) == '\0')) || (*(ptr + 3) == '\0')))
              {
                 /* OK */;
              }
              else
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Incorrect mode for option %s, only three or four octal numbers possible.",
                               LCHMOD_ID);
                 return(INCORRECT);
              }
           }
        }
   else if ((CHECK_STRNCMP(option, CHMOD_ID, CHMOD_ID_LENGTH) == 0) &&
            ((*(option + CHMOD_ID_LENGTH) == ' ') ||
             (*(option + CHMOD_ID_LENGTH) == '\t')))
        {
           ptr += CHMOD_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No mode specified for option %s.", CHMOD_ID);
              return(INCORRECT);
           }
           else
           {
              if (((*ptr >= '0') && (*ptr < '8')) &&
                  ((*(ptr + 1) >= '0') && (*(ptr + 1) < '8')) &&
                  ((*(ptr + 2) >= '0') && (*(ptr + 2) < '8')) &&
                  (((*(ptr + 3) >= '0') && (*(ptr + 3) < '8') &&
                   (*(ptr + 4) == '\0')) || (*(ptr + 3) == '\0')))
              {
                 /* OK */;
              }
              else
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Incorrect mode for option %s, only three or four octal numbers possible.",
                               CHMOD_ID);
                 return(INCORRECT);
              }
           }
        }
   else if ((CHECK_STRNCMP(option, REMOTE_HARDLINK_ID,
                           REMOTE_HARDLINK_ID_LENGTH) == 0) &&
            ((*(option + REMOTE_HARDLINK_ID_LENGTH) == ' ') ||
             (*(option + REMOTE_HARDLINK_ID_LENGTH) == '\t')))
        {
           ptr += REMOTE_HARDLINK_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No new name specified for option %s.",
                            REMOTE_HARDLINK_ID);
              return(INCORRECT);
           }
           else
           {
              /* OK */;
           }
        }
   else if ((CHECK_STRNCMP(option, REMOTE_SYMLINK_ID,
                           REMOTE_SYMLINK_ID_LENGTH) == 0) &&
            ((*(option + REMOTE_SYMLINK_ID_LENGTH) == ' ') ||
             (*(option + REMOTE_SYMLINK_ID_LENGTH) == '\t')))
        {
           ptr += REMOTE_SYMLINK_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No new name specified for option %s.",
                            REMOTE_SYMLINK_ID);
              return(INCORRECT);
           }
           else
           {
              /* OK */;
           }
        }
   else if (CHECK_STRNCMP(option, CREATE_TARGET_DIR_ID, CREATE_TARGET_DIR_ID_LENGTH) == 0)
        {
           ptr += CREATE_TARGET_DIR_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              /* It's OK if no mode is specified. */;
           }
           else
           {
              if (((*ptr >= '0') && (*ptr < '8')) &&
                  ((*(ptr + 1) >= '0') && (*(ptr + 1) < '8')) &&
                  ((*(ptr + 2) >= '0') && (*(ptr + 2) < '8')) &&
                  (((*(ptr + 3) >= '0') && (*(ptr + 3) < '8') &&
                   (*(ptr + 4) == '\0')) || (*(ptr + 3) == '\0')))
              {
                 /* OK */;
              }
              else
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Incorrect mode for option %s, only three or four octal numbers possible.",
                               CREATE_TARGET_DIR_ID);
                 return(INCORRECT);
              }
           }
        }
   else if ((CHECK_STRNCMP(option, CHOWN_ID, CHOWN_ID_LENGTH) == 0) &&
            ((*(option + CHOWN_ID_LENGTH) == ' ') ||
             (*(option + CHOWN_ID_LENGTH) == '\t')))
        {
           ptr += CHOWN_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No user or group specified.");
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, ATTACH_FILE_ID, ATTACH_FILE_ID_LENGTH) == 0) &&
            ((*(option + ATTACH_FILE_ID_LENGTH) == '\0') ||
             (*(option + ATTACH_FILE_ID_LENGTH) == ' ') ||
             (*(option + ATTACH_FILE_ID_LENGTH) == '\t')))
        {
           if (*(option + ATTACH_FILE_ID_LENGTH) != '\0')
           {
              ptr += ATTACH_FILE_ID_LENGTH + 1;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              if (check_rule(ptr, cmd_fp) == INCORRECT)
              {
                 return(INCORRECT);
              }
           }
        }
   else if ((CHECK_STRNCMP(option, ATTACH_ALL_FILES_ID,
                           ATTACH_ALL_FILES_ID_LENGTH) == 0) &&
            ((*(option + ATTACH_ALL_FILES_ID_LENGTH) == '\0') ||
             (*(option + ATTACH_ALL_FILES_ID_LENGTH) == ' ') ||
             (*(option + ATTACH_ALL_FILES_ID_LENGTH) == '\t')))
        {
           if (*(option + ATTACH_ALL_FILES_ID_LENGTH) != '\0')
           {
              ptr += ATTACH_ALL_FILES_ID_LENGTH + 1;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              if (check_rule(ptr, cmd_fp) == INCORRECT)
              {
                 return(INCORRECT);
              }
           }
        }
   else if ((CHECK_STRNCMP(option, RENAME_FILE_BUSY_ID,
                           RENAME_FILE_BUSY_ID_LENGTH) == 0) &&
            ((*(option + RENAME_FILE_BUSY_ID_LENGTH) == ' ') ||
             (*(option + RENAME_FILE_BUSY_ID_LENGTH) == '\t')))
        {
           ptr += RENAME_FILE_BUSY_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if ((isascii((int)(*ptr))) &&
               ((*(ptr + 1) == '\0') || (*(ptr + 1) == ' ') ||
                (*(ptr + 1) == '\t')))
           {
              /* OK */;
           }
           else
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No character specified for option %s.",
                            RENAME_FILE_BUSY_ID);
              return(INCORRECT);
           }
        }
#ifdef WITH_DUP_CHECK
   else if (CHECK_STRNCMP(option, DUPCHECK_ID, DUPCHECK_ID_LENGTH) == 0)
        {
           int          warn;
           unsigned int flag;
           time_t       timeout;

           warn = 0;
           (void)eval_dupcheck_options(ptr, &timeout, &flag, &warn);
           if (warn)
           {
              return(INCORRECT);
           }
        }
#endif
   else if (CHECK_STRNCMP(option, SUBJECT_ID, SUBJECT_ID_LENGTH) == 0)
        {
           if ((*(option + SUBJECT_ID_LENGTH) == ' ') ||
               (*(option + SUBJECT_ID_LENGTH) == '\t'))
           {
              ptr += SUBJECT_ID_LENGTH + 1;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              if (*ptr == '"')
              {
                 ptr++;
#ifdef WITH_ASCII_ONLY_SUBJECT
                 while ((*ptr != '"') && (*ptr != '\0') && (isascii(*ptr)))
#else
                 while ((*ptr != '"') && (*ptr != '\0'))
#endif
                 {
                    ptr++;
                 }
                 if (*ptr == '"')
                 {
                    ptr++;
                    while ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       ptr++;
                    }
                    if (*ptr != '\0')
                    {
                       if (check_rule(ptr, cmd_fp) == INCORRECT)
                       {
                          return(INCORRECT);
                       }
                    }
                 }
                 else
                 {
#ifdef WITH_ASCII_ONLY_SUBJECT
                    if (*ptr == '\0')
                    {
#endif
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     "Subject line not terminated with a \" sign.");
#ifdef WITH_ASCII_ONLY_SUBJECT
                    }
                    else
                    {
                       update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                     "Subject line contains an illegal character (integer value = %d) that does not fit into the 7-bit ASCII character set.",
                                     (int)((unsigned char)*ptr));
                    }
#endif
                    return(INCORRECT);
                 }
              }
              else if (*ptr == '/')
                   {
                      char *p_start;

                      p_start = ptr;
                      while ((*ptr != '\0') && (*ptr != ' ') && (*ptr != '\t'))
                      {
                         if (*ptr == '\\')
                         {
                            ptr++;
                         }
                         ptr++;
                      }
                      if ((*ptr == ' ') || (*ptr == '\t'))
                      {
                         *ptr = '\0';
                         if (access(p_start, R_OK) != 0)
                         {
                            update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                          "Failed to access subject file `%s' : %s",
                                          p_start, strerror(errno));
                            *ptr = ' ';
                            return(INCORRECT);
                         }
                         *ptr = ' ';
                         ptr++;
                         while ((*ptr == ' ') || (*ptr == '\t'))
                         {
                            ptr++;
                         }
                         if (check_rule(ptr, cmd_fp) == INCORRECT)
                         {
                            return(INCORRECT);
                         }
                      }
                      else
                      {
                         if (access(p_start, R_OK) != 0)
                         {
                            update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                          "Failed to access subject file `%s' : %s",
                                          p_start, strerror(errno));
                            return(INCORRECT);
                         }
                      }
                   }
                   else
                   {
                      update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                    "Unknown data behind %s.", SUBJECT_ID);
                      return(INCORRECT);
                   }
           }
           else
           {
              if (*(option + SUBJECT_ID_LENGTH) == '\0')
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "No %s specified.", SUBJECT_ID);
              }
              else
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Unknown option.");
              }
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, ADD_MAIL_HEADER_ID,
                           ADD_MAIL_HEADER_ID_LENGTH) == 0) &&
            ((*(option + ADD_MAIL_HEADER_ID_LENGTH) == ' ') ||
             (*(option + ADD_MAIL_HEADER_ID_LENGTH) == '\t')))
        {
           ptr += ADD_MAIL_HEADER_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '"')
           {
              ptr++;
           }
           if ((*ptr == '\0') || (*ptr == '"'))
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No mail header file specified.");
              return(INCORRECT);
           }
           else
           {
              char *p_start,
                   tmp_char;

              p_start = ptr;
              while ((*ptr != '\0') && (*ptr != '"'))
              {
                 ptr++;
              }
              tmp_char = *ptr;
              *ptr = '\0';
              if (access(p_start, R_OK) != 0)
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Failed to access mail header file `%s' : %s",
                               p_start, strerror(errno));
                 *ptr = tmp_char;
                 return(INCORRECT);
              }
              *ptr = tmp_char;
           }
        }
   else if ((CHECK_STRNCMP(option, FROM_ID, FROM_ID_LENGTH) == 0) &&
            ((*(option + FROM_ID_LENGTH) == ' ') ||
             (*(option + FROM_ID_LENGTH) == '\t')))
        {
           ptr += FROM_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No mail address specified.");
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, REPLY_TO_ID, REPLY_TO_ID_LENGTH) == 0) &&
            ((*(option + REPLY_TO_ID_LENGTH) == ' ') ||
             (*(option + REPLY_TO_ID_LENGTH) == '\t')))
        {
           ptr += REPLY_TO_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No mail address specified.");
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, GROUP_TO_ID, GROUP_TO_ID_LENGTH) == 0) &&
            ((*(option + GROUP_TO_ID_LENGTH) == ' ') ||
             (*(option + GROUP_TO_ID_LENGTH) == '\t')))
        {
           ptr += GROUP_TO_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No mail address specified.");
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, CHARSET_ID, CHARSET_ID_LENGTH) == 0) &&
            ((*(option + CHARSET_ID_LENGTH) == ' ') ||
             (*(option + CHARSET_ID_LENGTH) == '\t')))
        {
           ptr += CHARSET_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No %s specified.", CHARSET_ID);
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, FTP_EXEC_CMD, FTP_EXEC_CMD_LENGTH) == 0) &&
            ((*(option + FTP_EXEC_CMD_LENGTH) == ' ') ||
             (*(option + FTP_EXEC_CMD_LENGTH) == '\t')))
        {
           ptr += FTP_EXEC_CMD_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No command to execute specified.");
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, LOGIN_SITE_CMD, LOGIN_SITE_CMD_LENGTH) == 0) &&
            ((*(option + LOGIN_SITE_CMD_LENGTH) == ' ') ||
             (*(option + LOGIN_SITE_CMD_LENGTH) == '\t')))
        {
           ptr += LOGIN_SITE_CMD_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No command to execute specified.");
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, LOCK_POSTFIX_ID, LOCK_POSTFIX_ID_LENGTH) == 0) &&
            ((*(option + LOCK_POSTFIX_ID_LENGTH) == ' ') ||
             (*(option + LOCK_POSTFIX_ID_LENGTH) == '\t')))
        {
           ptr += LOCK_POSTFIX_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No postfix specified for option %s.",
                            LOCK_POSTFIX_ID);
              return(INCORRECT);
           }
        }
   else if ((CHECK_STRNCMP(option, SOCKET_SEND_BUFFER_ID,
                           SOCKET_SEND_BUFFER_ID_LENGTH) == 0) &&
            ((*(option + SOCKET_SEND_BUFFER_ID_LENGTH) == ' ') ||
             (*(option + SOCKET_SEND_BUFFER_ID_LENGTH) == '\t')))
        {
           ptr += SOCKET_SEND_BUFFER_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No socket buffer size for option %s specified.",
                            SOCKET_SEND_BUFFER_ID);
              return(INCORRECT);
           }
           else
           {
              int i;

              i = 0;
              while ((isdigit((int)(*(ptr + i)))) && (i < MAX_INT_LENGTH))
              {
                 i++;
              }
              if (i == MAX_INT_LENGTH)
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Value for %s option to large.",
                               SOCKET_SEND_BUFFER_ID);
                 return(INCORRECT);
              }
              if (i == 0)
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Invalid (%s) socket buffer specified.", ptr);
                 return(INCORRECT);
              }
              switch (*(ptr + i))
              {
                 case '\0' :
                 case ' '  :
                 case '\t' :
                    /* OK */;
                    break;
                 default   : /* Incorrect */
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Invalid socket buffer specified.");
                    return(INCORRECT);
              }
           }
        }
   else if ((CHECK_STRNCMP(option, SOCKET_RECEIVE_BUFFER_ID,
                           SOCKET_RECEIVE_BUFFER_ID_LENGTH) == 0) &&
            ((*(option + SOCKET_RECEIVE_BUFFER_ID_LENGTH) == ' ') ||
             (*(option + SOCKET_RECEIVE_BUFFER_ID_LENGTH) == '\t')))
        {
           ptr += SOCKET_RECEIVE_BUFFER_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No socket buffer size for option %s specified.",
                            SOCKET_RECEIVE_BUFFER_ID);
              return(INCORRECT);
           }
           else
           {
              int i;

              i = 0;
              while ((isdigit((int)(*(ptr + i)))) && (i < MAX_INT_LENGTH))
              {
                 i++;
              }
              if (i == MAX_INT_LENGTH)
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Value for %s option to large.",
                               SOCKET_RECEIVE_BUFFER_ID);
                 return(INCORRECT);
              }
              if (i == 0)
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Invalid (%s) socket buffer specified.", ptr);
                 return(INCORRECT);
              }
              switch (*(ptr + i))
              {
                 case '\0' :
                 case ' '  :
                 case '\t' :
                    /* OK */;
                    break;
                 default   : /* Incorrect */
                    update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                                  "Invalid socket buffer specified.");
                    return(INCORRECT);
              }
           }
        }
   else if (((CHECK_STRNCMP(option, TOUPPER_ID, TOUPPER_ID_LENGTH) == 0) &&
             (*(option + TOUPPER_ID_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, TOLOWER_ID, TOLOWER_ID_LENGTH) == 0) &&
             (*(option + TOLOWER_ID_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, DELETE_ID, DELETE_ID_LENGTH) == 0) &&
             (*(option + DELETE_ID_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, FORCE_COPY_ID, FORCE_COPY_ID_LENGTH) == 0) &&
             (*(option + FORCE_COPY_ID_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, DONT_CREATE_TARGET_DIR, DONT_CREATE_TARGET_DIR_LENGTH) == 0) &&
             (*(option + DONT_CREATE_TARGET_DIR_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, TIFF2GTS_ID, TIFF2GTS_ID_LENGTH) == 0) &&
             (*(option + TIFF2GTS_ID_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, GTS2TIFF_ID, GTS2TIFF_ID_LENGTH) == 0) &&
             (*(option + GTS2TIFF_ID_LENGTH) == '\0')) ||
            (CHECK_STRNCMP(option, FAX2GTS_ID, FAX2GTS_ID_LENGTH) == 0) ||
            ((CHECK_STRNCMP(option, WMO2ASCII_ID, WMO2ASCII_ID_LENGTH) == 0) &&
             (*(option + WMO2ASCII_ID_LENGTH) == '\0')) ||
#ifdef _WITH_AFW2WMO
            ((CHECK_STRNCMP(option, AFW2WMO_ID, AFW2WMO_ID_LENGTH) == 0) &&
             (*(option + AFW2WMO_ID_LENGTH) == '\0')) ||
#endif
            ((CHECK_STRNCMP(option, SEQUENCE_LOCKING_ID, SEQUENCE_LOCKING_ID_LENGTH) == 0) &&
             (*(option + SEQUENCE_LOCKING_ID_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, OUTPUT_LOG_ID, OUTPUT_LOG_ID_LENGTH) == 0) &&
             (*(option + OUTPUT_LOG_ID_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, FILE_NAME_IS_SUBJECT_ID, FILE_NAME_IS_SUBJECT_ID_LENGTH) == 0) &&
             (*(option + FILE_NAME_IS_SUBJECT_ID_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, FILE_NAME_IS_HEADER_ID, FILE_NAME_IS_HEADER_ID_LENGTH) == 0) &&
             (*(option + FILE_NAME_IS_HEADER_ID_LENGTH) == '\0')) ||
#ifdef _WITH_WMO_SUPPORT
            ((CHECK_STRNCMP(option, WITH_SEQUENCE_NUMBER_ID, WITH_SEQUENCE_NUMBER_ID_LENGTH) == 0) &&
             (*(option + WITH_SEQUENCE_NUMBER_ID_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, CHECK_REPLY_ID, CHECK_REPLY_ID_LENGTH) == 0) &&
             (*(option + CHECK_REPLY_ID_LENGTH) == '\0')) ||
#endif
            ((CHECK_STRNCMP(option, MIRROR_DIR_ID, MIRROR_DIR_ID_LENGTH) == 0) &&
             (*(option + MIRROR_DIR_ID_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, SHOW_ALL_GROUP_MEMBERS_ID, SHOW_ALL_GROUP_MEMBERS_ID_LENGTH) == 0) &&
             (*(option + SHOW_ALL_GROUP_MEMBERS_ID_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, HIDE_ALL_GROUP_MEMBERS_ID, HIDE_ALL_GROUP_MEMBERS_ID_LENGTH) == 0) &&
             (*(option + HIDE_ALL_GROUP_MEMBERS_ID_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, SHOW_NO_TO_LINE_ID, SHOW_NO_TO_LINE_ID_LENGTH) == 0) &&
             (*(option + SHOW_NO_TO_LINE_ID_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, MATCH_REMOTE_SIZE_ID, MATCH_REMOTE_SIZE_ID_LENGTH) == 0) &&
             (*(option + MATCH_REMOTE_SIZE_ID_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, SILENT_NOT_LOCKED_FILE_ID, SILENT_NOT_LOCKED_FILE_ID_LENGTH) == 0) &&
             (*(option + SILENT_NOT_LOCKED_FILE_ID_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, ENCODE_ANSI_ID, ENCODE_ANSI_ID_LENGTH) == 0) &&
             (*(option + ENCODE_ANSI_ID_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, ACTIVE_FTP_MODE, ACTIVE_FTP_MODE_LENGTH) == 0) &&
             (*(option + ACTIVE_FTP_MODE_LENGTH) == '\0')) ||
            ((CHECK_STRNCMP(option, PASSIVE_FTP_MODE, PASSIVE_FTP_MODE_LENGTH) == 0) &&
             (*(option + PASSIVE_FTP_MODE_LENGTH) == '\0')))
        {
           /* OK */;
        }
   else if ((CHECK_STRNCMP(option, BASENAME_ID, BASENAME_ID_LENGTH) == 0) &&
            ((*(option + BASENAME_ID_LENGTH) == '\0') ||
             (*(option + BASENAME_ID_LENGTH) == ' ') ||
             (*(option + BASENAME_ID_LENGTH) == '\t')))
        {
           if (*(option + BASENAME_ID_LENGTH) != '\0')
           {
              ptr += BASENAME_ID_LENGTH + 1;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              /* overwrite */
              if (!((*ptr == 'o') && (*(ptr + 1) == 'v') &&
                    (*(ptr + 2) == 'e') && (*(ptr + 3) == 'r') &&
                    (*(ptr + 4) == 'w') && (*(ptr + 5) == 'r') &&
                    (*(ptr + 6) == 'i') && (*(ptr + 7) == 't') &&
                    (*(ptr + 8) == 'e') &&
                    ((*(ptr + 9) == '\0') || (*(ptr + 9) == ' ') ||
                     (*(ptr + 9) == '\t'))))
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Only `overwrite' is possible for %s.",
                               BASENAME_ID);
                 return(INCORRECT);
              }
           }
        }
   else if ((CHECK_STRNCMP(option, EXTENSION_ID, EXTENSION_ID_LENGTH) == 0) &&
            ((*(option + EXTENSION_ID_LENGTH) == '\0') ||
             (*(option + EXTENSION_ID_LENGTH) == ' ') ||
             (*(option + EXTENSION_ID_LENGTH) == '\t')))
        {
           if (*(option + EXTENSION_ID_LENGTH) != '\0')
           {
              ptr += EXTENSION_ID_LENGTH + 1;
              while ((*ptr == ' ') || (*ptr == '\t'))
              {
                 ptr++;
              }
              /* overwrite */
              if (!((*ptr == 'o') && (*(ptr + 1) == 'v') &&
                    (*(ptr + 2) == 'e') && (*(ptr + 3) == 'r') &&
                    (*(ptr + 4) == 'w') && (*(ptr + 5) == 'r') &&
                    (*(ptr + 6) == 'i') && (*(ptr + 7) == 't') &&
                    (*(ptr + 8) == 'e') &&
                    ((*(ptr + 9) == '\0') || (*(ptr + 9) == ' ') ||
                     (*(ptr + 9) == '\t'))))
              {
                 update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                               "Only `overwrite' is possible for %s.",
                               EXTENSION_ID);
                 return(INCORRECT);
              }
           }
        }
#ifdef WITH_EUMETSAT_HEADERS
   else if ((CHECK_STRNCMP(option, EUMETSAT_HEADER_ID, EUMETSAT_HEADER_ID_LENGTH) == 0) &&
            ((*(option + EUMETSAT_HEADER_ID_LENGTH) == ' ') ||
             (*(option + EUMETSAT_HEADER_ID_LENGTH) == '\t')))
        {
           ptr += EUMETSAT_HEADER_ID_LENGTH + 1;
           while ((*ptr == ' ') || (*ptr == '\t'))
           {
              ptr++;
           }
           if (*ptr == '\0')
           {
              update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                            "No DestEnvId specified for option %s.",
                            EUMETSAT_HEADER_ID);
              return(INCORRECT);
           }
        }
#endif
        else
        {
           update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                         "Unknown option `%s'", option);
           return(INCORRECT);
        }

   return(SUCCESS);
}


/*++++++++++++++++++++++++++++ check_rule() +++++++++++++++++++++++++++++*/
static int
check_rule(char *rename_rule, FILE *cmd_fp)
{
   get_rename_rules(NO);
   if (no_of_rule_headers > 0)
   {
      int  i;
      char *ptr,
           tmp_char;

      /*
       * Lets cut off any data at end of rename_rule (eg. overwrite).
       */
      ptr = rename_rule;
      while ((*ptr != ' ') && (*ptr != '\t') && (*ptr != '\0'))
      {
         if (*ptr == '\\')
         {
            ptr++;
         }
         ptr++;
      }
      if ((*ptr == ' ') || (*ptr == '\t'))
      {
         tmp_char = *ptr;
         *ptr = '\0';
      }
      else
      {
         tmp_char = '\0';
      }

      for (i = 0; i < no_of_rule_headers; i++)
      {
         if (CHECK_STRCMP(rule[i].header, rename_rule) == 0)
         {
            if (tmp_char != '\0')
            {
               *ptr = tmp_char;
            }
            return(SUCCESS);
         }
      }
      if (tmp_char != '\0')
      {
         *ptr = tmp_char;
      }
      update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                    "There is no rule %s in rename.rule.", rename_rule);
   }
   else
   {
      update_db_log(WARN_SIGN, __FILE__, __LINE__, cmd_fp, NULL,
                    "There are no rules, you need to configure rename.rule.");
   }

   return(INCORRECT);
}
