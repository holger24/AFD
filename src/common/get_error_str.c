/*
 *  get_error_str.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 - 2025 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_error_str - returns the error string for the given error number
 **
 ** SYNOPSIS
 **   char *get_error_str(int error_code)
 **
 ** DESCRIPTION
 **   The function get_error_str() looks up the error string for the
 **   given error_code and returns this to the caller.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.06.2004 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "fddefs.h"


/*######################### get_error_str() #############################*/
char *
get_error_str(int error_code)
{
   switch (error_code)
   {
      case TRANSFER_SUCCESS : return(TRANSFER_SUCCESS_STR);                /*  0 */
      case CONNECT_ERROR : return(CONNECT_ERROR_STR);                      /*  1 */
      case USER_ERROR : return(USER_ERROR_STR);                            /*  2 */
      case PASSWORD_ERROR : return(PASSWORD_ERROR_STR);                    /*  3 */
      case TYPE_ERROR : return(TYPE_ERROR_STR);                            /*  4 */
      case LIST_ERROR : return(LIST_ERROR_STR);                            /*  5 */
      case MAIL_ERROR : return(MAIL_ERROR_STR);                            /*  6 */
      case JID_NUMBER_ERROR : return(JID_NUMBER_ERROR_STR);                /*  7 */
      case GOT_KILLED : return(GOT_KILLED_STR);                            /*  8 */
#ifdef WITH_SSL
      case AUTH_ERROR : return(AUTH_ERROR_STR);                            /*  9 */
#endif
      case OPEN_REMOTE_ERROR : return(OPEN_REMOTE_ERROR_STR);              /* 10 */
      case WRITE_REMOTE_ERROR : return(WRITE_REMOTE_ERROR_STR);            /* 11 */
      case CLOSE_REMOTE_ERROR : return(CLOSE_REMOTE_ERROR_STR);            /* 12 */
      case MOVE_REMOTE_ERROR : return(MOVE_REMOTE_ERROR_STR);              /* 13 */
      case CHDIR_ERROR : return(CHDIR_ERROR_STR);                          /* 14 */
      case WRITE_LOCK_ERROR : return(WRITE_LOCK_ERROR_STR);                /* 15 */
      case REMOVE_LOCKFILE_ERROR : return(REMOVE_LOCKFILE_ERROR_STR);      /* 16 */
      case STAT_ERROR : return(STAT_ERROR_STR);                            /* 17 */
      case MOVE_ERROR : return(MOVE_ERROR_STR);                            /* 18 */
      case RENAME_ERROR : return(RENAME_ERROR_STR);                        /* 19 */
      case TIMEOUT_ERROR : return(TIMEOUT_ERROR_STR);                      /* 20 */
#ifdef _WITH_WMO_SUPPORT
      case CHECK_REPLY_ERROR : return(CHECK_REPLY_ERROR_STR);              /* 21 */
#endif
      case READ_REMOTE_ERROR : return(READ_REMOTE_ERROR_STR);              /* 22 */
      case SIZE_ERROR : return(SIZE_ERROR_STR);                            /* 23 */
      case DATE_ERROR : return(DATE_ERROR_STR);                            /* 24 */
      case QUIT_ERROR : return(QUIT_ERROR_STR);                            /* 25 */
      case MKDIR_ERROR : return(MKDIR_ERROR_STR);                          /* 26 */
      case CHOWN_ERROR : return(CHOWN_ERROR_STR);                          /* 27 */
      case CONNECTION_RESET_ERROR : return(CONNECTION_RESET_ERROR_STR);    /* 28 */
      case CONNECTION_REFUSED_ERROR : return(CONNECTION_REFUSED_ERROR_STR);/* 29 */
      case OPEN_LOCAL_ERROR : return(OPEN_LOCAL_ERROR_STR);                /* 30 */
      case READ_LOCAL_ERROR : return(READ_LOCAL_ERROR_STR);                /* 31 */
      case LOCK_REGION_ERROR : return(LOCK_REGION_ERROR_STR);              /* 32 */
      case UNLOCK_REGION_ERROR : return(UNLOCK_REGION_ERROR_STR);          /* 33 */
      case ALLOC_ERROR : return(ALLOC_ERROR_STR);                          /* 34 */
      case SELECT_ERROR : return(SELECT_ERROR_STR);                        /* 35 */
      case WRITE_LOCAL_ERROR : return(WRITE_LOCAL_ERROR_STR);              /* 36 */
      case STAT_TARGET_ERROR : return(STAT_TARGET_ERROR_STR);              /* 37 */
      case FILE_SIZE_MATCH_ERROR : return(FILE_SIZE_MATCH_ERROR_STR);      /* 38 */
      case OPEN_FILE_DIR_ERROR : return(OPEN_FILE_DIR_ERROR_STR);          /* 40 */
      case NO_MESSAGE_FILE : return(NO_MESSAGE_FILE_STR);                  /* 41 */
      case STAT_REMOTE_ERROR : return(STAT_REMOTE_ERROR_STR);              /* 42 */
      case PIPE_CLOSED_ERROR : return(PIPE_CLOSED_ERROR_STR);              /* 43 */
      case REMOTE_USER_ERROR : return(REMOTE_USER_ERROR_STR);              /* 50 */
      case DATA_ERROR : return(DATA_ERROR_STR);                            /* 51 */
#ifdef _WITH_WMO_SUPPORT
      case SIG_PIPE_ERROR : return(SIG_PIPE_ERROR_STR);                    /* 52 */
#endif
#ifdef _WITH_MAP_SUPPORT
      case MAP_FUNCTION_ERROR : return(MAP_FUNCTION_ERROR_STR);            /* 55 */
#endif
      case EXEC_ERROR : return(EXEC_ERROR_STR);                            /* 56 */
#ifdef _WITH_DFAX_SUPPORT
      case DFAX_FUNCTION_ERROR : return(DFAX_FUNCTION_ERROR_STR);          /* 57 */
#endif
      case SYNTAX_ERROR : return(SYNTAX_ERROR_STR);                        /* 60 */
      case NO_FILES_TO_SEND : return(NO_FILES_TO_SEND_STR);                /* 61 */
      case STILL_FILES_TO_SEND : return(STILL_FILES_TO_SEND_STR);          /* 62 */
      case NOOP_ERROR : return(NOOP_ERROR_STR);                            /* 63 */
      case DELETE_REMOTE_ERROR : return(DELETE_REMOTE_ERROR_STR);          /* 64 */
      case SET_BLOCKSIZE_ERROR : return(SET_BLOCKSIZE_ERROR_STR);          /* 65 */
   }
   return(_("Unknown error"));
}
