/*
 *  ea_str.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2020 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __ea_str_h
#define __ea_str_h

static const char *eastr[] = /* Event action string. */
                  {
                     "",                          /*  0 */
                     "Reread DIR_CONFIG",         /*  1 EA_REREAD_DIR_CONFIG */
                     "Reread HOST_CONFIG",        /*  2 EA_REREAD_HOST_CONFIG */
                     "Reread rename.rule",        /*  3 EA_REREAD_RENAME_RULE */
                     "AFD_CONFIG changed",        /*  4 EA_AFD_CONFIG_CHANGE */
                     "Enable retrieve",           /*  5 EA_ENABLE_RETRIEVE */
                     "Disable retrieve",          /*  6 EA_DISABLE_RETRIEVE */
                     "Enable archive",            /*  7 EA_ENABLE_ARCHIVE */
                     "Disable archive",           /*  8 EA_DISABLE_ARCHIVE */
                     "Enable create target dir",  /*  9 EA_ENABLE_CREATE_TARGET_DIR */
                     "Disable create target dir", /* 10 EA_DISABLE_CREATE_TARGET_DIR */
                     "Enable dir warn time",      /* 11 EA_ENABLE_DIR_WARN_TIME */
                     "Disable dir warn time",     /* 12 EA_DISABLE_DIR_WARN_TIME */
                     "Stopping AMG",              /* 13 EA_AMG_STOP */
                     "Starting AMG",              /* 14 EA_AMG_START */
                     "Stopping FD",               /* 15 EA_FD_STOP */
                     "Starting FD",               /* 16 EA_FD_START */
                     "AFD shutdown",              /* 17 EA_AFD_STOP */
                     "AFD startup",               /* 18 EA_AFD_START */
                     "Production error",          /* 19 EA_PRODUCTION_ERROR */
                     "Error start",               /* 20 EA_ERROR_START */
                     "Error end",                 /* 21 EA_ERROR_END */
                     "Enable directory",          /* 22 EA_ENABLE_DIRECTORY */
                     "Disable directory",         /* 23 EA_DISABLE_DIRECTORY */
                     "Rescan directory",          /* 24 EA_RESCAN_DIRECTORY */
                     "Exec error action start",   /* 25 EA_EXEC_ERROR_ACTION_START */
                     "Exec error action stop",    /* 26 EA_EXEC_ERROR_ACTION_STOP */
                     "Offline",                   /* 27 EA_OFFLINE */
                     "Acknowledge",               /* 28 EA_ACKNOWLEDGE */
                     "Enable host",               /* 29 EA_ENABLE_HOST */
                     "Disable host",              /* 30 EA_DISABLE_HOST */
                     "Start transfer",            /* 31 EA_START_TRANSFER */
                     "Stop transfer",             /* 32 EA_STOP_TRANSFER */
                     "Start queue",               /* 33 EA_START_QUEUE */
                     "Stop queue",                /* 34 EA_STOP_QUEUE */
                     "Start error queue",         /* 35 EA_START_ERROR_QUEUE */
                     "Stop error queue",          /* 36 EA_STOP_ERROR_QUEUE */
                     "Switching",                 /* 37 EA_SWITCH_HOST */
                     "Retry transfer",            /* 38 EA_RETRY_HOST */
                     "Enable debug",              /* 39 EA_ENABLE_DEBUG_HOST */
                     "Enable trace",              /* 40 EA_ENABLE_TRACE_HOST */
                     "Enable full trace",         /* 41 EA_ENABLE_FULL_TRACE_HOST */
                     "Disable debug",             /* 42 EA_DISABLE_DEBUG_HOST */
                     "Disable trace",             /* 43 EA_DISABLE_TRACE_HOST */
                     "Disable full trace",        /* 44 EA_DISABLE_FULL_TRACE_HOST */
                     "Unset Acknowledge/Offline", /* 45 EA_UNSET_ACK_OFFL */
                     "Warn time set",             /* 46 EA_WARN_TIME_SET */
                     "Warn time unset",           /* 47 EA_WARN_TIME_UNSET */
                     "Enable host warn time",     /* 48 EA_ENABLE_HOST_WARN_TIME */
                     "Disable host warn time",    /* 49 EA_DISABLE_HOST_WARN_TIME */
                     "Enable delete data",        /* 50 EA_ENABLE_DELETE_DATA */
                     "Disable delete data",       /* 51 EA_DISABLE_DELETE_DATA */
                     "Exec warn action start",    /* 52 EA_EXEC_WARN_ACTION_START */
                     "Exec warn action stop",     /* 53 EA_EXEC_WARN_ACTION_STOP */
                     "Exec success action start", /* 54 EA_EXEC_SUCCESS_ACTION_START */
                     "Exec success action stop",  /* 55 EA_EXEC_SUCCESS_ACTION_STOP */
                     "Start directory",           /* 56 EA_START_DIRECTORY */
                     "Stop directory",            /* 57 EA_STOP_DIRECTORY */
                     "Changed information",       /* 58 EA_CHANGE_INFO */
                     "Enable create source dir",  /* 59 EA_ENABLE_CREATE_SOURCE_DIR */
                     "Disable create source dir", /* 60 EA_DISABLE_CREATE_SOURCE_DIR */
                     "Info time set",             /* 61 EA_INFO_TIME_SET */
                     "Info time unset",           /* 62 EA_INFO_TIME_UNSET */
                     "Exec info action start",    /* 63 EA_EXEC_INFO_ACTION_START */
                     "Exec info action stop",     /* 64 EA_EXEC_INFO_ACTION_STOP */
                     "Enable simulate send",      /* 65 EA_ENABLE_SIMULATE_SEND_MODE */
                     "Disable simulate send",     /* 66 EA_DISABLE_SIMULATE_SEND_MODE */
                     "Enable simulate host",      /* 67 EA_ENABLE_SIMULATE_SEND_HOST */
                     "Disable simulate host",     /* 68 EA_DISABLE_SIMULATE_SEND_HOST */
                     "Modify errors offline",     /* 69 EA_MODIFY_ERRORS_OFFLINE */
                     "Change real hostname"       /* 70 EA_CHANGE_REAL_HOSTNAME */
                  };

/* NOTE: If the maximum length changes, don't forget to change */
/*       MAX_EVENT_ACTION_LENGTH in afddefs.h !!!              */

#endif /* __ea_str_h */
