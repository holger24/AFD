/*
 *  handle_event.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_event - handles event for the given dir or host alias
 **
 ** SYNOPSIS
 **   handle_event [options]
 **              --version
 **              -d <dir alias> [... <dir alias n>]
 **              -f <font name>
 **              -h <host alias> [... <host alias n>]
 **              -p <user profile>
 **              -u[ <fake user>]
 **              -w <working directory>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.06.2007 H.Kiehl Created
 **   10.05.2014 H.Kiehl Make offline radio button as default selection.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>              /* strcpy(), strcat(), strcmp()         */
#include <ctype.h>               /* toupper()                            */
#include <sys/types.h>
#include <stdlib.h>              /* getenv(), atexit()                   */
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/ToggleBG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/Frame.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/ScrollBar.h>
#include <Xm/Form.h>
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif
#include <errno.h>
#include "handle_event.h"
#include "version.h"
#include "permission.h"

/* Global variables. */
Display                    *display;
XtAppContext               app;
Widget                     appshell,
                           end_time_w,
                           entertime_w,
                           start_time_w,
                           statusbox_w,
                           text_w;
int                        acknowledge_type,
                           event_log_fd = STDERR_FILENO,
                           fra_id,         /* ID of FRA.                 */
                           fra_fd = -1,    /* Needed by fra_attach().    */
                           fsa_id,         /* ID of FSA.                 */
                           fsa_fd = -1,    /* Needed by fsa_attach().    */
                           no_of_alias,
                           no_of_dirs,
                           no_of_hosts,
                           sys_log_fd = STDERR_FILENO;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
time_t                     end_time_val,
                           start_time_val;
char                       **dir_alias = NULL,
                           **host_alias = NULL,
                           font_name[40],
                           *p_work_dir,
                           user[MAX_FULL_USER_ID_LENGTH + 1];
const char                 *sys_log_name = SYSTEM_LOG_FIFO;
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;

/* Local function prototypes. */
static void                init_handle_event(int *, char **, char *),
                           usage(char *),
                           handle_event_exit(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char            window_title[MAX_WNINDOW_TITLE_LENGTH],
                   work_dir[MAX_PATH_LENGTH];
   static String   fallback_res[] =
                   {
                      "*mwmDecorations : 42",
                      "*mwmFunctions : 12",
                      ".handle_event*background : NavajoWhite2",
                      ".handle_event.form*XmText.background : NavajoWhite1",
                      ".handle_event.form.he_textSW.he_text.background : NavajoWhite1",
                      ".handle_event.form.buttonbox*background : PaleVioletRed2",
                      ".handle_event.form.buttonbox*foreground : Black",
                      ".handle_event.form.buttonbox*highlightColor : Black",
                      NULL
                   };
   Widget          ack_box_w,
                   button_w,
                   buttonbox_w,
                   form_w,
                   frame_w,
                   h_separator_w,
                   block_w,
                   enable_time_w,
                   label_w,
                   rowcol_w,
                   radio_w,
                   radiobox_w,
                   time_box_w;
   XmFontListEntry entry;
   XmFontList      fontlist;
   XmFontType      dummy;
   Arg             args[21];
   Cardinal        argcount;
   uid_t           euid, /* Effective user ID. */
                   ruid; /* Real user ID. */

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values. */
   p_work_dir = work_dir;
   init_handle_event(&argc, argv, window_title);

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
   disable_drag_drop(appshell);

   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                       euid, strerror(errno), __FILE__, __LINE__);
      }
   }

   /* Get display pointer. */
   if ((display = XtDisplay(appshell)) == NULL)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open Display : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

#ifdef HAVE_XPM
   /* Setup AFD logo as icon. */
   setup_icon(display, appshell);
#endif

   /* Create managing widget. */
   form_w = XmCreateForm(appshell, "form", NULL, 0);

   entry = XmFontListEntryLoad(XtDisplay(form_w), font_name,
                               XmFONT_IS_FONT, "TAG1");
   (void)XmFontListEntryGetFont(entry, &dummy);
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,     21);
   argcount++;
   buttonbox_w = XmCreateForm(form_w, "buttonbox", args, argcount);

   /* Create a horizontal separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,      XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,          buttonbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator_w = XmCreateSeparator(form_w, "h_separator", args, argcount);
   XtManageChild(h_separator_w);

   button_w = XtVaCreateManagedWidget("Set",
                                      xmPushButtonWidgetClass, buttonbox_w,
                                      XmNfontList,         fontlist,
                                      XmNtopAttachment,    XmATTACH_POSITION,
                                      XmNtopPosition,      2,
                                      XmNbottomAttachment, XmATTACH_POSITION,
                                      XmNbottomPosition,   19,
                                      XmNleftAttachment,   XmATTACH_POSITION,
                                      XmNleftPosition,     1,
                                      XmNrightAttachment,  XmATTACH_POSITION,
                                      XmNrightPosition,    10,
                                      NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)set_button, (XtPointer)0);
   button_w = XtVaCreateManagedWidget("Close",
                                      xmPushButtonWidgetClass, buttonbox_w,
                                      XmNfontList,         fontlist,
                                      XmNtopAttachment,    XmATTACH_POSITION,
                                      XmNtopPosition,      2,
                                      XmNbottomAttachment, XmATTACH_POSITION,
                                      XmNbottomPosition,   19,
                                      XmNleftAttachment,   XmATTACH_POSITION,
                                      XmNleftPosition,     11,
                                      XmNrightAttachment,  XmATTACH_POSITION,
                                      XmNrightPosition,    20,
                                      NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)close_button, (XtPointer)0);
   XtManageChild(buttonbox_w);

   /*-------------------------------------------------------------------*/
   /*                            Status Box                             */
   /*                            ----------                             */
   /* The status of the handle event window is shown here. If operation */
   /* was succesful or not.                                             */
   /*-------------------------------------------------------------------*/
   statusbox_w = XtVaCreateManagedWidget(" ",
                        xmLabelWidgetClass,  form_w,
                        XmNfontList,         fontlist,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNrightAttachment,  XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_WIDGET,
                        XmNbottomWidget,     h_separator_w,
                        NULL);

   /* Create a horizontal separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,      XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,          statusbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator_w = XmCreateSeparator(form_w, "h_separator", args, argcount);
   XtManageChild(h_separator_w);

   /*--------------------------------------------------------------------*/
   /*                     Acknowledge type box                           */
   /*                     --------------------                           */
   /* Either acknowledge or offline is possible.                         */
   /*--------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   ack_box_w = XmCreateForm(form_w, "acknowledge_box", args, argcount);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,          XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNnumColumns,       1);
   argcount++;
   radiobox_w = XmCreateRadioBox(ack_box_w, "radiobox", args, argcount);
   radio_w = XtVaCreateManagedWidget("Acknowledge",
                            xmToggleButtonGadgetClass, radiobox_w,
                            XmNfontList,               fontlist,
                            XmNset,                    False,
                            NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)radio_button, (XtPointer)ACKNOWLEDGE_SELECT);
   radio_w = XtVaCreateManagedWidget("Offline",
                            xmToggleButtonGadgetClass, radiobox_w,
                            XmNfontList,               fontlist,
                            XmNset,                    True,
                            NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)radio_button, (XtPointer)OFFLINE_SELECT);
   acknowledge_type = OFFLINE_SELECT;
   radio_w = XtVaCreateManagedWidget("Unset",
                            xmToggleButtonGadgetClass, radiobox_w,
                            XmNfontList,               fontlist,
                            XmNset,                    False,
                            NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)radio_button, (XtPointer)UNSET_SELECT);
   XtManageChild(radiobox_w);
   XtManageChild(ack_box_w);

   /* Create a horizontal separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,      XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,          ack_box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator_w = XmCreateSeparator(form_w, "h_separator", args, argcount);
   XtManageChild(h_separator_w);

   /*--------------------------------------------------------------------*/
   /*                          Timeframe box                             */
   /*                          -------------                             */
   /* Optionally enter a time frame when this acknowledge/offline is     */
   /* valid.                                                             */
   /*--------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   time_box_w = XmCreateForm(form_w, "timeframe_box", args, argcount);

   enable_time_w = XtVaCreateManagedWidget("Time frame",
                            xmToggleButtonGadgetClass, time_box_w,
                            XmNfontList,         fontlist,
                            XmNset,              False,
                            XmNtopAttachment,    XmATTACH_FORM,
                            XmNleftAttachment,   XmATTACH_FORM,
                            XmNleftOffset,       1,
                            XmNbottomAttachment, XmATTACH_FORM,
                            NULL);
   XtAddCallback(enable_time_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggle_button, NULL);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;                                                  
   XtSetArg(args[argcount], XmNleftWidget,       enable_time_w);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   entertime_w = XmCreateForm(time_box_w, "entertime", args, argcount);
   rowcol_w = XtVaCreateWidget("rowcol", xmRowColumnWidgetClass, entertime_w,
                               XmNorientation, XmHORIZONTAL,
                               NULL);
   block_w = XmCreateForm(rowcol_w, "rowcol", NULL, 0);
   label_w = XtVaCreateManagedWidget(" Start time:",
                           xmLabelGadgetClass,  block_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNleftAttachment,   XmATTACH_FORM,
                           XmNalignment,        XmALIGNMENT_END,
                           NULL);
   start_time_w = XtVaCreateManagedWidget("starttime",
                           xmTextWidgetClass,   block_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNrightAttachment,  XmATTACH_FORM,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNcolumns,          8,
                           XmNmaxLength,        8,
                           NULL);                 
   XtAddCallback(start_time_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)START_TIME_NO_ENTER);                 
   XtAddCallback(start_time_w, XmNactivateCallback, save_input,
                 (XtPointer)START_TIME);
   XtManageChild(block_w);

   block_w = XmCreateForm(rowcol_w, "rowcol", NULL, 0);
   label_w = XtVaCreateManagedWidget("End time:",
                           xmLabelGadgetClass,  block_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNleftAttachment,   XmATTACH_FORM,
                           XmNalignment,        XmALIGNMENT_END,
                           NULL);                               
   end_time_w = XtVaCreateManagedWidget("endtime",
                           xmTextWidgetClass,   block_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,       
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNrightAttachment,  XmATTACH_FORM,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,        
                           XmNcolumns,          8,      
                           XmNmaxLength,        8,
                           NULL);
   XtAddCallback(end_time_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)END_TIME_NO_ENTER);
   XtAddCallback(end_time_w, XmNactivateCallback, save_input,
                 (XtPointer)END_TIME);
   XtManageChild(block_w);
   XtManageChild(rowcol_w);
   XtManageChild(entertime_w);
   XtManageChild(time_box_w);
   XtSetSensitive(entertime_w, False);

   /* Create a horizontal separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,      XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,          time_box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator_w = XmCreateSeparator(form_w, "h_separator", args, argcount);
   XtManageChild(h_separator_w);

   /*---------------------------------------------------------------*/
   /*                          Frame Box                            */
   /*---------------------------------------------------------------*/
   frame_w = XtVaCreateManagedWidget("reason_frame",
                            xmFrameWidgetClass,  form_w,
                            XmNshadowType,       XmSHADOW_ETCHED_IN,
                            XmNmarginHeight,     5,
                            XmNmarginWidth,      5,
                            XmNtopAttachment,    XmATTACH_FORM,
                            XmNtopOffset,        5,
                            XmNleftAttachment,   XmATTACH_FORM,
                            XmNleftOffset,       5,
                            XmNrightAttachment,  XmATTACH_FORM,
                            XmNrightOffset,      5,
                            XmNbottomAttachment, XmATTACH_WIDGET,
                            XmNbottomWidget,     h_separator_w,
                            XmNbottomOffset,     5,
                            NULL);
   XtVaCreateManagedWidget("Enter Reason :",
                            xmLabelGadgetClass,        frame_w,
                            XmNchildType,              XmFRAME_TITLE_CHILD,
                            XmNchildVerticalAlignment, XmALIGNMENT_CENTER,
                            NULL);

   /* Create event input field as a ScrolledText window. */
   argcount = 0;
   XtSetArg(args[argcount], XmNfontList,               fontlist);
   argcount++;
   XtSetArg(args[argcount], XmNeditable,               True);
   argcount++;
   XtSetArg(args[argcount], XmNeditMode,               XmMULTI_LINE_EDIT);
   argcount++;
   XtSetArg(args[argcount], XmNwordWrap,               False);
   argcount++;
   XtSetArg(args[argcount], XmNscrollHorizontal,       False);
   argcount++;
   XtSetArg(args[argcount], XmNcursorPositionVisible,  True);
   argcount++;
   XtSetArg(args[argcount], XmNautoShowCursorPosition, True);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrows,                   (MAX_EVENT_REASON_LENGTH / ADDITIONAL_INFO_LENGTH / 2));
   argcount++;
   XtSetArg(args[argcount], XmNmaxLength,              MAX_EVENT_REASON_LENGTH);
   argcount++;
   XtSetArg(args[argcount], XmNwordWrap,               True);
   argcount++;
   XtSetArg(args[argcount], XmNcolumns,                ADDITIONAL_INFO_LENGTH);
   argcount++;
   text_w = XmCreateScrolledText(frame_w, "he_text", args, argcount);
   XtManageChild(text_w);
   XtManageChild(form_w);

   /* Free font list. */
   XmFontListFree(fontlist);

#ifdef WITH_EDITRES
   XtAddEventHandler(appshell, (EventMask)0, True,
                     _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets. */
   XtRealizeWidget(appshell);

   /* We want the keyboard focus on the text field */
   /* where the user enters his reason.            */
   XmProcessTraversal(text_w, XmTRAVERSE_CURRENT);

   /* Write window ID, so afd_ctrl can set focus if it is called again. */
   write_window_id(XtWindow(appshell), getpid(), HANDLE_EVENT);

   /* Start the main event-handling loop. */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++ init_handle_event() +++++++++++++++++++++++++*/
static void
init_handle_event(int *argc, char *argv[], char *window_title)
{
   int  user_offset;
   char fake_user[MAX_FULL_USER_ID_LENGTH],
        *perm_buffer,
        profile[MAX_PROFILE_NAME_LENGTH + 1];

   if ((get_arg(argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
      exit(SUCCESS);
   }
   if (get_afd_path(argc, argv, p_work_dir) < 0)
   {
      (void)fprintf(stderr,
                    "Failed to get working directory of AFD. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Check if title is specified. */
   if (get_arg(argc, argv, "-t", font_name, 40) == INCORRECT)
   {
      (void)strcpy(window_title, "Handle Event");
   }
   else
   {
      (void)snprintf(window_title, MAX_WNINDOW_TITLE_LENGTH,
                     "Handle Event %s", font_name);
   }
   if (get_arg(argc, argv, "-f", font_name, 40) == INCORRECT)
   {
      (void)strcpy(font_name, DEFAULT_FONT);
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
   if (get_arg_array(argc, argv, "-d", &dir_alias, &no_of_alias) == INCORRECT)
   {
      if (get_arg_array(argc, argv, "-h", &host_alias,
                        &no_of_alias) == INCORRECT)
      {
         usage(argv[0]);
         exit(INCORRECT);
      }
      else
      {
         int ret;

         if ((ret = fsa_attach(HANDLE_EVENT)) != SUCCESS)
         {
            if (ret == INCORRECT_VERSION)
            {
               (void)fprintf(stderr, "This program is not able to attach to the FSA due to incorrect version.");
            }
            else
            {
               if (ret < 0)
               {
                  (void)fprintf(stderr, "Failed to attach to FSA.");
               }
               else
               {
                  (void)fprintf(stderr, "Failed to attach to FSA : %s",
                                strerror(ret));
               }
            }
            exit(INCORRECT);
         }
      }
   }
   else
   {
      int ret;

      if ((ret = fra_attach()) != SUCCESS)
      {
         if (ret == INCORRECT_VERSION)
         {
            (void)fprintf(stderr, "This program is not able to attach to the FRA due to incorrect version.");
         }
         else
         {
            if (ret < 0)
            {
               (void)fprintf(stderr, "Failed to attach to FRA.");
            }
            else
            {
               (void)fprintf(stderr, "Failed to attach to FRA : %s",
                             strerror(ret));
            }
         }
         exit(INCORRECT);
      }
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

      case NONE :
         (void)fprintf(stderr, "%s (%s %d)\n",
                       PERMISSION_DENIED_STR, __FILE__, __LINE__);
         exit(INCORRECT);

      case SUCCESS :
         /* Lets evaluate the permissions and see what */
         /* the user may do.                           */
         if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
             (perm_buffer[2] == 'l') &&
             ((perm_buffer[3] == '\0') || (perm_buffer[3] == ',') ||
              (perm_buffer[3] == ' ') || (perm_buffer[3] == '\t')))
         {
            free(perm_buffer);
            break;
         }
         else if (posi(perm_buffer, VIEW_DIR_CONFIG_PERM) == NULL)
              {
                 (void)fprintf(stderr, "%s (%s %d)\n",
                               PERMISSION_DENIED_STR, __FILE__, __LINE__);
                 exit(INCORRECT);
              }
         free(perm_buffer);
         break;

      case INCORRECT:
         /* Hmm. Something did go wrong. Since we want to */
         /* be able to disable permission checking let    */
         /* the user have all permissions.                */
         break;

      default :
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   get_user(user, fake_user, user_offset);
   start_time_val = -1;
   end_time_val = -1;

   if (atexit(handle_event_exit) != 0)
   {
      (void)xrec(WARN_DIALOG, "Failed to set exit handler for %s : %s",
                 HANDLE_EVENT, strerror(errno));
   }
   check_window_ids(HANDLE_EVENT);

   return;
}


/*------------------------------- usage() -------------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 "Usage : %s [options] -d <dir alias>[ ... <dir alias n>] | -h <host alias>[ ... <host alias n>]\n",
                 progname);
   (void)fprintf(stderr, "             --version\n");
   (void)fprintf(stderr, "             -d <dir alias>[ ... <dir alias>]\n");
   (void)fprintf(stderr, "             -f <font name>\n");
   (void)fprintf(stderr, "             -h <host alias>[ ... <host alias>]\n");
   (void)fprintf(stderr, "             -p <user profile>\n");
   (void)fprintf(stderr, "             -u[ <user>]\n");
   (void)fprintf(stderr, "             -w <working directory>\n");
   return;
}


/*------------------------ handle_event_exit() --------------------------*/
static void
handle_event_exit(void)
{
   remove_window_id(getpid(), HANDLE_EVENT);
   return;
}
