/*
 *  xsend_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   xsend_file - sends a given list of files to a given destination
 **
 ** SYNOPSIS
 **   xsend_file [--version]
 **                OR
 **   xsend_file [-f <font name>] <file name file>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.01.2005 H.Kiehl Created
 **   12.08.2006 H.Kiehl Added extended active/passive mode option.
 **
 */
DESCR__E_M1

#include <stdio.h>               /* fopen(), NULL                        */
#include <string.h>              /* strcpy(), strcat(), strcmp()         */
#include <signal.h>              /* signal(), kill()                     */
#include <sys/types.h>
#include <sys/wait.h>            /* wait()                               */
#include <unistd.h>              /* gethostname(), STDERR_FILENO         */
#include <stdlib.h>              /* getenv()                             */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/LabelP.h>
#include <Xm/Frame.h>
#include <Xm/Text.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <Xm/ToggleBG.h>
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif
#include <errno.h>
#include "mafd_ctrl.h"
#include "xsend_file.h"
#include "ftpdefs.h"
#include "smtpdefs.h"
#include "ssh_commondefs.h"
#ifdef _WITH_WMO_SUPPORT
# include "wmodefs.h"
#endif
#include "version.h"

/* Global variables. */
Display          *display;
XmTextPosition   wpr_position;
XtInputId        cmd_input_id;
XtAppContext     app;
Widget           active_passive_w,
                 address_w = NULL,
                 address_label_w = NULL,
                 ap_radio_box_w,
                 appshell,
                 ca_button_w,
                 cmd_output,
                 create_attach_w,
                 dir_subject_label_w,
                 dir_subject_w,
                 hs_label_w = NULL,
                 hs_w = NULL,
                 lock_box_w,
                 mode_box_w,
                 option_menu_w,
                 password_label_w = NULL,
                 password_w = NULL,
                 port_label_w,
                 port_w,
                 prefix_w,
                 proxy_label_w,
                 proxy_w,
                 recipientbox_w,
                 special_button_w,
                 statusbox_w,
                 timeout_label_w,
                 timeout_w,
                 user_name_label_w = NULL,
                 user_name_w = NULL;
XmFontList       fontlist;
int              button_flag,
                 cmd_fd,
                 sys_log_fd = STDERR_FILENO;
pid_t            cmd_pid;
char             file_name_file[MAX_PATH_LENGTH],
                 url_file_name[MAX_PATH_LENGTH],
                 work_dir[MAX_PATH_LENGTH],
                 font_name[20],
                 *p_work_dir;
struct send_data *db;
const char       *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void      init_xsend_file(int *, char **, char *, char *),
                 sig_bus(int),
                 sig_exit(int),
                 sig_segv(int),
                 usage(char *),
                 xsend_file_exit(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char            window_title[11 + MAX_AFD_NAME_LENGTH + 1];
   static String   fallback_res[] =
                   {
                      ".xsend_file*mwmDecorations : 110",
                      ".xsend_file*mwmFunctions : 30",
                      ".xsend_file*background : NavajoWhite2",
                      ".xsend_file*XmText.background : NavajoWhite1",
                      ".xsend_file.main_form_w.buttonbox*background : PaleVioletRed2",
                      ".xsend_file.main_form_w.buttonbox*foreground : Black",
                      ".xsend_file.main_form_w.buttonbox*highlightColor : Black",
                      NULL
                   };
   Widget          button_w,
                   buttonbox_w,
                   criteriabox_w,
                   main_form_w,
                   optionbox_w,
                   pane_w,
                   radio_w,
                   separator_w,
                   separator1_w;
   XmString        label;
   Boolean         set_button;
   Arg             args[MAXARGS];
   Cardinal        argcount;
   XmFontListEntry entry;
   uid_t           euid, /* Effective user ID. */
                   ruid; /* Real user ID. */

   CHECK_FOR_VERSION(argc, argv);

   p_work_dir = work_dir;
   init_xsend_file(&argc, argv, window_title, file_name_file);

   /*
    * SSH wants to look at .Xauthority and with setuid flag
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
   main_form_w = XmCreateForm(appshell, "main_form_w", NULL, 0);

   /* Prepare font. */
   if ((entry = XmFontListEntryLoad(display, font_name,
                                    XmFONT_IS_FONT, "TAG1")) == NULL)
   {
      if ((entry = XmFontListEntryLoad(display, DEFAULT_FONT,
                                       XmFONT_IS_FONT, "TAG1")) == NULL)
      {
         (void)fprintf(stderr,
                       "Failed to load font with XmFontListEntryLoad() : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      else
      {
         (void)strcpy(font_name, DEFAULT_FONT);
      }
   }
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

   /*---------------------------------------------------------------*/
   /*                         Button Box                            */
   /*---------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,     21);
   argcount++;
   buttonbox_w = XmCreateForm(main_form_w, "buttonbox", args, argcount);

   /* Create Send Button. */
   special_button_w = XtVaCreateManagedWidget("Send",
                     xmPushButtonWidgetClass, buttonbox_w,
                     XmNfontList,             fontlist,
                     XmNtopAttachment,        XmATTACH_POSITION,
                     XmNtopPosition,          1,
                     XmNleftAttachment,       XmATTACH_POSITION,
                     XmNleftPosition,         1,
                     XmNrightAttachment,      XmATTACH_POSITION,
                     XmNrightPosition,        10,
                     XmNbottomAttachment,     XmATTACH_POSITION,
                     XmNbottomPosition,       20,
                     NULL);
   XtAddCallback(special_button_w, XmNactivateCallback,
                 (XtCallbackProc)send_button, 0);

   /* Create Cancel Button. */
   button_w = XtVaCreateManagedWidget("Close",
                     xmPushButtonWidgetClass, buttonbox_w,
                     XmNfontList,             fontlist,
                     XmNtopAttachment,        XmATTACH_POSITION,
                     XmNtopPosition,          1,
                     XmNleftAttachment,       XmATTACH_POSITION,
                     XmNleftPosition,         11,
                     XmNrightAttachment,      XmATTACH_POSITION,
                     XmNrightPosition,        20,
                     XmNbottomAttachment,     XmATTACH_POSITION,
                     XmNbottomPosition,       20,
                     NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)close_button, 0);
   XtManageChild(buttonbox_w);

   /*---------------------------------------------------------------*/
   /*                      Horizontal Separator                     */
   /*---------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     buttonbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(main_form_w, "separator", args, argcount);
   XtManageChild(separator_w);

   /*---------------------------------------------------------------*/
   /*                         Status Box                            */
   /*---------------------------------------------------------------*/
   statusbox_w = XtVaCreateManagedWidget(" ",
                     xmLabelWidgetClass,  main_form_w,
                     XmNfontList,         fontlist,
                     XmNleftAttachment,   XmATTACH_FORM,
                     XmNrightAttachment,  XmATTACH_FORM,
                     XmNbottomAttachment, XmATTACH_WIDGET,
                     XmNbottomWidget,     separator_w,
                     NULL);

   /*---------------------------------------------------------------*/
   /*                      Horizontal Separator                     */
   /*---------------------------------------------------------------*/
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
   separator1_w = XmCreateSeparator(main_form_w, "separator", args, argcount);
   XtManageChild(separator1_w);

   /*---------------------------------------------------------------*/
   /*                        Criteria Box                           */
   /*---------------------------------------------------------------*/
   criteriabox_w = XtVaCreateWidget("criteriabox",
                     xmFormWidgetClass,  main_form_w,
                     XmNtopAttachment,   XmATTACH_FORM,
                     XmNleftAttachment,  XmATTACH_FORM,
                     XmNrightAttachment, XmATTACH_FORM,
                     NULL);

   /*---------------------------------------------------------------*/
   /*                       Recipient Box                           */
   /*---------------------------------------------------------------*/
   recipientbox_w = XtVaCreateManagedWidget("recipientbox",
                     xmFormWidgetClass,  criteriabox_w,
                     XmNtopAttachment,   XmATTACH_FORM,
                     XmNleftAttachment,  XmATTACH_FORM,
                     XmNrightAttachment, XmATTACH_FORM,
                     NULL);

   /* Distribution type (FTP, SMTP, LOC, etc). */
   /* Create a pulldown pane and attach it to the option menu. */
   argcount = 0;
   XtSetArg(args[argcount], XmNfontList, fontlist);
   argcount++;
   pane_w = XmCreatePulldownMenu(recipientbox_w, "pane", args, argcount);

   label = XmStringCreateLocalized("Scheme :");
   argcount = 0;
   XtSetArg(args[argcount], XmNsubMenuId,      pane_w);
   argcount++;
   XtSetArg(args[argcount], XmNlabelString,    label);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,      -2);
   argcount++;
   option_menu_w = XmCreateOptionMenu(recipientbox_w, "proc_selection",
                                      args, argcount);
   XtManageChild(option_menu_w);
   XmStringFree(label);

   argcount = 0;
   XtSetArg(args[argcount], XmNfontList,       fontlist);
   argcount++;
   XtSetValues(XmOptionLabelGadget(option_menu_w), args, argcount);

   /* Add all possible buttons. */
   argcount = 0;
   XtSetArg(args[argcount], XmNfontList, fontlist);
   argcount++;
   button_w = XtCreateManagedWidget("FTP", xmPushButtonWidgetClass,
                                    pane_w, args, argcount);
   XtAddCallback(button_w, XmNactivateCallback, protocol_toggled, (XtPointer)FTP);
   button_w = XtCreateManagedWidget("SFTP", xmPushButtonWidgetClass,
                                    pane_w, args, argcount);
   XtAddCallback(button_w, XmNactivateCallback, protocol_toggled, (XtPointer)SFTP);
#ifdef _WHEN_DONE
   button_w = XtCreateManagedWidget("FILE", xmPushButtonWidgetClass,
                                    pane_w, args, argcount);
   XtAddCallback(button_w, XmNactivateCallback, protocol_toggled, (XtPointer)LOC);
   button_w = XtCreateManagedWidget("EXEC", xmPushButtonWidgetClass,
                                    pane_w, args, argcount);
   XtAddCallback(button_w, XmNactivateCallback, protocol_toggled, (XtPointer)EXEC);
#endif
   button_w = XtCreateManagedWidget("MAILTO", xmPushButtonWidgetClass,
                                    pane_w, args, argcount);
   XtAddCallback(button_w, XmNactivateCallback, protocol_toggled, (XtPointer)SMTP);
#ifdef _WHEN_DONE
# ifdef _WITH_SCP_SUPPORT
   button_w = XtCreateManagedWidget("SCP", xmPushButtonWidgetClass,
                                    pane_w, args, argcount);
   XtAddCallback(button_w, XmNactivateCallback, protocol_toggled, (XtPointer)SCP);
# endif
#endif /* _WHEN_DONE */
#ifdef _WITH_WMO_SUPPORT
   button_w = XtCreateManagedWidget("WMO", xmPushButtonWidgetClass,
                                    pane_w, args, argcount);
   XtAddCallback(button_w, XmNactivateCallback, protocol_toggled, (XtPointer)WMO);
#endif

   /* User. */
   CREATE_USER_FIELD();

   /* Password. */
   CREATE_PASSWORD_FIELD();

   /* Hostname. */
   hs_label_w = XtVaCreateManagedWidget("Hostname :",
                        xmLabelGadgetClass,  recipientbox_w,
                        XmNfontList,         fontlist,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       password_w,
                        XmNalignment,        XmALIGNMENT_END,
                        NULL);
   hs_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   recipientbox_w,
                        XmNfontList,         fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNrows,             1,
                        XmNcolumns,          12,
                        XmNmaxLength,        MAX_FILENAME_LENGTH - 1,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       hs_label_w,
                        NULL);
   XtAddCallback(hs_w, XmNlosingFocusCallback, send_save_input,
                 (XtPointer)HOSTNAME_NO_ENTER);
   XtAddCallback(hs_w, XmNactivateCallback, send_save_input,
                 (XtPointer)HOSTNAME_ENTER);

   /* Port. */
   port_label_w = XtVaCreateManagedWidget("Port :",
                        xmLabelGadgetClass,  recipientbox_w,
                        XmNfontList,         fontlist,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       hs_w,
                        XmNalignment,        XmALIGNMENT_END,
                        NULL);
   port_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   recipientbox_w,
                        XmNfontList,         fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNrows,             1,
                        XmNcolumns,          MAX_PORT_DIGITS,
                        XmNmaxLength,        MAX_PORT_DIGITS,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       port_label_w,
                        NULL);
   XtAddCallback(port_w, XmNlosingFocusCallback, send_save_input,
                 (XtPointer)PORT_NO_ENTER);
   XtAddCallback(port_w, XmNactivateCallback, send_save_input,
                 (XtPointer)PORT_ENTER);
   XtManageChild(recipientbox_w);

   /*---------------------------------------------------------------*/
   /*                      Horizontal Separator                     */
   /*---------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       recipientbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(criteriabox_w, "separator",
                                   args, argcount);
   XtManageChild(separator_w);

   /*---------------------------------------------------------------*/
   /*                        1st Option Box                         */
   /*---------------------------------------------------------------*/
   optionbox_w = XtVaCreateManagedWidget("optionbox1",
                        xmFormWidgetClass,  criteriabox_w,
                        XmNtopAttachment,   XmATTACH_WIDGET,
                        XmNtopWidget,       separator_w,
                        XmNleftAttachment,  XmATTACH_FORM,
                        XmNrightAttachment, XmATTACH_FORM,
                        NULL);

   /* Directory. */
   dir_subject_label_w = XtVaCreateManagedWidget("Directory :",
                        xmLabelGadgetClass,  optionbox_w,
                        XmNfontList,         fontlist,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNalignment,        XmALIGNMENT_END,
                        NULL);
   dir_subject_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   optionbox_w,
                        XmNfontList,         fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNrows,             1,
                        XmNcolumns,          50,
                        XmNmaxLength,        MAX_PATH_LENGTH - 1,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       dir_subject_label_w,
                        NULL);
   XtAddCallback(dir_subject_w, XmNlosingFocusCallback, send_save_input,
                 (XtPointer)TARGET_DIR_NO_ENTER);
   XtAddCallback(dir_subject_w, XmNactivateCallback, send_save_input,
                 (XtPointer)TARGET_DIR_ENTER);

   /* Toggle box for creating target directory. */
   create_attach_w = XtVaCreateWidget("create_togglebox",
                        xmRowColumnWidgetClass, optionbox_w,
                        XmNorientation,         XmHORIZONTAL,
                        XmNpacking,             XmPACK_TIGHT,
                        XmNnumColumns,          1,
                        XmNtopAttachment,       XmATTACH_FORM,
                        XmNbottomAttachment,    XmATTACH_FORM,
                        XmNleftAttachment,      XmATTACH_WIDGET,
                        XmNleftWidget,          dir_subject_w,
                        XmNresizable,           False,
                        NULL);
   ca_button_w = XtVaCreateManagedWidget("Create Dir  ",
                        xmToggleButtonGadgetClass, create_attach_w,
                        XmNfontList,               fontlist,
                        XmNset,                    False,
                        NULL);
   XtAddCallback(ca_button_w, XmNvalueChangedCallback,
                 (XtCallbackProc)create_attach_toggle,
                 (XtPointer)CREATE_DIR_TOGGLE);
   db->create_target_dir = NO;
   db->attach_file_flag = NO;
   XtManageChild(create_attach_w);

   /*---------------------------------------------------------------*/
   /*                      Vertical Separator                       */
   /*---------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       create_attach_w);
   argcount++;
   separator_w = XmCreateSeparator(optionbox_w, "separator", args, argcount);
   XtManageChild(separator_w);

   /* Transfer timeout. */
   timeout_label_w = XtVaCreateManagedWidget("Timeout :",
                        xmLabelGadgetClass,  optionbox_w,
                        XmNfontList,         fontlist,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       separator_w,
                        XmNleftOffset,       5,
                        XmNalignment,        XmALIGNMENT_END,
                        NULL);
   timeout_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   optionbox_w,
                        XmNfontList,         fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       timeout_label_w,
                        XmNcolumns,          MAX_TIMEOUT_DIGITS,
                        XmNmaxLength,        MAX_TIMEOUT_DIGITS,
                        NULL);
   XtAddCallback(timeout_w, XmNlosingFocusCallback, send_save_input,
                 (XtPointer)TIMEOUT_NO_ENTER);
   XtAddCallback(timeout_w, XmNactivateCallback, send_save_input,
                 (XtPointer)TIMEOUT_ENTER);

   XtManageChild(optionbox_w);

   /*---------------------------------------------------------------*/
   /*                      Horizontal Separator                     */
   /*---------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       optionbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(criteriabox_w, "separator", args, argcount);
   XtManageChild(separator_w);

   /*---------------------------------------------------------------*/
   /*                       2nd  Option Box                         */
   /*---------------------------------------------------------------*/
   optionbox_w = XtVaCreateManagedWidget("optionbox2",
                        xmFormWidgetClass,  criteriabox_w,
                        XmNtopAttachment,   XmATTACH_WIDGET,
                        XmNtopWidget,       separator_w,
                        XmNleftAttachment,  XmATTACH_FORM,
                        XmNrightAttachment, XmATTACH_FORM,
                        NULL);

   /* Transfer type (ASCII, BINARY, DOS, etc). */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNorientation,    XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,        XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNnumColumns,     1);
   argcount++;
   mode_box_w = XmCreateRadioBox(optionbox_w, "radiobox", args, argcount);
   if (db->transfer_mode == SET_ASCII)
   {
      set_button = True;
   }
   else
   {
      set_button = False;
   }
   radio_w = XtVaCreateManagedWidget("ASCII",
                        xmToggleButtonGadgetClass, mode_box_w,
                        XmNfontList,               fontlist,
                        XmNset,                    set_button,
                        NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)mode_radio, (XtPointer)SET_ASCII);
   if (db->transfer_mode == SET_BIN)
   {
      set_button = True;
   }
   else
   {
      set_button = False;
   }
   radio_w = XtVaCreateManagedWidget("BIN",
                        xmToggleButtonGadgetClass, mode_box_w,
                        XmNfontList,               fontlist,
                        XmNset,                    set_button,
                        NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)mode_radio, (XtPointer)SET_BIN);
   if (db->transfer_mode == SET_DOS)
   {
      set_button = True;
   }
   else
   {
      set_button = False;
   }
   radio_w = XtVaCreateManagedWidget("DOS",
                        xmToggleButtonGadgetClass, mode_box_w,
                        XmNfontList,               fontlist,
                        XmNset,                    set_button,
                        NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)mode_radio, (XtPointer)SET_DOS);
   XtManageChild(mode_box_w);
   if (db->protocol != FTP)
   {
      XtSetSensitive(mode_box_w, False);
   }

   /*---------------------------------------------------------------*/
   /*                      Vertical Separator                       */
   /*---------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       mode_box_w);
   argcount++;
   separator_w = XmCreateSeparator(optionbox_w, "separator", args, argcount);
   XtManageChild(separator_w);

   /* Lock type (DOT, OFF, DOT_VMS and prefix. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,     separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNorientation,    XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,        XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNnumColumns,     1);
   argcount++;
   lock_box_w = XmCreateRadioBox(optionbox_w, "radiobox", args, argcount);
   if (db->lock == SET_LOCK_DOT)
   {
      set_button = True;
   }
   else
   {
      set_button = False;
   }
   radio_w = XtVaCreateManagedWidget("DOT",
                        xmToggleButtonGadgetClass, lock_box_w,
                        XmNfontList,               fontlist,
                        XmNset,                    set_button,
                        NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)lock_radio, (XtPointer)SET_LOCK_DOT);
   if (db->lock == SET_LOCK_OFF)
   {
      set_button = True;
   }
   else
   {
      set_button = False;
   }
   radio_w = XtVaCreateManagedWidget("OFF",
                        xmToggleButtonGadgetClass, lock_box_w,
                        XmNfontList,               fontlist,
                        XmNset,                    set_button,
                        NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)lock_radio, (XtPointer)SET_LOCK_OFF);
   if (db->lock == SET_LOCK_DOT_VMS)
   {
      set_button = True;
   }
   else
   {
      set_button = False;
   }
   radio_w = XtVaCreateManagedWidget("DOT_VMS",
                        xmToggleButtonGadgetClass, lock_box_w,
                        XmNfontList,               fontlist,
                        XmNset,                    set_button,
                        NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)lock_radio, (XtPointer)SET_LOCK_DOT_VMS);
   if (db->lock == SET_LOCK_PREFIX)
   {
      set_button = True;
   }
   else
   {
      set_button = False;
   }
   radio_w = XtVaCreateManagedWidget("Prefix",
                        xmToggleButtonGadgetClass, lock_box_w,
                        XmNfontList,               fontlist,
                        XmNset,                    set_button,
                        NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)lock_radio, (XtPointer)SET_LOCK_PREFIX);
   XtManageChild(lock_box_w);
   if ((db->protocol != FTP) && (db->protocol != LOC) && (db->protocol != EXEC))
   {
      XtSetSensitive(lock_box_w, False);
   }

   /* Text box to enter the prefix. */
   prefix_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   optionbox_w,
                        XmNfontList,         fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNrows,             1,
                        XmNcolumns,          8,
                        XmNmaxLength,        MAX_FILENAME_LENGTH - 1,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       lock_box_w,
                        NULL);
   XtAddCallback(prefix_w, XmNlosingFocusCallback, send_save_input,
                 (XtPointer)PREFIX_NO_ENTER);
   XtAddCallback(prefix_w, XmNactivateCallback, send_save_input,
                 (XtPointer)PREFIX_ENTER);
   XtSetSensitive(prefix_w, set_button);

   XtManageChild(optionbox_w);
   XtManageChild(criteriabox_w);

   /*---------------------------------------------------------------*/
   /*                      Horizontal Separator                     */
   /*---------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       optionbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(criteriabox_w, "separator", args, argcount);
   XtManageChild(separator_w);

   /*---------------------------------------------------------------*/
   /*                       3rd  Option Box                         */
   /*---------------------------------------------------------------*/
   optionbox_w = XtVaCreateManagedWidget("optionbox3",
                        xmFormWidgetClass,  criteriabox_w,
                        XmNtopAttachment,   XmATTACH_WIDGET,
                        XmNtopWidget,       separator_w,
                        XmNleftAttachment,  XmATTACH_FORM,
                        XmNrightAttachment, XmATTACH_FORM,
                        NULL);

   buttonbox_w = XtVaCreateWidget("debug_togglebox",
                        xmRowColumnWidgetClass, optionbox_w,
                        XmNorientation,         XmHORIZONTAL,
                        XmNpacking,             XmPACK_TIGHT,
                        XmNnumColumns,          1,
                        XmNtopAttachment,       XmATTACH_FORM,
                        XmNleftAttachment,      XmATTACH_FORM,
                        XmNbottomAttachment,    XmATTACH_FORM,
                        XmNresizable,           False,
                        NULL);
   button_w = XtVaCreateManagedWidget("Debug",
                        xmToggleButtonGadgetClass, buttonbox_w,
                        XmNfontList,               fontlist,
                        XmNset,                    False,
                        NULL);
   XtAddCallback(button_w, XmNvalueChangedCallback,
                 (XtCallbackProc)debug_toggle, NULL);
   db->debug = NO;
   XtManageChild(buttonbox_w);

   /*---------------------------------------------------------------*/
   /*                      Vertical Separator                       */
   /*---------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       buttonbox_w);
   argcount++;
   separator_w = XmCreateSeparator(optionbox_w, "separator", args, argcount);
   XtManageChild(separator_w);

   active_passive_w = XtVaCreateWidget("eap_togglebox",
                        xmRowColumnWidgetClass, optionbox_w,
                        XmNorientation,         XmHORIZONTAL,
                        XmNpacking,             XmPACK_TIGHT,
                        XmNnumColumns,          1,
                        XmNtopAttachment,       XmATTACH_FORM,
                        XmNbottomAttachment,    XmATTACH_FORM,
                        XmNleftAttachment,      XmATTACH_WIDGET,
                        XmNleftWidget,          separator_w,
                        XmNresizable,           False,
                        NULL);
   button_w = XtVaCreateManagedWidget("Extended",
                        xmToggleButtonGadgetClass, active_passive_w,
                        XmNfontList,               fontlist,
                        XmNset,                    False,
                        NULL);
   XtAddCallback(button_w, XmNvalueChangedCallback,
                 (XtCallbackProc)extended_toggle, NULL);

   /* Active or passive mode. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,     button_w);
   argcount++;
   XtSetArg(args[argcount], XmNorientation,    XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,        XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNnumColumns,     1);
   argcount++;
   ap_radio_box_w = XmCreateRadioBox(optionbox_w, "radiobox", args, argcount);
   radio_w = XtVaCreateManagedWidget("Active",
                        xmToggleButtonGadgetClass, ap_radio_box_w,
                        XmNfontList,               fontlist,
                        XmNset,                    True,
                        NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)active_passive_radio, (XtPointer)SET_ACTIVE);
   radio_w = XtVaCreateManagedWidget("Passive",
                        xmToggleButtonGadgetClass, ap_radio_box_w,
                        XmNfontList,               fontlist,
                        XmNset,                    False,
                        NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)active_passive_radio, (XtPointer)SET_PASSIVE);
   XtManageChild(ap_radio_box_w);
   XtManageChild(active_passive_w);
   db->mode_flag = ACTIVE_MODE;

   /*---------------------------------------------------------------*/
   /*                      Vertical Separator                       */
   /*---------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       ap_radio_box_w);
   argcount++;
   separator_w = XmCreateSeparator(optionbox_w, "separator", args, argcount);
   XtManageChild(separator_w);

   /* Proxy. */
   proxy_label_w = XtVaCreateManagedWidget("Proxy:",
                        xmLabelGadgetClass,  optionbox_w,
                        XmNfontList,         fontlist,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       separator_w,
                        XmNalignment,        XmALIGNMENT_END,
                        NULL);
   proxy_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   optionbox_w,
                        XmNfontList,         fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNrows,             1,
                        XmNcolumns,          20,
                        XmNmaxLength,        MAX_FILENAME_LENGTH - 1,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        6,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       proxy_label_w,
                        NULL);
   XtAddCallback(proxy_w, XmNlosingFocusCallback, send_save_input,
                 (XtPointer)PROXY_NO_ENTER);
   XtAddCallback(proxy_w, XmNactivateCallback, send_save_input,
                 (XtPointer)PROXY_ENTER);

   XtManageChild(optionbox_w);

   /*---------------------------------------------------------------*/
   /*                      Horizontal Separator                     */
   /*---------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       optionbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(criteriabox_w, "separator", args, argcount);
   XtManageChild(separator_w);

   /*---------------------------------------------------------------*/
   /*                         Output Box                            */
   /*---------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNrows,                   20);
   argcount++;
   XtSetArg(args[argcount], XmNcolumns,                80);
   argcount++;
   XtSetArg(args[argcount], XmNeditable,               False);
   argcount++;
   XtSetArg(args[argcount], XmNeditMode,               XmMULTI_LINE_EDIT);
   argcount++;
   XtSetArg(args[argcount], XmNwordWrap,               False);
   argcount++;
   XtSetArg(args[argcount], XmNscrollHorizontal,       True);
   argcount++;
   XtSetArg(args[argcount], XmNcursorPositionVisible,  False);
   argcount++;
   XtSetArg(args[argcount], XmNautoShowCursorPosition, False);
   argcount++;
   XtSetArg(args[argcount], XmNfontList,               fontlist);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,              separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,           separator1_w);
   argcount++;
   cmd_output = XmCreateScrolledText(main_form_w, "cmd_output", args, argcount);
   XtManageChild(cmd_output);
   XtManageChild(main_form_w);

#ifdef WITH_EDITRES
   XtAddEventHandler(appshell, (EventMask)0, True,
                     _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets. */
   XtRealizeWidget(appshell);

   if (db->port > 0)
   {
      char str_line[MAX_PORT_DIGITS + 2];

      (void)snprintf(str_line, MAX_PORT_DIGITS + 1, "%d",
                     (unsigned short)db->port);
      XmTextSetString(port_w, str_line);
   }
   if (db->timeout > 0)
   {
      char str_line[MAX_TIMEOUT_DIGITS + 4];

      (void)snprintf(str_line, MAX_TIMEOUT_DIGITS + 1, "%d",
                     (unsigned short)db->timeout % 10000);
      XmTextSetString(timeout_w, str_line);
   }
   wpr_position = 0;
   XmTextSetInsertionPosition(cmd_output, 0);

   /* Set some signal handlers. */
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(WARN_DIALOG, "Failed to set signal handler's for %s : %s",
                 XSEND_FILE, strerror(errno));
   }

   if (atexit(xsend_file_exit) != 0)
   {
      (void)xrec(WARN_DIALOG, "Failed to set exit handler for %s : %s\n",
                 XSEND_FILE, strerror(errno));
   }

   /* We want the keyboard focus on the cmd output. */
   XmProcessTraversal(cmd_output, XmTRAVERSE_CURRENT);

   /* Start the main event-handling loop. */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++ init_xsend_file() +++++++++++++++++++++++++++*/
static void
init_xsend_file(int  *argc,
                char *argv[],
                char *title_name,
                char *file_name_file)
{
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
   if (get_arg(argc, argv, "-f", font_name, 20) == INCORRECT)
   {
      (void)strcpy(font_name, DEFAULT_FONT);
   }
   if (*argc < 2)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   (void)my_strncpy(file_name_file, argv[1], MAX_PATH_LENGTH);
   url_file_name[0] = '\0';

   /* Prepare title for window. */
   (void)strcpy(title_name, "xsend_file ");
   if (get_afd_name(&title_name[11]) == INCORRECT)
   {
      (void)gethostname(&title_name[11], MAX_AFD_NAME_LENGTH - 11);
   }

   if ((db = malloc(sizeof(struct send_data))) == NULL)
   {
      (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   memset(db, 0, sizeof(struct send_data));

   /* Now set some default values. */
   button_flag = SEND_BUTTON;
   db->protocol       = FTP;
   db->lock           = SET_LOCK_DOT;
   db->transfer_mode  = SET_BIN;
   db->timeout        = DEFAULT_TRANSFER_TIMEOUT;
   db->hostname[0]    = '\0';
   db->user[0]        = '\0';
   db->proxy_name[0]  = '\0';
   db->target_dir[0]  = '\0';
   db->prefix[0]      = '\0';
   db->subject[0]     = '\0';
   db->smtp_server[0] = '\0';
   if (db->protocol == FTP)
   {
      db->port = DEFAULT_FTP_PORT;
   }
   else if (db->protocol == SMTP)
        {
           db->port = DEFAULT_SMTP_PORT;
        }
#ifdef _WITH_SCP_SUPPORT
   else if (db->protocol == SCP)
        {
           db->port = DEFAULT_SSH_PORT;
        }
#endif
#ifdef _WITH_WMO_SUPPORT
   else if (db->protocol == WMO)
        {
           db->port = -1;
        }
#endif

   return;
}


/*-------------------------------- usage() ------------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 "Usage: %s [options] <file name file>\n", progname);
   (void)fprintf(stderr, "              --version\n");
   (void)fprintf(stderr, "              -f <font name>\n");
   return;
}


/*++++++++++++++++++++++++++ xsend_file_exit() ++++++++++++++++++++++++++*/
static void
xsend_file_exit(void)
{
   if (cmd_pid > 0)
   {
      if (kill(cmd_pid, SIGINT) == -1)
      {
#if SIZEOF_PID_T == 4
         (void)fprintf(stderr, "Failed to kill() process %d : %s (%s %d)\n",
#else
         (void)fprintf(stderr, "Failed to kill() process %lld : %s (%s %d)\n",
#endif
                       (pri_pid_t)cmd_pid, strerror(errno), __FILE__, __LINE__);
      }
   }
   (void)unlink(file_name_file);
   if (url_file_name[0] != '\0')
   {
      (void)unlink(url_file_name);
   }
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


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
