/*
 *  permission.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2016 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __permission_h
#define __permission_h

#define NO_PERMISSION            -2
#define NO_LIMIT                 -3

#define STARTUP_PERM             "startup"         /* Startup AFD          */
#define STARTUP_PERM_LENGTH      (sizeof(STARTUP_PERM) - 1)
#define SHUTDOWN_PERM            "shutdown"        /* Shutdown AFD         */
#define SHUTDOWN_PERM_LENGTH     (sizeof(SHUTDOWN_PERM) - 1)
#define INITIALIZE_PERM          "initialize"      /* Initialize AFD       */
#define INITIALIZE_PERM_LENGTH   (sizeof(INITIALIZE_PERM) - 1)
#define RESEND_PERM              "resend"          /* Resend files         */
#define SEND_PERM                "send"            /* Send files to host   */
                                                   /* not in FSA.          */
#define VIEW_PASSWD_PERM         "view_passwd"     /* View password.       */
#define VIEW_PASSWD_PERM_LENGTH  (sizeof(VIEW_PASSWD_PERM) - 1)
#define SET_PASSWD_PERM          "set_passwd"      /* Set password.        */
#define SET_PASSWD_PERM_LENGTH   (sizeof(SET_PASSWD_PERM) - 1)
#define VIEW_DATA_PERM           "view_data"       /* View data.           */
#define LIST_LIMIT               "list_limit"      /* List limit show_xlog */
#define SHOW_SLOG_PERM           "show_slog"       /* System Log           */
#define SHOW_MLOG_PERM           "show_mlog"       /* Maintainer Log       */
#define SHOW_RLOG_PERM           "show_rlog"       /* Receive Log          */
#define SHOW_TLOG_PERM           "show_tlog"       /* Transfer Log         */
#define SHOW_TDLOG_PERM          "show_tdlog"      /* Transfer Debug Log   */
#define SHOW_ILOG_PERM           "show_ilog"       /* Input Log            */
#define SHOW_OLOG_PERM           "show_olog"       /* Output Log           */
#define SHOW_DLOG_PERM           "show_dlog"       /* Delete Log           */
#define SHOW_ELOG_PERM           "show_elog"       /* Event Log            */
#define MON_SYS_LOG_PERM         "mon_sys_log"     /* Monitor System Log   */
#define MON_LOG_PERM             "mon_log"         /* Monitor Log          */
#define MON_INFO_PERM            "mon_info"        /* Monitor Information  */
#define EDIT_HC_PERM             "edit_hc"         /* Edit HOST_CONFIG     */
#define AFD_CTRL_PERM            "afd_ctrl"        /* afd_ctrl dialog      */
#define AFD_CTRL_PERM_LENGTH     (sizeof(AFD_CTRL_PERM) - 1)
#define RAFD_CTRL_PERM           "rafd_ctrl"       /* remote afd_ctrl      */
#define AMG_CTRL_PERM            "AMG_control"     /* Start/Stop AMG       */
#define AMG_CTRL_PERM_LENGTH     (sizeof(AMG_CTRL_PERM) - 1)
#define FD_CTRL_PERM             "FD_control"      /* Start/Stop FD        */
#define FD_CTRL_PERM_LENGTH      (sizeof(FD_CTRL_PERM) - 1)
#define HANDLE_EVENT_PERM        "handle_events"   /* Handle event.        */
#define CTRL_QUEUE_PERM          "control_queue"   /* Start/Stop queue     */
#define CTRL_QUEUE_PERM_LENGTH   (sizeof(CTRL_QUEUE_PERM) - 1)
#define CTRL_TRANSFER_PERM       "control_transfer"/* Start/Stop transfer  */
#define CTRL_TRANSFER_PERM_LENGTH (sizeof(CTRL_TRANSFER_PERM) - 1)
#define CTRL_QUEUE_TRANSFER_PERM "control_host"    /* Start/Stop host      */
#define SWITCH_HOST_PERM         "switch_host"
#define SWITCH_HOST_PERM_LENGTH  (sizeof(SWITCH_HOST_PERM) - 1)
#define DISABLE_HOST_PERM        "disable"         /* Disable host         */
#define DISABLE_HOST_PERM_LENGTH (sizeof(DISABLE_HOST_PERM) - 1)
#define DISABLE_AFD_PERM         "disable_afd"     /* Disable AFD          */
#define DISABLE_AFD_PERM_LENGTH  (sizeof(DISABLE_AFD_PERM) - 1)
#define INFO_PERM                "info"            /* Info about host      */
#define DEBUG_PERM               "debug"           /* Enable/Disable debug */
#define DEBUG_PERM_LENGTH        (sizeof(DEBUG_PERM) - 1)
#define TRACE_PERM               "trace"           /* Enable/Disable trace */
#define TRACE_PERM_LENGTH        (sizeof(TRACE_PERM) - 1)
#define FULL_TRACE_PERM          "full_trace"      /* Enable/Disable full  */
                                                   /* trace                */
#define FULL_TRACE_PERM_LENGTH   (sizeof(FULL_TRACE_PERM) - 1)
#define SIMULATE_MODE_PERM       "simulate_mode"   /* Enable/Disable       */
                                                   /* simulate mode        */
#define SIMULATE_MODE_PERM_LENGTH (sizeof(SIMULATE_MODE_PERM) - 1)
#define SHOW_EXEC_STAT_PERM      "show_exec_stat"  /* Show and reset exec  */
                                                   /* stats for dir_check  */
#define SHOW_EXEC_STAT_PERM_LENGTH (sizeof(SHOW_EXEC_STAT_PERM) - 1)
#define DO_NOT_DELETE_DATA_PERM  "ctrl_delete_data"/* Enable/disable ctrl  */
                                                   /* of deleteing data.   */
#define DO_NOT_DELETE_DATA_PERM_LENGTH (sizeof(DO_NOT_DELETE_DATA_PERM) - 1)
#define RETRY_PERM               "retry"
#define RETRY_PERM_LENGTH        (sizeof(RETRY_PERM) - 1)
#define VIEW_JOBS_PERM           "view_jobs"       /* Detailed transfer.   */
#define AFD_LOAD_PERM            "afd_load"
#define RR_DC_PERM               "reread_dir_config"/* Reread DIR_CONFIG.  */
#define RR_DC_PERM_LENGTH        (sizeof(RR_DC_PERM) - 1)
#define RR_HC_PERM               "reread_host_config"/* Reread HOST_CONFIG.*/
#define RR_HC_PERM_LENGTH        (sizeof(RR_HC_PERM) - 1)
#define AFD_CMD_PERM             "afdcmd"          /* Send commands to AFD.*/
#define AFD_CMD_PERM_LENGTH      (sizeof(AFD_CMD_PERM) - 1)
#define AFD_CFG_PERM             "afdcfg"          /* Configure AFD.       */
#define AFD_CFG_PERM_LENGTH      (sizeof(AFD_CFG_PERM) - 1)
#define VIEW_DIR_CONFIG_PERM     "view_dir_config" /* Info on DIR_CONFIG   */
#define VIEW_DIR_CONFIG_PERM_LENGTH (sizeof(VIEW_DIR_CONFIG_PERM) - 1)
#define SHOW_QUEUE_PERM          "show_queue"      /* Show + delete queue. */
#define DELETE_QUEUE_PERM        "delete_queue"    /* Delete queue.        */
#define XSHOW_STAT_PERM          "xshow_stat"      /* Show statistics.     */
#define EDIT_AFD_INFO_PERM       "edit_afd_info"   /* Allow to edit the    */
                                                   /* information about a  */
                                                   /* host.                */
#define EDIT_MON_INFO_PERM       "edit_mon_info"   /* Allow to edit the    */
                                                   /* information about an */
                                                   /* AFD.                 */

#define MON_CTRL_PERM            "mon_ctrl"        /* mon_ctrl dialog.     */
#define MON_CTRL_PERM_LENGTH     (sizeof(MON_CTRL_PERM) - 1)
#define MON_STARTUP_PERM         "mon_startup"     /* Startup AFD_MON.     */
#define MON_STARTUP_PERM_LENGTH  (sizeof(MON_STARTUP_PERM) - 1)
#define MON_SHUTDOWN_PERM        "mon_shutdown"    /* Shutdown AFD_MON.    */
#define MON_SHUTDOWN_PERM_LENGTH (sizeof(MON_SHUTDOWN_PERM) - 1)
#define MAFD_CMD_PERM            "mafdcmd"         /* Send commands to AFD */
                                                   /* monitor.             */

#define MAFD_CMD_PERM_LENGTH     (sizeof(MAFD_CMD_PERM) - 1)
#define DIR_CTRL_PERM            "dir_ctrl"        /* dir_ctrl dialog.     */
#define DIR_INFO_PERM            "dir_info"        /* dir_info dialog.     */
#define DISABLE_DIR_PERM         "disable_dir"     /* Disable directory.   */
#define DISABLE_DIR_PERM_LENGTH  (sizeof(DISABLE_DIR_PERM) - 1)
#define STOP_DIR_PERM            "stop_dir"        /* Stop directory.      */
#define STOP_DIR_PERM_LENGTH     (sizeof(STOP_DIR_PERM) - 1)
#define RESCAN_PERM              "rescan"          /* Rescan directory.    */
#define RESCAN_PERM_LENGTH       (sizeof(RESCAN_PERM) - 1)
#define RR_LC_FILE_PERM          "reread_interface_file"/* Reread local    */
                                                   /* interface file.      */
#define RR_LC_FILE_PERM_LENGTH   (sizeof(RR_LC_FILE_PERM) - 1)
#define FILE_DIR_CHECK_PERM      "file_dir_check"  /* Force file dir check.*/
#define FILE_DIR_CHECK_PERM_LENGTH (sizeof(FILE_DIR_CHECK_PERM) - 1)
#define FORCE_AC_PERM            "force_archive_check"/* Force archive     */
                                                   /* check.               */
#define FORCE_AC_PERM_LENGTH     (sizeof(FORCE_AC_PERM) - 1)

#define PERMISSION_DENIED_STR     "You are not permitted to use this program."

#endif /* __permission_h */
