/*
 *  select_host_dialog.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   select_host_dialog - searches for a host in the afd_ctrl dialog
 **
 ** SYNOPSIS
 **   void select_host_dialog(Widget    w,
 **                           XtPointer client_data,
 **                           XtPointer call_data)
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
 **   31.03.2001 H.Kiehl Created
 **   17.07.2009 H.Kiehl Added choise for protocol.
 **   24.08.2009 H.Kiehl Added choise to search in host information.
 **   07.04.2010 H.Kiehl Fix bug with searching for SFTP and added
 **                      toggle button for no protocol.
 **
 */
DESCR__E_M3

#include <stdio.h>
#ifdef HAVE_STRCASESTR
#include <string.h>
#endif
#include <stdlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/LabelG.h>
#include <Xm/Text.h>
#include <Xm/Separator.h>
#include <Xm/PushB.h>
#include <Xm/ToggleBG.h>
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif
#include <errno.h>
#include "mafd_ctrl.h"


/* Global variables. */
Widget                            findshell = (Widget)NULL;

/* External global variables. */
extern Display                    *display;
extern Widget                     appshell;
extern int                        no_of_hosts,
                                  no_of_hosts_invisible,
                                  no_of_hosts_visible,
                                  no_selected,
                                  no_selected_static,
                                  *vpl,
                                  window_width;
extern char                       font_name[],
                                  *info_data;
extern struct line                *connect_data;
extern struct filetransfer_status *fsa;

/* Local global variables. */
static Widget                     alias_toggle_w,
                                  find_text_w,
                                  host_radiobox_w,
                                  proto_togglebox_w;
static int                        deselect,
                                  hostname_type,
                                  redraw_counter,
                                  *redraw_line,
                                  search_type,
                                  static_select;
static XT_PTR_TYPE                toggles_set;

/* Local function prototypes. */
static void                       done_button(Widget, XtPointer, XtPointer),
                                  draw_selections(void),
                                  search_select_host(Widget, XtPointer, XtPointer),
                                  select_callback(Widget, XtPointer, XtPointer),
                                  select_line(int),
                                  toggled(Widget, XtPointer, XtPointer);

#define STATIC_SELECT_CB          1
#define DESELECT_CB               2
#define ALIAS_HOSTNAME_CB         3
#define REAL_HOSTNAME_CB          4
#define SEARCH_INFORMATION_CB     5
#define SEARCH_HOSTNAME_CB        6
#define ALIAS_NAME                1
#define REAL_NAME                 2
#define SEARCH_INFORMATION        3
#define SEARCH_HOSTNAME           4


/*######################### select_host_dialog() ########################*/
void
select_host_dialog(Widget w, XtPointer client_data, XtPointer call_data)
{
   /*
    * First, see if the window has already been created. If
    * no create a new window.
    */
   if ((findshell == (Widget)NULL) || (XtIsRealized(findshell) == False) ||
       (XtIsSensitive(findshell) != True))
   {
      Widget          box_w,
                      buttonbox_w,
                      button_w,
                      criteriabox_w,
                      dialog_w,
                      main_form_w,
                      radiobox_w,
                      separator_w,
                      toggle_w,
                      togglebox_w;
      Arg             args[MAXARGS];
      Cardinal        argcount;
      XmFontList      p_fontlist;
      XmFontListEntry entry;

      findshell = XtVaCreatePopupShell("Search Host", topLevelShellWidgetClass,
                                       appshell, NULL);

      /* Create managing widget */
      main_form_w = XmCreateForm(findshell, "main_form", NULL, 0);

      /* Prepare font */
      if ((entry = XmFontListEntryLoad(XtDisplay(main_form_w), font_name,
                                       XmFONT_IS_FONT, "TAG1")) == NULL)
      {
         if ((entry = XmFontListEntryLoad(XtDisplay(main_form_w), DEFAULT_FONT,
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
      p_fontlist = XmFontListAppendEntry(NULL, entry);
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

      /* Create Search Button. */
      button_w = XtVaCreateManagedWidget("Search",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,             p_fontlist,
                        XmNtopAttachment,        XmATTACH_POSITION,
                        XmNtopPosition,          1,
                        XmNleftAttachment,       XmATTACH_POSITION,
                        XmNleftPosition,         1,
                        XmNrightAttachment,      XmATTACH_POSITION,
                        XmNrightPosition,        10,
                        XmNbottomAttachment,     XmATTACH_POSITION,
                        XmNbottomPosition,       20,
                        NULL);
      XtAddCallback(button_w, XmNactivateCallback,
                    (XtCallbackProc)search_select_host, (XtPointer)0);

      /* Create Done Button. */
      button_w = XtVaCreateManagedWidget("Close",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,             p_fontlist,
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
                    (XtCallbackProc)done_button, (XtPointer)0);
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
      /*                        Criteria Box                           */
      /*---------------------------------------------------------------*/
      criteriabox_w = XtVaCreateWidget("criteriabox",
                        xmFormWidgetClass,   main_form_w,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNrightAttachment,  XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_WIDGET,
                        XmNbottomWidget,     separator_w,
                        NULL);

      /*---------------------------------------------------------------*/
      /*                        Enter Hostname                         */
      /*---------------------------------------------------------------*/
      argcount = 0;
      XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
      argcount++;
      box_w = XmCreateForm(criteriabox_w, "search_box", args, argcount);
      dialog_w = XtVaCreateWidget("Search ",
                        xmLabelGadgetClass,  box_w,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNleftOffset,       5,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        5,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNfontList,         p_fontlist,
                        XmNalignment,        XmALIGNMENT_END,
                        NULL);
      XtManageChild(dialog_w);
      argcount = 0;
      XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNtopOffset,        5);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNleftWidget,       dialog_w);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
      argcount++;
      XtSetArg(args[argcount], XmNpacking,          XmPACK_TIGHT);
      argcount++;
      XtSetArg(args[argcount], XmNnumColumns,       1);
      argcount++;
      radiobox_w = XmCreateRadioBox(box_w, "radiobox", args, argcount);
      dialog_w = XtVaCreateManagedWidget("Hostname",
                                   xmToggleButtonGadgetClass, radiobox_w,
                                   XmNfontList,               p_fontlist,
                                   XmNset,                    True,
                                   NULL);
      XtAddCallback(dialog_w, XmNdisarmCallback,
                    (XtCallbackProc)select_callback,
                    (XtPointer)SEARCH_HOSTNAME_CB);
      dialog_w = XtVaCreateManagedWidget("Information",
                                   xmToggleButtonGadgetClass, radiobox_w,
                                   XmNfontList,               p_fontlist,
                                   XmNset,                    False,
                                   NULL);
      XtAddCallback(dialog_w, XmNdisarmCallback,
                    (XtCallbackProc)select_callback,
                    (XtPointer)SEARCH_INFORMATION_CB);
      search_type = SEARCH_HOSTNAME;
      XtManageChild(radiobox_w);
      XtManageChild(box_w);

      find_text_w = XtVaCreateWidget("find_hostname",
                        xmTextWidgetClass,   criteriabox_w,
                        XmNtopAttachment,    XmATTACH_WIDGET,
                        XmNtopWidget,        box_w,
                        XmNtopOffset,        5,
                        XmNrightAttachment,  XmATTACH_FORM,
                        XmNrightOffset,      5,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNleftOffset,       5,
                        XmNfontList,         p_fontlist,
                        XmNeditMode,         XmSINGLE_LINE_EDIT,
                        NULL);
      XtManageChild(find_text_w);
      XtAddCallback(find_text_w, XmNmodifyVerifyCallback, remove_paste_newline,
                    NULL);

      /*---------------------------------------------------------------*/
      /*                      Horizontal Separator                     */
      /*---------------------------------------------------------------*/
      argcount = 0;
      XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
      argcount++;
      XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNtopWidget,        find_text_w);
      argcount++;
      XtSetArg(args[argcount], XmNtopOffset,        5);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
      argcount++;
      separator_w = XmCreateSeparator(criteriabox_w, "separator", args,
                                      argcount);
      XtManageChild(separator_w);

      /*---------------------------------------------------------------*/
      /*                        Protocol Selection                     */
      /*---------------------------------------------------------------*/
      proto_togglebox_w = XtVaCreateWidget("proto_togglebox",
                                xmRowColumnWidgetClass, criteriabox_w,
                                XmNorientation,         XmHORIZONTAL,
                                XmNpacking,             XmPACK_TIGHT,
                                XmNspacing,             0,
                                XmNnumColumns,          1,
                                XmNtopAttachment,       XmATTACH_WIDGET,
                                XmNtopWidget,           separator_w,
                                XmNleftAttachment,      XmATTACH_FORM,
                                XmNresizable,           False,
                                NULL);
      toggle_w = XtVaCreateManagedWidget("FTP",
                                xmToggleButtonGadgetClass, proto_togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    True,
                                NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_FTP);
#ifdef WITH_SSL
      toggle_w = XtVaCreateManagedWidget("FTPS",
                                xmToggleButtonGadgetClass, proto_togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    True,
                                NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_FTPS);
#endif
      toggle_w = XtVaCreateManagedWidget("HTTP",
                                xmToggleButtonGadgetClass, proto_togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    True,
                                NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_HTTP);
#ifdef WITH_SSL
      toggle_w = XtVaCreateManagedWidget("HTTPS",
                                xmToggleButtonGadgetClass, proto_togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    True,
                                NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_HTTPS);
#endif
      toggle_w = XtVaCreateManagedWidget("SMTP",
                                xmToggleButtonGadgetClass, proto_togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    True,
                                NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_SMTP);
#ifdef WITH_SSL
      toggle_w = XtVaCreateManagedWidget("SMTPS",
                                xmToggleButtonGadgetClass, proto_togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    True,
                                NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_SMTPS);
#endif
#ifdef _WITH_DE_MAIL
      toggle_w = XtVaCreateManagedWidget("DEMAIL",
                                xmToggleButtonGadgetClass, proto_togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    True,
                                NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_DEMAIL);
#endif
      toggle_w = XtVaCreateManagedWidget("FILE",
                                xmToggleButtonGadgetClass, proto_togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    True,
                                NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_FILE);
      toggle_w = XtVaCreateManagedWidget("EXEC",
                                xmToggleButtonGadgetClass, proto_togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    True,
                                NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_EXEC);
      toggle_w = XtVaCreateManagedWidget("SFTP",
                                xmToggleButtonGadgetClass, proto_togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    True,
                                NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_SFTP);
#ifdef _WITH_SCP_SUPPORT
      toggle_w = XtVaCreateManagedWidget("SCP",
                                xmToggleButtonGadgetClass, proto_togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    True,
                                NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_SCP);
#endif
#ifdef _WITH_WMO_SUPPORT
      toggle_w = XtVaCreateManagedWidget("WMO",
                                xmToggleButtonGadgetClass, proto_togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    True,
                                NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_WMO);
#endif
#ifdef _WITH_MAP_SUPPORT
      toggle_w = XtVaCreateManagedWidget("MAP",
                                xmToggleButtonGadgetClass, proto_togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    True,
                                NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_MAP);
#endif
#ifdef _WITH_DFAX_SUPPORT
      toggle_w = XtVaCreateManagedWidget("DFAX",
                                xmToggleButtonGadgetClass, proto_togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    True,
                                NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_DFAX);
#endif
      toggle_w = XtVaCreateManagedWidget("None",
                                xmToggleButtonGadgetClass, proto_togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    True,
                                NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_NONE);
      XtManageChild(proto_togglebox_w);

      toggles_set = SHOW_FTP |
                    SHOW_HTTP |
                    SHOW_SMTP |
#ifdef _WITH_DE_MAIL
                    SHOW_DEMAIL |
#endif
                    SHOW_SFTP |
#ifdef _WITH_SCP_SUPPORT
                    SHOW_SCP |
#endif
#ifdef _WITH_WMO_SUPPORT
                    SHOW_WMO |
#endif
#ifdef _WITH_MAP_SUPPORT
                    SHOW_MAP |
#endif
#ifdef _WITH_DFAX_SUPPORT
                    SHOW_DFAX |
#endif
#ifdef WITH_SSL
                    SHOW_FTPS |
                    SHOW_HTTPS |
                    SHOW_SMTPS |
#endif
                    SHOW_FILE |
                    SHOW_EXEC |
                    SHOW_NONE;

      /*---------------------------------------------------------------*/
      /*                      Horizontal Separator                     */
      /*---------------------------------------------------------------*/
      argcount = 0;
      XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
      argcount++;
      XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNtopWidget,        proto_togglebox_w);
      argcount++;
      XtSetArg(args[argcount], XmNtopOffset,        5);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
      argcount++;
      separator_w = XmCreateSeparator(criteriabox_w, "separator", args,
                                      argcount);
      XtManageChild(separator_w);

      /*---------------------------------------------------------------*/
      /*                         Select Box                            */
      /*---------------------------------------------------------------*/
      togglebox_w = XtVaCreateWidget("togglebox",
                                xmRowColumnWidgetClass, criteriabox_w,
                                XmNorientation,         XmHORIZONTAL,
                                XmNpacking,             XmPACK_TIGHT,
                                XmNnumColumns,          1,
                                XmNtopAttachment,       XmATTACH_WIDGET,
                                XmNtopWidget,           separator_w,
                                XmNleftAttachment,      XmATTACH_FORM,
                                XmNbottomAttachment,    XmATTACH_FORM,
                                XmNresizable,           False,
                                NULL);
      alias_toggle_w = XtVaCreateManagedWidget("Static",
                                xmToggleButtonGadgetClass, togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    False,
                                NULL);
      XtAddCallback(alias_toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)select_callback,
                    (XtPointer)STATIC_SELECT_CB);
      dialog_w = XtVaCreateManagedWidget("Deselect",
                                xmToggleButtonGadgetClass, togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    False,
                                NULL);
      XtAddCallback(dialog_w, XmNvalueChangedCallback,
                    (XtCallbackProc)select_callback, (XtPointer)DESELECT_CB);
      XtManageChild(togglebox_w);
      static_select = NO;
      deselect = NO;

      /*---------------------------------------------------------------*/
      /*                      Vertical Separator                       */
      /*---------------------------------------------------------------*/
      argcount = 0;
      XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
      argcount++;
      XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNtopWidget,        separator_w);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNleftWidget,       togglebox_w);
      argcount++;
      dialog_w = XmCreateSeparator(criteriabox_w, "separator", args, argcount);
      XtManageChild(dialog_w);

      /*---------------------------------------------------------------*/
      /*                          Radio Box                            */
      /*---------------------------------------------------------------*/
      dialog_w = XtVaCreateWidget("Hostname :",
                                  xmLabelGadgetClass,  criteriabox_w,
                                  XmNfontList,         p_fontlist,
                                  XmNalignment,        XmALIGNMENT_END,
                                  XmNtopAttachment,    XmATTACH_WIDGET,
                                  XmNtopWidget,        separator_w,
                                  XmNleftAttachment,   XmATTACH_WIDGET,
                                  XmNleftWidget,       dialog_w,
                                  XmNleftOffset,       5,
                                  XmNbottomAttachment, XmATTACH_FORM,
                                  NULL);
      XtManageChild(dialog_w);
      argcount = 0;
      XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNtopWidget,        separator_w);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNleftWidget,       dialog_w);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
      argcount++;
      XtSetArg(args[argcount], XmNpacking,          XmPACK_TIGHT);
      argcount++;
      XtSetArg(args[argcount], XmNnumColumns,       1);
      argcount++;
      host_radiobox_w = XmCreateRadioBox(criteriabox_w, "host_radiobox",
                                         args, argcount);
      dialog_w = XtVaCreateManagedWidget("Alias",
                                   xmToggleButtonGadgetClass, host_radiobox_w,
                                   XmNfontList,               p_fontlist,
                                   XmNset,                    True,
                                   NULL);
      XtAddCallback(dialog_w, XmNdisarmCallback,
                    (XtCallbackProc)select_callback,
                    (XtPointer)ALIAS_HOSTNAME_CB);
      dialog_w = XtVaCreateManagedWidget("Real",
                                   xmToggleButtonGadgetClass, host_radiobox_w,
                                   XmNfontList,               p_fontlist,
                                   XmNset,                    False,
                                   NULL);
      XtAddCallback(dialog_w, XmNdisarmCallback,
                    (XtCallbackProc)select_callback,
                    (XtPointer)REAL_HOSTNAME_CB);
      hostname_type = ALIAS_NAME;
      XtManageChild(host_radiobox_w);
      XtManageChild(criteriabox_w);
      XtManageChild(main_form_w);

      XmFontListFree(p_fontlist);

#ifdef WITH_EDITRES
      XtAddEventHandler(findshell, (EventMask)0, True, _XEditResCheckMessages,
                        NULL);
#endif
   }
   XtPopup(findshell, XtGrabNone);

   /* We want the keyboard focus on the text field. */
   XmProcessTraversal(find_text_w, XmTRAVERSE_CURRENT);

   return;
}


/*++++++++++++++++++++++++++++++ toggled() ++++++++++++++++++++++++++++++*/
void                                                                       
toggled(Widget w, XtPointer client_data, XtPointer call_data)
{
   toggles_set ^= (XT_PTR_TYPE)client_data;

   return;
}


/*++++++++++++++++++++++++++ select_callback() ++++++++++++++++++++++++++*/
static void
select_callback(Widget w, XtPointer client_data, XtPointer call_data)
{
   switch ((XT_PTR_TYPE)client_data)
   {
      case STATIC_SELECT_CB :
         if (static_select == YES)
         {
            static_select = NO;
         }
         else
         {
            static_select = YES;
         }
         break;

      case DESELECT_CB :
         if (deselect == YES)
         {
            deselect = NO;
            XtSetSensitive(alias_toggle_w, True);
         }
         else
         {
            deselect = YES;
            XtSetSensitive(alias_toggle_w, False);
         }
         break;

      case ALIAS_HOSTNAME_CB :
         hostname_type = ALIAS_NAME;
         break;

      case REAL_HOSTNAME_CB :
         hostname_type = REAL_NAME;
         break;

      case SEARCH_INFORMATION_CB :
         search_type = SEARCH_INFORMATION;
         XtSetSensitive(proto_togglebox_w, False);
         XtSetSensitive(host_radiobox_w, False);
         XmProcessTraversal(find_text_w, XmTRAVERSE_NEXT_TAB_GROUP);
         break;

      case SEARCH_HOSTNAME_CB :
         search_type = SEARCH_HOSTNAME;
         XtSetSensitive(proto_togglebox_w, True);
         XtSetSensitive(host_radiobox_w, True);
         XmProcessTraversal(find_text_w, XmTRAVERSE_NEXT_TAB_GROUP);
         break;

      default :
#if SIZEOF_LONG == 4
         (void)xrec(WARN_DIALOG, "Impossible callback %d! (%s %d)\n",
#else
         (void)xrec(WARN_DIALOG, "Impossible callback %ld! (%s %d)\n",
#endif
                    (XT_PTR_TYPE)client_data, __FILE__, __LINE__);
         break;
   }

   return;
}


/*++++++++++++++++++++++++ search_select_host() +++++++++++++++++++++++++*/
static void
search_select_host(Widget w, XtPointer client_data, XtPointer call_data)
{
   char *text = XmTextGetString(find_text_w);
   int  i;

   redraw_counter = 0;
   if ((redraw_line = malloc((no_of_hosts * sizeof(int)))) == NULL)
   {
      (void)fprintf(stderr,
                    "ERROR : Failed to malloc() memory : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (search_type == SEARCH_HOSTNAME)
   {
      int match;

      for (i = 0; i < no_of_hosts; i++)
      {
         if (((fsa[i].protocol & FTP_FLAG) && (toggles_set & SHOW_FTP)) ||
             ((fsa[i].protocol & SFTP_FLAG) && (toggles_set & SHOW_SFTP)) ||
#ifdef WITH_SSL
             ((fsa[i].protocol & FTP_FLAG) && (fsa[i].protocol & SSL_FLAG) &&
              (toggles_set & SHOW_FTPS)) ||
             ((fsa[i].protocol & HTTP_FLAG) && (fsa[i].protocol & SSL_FLAG) &&
              (toggles_set & SHOW_HTTPS)) ||
#endif
             ((fsa[i].protocol & LOC_FLAG) && (toggles_set & SHOW_FILE)) ||
             ((fsa[i].protocol & EXEC_FLAG) && (toggles_set & SHOW_EXEC)) ||
             ((fsa[i].protocol & SMTP_FLAG) && (toggles_set & SHOW_SMTP)) ||
#ifdef _WITH_DE_MAIL_SUPPORT
             ((fsa[i].protocol & DE_MAIL_FLAG) && (toggles_set & SHOW_DEMAIL)) ||
#endif
#ifdef _WITH_SCP_SUPPORT
             ((fsa[i].protocol & SCP_FLAG) && (toggles_set & SHOW_SCP)) ||
#endif
#ifdef _WITH_WMO_SUPPORT
             ((fsa[i].protocol & WMO_FLAG) && (toggles_set & SHOW_WMO)) ||
#endif
#ifdef _WITH_MAP_SUPPORT
             ((fsa[i].protocol & MAP_FLAG) && (toggles_set & SHOW_MAP)) ||
#endif
#ifdef _WITH_DFAX_SUPPORT
             ((fsa[i].protocol & DFAX_FLAG) && (toggles_set & SHOW_DFAX)) ||
#endif
             ((fsa[i].protocol & HTTP_FLAG) && (toggles_set & SHOW_HTTP)) ||
             ((fsa[i].protocol == 0) && (toggles_set & SHOW_NONE)))
         {
            if (hostname_type == ALIAS_NAME)
            {
               match = pmatch((text[0] == '\0') ? "*" : text, connect_data[i].hostname, NULL);
            }
            else
            {
               if (fsa[i].real_hostname[0][0] == 1)
               {
                  match = 1;
               }
               else
               {
                  if ((fsa[i].toggle_pos > 0) &&
                      (fsa[i].host_toggle_str[0] != '\0'))
                  {
                     if (fsa[i].host_toggle == HOST_ONE)
                     {
                        match = pmatch((text[0] == '\0') ? "*" : text, fsa[i].real_hostname[HOST_ONE - 1], NULL);
                     }
                     else
                     {
                        match = pmatch((text[0] == '\0') ? "*" : text, fsa[i].real_hostname[HOST_TWO - 1], NULL);
                     }
                  }
                  else
                  {
                     match = pmatch((text[0] == '\0') ? "*" : text, fsa[i].real_hostname[HOST_ONE - 1], NULL);
                  }
               }
            }
            if (match == 0)
            {
               select_line(i);
            }
         }
      }
   }
   else
   {
#ifndef HAVE_STRCASESTR
      size_t length;
      char   *real_text;

      length = strlen(text);
      if ((real_text = malloc((length + 3))) == NULL)
      {
         (void)fprintf(stderr, "Failed to malloc() memory : %s\n",
                       strerror(errno));
         exit(INCORRECT);
      }
      real_text[0] = '*';
      (void)strcpy(&real_text[1], text);
      real_text[1 + length] = '*';
      real_text[1 + length + 1] = '\0';
#endif
      for (i = 0; i < no_of_hosts; i++)
      {
         (void)check_info_file(connect_data[i].hostname, HOST_INFO_FILE, NO);
         if (info_data != NULL)
         {
#ifdef HAVE_STRCASESTR
            if (strcasestr(info_data, text) != NULL)
#else
            if (pmatch(real_text, info_data, NULL) == 0)
#endif
            {
               select_line(i);
            }
            free(info_data);
            info_data = NULL;
         }
      }
#ifndef HAVE_STRCASESTR
      (void)free(real_text);
#endif
   }
   draw_selections();
   XFlush(display);
   XtFree(text);
   free(redraw_line);
   redraw_line = NULL;

   return;
}


/*---------------------------- select_line() ----------------------------*/
static void
select_line(int i)
{
   if (connect_data[i].type == 0)
   {
      if (deselect == YES)
      {
         if (connect_data[i].inverse == STATIC)
         {
            ABS_REDUCE_GLOBAL(no_selected_static);
            redraw_line[redraw_counter] = i;
            redraw_counter++;
         }
         else if (connect_data[i].inverse == ON)
              {
                 ABS_REDUCE_GLOBAL(no_selected);
                 redraw_line[redraw_counter] = i;
                 redraw_counter++;
              }
         connect_data[i].inverse = OFF;
      }
      else
      {
         if (static_select == YES)
         {
            if (connect_data[i].inverse != STATIC)
            {
               if (connect_data[i].inverse == ON)
               {
                  ABS_REDUCE_GLOBAL(no_selected);
               }
               no_selected_static++;
               connect_data[i].inverse = STATIC;
               redraw_line[redraw_counter] = i;
               redraw_counter++;
            }
         }
         else
         {
            if (connect_data[i].inverse != ON)
            {
               if (connect_data[i].inverse == STATIC)
               {
                  ABS_REDUCE_GLOBAL(no_selected_static);
               }
               no_selected++;
               connect_data[i].inverse = ON;
               redraw_line[redraw_counter] = i;
               redraw_counter++;
            }
         }
      }
   }

   return;
}


/*-------------------------- draw_selections() --------------------------*/
static void
draw_selections(void)
{
   int i,
       j,
       redraw_everything = NO;

   /*
    * First lets see if we have to open a group. If that is the
    * case we need to redraw everything.
    */
   for (i = 0; i < redraw_counter; i++)
   {
      if (connect_data[redraw_line[i]].plus_minus == PM_CLOSE_STATE)
      {
         for (j = redraw_line[i]; ((j > 0) &&
                                   (connect_data[j].type != 1)); j--)
         {
#ifdef _WITH_DEBUG
            (void)fprintf(stderr,
                          "Opening (%d) %s no_of_hosts_visible=%d no_of_hosts_invisible=%d\n",
                          j, connect_data[j].hostname, no_of_hosts_visible,
                          no_of_hosts_invisible);
#endif
            connect_data[j].plus_minus = PM_OPEN_STATE;
            no_of_hosts_visible++;
            no_of_hosts_invisible--;
         }
#ifdef _WITH_DEBUG
         (void)fprintf(stderr,
                       "!Opening Group! (%d) %s no_of_hosts_visible=%d no_of_hosts_invisible=%d\n",
                       j, connect_data[j].hostname, no_of_hosts_visible,
                       no_of_hosts_invisible);
#endif
         connect_data[j].plus_minus = PM_OPEN_STATE;
         for (j = redraw_line[i] + 1; ((j < no_of_hosts) &&
                                       (connect_data[j].type != 1)); j++)
         {
#ifdef _WITH_DEBUG
            (void)fprintf(stderr,
                          "Opening (%d) %s no_of_hosts_visible=%d no_of_hosts_invisible=%d\n",
                          j, connect_data[j].hostname, no_of_hosts_visible,
                          no_of_hosts_invisible);
#endif
            connect_data[j].plus_minus = PM_OPEN_STATE;
            no_of_hosts_visible++;
            no_of_hosts_invisible--;
         }
         redraw_everything = YES;
      }
   }

   if (redraw_everything == YES)
   {
      /* First lets redo the visible position list (vpl). */
      j = 0;
      for (i = 0; i < no_of_hosts; i++)
      {
         if ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
             (connect_data[i].type == 1))
         {
            vpl[j] = i;
            j++;
         }
      }

      /* Resize and redraw window. */
      if (resize_window() == YES)
      {
         calc_but_coord(window_width);
      }
      redraw_all();
   }
   else
   {
      for (i = 0; i < redraw_counter; i++)
      {
         for (j = 0; j < no_of_hosts; j++)
         {
            if (redraw_line[i] == vpl[j])
            {
               draw_line_status(j, 1);
               break;
            }
         }
      }
   }

   return;
}


/*++++++++++++++++++++++++++++ done_button() ++++++++++++++++++++++++++++*/
static void
done_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   XtPopdown(findshell);

   return;
}
