/*
 *  get_update_config_str.c - Part of AFD, an automatic file distribution
 *                            program.
 *  Copyright (c) 2007 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_update_config_str - returns the result string
 **
 ** SYNOPSIS
 **   void get_dc_result_str(char *str,
 **                          int  result,
 **                          int  warn_counter,
 **                          int  *see_sys_log,
 **                          int  *type)
 **   void get_hc_result_str(char *str,
 **                          int  result,
 **                          int  warn_counter,
 **                          int  *see_sys_log,
 **                          int  *type)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.05.2007 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>      /* snprintf()                                    */
#include <string.h>     /* strcpy()                                      */

#define NO_CHANGE_IN_DIR_CONFIG_STR         "No changes"
#define DIR_CONFIG_UPDATED_STR              "Updated configuration"
#define DIR_CONFIG_UPDATED_DC_PROBLEMS_STR  "Failed to restart process, config updated"
#define DIR_CONFIG_NO_VALID_DATA_STR        "No valid data found"
#define DIR_CONFIG_EMPTY_STR                "Config file(s) empty"
#define DIR_CONFIG_ACCESS_ERROR_STR         "Failed to access config file"
#define DIR_CONFIG_NOTHING_DONE_STR         "Unable to do any changes"
#define NO_CHANGE_IN_HOST_CONFIG_STR        "No changes"
#define HOST_CONFIG_RECREATED_STR           "Recreated HOST_CONFIG"
#define HOST_CONFIG_DATA_CHANGED_STR        "Updated HOST_CONFIG"
#define HOST_CONFIG_DATA_ORDER_CHANGED_STR  "HOST_CONFIG updated and host order changed"
#define HOST_CONFIG_ORDER_CHANGED_STR       "Host order changed"
#define HOST_CONFIG_UPDATED_DC_PROBLEMS_STR "Failed to restart process, HOST_CONFIG updated"
#define INCORRECT_STR                       "Unable to update config due to internal errors"
#define UNKNOWN_ERROR_STR                   "Unkown error returned, please contact maintainer"


/*####################### get_dc_result_str() ###########################*/
void
get_dc_result_str(char *str,
                  int  result,
                  int  warn_counter,
                  int  *see_sys_log,
                  int  *type)
{
   int length;

   switch (result)
   {
      case NO_CHANGE_IN_DIR_CONFIG :
         if (warn_counter > 0)
         {
            if (type == NULL)
            {
               (void)strcpy(str, _("Warning: "));
               length = 9;
            }
            else
            {
               *type = WARN_NO;
               length = 0;
            }
            (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                           _("%s, but %u warnings."),
                           NO_CHANGE_IN_DIR_CONFIG_STR, warn_counter);
            *see_sys_log = YES;
         }
         else
         {
            if (type == NULL)
            {
               (void)strcpy(str, _("Info   : "));
               length = 9;
            }
            else
            {
               *type = INFO_NO;
               length = 0;
            }
            (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                           "%s.", NO_CHANGE_IN_DIR_CONFIG_STR);
            if (*see_sys_log != YES)
            {
               *see_sys_log = NO;
            }
         }
         break;

      case DIR_CONFIG_UPDATED :
         if (warn_counter > 0)
         {
            if (type == NULL)
            {
               (void)strcpy(str, _("Warning: "));
               length = 9;
            }
            else
            {
               *type = WARN_NO;
               length = 0;
            }
            (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                           _("%s with %u warnings."),
                           DIR_CONFIG_UPDATED_STR, warn_counter);
            *see_sys_log = YES;
         }
         else
         {
            if (type == NULL)
            {
               (void)strcpy(str, _("Info   : "));
               length = 9;
            }
            else
            {
               *type = INFO_NO;
               length = 0;
            }
            (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                           "%s.", DIR_CONFIG_UPDATED_STR);
            if (*see_sys_log != YES)
            {
               *see_sys_log = NO;
            }
         }
         break;

      case DIR_CONFIG_UPDATED_DC_PROBLEMS :
         if (type == NULL)
         {
            (void)strcpy(str, _("ERROR  : "));
            length = 9;
         }
         else
         {
            *type = ERROR_NO;
            length = 0;
         }
         if (warn_counter > 0)
         {
            (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                           _("%s with %u warnings."),
                           DIR_CONFIG_UPDATED_DC_PROBLEMS_STR, warn_counter);
         }
         else
         {
            (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                           "%s.", DIR_CONFIG_UPDATED_DC_PROBLEMS_STR);
         }
         *see_sys_log = YES;
         break;

      case DIR_CONFIG_NO_VALID_DATA :
         if (type == NULL)
         {
            (void)strcpy(str, _("ERROR  : "));
            length = 9;
         }
         else
         {
            *type = ERROR_NO;
            length = 0;
         }
         (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                        "%s.", DIR_CONFIG_NO_VALID_DATA_STR);
         *see_sys_log = YES;
         break;

      case DIR_CONFIG_EMPTY :
         if (type == NULL)
         {
            (void)strcpy(str, _("ERROR  : "));
            length = 9;
         }
         else
         {
            *type = ERROR_NO;
            length = 0;
         }
         (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                        "%s.", DIR_CONFIG_EMPTY_STR);
         *see_sys_log = NO;
         break;

      case DIR_CONFIG_ACCESS_ERROR :
         if (type == NULL)
         {
            (void)strcpy(str, _("ERROR  : "));
            length = 9;
         }
         else
         {
            *type = ERROR_NO;
            length = 0;
         }
         (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                        "%s.", DIR_CONFIG_ACCESS_ERROR_STR);
         *see_sys_log = YES;
         break;

      case DIR_CONFIG_NOTHING_DONE :
         if (type == NULL)
         {
            (void)strcpy(str, _("Warning: "));
            length = 9;
         }
         else
         {
            *type = WARN_NO;
            length = 0;
         }
         (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                        "%s.", DIR_CONFIG_NOTHING_DONE_STR);
         *see_sys_log = YES;
         break;

      case INCORRECT :
         if (type == NULL)
         {
            (void)strcpy(str, _("ERROR  : "));
            length = 9;
         }
         else
         {
            *type = ERROR_NO;
            length = 0;
         }
         (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                        "%s!", INCORRECT_STR);
         *see_sys_log = YES;
         break;

      default :
         if (type == NULL)
         {
            (void)strcpy(str, _("ERROR  : "));
            length = 9;
         }
         else
         {
            *type = ERROR_NO;
            length = 0;
         }
         (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                        "%s.", UNKNOWN_ERROR_STR);
         *see_sys_log = YES;
         break;
   }
   return;
} 


/*####################### get_hc_result_str() ###########################*/
void
get_hc_result_str(char *str,
                  int  result,
                  int  warn_counter,
                  int  *see_sys_log,
                  int  *type)
{
   int length;

   switch (result)
   {
      case NO_CHANGE_IN_HOST_CONFIG : 
         if (warn_counter > 0)
         {
            if (type == NULL)
            {
               (void)strcpy(str, _("Warning: "));
               length = 9;
            }
            else
            {
               *type = WARN_NO;
               length = 0;
            }
            (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                           _("%s, but %u warnings."),
                           NO_CHANGE_IN_HOST_CONFIG_STR, warn_counter);
            *see_sys_log = YES;
         }
         else
         {
            if (type == NULL)
            {
               (void)strcpy(str, _("Info   : "));
               length = 9;
            }
            else
            {
               *type = INFO_NO;
               length = 0;
            }
            (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                           "%s.", NO_CHANGE_IN_HOST_CONFIG_STR);
            if (*see_sys_log != YES)
            {
               *see_sys_log = NO;
            }
         }
         break;

      case HOST_CONFIG_RECREATED :
         if (type == NULL)
         {
            (void)strcpy(str, _("Warning: "));
            length = 9;
         }
         else
         {
            *type = WARN_NO;
            length = 0;
         }
         (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                        "%s.", HOST_CONFIG_RECREATED_STR);
         if (*see_sys_log != YES)
         {
            *see_sys_log = NO;
         }
         break;

      case HOST_CONFIG_DATA_CHANGED :
         if (warn_counter > 0)
         {
            if (type == NULL)
            {
               (void)strcpy(str, _("Warning: "));
               length = 9;
            }
            else
            {
               *type = WARN_NO;
               length = 0;
            }
            (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                           _("%s with %u warnings."),
                          HOST_CONFIG_DATA_CHANGED_STR, warn_counter);
            *see_sys_log = YES;
         }
         else
         {
            if (type == NULL)
            {
               (void)strcpy(str, _("Info   : "));
               length = 9;
            }
            else
            {
               *type = INFO_NO;
               length = 0;
            }
            (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                           "%s.", HOST_CONFIG_DATA_CHANGED_STR);
            if (*see_sys_log != YES)
            {
               *see_sys_log = NO;
            }
         }
         break;

      case HOST_CONFIG_DATA_ORDER_CHANGED :
         if (warn_counter > 0)
         {
            if (type == NULL)
            {
               (void)strcpy(str, _("Warning: "));
               length = 9;
            }
            else
            {
               *type = WARN_NO;
               length = 0;
            }
            (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                           _("%s, but %u warnings."),
                           HOST_CONFIG_DATA_ORDER_CHANGED_STR, warn_counter);
            *see_sys_log = YES;
         }
         else
         {
            if (type == NULL)
            {
               (void)strcpy(str, _("Info   : "));
               length = 9;
            }
            else
            {
               *type = INFO_NO;
               length = 0;
            }
            (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                           "%s.", HOST_CONFIG_DATA_ORDER_CHANGED_STR);
            if (*see_sys_log != YES)
            {
               *see_sys_log = NO;
            }
         }
         break;

      case HOST_CONFIG_ORDER_CHANGED :
         if (warn_counter > 0)
         {
            if (type == NULL)
            {
               (void)strcpy(str, _("Warning: "));
               length = 9;
            }
            else
            {
               *type = WARN_NO;
               length = 0;
            }
            (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                           _("%s with %u warnings."),
                           HOST_CONFIG_ORDER_CHANGED_STR, warn_counter);
            *see_sys_log = YES;
         }
         else
         {
            if (type == NULL)
            {
               (void)strcpy(str, _("Info   : "));
               length = 9;
            }
            else
            {
               *type = INFO_NO;
               length = 0;
            }
            (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                           "%s.", HOST_CONFIG_ORDER_CHANGED_STR);
            if (*see_sys_log != YES)
            {
               *see_sys_log = NO;
            }
         }
         break;

      case HOST_CONFIG_UPDATED_DC_PROBLEMS :
         if (type == NULL)
         {
            (void)strcpy(str, _("ERROR  : "));
            length = 9;
         }
         else
         {
            *type = ERROR_NO;
            length = 0;
         }
         if (warn_counter > 0)
         {
            (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                           _("%s with %u warnings."),
                           HOST_CONFIG_UPDATED_DC_PROBLEMS_STR, warn_counter);
         }
         else
         {
            (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                           "%s.", HOST_CONFIG_UPDATED_DC_PROBLEMS_STR);
         }
         *see_sys_log = YES;
         break;

      case INCORRECT :
         if (type == NULL)
         {
            (void)strcpy(str, _("ERROR  : "));
            length = 9;
         }
         else
         {
            *type = ERROR_NO;
            length = 0;
         }
         (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                        "%s!", INCORRECT_STR);
         *see_sys_log = YES;
         break;

      default :
         if (type == NULL)
         {
            (void)strcpy(str, _("ERROR  : "));
            length = 9;
         }
         else
         {
            *type = ERROR_NO;
            length = 0;
         }
         (void)snprintf(str + length, MAX_UPDATE_REPLY_STR_LENGTH - length,
                        "%s.", UNKNOWN_ERROR_STR);
         *see_sys_log = YES;
         break;
   }
   return;
}
