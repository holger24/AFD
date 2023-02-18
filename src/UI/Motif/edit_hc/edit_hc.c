/*
 *  edit_hc.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   edit_hc - edits the AFD host configuration file
 **
 ** SYNOPSIS
 **   edit_hc [options]
 **            --version
 **            -w <AFD working directory>
 **            -f <font name>
 **            -h <host alias>
 **
 ** DESCRIPTION
 **   This dialog allows the user to change the following parameters
 **   for a given hostname:
 **       - Real Hostaname/IP Number
 **       - Transfer timeout
 **       - Retry interval
 **       - Maximum errors
 **       - Successful retries
 **       - Transfer rate limit
 **       - Max. parallel transfers
 **       - Transfer Blocksize
 **       - File size offset
 **       - Number of transfers that may not burst
 **       - Proxy name
 **   additionally some protocol specific options can be set:
 **       - FTP active/passive mode
 **       - set FTP idle time
 **       - send STAT to keep control connection alive (FTP)
 **       - FTP fast rename
 **       - FTP fast cd
 **
 **   In the list widget "Alias Hostname" the user can change the order
 **   of the host names in the afd_ctrl dialog by using drag & drop.
 **   During the drag operation the cursor will change into a bee. The
 **   hot spot of this cursor are the two feelers.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.08.1997 H.Kiehl Created
 **   28.02.1998 H.Kiehl Added host switching information.
 **   21.10.1998 H.Kiehl Added Remove button to remove hosts from the FSA.
 **   04.08.2001 H.Kiehl Added support for active or passive mode and
 **                      FTP idle time.
 **   11.09.2001 H.Kiehl Added support to send FTP STAT to keep control
 **                      connection alive.
 **   10.01.2004 H.Kiehl Added FTP fast rename option.
 **   17.04.2004 H.Kiehl Added option -h to select a host alias.
 **   10.06.2004 H.Kiehl Added transfer rate limit.
 **   17.02.2006 H.Kiehl Added option to change socket send and/or receive
 **                      buffer.
 **   28.02.2006 H.Kiehl Added option for setting the keep connected
 **                      parameter.
 **   16.03.2006 H.Kiehl Added duplicate check option.
 **   17.08.2006 H.Kiehl Added extended active or passive mode.
 **   05.03.2008 H.Kiehl Added another dupcheck option.
 **   23.06.2008 H.Kiehl Added option to NOT delete data.
 **   14.09.2008 H.Kiehl Increase transfer block size up to 8 MiB.
 **   07.04.2009 H.Kiehl Added warn time.
 **   19.04.2009 H.Kiehl Added compression for protocol options.
 **   24.11.2012 H.Kiehl Added support for selecting CRC type.
 **   13.01.2014 H.Kiehl Added support for makeing dupcheck timeout fixed.
 **   06.03.2020 H.Kiehl Added implicit FTPS option.
 **   10.01.2022 H.Kiehl Added no expect option.
 **   18.02.2023 H.Kiehl Added 'Send UTF8 on' option.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>              /* strcpy(), strcat(), strerror()       */
#include <stdlib.h>              /* malloc(), free()                     */
#include <ctype.h>               /* isdigit()                            */
#include <signal.h>              /* signal()                             */
#include <sys/types.h>
#include <sys/stat.h>            /* umask()                              */
#include <fcntl.h>               /* O_RDWR                               */
#include <unistd.h>              /* STDERR_FILENO                        */
#include <errno.h>
#include "amgdefs.h"
#include "version.h"
#include "permission.h"

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/List.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/Text.h>
#include <Xm/ToggleBG.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/PushB.h>
#include <Xm/CascadeB.h>
#include <Xm/DragDrop.h>
#include <Xm/AtomMgr.h>
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif
#include <X11/Intrinsic.h>
#include "edit_hc.h"
#include "mafd_ctrl.h"
#include "source.h"
#include "source_mask.h"
#include "no_source.h"
#include "no_source_mask.h"

/* Global variables. */
XtAppContext               app;
Display                    *display;
Widget                     active_mode_w,
#ifdef _WITH_BURST_2
                           allow_burst_w,
#endif
                           appshell,
                           auto_toggle_w,
                           bucketname_in_path_w,
                           compression_w,
#ifdef WITH_DUP_CHECK
                           dc_alias_w,
                           dc_delete_w,
                           dc_disable_w,
                           dc_enable_w,
                           dc_filecontent_w,
                           dc_filenamecontent_w,
                           dc_filename_w,
                           dc_label_w,
                           dc_namesize_w,
                           dc_nosuffix_w,
                           dc_recipient_w,
                           dc_reference_w,
                           dc_ref_label_w,
                           dc_store_w,
                           dc_timeout_label_w,
                           dc_timeout_w,
                           dc_timeout_fixed_w,
                           dc_type_w,
                           dc_warn_w,
                           dc_crc_label_w,
                           dc_crc_w,
                           dc_crc32_w,
                           dc_crc32c_w,
                           dc_murmur3_w,
#endif
                           disable_mlst_w,
                           disable_strict_host_key_w,
                           disconnect_w,
                           do_not_delete_data_toggle_w,
                           extended_mode_w,
                           first_label_w,
                           ftp_fast_cd_w,
                           ftp_fast_rename_w,
                           ftp_idle_time_w,
                           ftp_ignore_bin_w,
                           ftp_mode_w,
                           ftps_label_w,
                           host_1_w,
                           host_2_w,
                           host_1_label_w,
                           host_2_label_w,
                           host_list_w,
                           host_switch_toggle_w,
                           ignore_errors_toggle_w,
                           interrupt_w,
                           kc_both_w,
                           kc_fetch_w,
                           kc_send_w,
                           keep_connected_w,
                           keep_connected_label_w,
                           keep_time_stamp_w,
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
                           ftp_keepalive_w,
#endif
                           match_size_w,
                           max_errors_w,
                           max_errors_label_w,
                           mode_label_w,
                           no_ageing_jobs_w,
                           no_expect_w,
                           no_source_icon_w,
                           passive_mode_w,
                           passive_redirect_w,
                           proxy_label_w,
                           proxy_name_w,
                           real_hostname_1_w,
                           real_hostname_2_w,
                           retry_interval_w,
                           retry_interval_label_w,
                           rm_button_w,
                           second_label_w,
                           send_utf8_on_w,
                           sequence_locking_w,
                           socket_send_buffer_size_label_w,
                           socket_send_buffer_size_w,
                           socket_receive_buffer_size_label_w,
                           socket_receive_buffer_size_w,
                           sort_file_names_w,
                           source_icon_w,
                           ssl_ccc_w,
                           ssl_implicit_ftps_w,
                           start_drag_w,
                           statusbox_w,
#ifdef WITH_SSL
                           strict_tls_w,
                           tls_legacy_renegotiation_w,
#endif
                           successful_retries_label_w,
                           successful_retries_w,
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
                           tcp_keepalive_w,
#endif
                           transfer_rate_limit_label_w,
                           transfer_rate_limit_w,
                           transfer_timeout_label_w,
                           transfer_timeout_w,
                           use_file_when_local_w,
                           use_list_w,
                           use_stat_list_w,
                           warn_time_days_w,
                           warn_time_days_label_w,
                           warn_time_hours_w,
                           warn_time_hours_label_w,
                           warn_time_label_w,
                           warn_time_mins_w,
                           warn_time_mins_label_w,
                           warn_time_secs_w,
                           warn_time_secs_label_w;
XmFontList                 fontlist;
Atom                       compound_text;
XtInputId                  db_update_cmd_id;
int                        event_log_fd = STDERR_FILENO,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           host_alias_order_change = NO,
                           in_drop_site = -2,
                           last_selected = -1,
                           no_of_dirs,
                           no_of_hosts,
                           sys_log_fd = STDERR_FILENO;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
char                       fake_user[MAX_FULL_USER_ID_LENGTH + 1],
                           last_selected_host[MAX_HOSTNAME_LENGTH + 1],
                           *p_work_dir,
                           user[MAX_FULL_USER_ID_LENGTH + 1];
struct fileretrieve_status *fra;
struct filetransfer_status *fsa;
struct afd_status          *p_afd_status;
struct host_list           *hl = NULL; /* Required by change_alias_order(). */
struct changed_entry       *ce = NULL;
struct parallel_transfers  pt;
struct transfer_blocksize  tb;
struct file_size_offset    fso;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local global variables. */
static int                 seleted_host_no = 0;
static char                font_name[40],
                           translation_table[] = "#override <Btn2Down>: start_drag()";
static XtActionsRec        action_table[] =
                           {
                              { "start_drag", (XtActionProc)start_drag }
                           };

/* Local function prototypes. */
static void                create_option_menu_fso(Widget, XmFontList),
                           create_option_menu_pt(Widget, XmFontList),
                           create_option_menu_tb(Widget, XmFontList),
                           init_edit_hc(int *, char **, char *),
                           init_widget_data(void),
                           sig_bus(int),
                           sig_segv(int),
                           usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char            label_str[HOST_ALIAS_LABEL_LENGTH + MAX_HOSTNAME_LENGTH],
                   window_title[MAX_WNINDOW_TITLE_LENGTH],
                   work_dir[MAX_PATH_LENGTH];
   static String   fallback_res[] =
                   {
                      ".edit_hc*mwmDecorations : 42",
                      ".edit_hc*mwmFunctions : 12",
                      ".edit_hc*background : NavajoWhite2",
                      ".edit_hc.form_w.host_list_box_w.host_list_wSW*background : NavajoWhite1",
                      ".edit_hc.form_w*XmText.background : NavajoWhite1",
                      ".edit_hc.form_w.button_box*background : PaleVioletRed2",
                      ".edit_hc.form_w.button_box.Remove.XmDialogShell*background : NavajoWhite2",
                      ".edit_hc.form_w.button_box*foreground : Black",
                      ".edit_hc.form_w.button_box*highlightColor : Black",
                      NULL
                   };
   Widget          box_w,
                   button_w,
#ifdef WITH_DUP_CHECK
                   dupcheck_w,
#endif
                   form_w,
                   label_w,
                   h_separator_bottom_w,
                   h_separator_top_w,
                   keep_connected_radio_w,
                   v_separator_w;
   XtTranslations  translations;
   Atom            targets[1];
   Arg             args[MAXARGS];
   Cardinal        argcount;
   XmFontListEntry entry;
   uid_t           euid, /* Effective user ID. */
                   ruid; /* Real user ID. */

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values. */
   p_work_dir = work_dir;
   init_edit_hc(&argc, argv, window_title);

#ifdef _X_DEBUG
   XSynchronize(display, 1);
#endif
   /*
    * SSH uses wants to look at .Xauthority and with setuid flag
    * set we cannot do that. So when we initialize X lets temporaly
    * disable it. After XtAppInitialize() we set it back.
    */
   euid = geteuid();
   ruid = getuid();
   if (euid != ruid)
   {
      if (seteuid(ruid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                       ruid, strerror(errno), __FILE__, __LINE__);
      }
   }

   argcount = 0;
   XtSetArg(args[argcount], XmNtitle, window_title); argcount++;
   appshell = XtAppInitialize(&app, "AFD", NULL, 0,
                              &argc, argv, fallback_res, args, argcount);
   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
#ifdef WITH_SETUID_PROGS
         if (errno == EPERM)
         {
            if (seteuid(0) == -1)
            {
               (void)fprintf(stderr,
                             "Failed to seteuid() to 0 : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
            }
            else
            {
               if (seteuid(euid) == -1)
               {
                  (void)fprintf(stderr,
                                "Failed to seteuid() to %d (from %d) : %s (%s %d)\n",
                                euid, ruid, strerror(errno),
                                __FILE__, __LINE__);
               }
            }
         }
         else
         {
#endif
            (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                          euid, strerror(errno), __FILE__, __LINE__);
#ifdef WITH_SETUID_PROGS
         }
#endif
      }
   }
   compound_text = XmInternAtom(display, "COMPOUND_TEXT", False);

#ifdef HAVE_XPM
   /* Setup AFD logo as icon. */
   setup_icon(XtDisplay(appshell), appshell);
#endif

   /* Create managing widget. */
   form_w = XmCreateForm(appshell, "form_w", NULL, 0);

   /* Prepare the font. */
   entry = XmFontListEntryLoad(XtDisplay(form_w), font_name,
                               XmFONT_IS_FONT, "TAG1");
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

/*-----------------------------------------------------------------------*/
/*                            Button Box                                 */
/*                            ----------                                 */
/* Contains three buttons, one to activate the changes, to remove a host */
/* and to close this window.                                             */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,     31);
   argcount++;
   box_w = XmCreateForm(form_w, "button_box", args, argcount);

   button_w = XtVaCreateManagedWidget("Update",
                                     xmPushButtonWidgetClass, box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      1,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     1,
                                     XmNrightAttachment,  XmATTACH_POSITION,
                                     XmNrightPosition,    10,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   30,
                                     NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)submite_button, NULL);
   rm_button_w = XtVaCreateManagedWidget("Remove",
                                     xmPushButtonWidgetClass, box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      1,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     11,
                                     XmNrightAttachment,  XmATTACH_POSITION,
                                     XmNrightPosition,    20,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   30,
                                     NULL);
   XtAddCallback(rm_button_w, XmNactivateCallback,
                 (XtCallbackProc)remove_button, NULL);
   button_w = XtVaCreateManagedWidget("Close",
                                     xmPushButtonWidgetClass, box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      1,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     21,
                                     XmNrightAttachment,  XmATTACH_POSITION,
                                     XmNrightPosition,    30,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   30,
                                     NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)close_button, NULL);
   XtManageChild(box_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_bottom_w = XmCreateSeparator(form_w, "h_separator_bottom",
                                            args, argcount);
   XtManageChild(h_separator_bottom_w);

/*-----------------------------------------------------------------------*/
/*                            Status Box                                 */
/*                            ----------                                 */
/* Here any feedback from the program will be shown.                     */
/*-----------------------------------------------------------------------*/
   statusbox_w = XtVaCreateManagedWidget(" ",
                           xmLabelWidgetClass,       form_w,
                           XmNfontList,              fontlist,
                           XmNleftAttachment,        XmATTACH_FORM,
                           XmNrightAttachment,       XmATTACH_FORM,
                           XmNbottomAttachment,      XmATTACH_WIDGET,
                           XmNbottomWidget,          h_separator_bottom_w,
                           NULL);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     statusbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_bottom_w = XmCreateSeparator(form_w, "h_separator_bottom",
                                            args, argcount);
   XtManageChild(h_separator_bottom_w);

/*-----------------------------------------------------------------------*/
/*                             Host List Box                             */
/*                             -------------                            */
/* Lists all hosts that are stored in the FSA. They are listed in there  */
/* short form, ie MAX_HOSTNAME_LENGTH as displayed by afd_ctrl.          */
/* ----------------------------------------------------------------------*/
   /* Create managing widget for host list box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator_bottom_w);
   argcount++;
   box_w = XmCreateForm(form_w, "host_list_box_w", args, argcount);

   (void)memcpy(label_str, HOST_ALIAS_LABEL, HOST_ALIAS_LABEL_LENGTH);

   /* We want at least MAX_HOSTNAME_LENGTH be visible to the user. */
   if (HOST_ALIAS_LABEL_LENGTH < (MAX_HOSTNAME_LENGTH + 4))
   {
      /* Note, the '4 + ' is for the slider. */
      (void)memset(&label_str[HOST_ALIAS_LABEL_LENGTH], ' ',
                   (4 + MAX_HOSTNAME_LENGTH - HOST_ALIAS_LABEL_LENGTH));
      label_str[4 + MAX_HOSTNAME_LENGTH] = ':';
      label_str[4 + MAX_HOSTNAME_LENGTH + 1] = '\0';
   }
   else
   {
      label_str[HOST_ALIAS_LABEL_LENGTH] = ':';
      label_str[HOST_ALIAS_LABEL_LENGTH + 1] = '\0';
   }
   label_w = XtVaCreateManagedWidget(label_str,
                             xmLabelGadgetClass,  box_w,
                             XmNfontList,         fontlist,
                             XmNtopAttachment,    XmATTACH_FORM,
                             XmNtopOffset,        SIDE_OFFSET,
                             XmNleftAttachment,   XmATTACH_FORM,
                             XmNleftOffset,       SIDE_OFFSET,
                             XmNrightAttachment,  XmATTACH_FORM,
                             XmNrightOffset,      SIDE_OFFSET,
                             XmNalignment,        XmALIGNMENT_BEGINNING,
                             NULL);

   /* Add actions. */
   XtAppAddActions(app, action_table, XtNumber(action_table));
   translations = XtParseTranslationTable(translation_table);

   /* Create the host list widget. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,              label_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftOffset,             SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,            SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomOffset,           SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNvisibleItemCount,       10);
   argcount++;
   XtSetArg(args[argcount], XmNselectionPolicy,        XmEXTENDED_SELECT);
   argcount++;
   XtSetArg(args[argcount], XmNfontList,               fontlist);
   argcount++;
   XtSetArg(args[argcount], XmNtranslations,           translations);
   argcount++;
   host_list_w = XmCreateScrolledList(box_w, "host_list_w", args, argcount);
   XtManageChild(host_list_w);
   XtManageChild(box_w);
   XtAddCallback(host_list_w, XmNextendedSelectionCallback, selected, NULL);

   /* Set up host_list_w as a drop site. */
   targets[0] = XmInternAtom(display, "COMPOUND_TEXT", False);
   argcount = 0;
   XtSetArg(args[argcount], XmNimportTargets,      targets);
   argcount++;
   XtSetArg(args[argcount], XmNnumImportTargets,   1);
   argcount++;
   XtSetArg(args[argcount], XmNdropSiteOperations, XmDROP_MOVE);
   argcount++;
   XtSetArg(args[argcount], XmNdropProc,           accept_drop);
   argcount++;
   XmDropSiteRegister(box_w, args, argcount);

/*-----------------------------------------------------------------------*/
/*                          Vertical Separator                           */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator_bottom_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftOffset,       SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   v_separator_w = XmCreateSeparator(form_w, "v_separator", args, argcount);
   XtManageChild(v_separator_w);

/*-----------------------------------------------------------------------*/
/*                           Host Switch Box                             */
/*                           ---------------                             */
/* Allows user to set host or auto host switching.                       */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for other real hostname box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,     SIDE_OFFSET);
   argcount++;
   box_w = XmCreateForm(form_w, "host_switch_box_w", args, argcount);

   host_switch_toggle_w = XtVaCreateManagedWidget("Host switching",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_FORM,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(host_switch_toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)host_switch_toggle,
                 (XtPointer)HOST_SWITCHING);

   host_1_label_w = XtVaCreateManagedWidget("Host toggle character 1:",
                       xmLabelGadgetClass, box_w,
                       XmNfontList,         fontlist,
                       XmNtopAttachment,    XmATTACH_FORM,
                       XmNtopOffset,        SIDE_OFFSET,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       host_switch_toggle_w,
                       XmNleftOffset,       2 * SIDE_OFFSET,
                       XmNbottomAttachment, XmATTACH_FORM,
                       NULL);
   host_1_w = XtVaCreateManagedWidget("",
                       xmTextWidgetClass,   box_w,
                       XmNfontList,         fontlist,
                       XmNcolumns,          1,
                       XmNmaxLength,        1,
                       XmNmarginHeight,     1,
                       XmNmarginWidth,      1,
                       XmNshadowThickness,  1,
                       XmNtopAttachment,    XmATTACH_FORM,
                       XmNtopOffset,        SIDE_OFFSET,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       host_1_label_w,
                       XmNbottomAttachment, XmATTACH_FORM,
                       XmNbottomOffset,     SIDE_OFFSET - 1,
                       XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                       NULL);
   XtAddCallback(host_1_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(host_1_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)HOST_1_ID);
   host_2_label_w = XtVaCreateManagedWidget("Host toggle character 2:",
                       xmLabelGadgetClass,  box_w,
                       XmNfontList,         fontlist,
                       XmNtopAttachment,    XmATTACH_FORM,
                       XmNtopOffset,        SIDE_OFFSET,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       host_1_w,
                       XmNbottomAttachment, XmATTACH_FORM,
                       NULL);
   host_2_w = XtVaCreateManagedWidget("",
                       xmTextWidgetClass,   box_w,
                       XmNfontList,         fontlist,
                       XmNcolumns,          1,
                       XmNmaxLength,        1,
                       XmNmarginHeight,     1,
                       XmNmarginWidth,      1,
                       XmNshadowThickness,  1,
                       XmNtopAttachment,    XmATTACH_FORM,
                       XmNtopOffset,        SIDE_OFFSET,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       host_2_label_w,
                       XmNbottomAttachment, XmATTACH_FORM,
                       XmNbottomOffset,     SIDE_OFFSET - 1,
                       XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                       NULL);
   XtAddCallback(host_2_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(host_2_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)HOST_2_ID);
   auto_toggle_w = XtVaCreateManagedWidget("Auto",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             host_2_w,
                       XmNleftOffset,             2 * SIDE_OFFSET,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(auto_toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)host_switch_toggle,
                 (XtPointer)AUTO_SWITCHING);
   XtManageChild(box_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);

/*-----------------------------------------------------------------------*/
/*                           Real Hostname Box                           */
/*                           -----------------                           */
/* One text widget in which the user can enter the true host name or     */
/* IP-Address of the remote host. Another text widget is there for the   */
/* user to enter a proxy name.                                           */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for other real hostname box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,    62);
   argcount++;
   box_w = XmCreateForm(form_w, "real_hostname_box_w", args, argcount);

   first_label_w = XtVaCreateManagedWidget("Host/IP 1:",
                       xmLabelGadgetClass,  box_w,
                       XmNfontList,         fontlist,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      1,
                       XmNleftAttachment,   XmATTACH_POSITION,
                       XmNleftPosition,     0,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   61,
                       XmNalignment,        XmALIGNMENT_BEGINNING,
                       NULL);
   real_hostname_1_w = XtVaCreateManagedWidget("",
                       xmTextWidgetClass,   box_w,
                       XmNfontList,         fontlist,
                       XmNcolumns,          16,
                       XmNmaxLength,        MAX_REAL_HOSTNAME_LENGTH,
                       XmNmarginHeight,     1,
                       XmNmarginWidth,      1,
                       XmNshadowThickness,  1,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      1,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       first_label_w,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   61,
                       XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                       NULL);
   XtAddCallback(real_hostname_1_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(real_hostname_1_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)REAL_HOST_NAME_1);

   second_label_w = XtVaCreateManagedWidget("2:",
                       xmLabelGadgetClass,  box_w,
                       XmNfontList,         fontlist,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      1,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       real_hostname_1_w,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   61,
                       XmNalignment,        XmALIGNMENT_BEGINNING,
                       NULL);
   real_hostname_2_w = XtVaCreateManagedWidget("",
                       xmTextWidgetClass,   box_w,
                       XmNfontList,         fontlist,
                       XmNcolumns,          16,
                       XmNmaxLength,        MAX_REAL_HOSTNAME_LENGTH,
                       XmNmarginHeight,     1,
                       XmNmarginWidth,      1,
                       XmNshadowThickness,  1,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      1,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       second_label_w,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   61,
                       XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                       NULL);
   XtAddCallback(real_hostname_2_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(real_hostname_2_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)REAL_HOST_NAME_2);

   proxy_label_w = XtVaCreateManagedWidget("Proxy Name:",
                       xmLabelGadgetClass,  box_w,
                       XmNfontList,         fontlist,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      1,
                       XmNleftAttachment,   XmATTACH_POSITION,
                       XmNleftPosition,     33,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   61,
                       XmNalignment,        XmALIGNMENT_BEGINNING,
                       NULL);
   proxy_name_w = XtVaCreateManagedWidget("",
                       xmTextWidgetClass,   box_w,
                       XmNfontList,         fontlist,
                       XmNcolumns,          36,
                       XmNmarginHeight,     1,
                       XmNmarginWidth,      1,
                       XmNshadowThickness,  1,
                       XmNtopAttachment,    XmATTACH_POSITION,
                       XmNtopPosition,      1,
                       XmNleftAttachment,   XmATTACH_WIDGET,
                       XmNleftWidget,       proxy_label_w,
                       XmNbottomAttachment, XmATTACH_POSITION,
                       XmNbottomPosition,   61,
                       XmNrightAttachment,  XmATTACH_FORM,
                       XmNrightOffset,      SIDE_OFFSET,
                       XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                       NULL);
   XtAddCallback(proxy_name_w, XmNvalueChangedCallback, value_change, NULL);
   XtAddCallback(proxy_name_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)PROXY_NAME);
   XtManageChild(box_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   box_w = XmCreateForm(form_w, "real_hostname_box_w", args, argcount);
   use_file_when_local_w = XtVaCreateManagedWidget("Use scheme file when hostname matches local nodename",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNleftAttachment,         XmATTACH_FORM,
                       XmNleftOffset,             SIDE_OFFSET,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       XmNset,                    False,
                       NULL);
   XtAddCallback(use_file_when_local_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)FILE_WHEN_LOCAL_CHANGED);
   XtManageChild(box_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);

/*-----------------------------------------------------------------------*/
/*                         Text Input Box                                */
/*                         --------------                                */
/* Here more control parameters can be entered, such as: maximum number  */
/* of errors, transfer timeout, retry interval and successful retries.   */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for other text input box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,       SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,     SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,    60);
   argcount++;
   box_w = XmCreateForm(form_w, "text_input_box", args, argcount);

   transfer_timeout_label_w = XtVaCreateManagedWidget("Transfer timeout:",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      0,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   20,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   transfer_timeout_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          4,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       transfer_timeout_label_w,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(transfer_timeout_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(transfer_timeout_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(transfer_timeout_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)TRANSFER_TIMEOUT);
   interrupt_w = XtVaCreateManagedWidget("Interrupt",
                           xmToggleButtonGadgetClass, box_w,
                           XmNfontList,               fontlist,
                           XmNtopAttachment,          XmATTACH_POSITION,
                           XmNtopPosition,            0,
                           XmNleftAttachment,         XmATTACH_WIDGET,
                           XmNleftWidget,             transfer_timeout_w,
                           XmNbottomAttachment,       XmATTACH_POSITION,
                           XmNbottomPosition,         20,
                           XmNset,                    False,
                           NULL);
   XtAddCallback(interrupt_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)TIMEOUT_TRANSFER_CHANGED);


   ignore_errors_toggle_w = XtVaCreateManagedWidget("Ignore errors+warnings",
                           xmToggleButtonGadgetClass, box_w,
                           XmNfontList,               fontlist,
                           XmNtopAttachment,          XmATTACH_POSITION,
                           XmNtopPosition,            0,
                           XmNleftAttachment,         XmATTACH_POSITION,
                           XmNleftPosition,           29,
                           XmNbottomAttachment,       XmATTACH_POSITION,
                           XmNbottomPosition,         20,
                           XmNset,                    False,
                           NULL);
   XtAddCallback(ignore_errors_toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)ERROR_OFFLINE_STATIC_CHANGED);

   do_not_delete_data_toggle_w = XtVaCreateManagedWidget("Do not delete data",
                           xmToggleButtonGadgetClass, box_w,
                           XmNfontList,               fontlist,
                           XmNtopAttachment,          XmATTACH_POSITION,
                           XmNtopPosition,            0,
                           XmNleftAttachment,         XmATTACH_POSITION,
                           XmNleftPosition,           43,
                           XmNbottomAttachment,       XmATTACH_POSITION,
                           XmNbottomPosition,         20,
                           XmNset,                    False,
                           NULL);
   XtAddCallback(do_not_delete_data_toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)DO_NOT_DELETE_DATA_CHANGED);


   max_errors_label_w = XtVaCreateManagedWidget("Maximum errors  :",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      20,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   40,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   max_errors_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          4,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      21,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       max_errors_label_w,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(max_errors_w, XmNmodifyVerifyCallback, check_nummeric, NULL);
   XtAddCallback(max_errors_w, XmNvalueChangedCallback, value_change, NULL);
   XtAddCallback(max_errors_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)MAXIMUM_ERRORS);

   successful_retries_label_w = XtVaCreateManagedWidget("Successful retries :",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      20,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       max_errors_w,
                           XmNleftOffset,       5,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   40,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   successful_retries_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          4,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      21,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       successful_retries_label_w,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(successful_retries_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(successful_retries_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(successful_retries_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)SUCCESSFUL_RETRIES);

   retry_interval_label_w = XtVaCreateManagedWidget("Retry interval :",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      20,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       successful_retries_w,
                           XmNleftOffset,       5,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   40,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   retry_interval_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          4,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      21,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       retry_interval_label_w,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(retry_interval_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(retry_interval_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(retry_interval_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)RETRY_INTERVAL);

   keep_connected_label_w = XtVaCreateManagedWidget("Keep connected  :",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      40,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     0,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   60,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   keep_connected_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          6,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      41,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       keep_connected_label_w,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(keep_connected_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(keep_connected_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(keep_connected_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)KEEP_CONNECTED);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNtopPosition,      40);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       keep_connected_w);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNbottomPosition,   60);
   argcount++;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,          XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNnumColumns,       1);
   argcount++;
   XtSetArg(args[argcount], XmNresizeHeight,     False);
   argcount++;
   keep_connected_radio_w = XmCreateRadioBox(box_w, "kc_radiobox",
                                             args, argcount);
   kc_both_w = XtVaCreateManagedWidget("Both",
                            xmToggleButtonGadgetClass, keep_connected_radio_w,
                            XmNfontList,               fontlist,
                            XmNset,                    True,
                            NULL);
   XtAddCallback(kc_both_w, XmNdisarmCallback,
                 (XtCallbackProc)kc_radio_button, (XtPointer)KC_BOTH_SEL);
   kc_fetch_w = XtVaCreateManagedWidget("Fetch",
                            xmToggleButtonGadgetClass, keep_connected_radio_w,
                            XmNfontList,               fontlist,
                            XmNset,                    False,
                            NULL);
   XtAddCallback(kc_fetch_w, XmNdisarmCallback,
                 (XtCallbackProc)kc_radio_button, (XtPointer)KC_FETCH_ONLY_SEL);
   kc_send_w = XtVaCreateManagedWidget("Send",
                            xmToggleButtonGadgetClass, keep_connected_radio_w,
                            XmNfontList,               fontlist,
                            XmNset,                    False,
                            NULL);
   XtAddCallback(kc_send_w, XmNdisarmCallback,
                 (XtCallbackProc)kc_radio_button, (XtPointer)KC_SEND_ONLY_SEL);
   XtManageChild(keep_connected_radio_w);

   disconnect_w = XtVaCreateManagedWidget("Disconnect",
                           xmToggleButtonGadgetClass, box_w,
                           XmNfontList,               fontlist,
                           XmNtopAttachment,          XmATTACH_POSITION,
                           XmNtopPosition,            40,
                           XmNleftAttachment,         XmATTACH_WIDGET,
                           XmNleftWidget,             keep_connected_radio_w,
                           XmNbottomAttachment,       XmATTACH_POSITION,
                           XmNbottomPosition,         60,
                           XmNset,                    False,
                           NULL);
   XtAddCallback(disconnect_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)DISCONNECT_CHANGED);

   warn_time_label_w = XtVaCreateManagedWidget("Warn time :",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      40,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     34,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   60,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   warn_time_days_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          6,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      41,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       warn_time_label_w,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(warn_time_days_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(warn_time_days_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(warn_time_days_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)WARN_TIME_DAYS);
   warn_time_days_label_w = XtVaCreateManagedWidget("days ",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      40,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       warn_time_days_w,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   60,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   warn_time_hours_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          2,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      41,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       warn_time_days_label_w,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(warn_time_hours_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(warn_time_hours_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(warn_time_hours_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)WARN_TIME_HOURS);
   warn_time_hours_label_w = XtVaCreateManagedWidget("hours ",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      40,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       warn_time_hours_w,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   60,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   warn_time_mins_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          2,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      41,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       warn_time_hours_label_w,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(warn_time_mins_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(warn_time_mins_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(warn_time_mins_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)WARN_TIME_MINS);
   warn_time_mins_label_w = XtVaCreateManagedWidget("mins ",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      40,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       warn_time_mins_w,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   60,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   warn_time_secs_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          2,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      41,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       warn_time_mins_label_w,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(warn_time_secs_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(warn_time_secs_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(warn_time_secs_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)WARN_TIME_SECS);
   warn_time_secs_label_w = XtVaCreateManagedWidget("secs",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      40,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       warn_time_secs_w,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   60,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   XtManageChild(box_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);

/*-----------------------------------------------------------------------*/
/*                       General Transfer Parameters                     */
/*                       ---------------------------                     */
/* Here transfer control parameters can be entered such as the transfer  */
/* rate limit.                                                           */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,       SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,     SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,     61);
   argcount++;
   box_w = XmCreateForm(form_w, "transfer_input_box", args, argcount);

   transfer_rate_limit_label_w = XtVaCreateManagedWidget(
                           "Transfer rate limit (in kilobytes) :",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   30,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   transfer_rate_limit_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          7,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       transfer_rate_limit_label_w,
                           XmNleftOffset,       SIDE_OFFSET,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   30,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(transfer_rate_limit_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(transfer_rate_limit_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(transfer_rate_limit_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)TRANSFER_RATE_LIMIT);
   socket_send_buffer_size_label_w = XtVaCreateManagedWidget(
                           "Socket send buffer size (in kilobytes)    :",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     29,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   30,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   socket_send_buffer_size_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          7,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       socket_send_buffer_size_label_w,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   30,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(socket_send_buffer_size_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(socket_send_buffer_size_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(socket_send_buffer_size_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)SOCKET_SEND_BUFFER);
   socket_receive_buffer_size_label_w = XtVaCreateManagedWidget(
                           "Socket receive buffer size (in kilobytes) :",
                           xmLabelGadgetClass,  box_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      31,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     29,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   60,
                           XmNalignment,        XmALIGNMENT_BEGINNING,
                           NULL);
   socket_receive_buffer_size_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   box_w,
                           XmNfontList,         fontlist,
                           XmNcolumns,          7,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      31,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       socket_receive_buffer_size_label_w,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   60,
                           XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                           NULL);
   XtAddCallback(socket_receive_buffer_size_w, XmNmodifyVerifyCallback, check_nummeric,
                 NULL);
   XtAddCallback(socket_receive_buffer_size_w, XmNvalueChangedCallback, value_change,
                 NULL);
   XtAddCallback(socket_receive_buffer_size_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)SOCKET_RECEIVE_BUFFER);
   XtManageChild(box_w);

#ifdef WITH_DUP_CHECK
/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);

/*-----------------------------------------------------------------------*/
/*                           Check for duplicates                        */
/*                           --------------------                        */
/* Parameters of how to perform the dupcheck, what action should be      */
/* taken and when to remove the checksum.                                */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,      SIDE_OFFSET);
   argcount++;
   box_w = XmCreateForm(form_w, "dupcheck_box_w", args, argcount);

   dc_label_w = XtVaCreateManagedWidget("Check for duplicates :",
                            xmLabelGadgetClass,  box_w,
                            XmNfontList,         fontlist,
                            XmNalignment,        XmALIGNMENT_END,
                            XmNtopAttachment,    XmATTACH_FORM,
                            XmNleftAttachment,   XmATTACH_FORM,
                            XmNleftOffset,       5,
                            XmNbottomAttachment, XmATTACH_FORM,
                            NULL);
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       dc_label_w);
   argcount++;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,          XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNnumColumns,       1);
   argcount++;
   dupcheck_w = XmCreateRadioBox(box_w, "radiobox", args, argcount);
   dc_enable_w = XtVaCreateManagedWidget("Enabled",
                            xmToggleButtonGadgetClass, dupcheck_w,
                            XmNfontList,               fontlist,
                            XmNset,                    True,
                            NULL);
   XtAddCallback(dc_enable_w, XmNdisarmCallback,
                 (XtCallbackProc)edc_radio_button,
                 (XtPointer)ENABLE_DUPCHECK_SEL);
   dc_disable_w = XtVaCreateManagedWidget("Disabled",
                            xmToggleButtonGadgetClass, dupcheck_w,
                            XmNfontList,               fontlist,
                            XmNset,                    False,
                            NULL);
   XtAddCallback(dc_disable_w, XmNdisarmCallback,
                 (XtCallbackProc)edc_radio_button,
                 (XtPointer)DISABLE_DUPCHECK_SEL);
   XtManageChild(dupcheck_w);

   dc_timeout_label_w = XtVaCreateManagedWidget("Timeout:",
                            xmLabelGadgetClass,  box_w,
                            XmNfontList,         fontlist,
                            XmNtopAttachment,    XmATTACH_FORM,
                            XmNleftAttachment,   XmATTACH_WIDGET,
                            XmNleftWidget,       dupcheck_w,
                            XmNleftOffset,       SIDE_OFFSET,
                            XmNbottomAttachment, XmATTACH_FORM,
                            XmNalignment,        XmALIGNMENT_BEGINNING,
                            NULL);
   dc_timeout_w = XtVaCreateManagedWidget("",
                            xmTextWidgetClass,   box_w,
                            XmNfontList,         fontlist,
                            XmNcolumns,          7,
                            XmNmarginHeight,     1,
                            XmNmarginWidth,      1,
                            XmNshadowThickness,  1,
                            XmNtopAttachment,    XmATTACH_FORM,
                            XmNtopOffset,        SIDE_OFFSET,
                            XmNleftAttachment,   XmATTACH_WIDGET,
                            XmNleftWidget,       dc_timeout_label_w,
                            XmNdropSiteActivity, XmDROP_SITE_INACTIVE,
                            NULL);
   XtAddCallback(dc_timeout_w, XmNmodifyVerifyCallback, check_nummeric, NULL);
   XtAddCallback(dc_timeout_w, XmNvalueChangedCallback, value_change, NULL);
   XtAddCallback(dc_timeout_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)DC_TIMEOUT);

   dc_timeout_fixed_w = XtVaCreateManagedWidget("Fixed",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             dc_timeout_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(dc_timeout_fixed_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)DC_TIMEOUT_FIXED_CHANGED);

   dc_ref_label_w = XtVaCreateManagedWidget("Reference :",
                            xmLabelGadgetClass,  box_w,
                            XmNfontList,         fontlist,
                            XmNalignment,        XmALIGNMENT_END,
                            XmNtopAttachment,    XmATTACH_WIDGET,
                            XmNtopWidget,        h_separator_top_w,
                            XmNleftAttachment,   XmATTACH_WIDGET,
                            XmNleftWidget,       dc_timeout_fixed_w,
                            XmNleftOffset,       10,
                            XmNbottomAttachment, XmATTACH_FORM,
                            NULL);
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       dc_ref_label_w);
   argcount++;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,          XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNnumColumns,       1);
   argcount++;
   dc_reference_w = XmCreateRadioBox(box_w, "radiobox", args, argcount);
   dc_alias_w = XtVaCreateManagedWidget("Alias",
                            xmToggleButtonGadgetClass, dc_reference_w,
                            XmNfontList,               fontlist,
                            XmNset,                    True,
                            NULL);
   XtAddCallback(dc_alias_w, XmNdisarmCallback,
                 (XtCallbackProc)dc_ref_radio_button,
                 (XtPointer)ALIAS_DUPCHECK_SEL);
   dc_recipient_w = XtVaCreateManagedWidget("Recipient",
                            xmToggleButtonGadgetClass, dc_reference_w,
                            XmNfontList,               fontlist,
                            XmNset,                    False,
                            NULL);
   XtAddCallback(dc_recipient_w, XmNdisarmCallback,
                 (XtCallbackProc)dc_ref_radio_button,
                 (XtPointer)RECIPIENT_DUPCHECK_SEL);
   XtManageChild(dc_reference_w);
   XtManageChild(box_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,      SIDE_OFFSET);
   argcount++;
   box_w = XmCreateForm(form_w, "dupcheck_box_w", args, argcount);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,          XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNnumColumns,       1);
   argcount++;
   dc_type_w = XmCreateRadioBox(box_w, "radiobox", args, argcount);
   dc_filename_w = XtVaCreateManagedWidget("Name",
                                   xmToggleButtonGadgetClass, dc_type_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    True,
                                   NULL);
   XtAddCallback(dc_filename_w, XmNdisarmCallback,
                 (XtCallbackProc)dc_type_radio_button,
                 (XtPointer)FILE_NAME_SEL);
   dc_namesize_w = XtVaCreateManagedWidget("Name+size",
                                   xmToggleButtonGadgetClass, dc_type_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    False,
                                   NULL);
   XtAddCallback(dc_namesize_w, XmNdisarmCallback,
                 (XtCallbackProc)dc_type_radio_button,
                 (XtPointer)FILE_NAMESIZE_SEL);
   dc_nosuffix_w = XtVaCreateManagedWidget("Name no suffix",
                                   xmToggleButtonGadgetClass, dc_type_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    False,
                                   NULL);
   XtAddCallback(dc_nosuffix_w, XmNdisarmCallback,
                 (XtCallbackProc)dc_type_radio_button,
                 (XtPointer)FILE_NOSUFFIX_SEL);
   dc_filecontent_w = XtVaCreateManagedWidget("Content",
                                   xmToggleButtonGadgetClass, dc_type_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    False,
                                   NULL);
   XtAddCallback(dc_filecontent_w, XmNdisarmCallback,
                 (XtCallbackProc)dc_type_radio_button,
                 (XtPointer)FILE_CONTENT_SEL);
   dc_filenamecontent_w = XtVaCreateManagedWidget("Name+content",
                                   xmToggleButtonGadgetClass, dc_type_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    False,
                                   NULL);
   XtAddCallback(dc_filenamecontent_w, XmNdisarmCallback,
                 (XtCallbackProc)dc_type_radio_button,
                 (XtPointer)FILE_NAME_CONTENT_SEL);
   XtManageChild(dc_type_w);

   dc_delete_w = XtVaCreateManagedWidget("Delete",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    True,
                       XmNtopAttachment,          XmATTACH_WIDGET,
                       XmNtopWidget,              dc_type_w,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_FORM,
                       NULL);
   XtAddCallback(dc_delete_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button, (XtPointer)DC_DELETE_CHANGED);
   dc_store_w = XtVaCreateManagedWidget("Store",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_WIDGET,
                       XmNtopWidget,              dc_type_w,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             dc_delete_w,
                       NULL);
   XtAddCallback(dc_store_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button, (XtPointer)DC_STORE_CHANGED);
   dc_warn_w = XtVaCreateManagedWidget("Warn",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_WIDGET,
                       XmNtopWidget,              dc_type_w,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             dc_store_w,
                       NULL);
   XtAddCallback(dc_warn_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button, (XtPointer)DC_WARN_CHANGED);

   dc_crc_label_w = XtVaCreateManagedWidget("CRC type :",
                            xmLabelGadgetClass,  box_w,
                            XmNfontList,         fontlist,
                            XmNalignment,        XmALIGNMENT_END,
                            XmNtopAttachment,    XmATTACH_WIDGET,
                            XmNtopWidget,        dc_type_w,
                            XmNleftAttachment,   XmATTACH_WIDGET,
                            XmNleftWidget,       dc_warn_w,
                            XmNleftOffset,       5,
                            XmNbottomAttachment, XmATTACH_FORM,
                            NULL);
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        dc_type_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       dc_crc_label_w);
   argcount++;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,          XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNnumColumns,       1);
   argcount++;
   dc_crc_w = XmCreateRadioBox(box_w, "radiobox", args, argcount);
   dc_crc32_w = XtVaCreateManagedWidget("CRC-32",
                            xmToggleButtonGadgetClass, dc_crc_w,
                            XmNfontList,               fontlist,
                            XmNset,                    True,
                            NULL);
   XtAddCallback(dc_crc32_w, XmNdisarmCallback,
                 (XtCallbackProc)dc_crc_radio_button,
                 (XtPointer)CRC32_DUPCHECK_SEL);
   dc_crc32c_w = XtVaCreateManagedWidget("CRC-32c",
                            xmToggleButtonGadgetClass, dc_crc_w,
                            XmNfontList,               fontlist,
                            XmNset,                    False,
                            NULL);
   XtAddCallback(dc_crc32c_w, XmNdisarmCallback,
                 (XtCallbackProc)dc_crc_radio_button,
                 (XtPointer)CRC32C_DUPCHECK_SEL);
   dc_murmur3_w = XtVaCreateManagedWidget("Murmur3",
                            xmToggleButtonGadgetClass, dc_crc_w,
                            XmNfontList,               fontlist,
                            XmNset,                    False,
                            NULL);
   XtAddCallback(dc_murmur3_w, XmNdisarmCallback,
                 (XtCallbackProc)dc_crc_radio_button,
                 (XtPointer)MURMUR3_DUPCHECK_SEL);
   XtManageChild(dc_crc_w);
   XtManageChild(box_w);
#endif /* WITH_DUP_CHECK */

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);

/*-----------------------------------------------------------------------*/
/*                             Option Box                                */
/*                             ----------                                */
/* Here more control parameters can be selected, such as: maximum number */
/* of parallel transfers, transfer blocksize and file size offset.       */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for other text input box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,     SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,    61);
   argcount++;
   box_w = XmCreateForm(form_w, "text_input_box", args, argcount);

   /* Parallel transfers. */
   pt.label_w = XtVaCreateManagedWidget("Max. parallel transfers:",
                                     xmLabelGadgetClass,  box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      1,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   60,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     0,
                                     NULL);
   create_option_menu_pt(box_w, fontlist);

   /* Transfer blocksize. */
   tb.label_w = XtVaCreateManagedWidget("Transfer Blocksize:",
                                     xmLabelGadgetClass,  box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      1,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   60,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     19,
                                     NULL);
   create_option_menu_tb(box_w, fontlist);

   /* File size offset. */
   fso.label_w = XtVaCreateManagedWidget("File size offset for append:",
                                     xmLabelGadgetClass,  box_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_POSITION,
                                     XmNtopPosition,      1,
                                     XmNbottomAttachment, XmATTACH_POSITION,
                                     XmNbottomPosition,   60,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     39,
                                     NULL);
   create_option_menu_fso(box_w, fontlist);
   XtManageChild(box_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,        SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   h_separator_top_w = XmCreateSeparator(form_w, "h_separator_top",
                                         args, argcount);
   XtManageChild(h_separator_top_w);

/*-----------------------------------------------------------------------*/
/*                       Protocol Specific Options                       */
/*                       -------------------------                       */
/* Select FTP active or passive mode and set FTP idle time for remote    */
/* FTP-server.                                                           */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for the protocol specific option box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       h_separator_top_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,     SIDE_OFFSET);
   argcount++;
   box_w = XmCreateForm(form_w, "protocol_specific1_box_w", args, argcount);

   mode_label_w = XtVaCreateManagedWidget("FTP Mode :",
                                xmLabelGadgetClass,  box_w,
                                XmNfontList,         fontlist,
                                XmNalignment,        XmALIGNMENT_END,
                                XmNtopAttachment,    XmATTACH_FORM,
                                XmNleftAttachment,   XmATTACH_FORM,
                                XmNleftOffset,       5,
                                XmNbottomAttachment, XmATTACH_FORM,
                                NULL);
   extended_mode_w = XtVaCreateManagedWidget("Extended",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             mode_label_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(extended_mode_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button,
                 (XtPointer)FTP_EXTENDED_MODE_CHANGED);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       extended_mode_w);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,          XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNnumColumns,       1);
   argcount++;
   ftp_mode_w = XmCreateRadioBox(box_w, "radiobox", args, argcount);
   active_mode_w = XtVaCreateManagedWidget("Active",
                                   xmToggleButtonGadgetClass, ftp_mode_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    True,
                                   NULL);
   XtAddCallback(active_mode_w, XmNdisarmCallback,
                 (XtCallbackProc)ftp_mode_radio_button,
                 (XtPointer)FTP_ACTIVE_MODE_SEL);
   passive_mode_w = XtVaCreateManagedWidget("Passive",
                                   xmToggleButtonGadgetClass, ftp_mode_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    False,
                                   NULL);
   XtAddCallback(passive_mode_w, XmNdisarmCallback,
                 (XtCallbackProc)ftp_mode_radio_button,
                 (XtPointer)FTP_PASSIVE_MODE_SEL);
   XtManageChild(ftp_mode_w);
   passive_redirect_w = XtVaCreateManagedWidget("Redirect",
                           xmToggleButtonGadgetClass, box_w,
                           XmNfontList,               fontlist,
                           XmNset,                    False,
                           XmNtopAttachment,          XmATTACH_FORM,
                           XmNtopOffset,              SIDE_OFFSET,
                           XmNleftAttachment,         XmATTACH_WIDGET,
                           XmNleftWidget,             ftp_mode_w,
                           XmNbottomAttachment,       XmATTACH_FORM,
                           NULL);
   XtAddCallback(passive_redirect_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)FTP_PASSIVE_REDIRECT_CHANGED);
   use_list_w = XtVaCreateManagedWidget("Use LIST",
                           xmToggleButtonGadgetClass, box_w,
                           XmNfontList,               fontlist,
                           XmNset,                    False,
                           XmNtopAttachment,          XmATTACH_FORM,
                           XmNtopOffset,              SIDE_OFFSET,
                           XmNleftAttachment,         XmATTACH_WIDGET,
                           XmNleftWidget,             passive_redirect_w,
                           XmNbottomAttachment,       XmATTACH_FORM,
                           NULL);
   XtAddCallback(use_list_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2, (XtPointer)USE_LIST_CHANGED);
   use_stat_list_w = XtVaCreateManagedWidget("STAT",
                           xmToggleButtonGadgetClass, box_w,
                           XmNfontList,               fontlist,
                           XmNset,                    False,
                           XmNtopAttachment,          XmATTACH_FORM,
                           XmNtopOffset,              SIDE_OFFSET,
                           XmNleftAttachment,         XmATTACH_WIDGET,
                           XmNleftWidget,             use_list_w,
                           XmNbottomAttachment,       XmATTACH_FORM,
                           NULL);
   XtAddCallback(use_stat_list_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)USE_STAT_LIST_CHANGED);
   disable_mlst_w = XtVaCreateManagedWidget("Disable MLST",
                           xmToggleButtonGadgetClass, box_w,
                           XmNfontList,               fontlist,
                           XmNset,                    False,
                           XmNtopAttachment,          XmATTACH_FORM,
                           XmNtopOffset,              SIDE_OFFSET,
                           XmNleftAttachment,         XmATTACH_WIDGET,
                           XmNleftWidget,             use_stat_list_w,
                           XmNbottomAttachment,       XmATTACH_FORM,
                           NULL);
   XtAddCallback(disable_mlst_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)DISABLE_MLST_CHANGED);
   send_utf8_on_w = XtVaCreateManagedWidget("Send UTF8 on",
                           xmToggleButtonGadgetClass, box_w,
                           XmNfontList,               fontlist,
                           XmNset,                    False,
                           XmNtopAttachment,          XmATTACH_FORM,
                           XmNtopOffset,              SIDE_OFFSET,
                           XmNleftAttachment,         XmATTACH_WIDGET,
                           XmNleftWidget,             disable_mlst_w,
                           XmNbottomAttachment,       XmATTACH_FORM,
                           NULL);
   XtAddCallback(send_utf8_on_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button3,
                 (XtPointer)SEND_UTF8_ON_CHANGED);
   ftps_label_w = XtVaCreateManagedWidget("FTPS :",
                                xmLabelGadgetClass,  box_w,
                                XmNfontList,         fontlist,
                                XmNalignment,        XmALIGNMENT_END,
                                XmNtopAttachment,    XmATTACH_FORM,
                                XmNleftAttachment,   XmATTACH_WIDGET,
                                XmNleftWidget,       send_utf8_on_w,
                                XmNleftOffset,       5,
                                XmNbottomAttachment, XmATTACH_FORM,
                                NULL);
   ssl_ccc_w = XtVaCreateManagedWidget("Clear Control Connection",
                           xmToggleButtonGadgetClass, box_w,
                           XmNfontList,               fontlist,
                           XmNset,                    False,
                           XmNtopAttachment,          XmATTACH_FORM,
                           XmNtopOffset,              SIDE_OFFSET,
                           XmNleftAttachment,         XmATTACH_WIDGET,
                           XmNleftWidget,             ftps_label_w,
                           XmNbottomAttachment,       XmATTACH_FORM,
                           NULL);
   XtAddCallback(ssl_ccc_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2, (XtPointer)FTPS_CCC_CHANGED);
   ssl_implicit_ftps_w = XtVaCreateManagedWidget("Implicit",
                           xmToggleButtonGadgetClass, box_w,
                           XmNfontList,               fontlist,
                           XmNset,                    False,
                           XmNtopAttachment,          XmATTACH_FORM,
                           XmNtopOffset,              SIDE_OFFSET,
                           XmNleftAttachment,         XmATTACH_WIDGET,
                           XmNleftWidget,             ssl_ccc_w,
                           XmNbottomAttachment,       XmATTACH_FORM,
                           NULL);
   XtAddCallback(ssl_implicit_ftps_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2, (XtPointer)FTPS_IMPLICIT_CHANGED);
   XtManageChild(box_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,      SIDE_OFFSET);
   argcount++;
   box_w = XmCreateForm(form_w, "protocol_specific2_box_w", args, argcount);
   ftp_idle_time_w = XtVaCreateManagedWidget("Set idle time",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_FORM,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(ftp_idle_time_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button,
                 (XtPointer)FTP_SET_IDLE_TIME_CHANGED);
   ftp_keepalive_w = XtVaCreateManagedWidget("STAT Keepalive",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             ftp_idle_time_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(ftp_keepalive_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button, 
                 (XtPointer)FTP_KEEPALIVE_CHANGED);
   ftp_fast_rename_w = XtVaCreateManagedWidget("Fast rename",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             ftp_keepalive_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(ftp_fast_rename_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button, 
                 (XtPointer)FTP_FAST_RENAME_CHANGED);
   ftp_fast_cd_w = XtVaCreateManagedWidget("Fast cd",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             ftp_fast_rename_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(ftp_fast_cd_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button, 
                 (XtPointer)FTP_FAST_CD_CHANGED);
   ftp_ignore_bin_w = XtVaCreateManagedWidget("Ignore type I",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             ftp_fast_cd_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(ftp_ignore_bin_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button, 
                 (XtPointer)FTP_IGNORE_BIN_CHANGED);
#ifdef _WITH_BURST_2
   allow_burst_w = XtVaCreateManagedWidget("Allow burst",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    True,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             ftp_ignore_bin_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(allow_burst_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)ALLOW_BURST_CHANGED);
#endif
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
   tcp_keepalive_w = XtVaCreateManagedWidget("TCP Keepalive",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
# ifdef _WITH_BURST_2
                       XmNleftWidget,             allow_burst_w,
# else
                       XmNleftWidget,             ftp_ignore_bin_w,
# endif
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(tcp_keepalive_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)TCP_KEEPALIVE_CHANGED);
#endif
   bucketname_in_path_w = XtVaCreateManagedWidget("Bucketname in path",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
#ifdef FTP_CTRL_KEEP_ALIVE_INTERVAL
                       XmNleftWidget,             tcp_keepalive_w,
#else
# ifdef _WITH_BURST_2
                       XmNleftWidget,             allow_burst_w,
# else
                       XmNleftWidget,             ftp_ignore_bin_w,
# endif
#endif
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(bucketname_in_path_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)BUCKETNAME_IN_PATH_CHANGED);
   no_expect_w = XtVaCreateManagedWidget("No expect",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             bucketname_in_path_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(no_expect_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2, (XtPointer)NO_EXPECT_CHANGED);
   XtManageChild(box_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,        box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,      SIDE_OFFSET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator_bottom_w);
   argcount++;
   box_w = XmCreateForm(form_w, "protocol_specific2_box_w", args, argcount);
   sequence_locking_w = XtVaCreateManagedWidget("Seq. Locking",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_FORM,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(sequence_locking_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)USE_SEQUENCE_LOCKING_CHANGED);
   compression_w = XtVaCreateManagedWidget("Compression",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             sequence_locking_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(compression_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2, (XtPointer)COMPRESION_CHANGED);
   disable_strict_host_key_w = XtVaCreateManagedWidget("Strict Host Key",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             compression_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(disable_strict_host_key_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2, (XtPointer)DISABLE_STRICT_HOST_KEY_CHANGED);
   keep_time_stamp_w = XtVaCreateManagedWidget("Keep time stamp",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    True,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             disable_strict_host_key_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(keep_time_stamp_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)KEEP_TIME_STAMP_CHANGED);
   sort_file_names_w = XtVaCreateManagedWidget("Sort file names",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             keep_time_stamp_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(sort_file_names_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)SORT_FILE_NAMES_CHANGED);
   no_ageing_jobs_w = XtVaCreateManagedWidget("No ageing",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             sort_file_names_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(no_ageing_jobs_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)NO_AGEING_JOBS_CHANGED);
   match_size_w = XtVaCreateManagedWidget("Match size",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             no_ageing_jobs_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(match_size_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2, (XtPointer)CHECK_SIZE_CHANGED);
#ifdef WITH_SSL
   strict_tls_w = XtVaCreateManagedWidget("Strict TLS",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             match_size_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(strict_tls_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2, (XtPointer)STRICT_TLS_CHANGED);
   tls_legacy_renegotiation_w = XtVaCreateManagedWidget("Legacy renegotiation",
                       xmToggleButtonGadgetClass, box_w,
                       XmNfontList,               fontlist,
                       XmNset,                    False,
                       XmNtopAttachment,          XmATTACH_FORM,
                       XmNtopOffset,              SIDE_OFFSET,
                       XmNleftAttachment,         XmATTACH_WIDGET,
                       XmNleftWidget,             strict_tls_w,
                       XmNbottomAttachment,       XmATTACH_FORM,
                       NULL);
   XtAddCallback(tls_legacy_renegotiation_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button2,
                 (XtPointer)TLS_LEGACY_RENEGOTIATION_CHANGED);
#endif /* WITH_SSL */
   XtManageChild(box_w);
   XtManageChild(form_w);

#ifdef WITH_EDITRES
   XtAddEventHandler(appshell, (EventMask)0, True,
                     _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets. */
   XtRealizeWidget(appshell);
   wait_visible(appshell);

   /* Set some signal handlers. */
   if ((signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(WARN_DIALOG, "Failed to set signal handler's for %s : %s",
                 EDIT_HC, strerror(errno));
   }

   /* Initialize widgets with data. */
   init_widget_data();

   /* Start the main event-handling loop. */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ init_edit_hc() +++++++++++++++++++++++++++*/
static void
init_edit_hc(int *argc, char *argv[], char *window_title)
{
   int  ret,
        user_offset;
   char hostname[MAX_AFD_NAME_LENGTH],
        *perm_buffer,
        profile[MAX_PROFILE_NAME_LENGTH + 1],
        *p_user,
        selected_host[MAX_HOSTNAME_LENGTH + 1];

   if ((get_arg(argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
      exit(SUCCESS);
   }
   if (get_afd_path(argc, argv, p_work_dir) < 0)
   {
      exit(INCORRECT);
   }

   /* Check if title is specified. */
   if (get_arg(argc, argv, "-t", font_name, 40) == INCORRECT)
   {
      /* Prepare title of this window. */
      (void)strcpy(window_title, "Host Config ");
      if (get_afd_name(hostname) == INCORRECT)
      {
         if (gethostname(hostname, MAX_AFD_NAME_LENGTH) == 0)
         {
            hostname[0] = toupper((int)hostname[0]);
            (void)strcat(window_title, hostname);
         }
      }
      else
      {
         (void)strcat(window_title, hostname);
      }
   }
   else
   {
      (void)snprintf(window_title, MAX_WNINDOW_TITLE_LENGTH,
                     "Host Config %s", font_name);
   }

#ifdef WITH_SETUID_PROGS
   set_afd_euid(p_work_dir);
#endif

   /* Do not start if binary dataset matches the one stort on disk. */
   if (check_typesize_data(NULL, NULL, NO) > 0)
   {
      (void)fprintf(stderr,
                    "The compiled binary does not match stored database.\n");
      (void)fprintf(stderr,
                    "Initialize database with the command : afd -i\n");
      exit(INCORRECT);
   }

   if (get_arg(argc, argv, "-h", selected_host,
               MAX_HOSTNAME_LENGTH) == INCORRECT)
   {
      selected_host[0] = '\0';
   }
   if (get_arg(argc, argv, "-p", profile, MAX_PROFILE_NAME_LENGTH) == INCORRECT)
   {
      user_offset = 0;
      profile[0] = '\0';
   }
   else
   {
      (void)my_strncpy(user, profile, MAX_FULL_USER_ID_LENGTH);
      user_offset = strlen(profile);
   }
   if (get_arg(argc, argv, "-f", font_name, 40) == INCORRECT)
   {
      (void)strcpy(font_name, DEFAULT_FONT);
   }

   /* Now lets see if user may use this program. */
   check_fake_user(argc, argv, AFD_CONFIG_FILE, fake_user);
   switch (get_permissions(&perm_buffer, fake_user, profile))
   {
      case NO_ACCESS : /* Cannot access afd.users file. */
         {
            char afd_user_file[MAX_PATH_LENGTH];

            (void)strcpy(afd_user_file, p_work_dir);
            (void)strcat(afd_user_file, ETC_DIR);
            (void)strcat(afd_user_file, AFD_USER_FILE);

            (void)fprintf(stderr,
                          "Failed to access `%s', unable to determine users permissions.\n",
                          afd_user_file);
         }
         exit(INCORRECT);

      case NONE     : (void)fprintf(stderr, "%s (%s %d)\n",
                                    PERMISSION_DENIED_STR, __FILE__, __LINE__);
                      exit(INCORRECT);

      case SUCCESS  : /* Lets evaluate the permissions and see what */
                      /* the user may do.                           */
                      if ((perm_buffer[0] == 'a') &&
                          (perm_buffer[1] == 'l') &&
                          (perm_buffer[2] == 'l') &&
                          ((perm_buffer[3] == '\0') ||
                           (perm_buffer[3] == ' ') ||
                           (perm_buffer[3] == ',') ||
                           (perm_buffer[3] == '\t')))
                      {
                         free(perm_buffer);
                         break;
                      }
                      else if (posi(perm_buffer, EDIT_HC_PERM) == NULL)
                           {
                              (void)fprintf(stderr, "%s (%s %d)\n",
                                           PERMISSION_DENIED_STR,
                                           __FILE__, __LINE__);
                              exit(INCORRECT);
                           }
                      free(perm_buffer);
                      break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
                      break;

      default       : (void)fprintf(stderr,
                                    "Impossible!! Remove the programmer!\n");
                      exit(INCORRECT);
   }

   get_user(user, fake_user, user_offset);

   /* Check that no one else is using this dialog. */
   if ((p_user = lock_proc(EDIT_HC_LOCK_ID, NO)) != NULL)
   {
      (void)fprintf(stderr,
                    "Only one user may use this dialog. Currently %s is using it.\n",
                    p_user);
      exit(INCORRECT);
   }

   /*
    * Attach to the FSA and get the number of host
    * and the fsa_id of the FSA.
    */
   if ((ret = fsa_attach(EDIT_HC)) != SUCCESS)
   {
      if (ret == INCORRECT_VERSION)
      {
         (void)fprintf(stderr, "ERROR   : This program is not able to attach to the FSA due to incorrect version. (%s %d)\n",
                       __FILE__, __LINE__);
      }
      else
      {
         if (ret < 0)
         {
            (void)fprintf(stderr, "ERROR   : Failed to attach to FSA. (%s %d)\n",
                          __FILE__, __LINE__);
         }
         else
         {
            (void)fprintf(stderr, "ERROR   : Failed to attach to FSA : %s (%s %d)\n",
                          strerror(ret), __FILE__, __LINE__);
         }
      }
      exit(INCORRECT);
   }

   if (selected_host[0] != '\0')
   {
      int i;

      for (i = 0; i < no_of_hosts; i++)
      {
         if (CHECK_STRCMP(fsa[i].host_alias, selected_host) == 0)
         {
            seleted_host_no = i;
            break;
         }
      }
   }

   /* Get display pointer. */
   if ((display = XOpenDisplay(NULL)) == NULL)
   {
      (void)fprintf(stderr, "ERROR   : Could not open Display : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (attach_afd_status(NULL, WAIT_AFD_STATUS_ATTACH) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to attach to AFD status area. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Since we might create a new FSA via change_alias_order(), we need
    * the correct umask in place.
    */
   umask(0);

   return;
}


/*---------------------------------- usage() ----------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "Usage: %s [options]\n", progname);
   (void)fprintf(stderr, "              --version\n");
   (void)fprintf(stderr, "              -w <working directory>\n");
   (void)fprintf(stderr, "              -f <font name>\n");
   (void)fprintf(stderr, "              -h <host alias>\n");
   (void)fprintf(stderr, "              --version\n");
   return;
}


/*++++++++++++++++++++++++ create_option_menu_pt() +++++++++++++++++++++++*/
static void
create_option_menu_pt(Widget parent, XmFontList fontlist)
{
   XT_PTR_TYPE i;
   char        button_name[MAX_INT_LENGTH];
   Widget      pane_w;
   Arg         args[MAXARGS];
   Cardinal    argcount;


   /* Create a pulldown pane and attach it to the option menu. */
   pane_w = XmCreatePulldownMenu(parent, "pane", NULL, 0);

   argcount = 0;
   XtSetArg(args[argcount], XmNsubMenuId,        pane_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNtopPosition,      1);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNbottomPosition,   60);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       pt.label_w);
   argcount++;
   pt.option_menu_w = XmCreateOptionMenu(parent, "parallel_transfer",
                                         args, argcount);
   XtManageChild(pt.option_menu_w);

   /* Add all possible buttons. */
   for (i = 1; i <= MAX_NO_PARALLEL_JOBS; i++)
   {
#if SIZEOF_LONG == 4
      (void)sprintf(button_name, "%d", i);
#else
      (void)sprintf(button_name, "%ld", i);
#endif
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList, fontlist);
      argcount++;
      pt.value[i - 1] = i;
      pt.button_w[i - 1] = XtCreateManagedWidget(button_name,
                                             xmPushButtonWidgetClass,
                                             pane_w, args, argcount);

      /* Add callback handler. */
      XtAddCallback(pt.button_w[i - 1], XmNactivateCallback, pt_option_changed,
                    (XtPointer)i);
   }

   return;
}


/*++++++++++++++++++++++++ create_option_menu_tb() +++++++++++++++++++++++*/
static void
create_option_menu_tb(Widget parent, XmFontList fontlist)
{
   XT_PTR_TYPE i;
   char        *blocksize_name[MAX_TB_BUTTONS] =
               {
                  "256 B",
                  "512 B",
                  "1 KiB",
                  "2 KiB",
                  "4 KiB",
                  "8 KiB",
                  "16 KiB",
                  "32 KiB",
                  "64 KiB",
                  "128 KiB",
                  "256 KiB",
                  "512 KiB",
                  "1 MiB",
                  "2 MiB",
                  "4 MiB",
                  "8 MiB"
               };
   Widget      pane_w;
   Arg         args[MAXARGS];
   Cardinal    argcount;

   /* Create a pulldown pane and attach it to the option menu. */
   pane_w = XmCreatePulldownMenu(parent, "pane", NULL, 0);

   argcount = 0;
   XtSetArg(args[argcount], XmNsubMenuId,        pane_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNtopPosition,      1);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNbottomPosition,   60);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       tb.label_w);
   argcount++;
   tb.option_menu_w = XmCreateOptionMenu(parent, "transfer_blocksize",
                                         args, argcount);
   XtManageChild(tb.option_menu_w);

   /* Add all possible buttons. */
   for (i = 0; i < MAX_TB_BUTTONS; i++)
   {
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList, fontlist);
      argcount++;
      tb.button_w[i] = XtCreateManagedWidget(blocksize_name[i],
                                           xmPushButtonWidgetClass,
                                           pane_w, args, argcount);

      /* Add callback handler. */
      XtAddCallback(tb.button_w[i], XmNactivateCallback, tb_option_changed,
                    (XtPointer)i);
   }

   tb.value[0] = 256; tb.value[1] = 512; tb.value[2] = 1024;
   tb.value[3] = 2048; tb.value[4] = 4096; tb.value[5] = 8192;
   tb.value[6] = 16384; tb.value[7] = 32768; tb.value[8] = 65536;
   tb.value[9] = 131072; tb.value[10] = 262144; tb.value[11] = 524288;
   tb.value[12] = 1048576; tb.value[13] = 2097152; tb.value[14] = 4194304;
   tb.value[15] = 8388608;

   return;
}


/*+++++++++++++++++++++++ create_option_menu_fso() +++++++++++++++++++++++*/
static void
create_option_menu_fso(Widget parent, XmFontList fontlist)
{
   XT_PTR_TYPE i;
   char        button_name[MAX_INT_LENGTH];
   Widget      pane_w;
   Arg         args[MAXARGS];
   Cardinal    argcount;

   /* Create a pulldown pane and attach it to the option menu. */
   pane_w = XmCreatePulldownMenu(parent, "pane", NULL, 0);

   argcount = 0;
   XtSetArg(args[argcount], XmNsubMenuId, pane_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNtopPosition,      1);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_POSITION);
   argcount++;
   XtSetArg(args[argcount], XmNbottomPosition,   60);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       fso.label_w);
   argcount++;
   fso.option_menu_w = XmCreateOptionMenu(parent, "file_size_offset",
                                          args, argcount);
   XtManageChild(fso.option_menu_w);

   /* Add all possible buttons. */
   argcount = 0;
   XtSetArg(args[argcount], XmNfontList, fontlist);
   argcount++;
   fso.value[0] = -1;
   fso.button_w[0] = XtCreateManagedWidget("None",
                                           xmPushButtonWidgetClass,
                                           pane_w, args, argcount);
   XtAddCallback(fso.button_w[0], XmNactivateCallback, fso_option_changed,
                 (XtPointer)0);
   fso.value[1] = AUTO_SIZE_DETECT;
   fso.button_w[1] = XtCreateManagedWidget("Auto",
                                           xmPushButtonWidgetClass,
                                           pane_w, args, argcount);
   XtAddCallback(fso.button_w[1], XmNactivateCallback, fso_option_changed,
                 (XtPointer)1);
   for (i = 2; i < MAX_FSO_BUTTONS; i++)
   {
#if SIZEOF_LONG == 4
      (void)sprintf(button_name, "%d", i);
#else
      (void)sprintf(button_name, "%ld", i);
#endif
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList, fontlist);
      argcount++;
      fso.value[i] = i;
      fso.button_w[i] = XtCreateManagedWidget(button_name,
                                              xmPushButtonWidgetClass,
                                              pane_w, args, argcount);

      /* Add callback handler. */
      XtAddCallback(fso.button_w[i], XmNactivateCallback, fso_option_changed,
                    (XtPointer)i);
   }

   return;
}


/*+++++++++++++++++++++++++++ init_widget_data() ++++++++++++++++++++++++*/
static void
init_widget_data(void)
{
   Arg      args[MAXARGS];
   Cardinal argcount;
   Pixmap   icon,
            iconmask;
   Display  *sdisplay = XtDisplay(host_list_w);
   Window   win = XtWindow(host_list_w);

   /*
    * Create source cursor for drag & drop.
    */
   icon     = XCreateBitmapFromData(sdisplay, win,
                                    (char *)source_bits,
                                    source_width,
                                    source_height);
   iconmask = XCreateBitmapFromData(sdisplay, win,
                                    (char *)source_mask_bits,
                                    source_mask_width,
                                    source_mask_height);
   argcount = 0;
   XtSetArg(args[argcount], XmNwidth,  source_width);
   argcount++;
   XtSetArg(args[argcount], XmNheight, source_height);
   argcount++;
   XtSetArg(args[argcount], XmNpixmap, icon);
   argcount++;
   XtSetArg(args[argcount], XmNmask,   iconmask);
   argcount++;
   source_icon_w = XmCreateDragIcon(host_list_w, "source_icon",
                                    args, argcount);

   /*
    * Create invalid source cursor for drag & drop.
    */
   icon     = XCreateBitmapFromData(sdisplay, win,
                                    (char *)no_source_bits,
                                    no_source_width,
                                    no_source_height);
   iconmask = XCreateBitmapFromData(sdisplay, win,
                                    (char *)no_source_mask_bits,
                                    no_source_mask_width,
                                    no_source_mask_height);
   argcount = 0;
   XtSetArg(args[argcount], XmNwidth,  no_source_width);
   argcount++;
   XtSetArg(args[argcount], XmNheight, no_source_height);
   argcount++;
   XtSetArg(args[argcount], XmNpixmap, icon);
   argcount++;
   XtSetArg(args[argcount], XmNmask,   iconmask);
   argcount++;
   no_source_icon_w = XmCreateDragIcon(host_list_w, "no_source_icon",
                                       args, argcount);

   init_host_list(seleted_host_no);

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   (void)fprintf(stderr, "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
                 __FILE__, __LINE__);
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   (void)fprintf(stderr, "Uuurrrggh! Received SIGBUS. (%s %d)\n",
                 __FILE__, __LINE__);
   abort();
}
