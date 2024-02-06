/*
 *  mshow_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_log - displays log files from the AFD
 **
 ** SYNOPSIS
 **   mshow_log [--version]
 **                OR
 **   mshow_log [-w <AFD working directory>] 
 **             [-f <font name>]
 **             [-n <alias name>]
 **             -l System|Receive|Transfer|Debug|Monitor|Monsystem
 **             [hostname 1 hostname 2 ... hostname n]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.03.1996 H.Kiehl Created
 **   31.05.1997 H.Kiehl Added debug toggle.
 **                      Remove the font button. Font now gets selected
 **                      via afd_ctrl.
 **   27.12.2003 H.Kiehl Added trace log.
 **   06.02.2024 H.Kiehl Set a signal handler for SIGINT, SIGQUIT and
 **                      SIGTERM, so we close all additional windows
 **                      opened by this dialog.
 **
 */
DESCR__E_M1

#include <stdio.h>               /* fopen(), NULL                        */
#include <string.h>              /* strcpy(), strcat()                   */
#include <ctype.h>               /* toupper()                            */
#include <signal.h>              /* signal()                             */
#ifdef HAVE_SETPRIORITY
# include <sys/time.h>
# include <sys/resource.h>       /* setpriority()                        */
#endif
#include <unistd.h>              /* gethostname(), STDERR_FILENO         */
#include <stdlib.h>              /* getenv(), exit()                     */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/ToggleBG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/LabelG.h>
#include <Xm/Separator.h>
#ifdef _WITH_SCROLLBAR
#include <Xm/ScrollBar.h>
#else
#include <Xm/Scale.h>
#endif
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif
#include <errno.h>
#include "mafd_ctrl.h"
#include "mshow_log.h"
#include "mondefs.h"
#include "logdefs.h"
#include "version.h"
#include "cursor1.h"
#include "cursormask1.h"
#include "cursor2.h"
#include "cursormask2.h"

/* Global variables. */
Display          *display;
XtAppContext     app;
XtIntervalId     interval_id_host;
XmTextPosition   wpr_position;
Cursor           cursor1,
                 cursor2;
Widget           appshell,
                 counterbox,
                 log_output,
                 log_scroll_bar,
                 selectlog,
                 selectscroll;
XmFontList       fontlist;
int              alias_name_length,
                 current_log_number,
                 line_counter,
                 log_type_flag,
                 max_log_number,
                 no_of_active_process = 0,
                 no_of_hosts,
                 sys_log_fd = STDERR_FILENO,
                 toggles_set_parallel_jobs;
XT_PTR_TYPE      toggles_set;
off_t            max_logfile_size = 1024,
                 total_length;
ino_t            current_inode_no;
char             fake_user[MAX_FULL_USER_ID_LENGTH],
                 font_name[40],
                 **hosts = NULL,
                 log_dir[MAX_PATH_LENGTH],
                 log_name[MAX_FILENAME_LENGTH],
                 log_type[MAX_FILENAME_LENGTH],
                 *toggle_label[] =
                 {
                    "Info",
                    "Config",
                    "Warn",
                    "Error",
                    "Offline",
                    "Debug",
                    "Trace"
                 },
                 profile[MAX_PROFILE_NAME_LENGTH + 1],
                 *p_work_dir,
                 work_dir[MAX_PATH_LENGTH];
FILE             *p_log_file;
const char       *sys_log_name = SYSTEM_LOG_FIFO;
struct apps_list *apps_list;

/* Local function prototypes. */
static void      create_cursors(void),
#ifdef HAVE_SETPRIORITY
                 get_afd_config_value(void),
#endif
                 init_log_file(int *, char **, char *),
                 mshow_log_exit(void),
                 sig_bus(int),
                 sig_exit(int),
                 sig_segv(int),
                 usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
#ifdef _WITH_SCROLLBAR
   int             slider_size;
#endif
   char            window_title[MAX_FILENAME_LENGTH + 5 + 40],
                   str_number[MAX_LONG_LENGTH];
   static String   fallback_res[] =
                   {
                      ".show_log*mwmDecorations : 110",
                      ".mshow_log*mwmDecorations : 110",
                      ".show_log*mwmFunctions : 30",
                      ".mshow_log*mwmFunctions : 30",
                      ".show_log.form.log_outputSW*XmText.fontList : fixed",
                      ".mshow_log.form.log_outputSW*XmText.fontList : fixed",
                      ".show_log*background : NavajoWhite2",
                      ".mshow_log*background : NavajoWhite2",
                      ".show_log.form.log_outputSW.log_output.background : NavajoWhite1",
                      ".mshow_log.form.log_outputSW.log_output.background : NavajoWhite1",
                      ".show_log.form.counterbox*background : NavajoWhite1",
                      ".mshow_log.form.counterbox*background : NavajoWhite1",
                      ".show_log.form.searchbox*background : NavajoWhite1",
                      ".mshow_log.form.searchbox*background : NavajoWhite1",
                      ".show_log.form.buttonbox*background : PaleVioletRed2",
                      ".mshow_log.form.buttonbox*background : PaleVioletRed2",
                      ".show_log.form.buttonbox*foreground : Black",
                      ".mshow_log.form.buttonbox*foreground : Black",
                      ".show_log.form.buttonbox*highlightColor : Black",
                      ".mshow_log.form.buttonbox*highlightColor : Black",
                      NULL
                   };
   Widget          form,
                   togglebox,
                   button,
                   buttonbox,
                   v_separator1,
                   v_separator2,
                   h_separator1,
                   h_separator2,
                   searchbox,
                   toggle,
                   label_w,
                   scalebox;
   Arg             args[MAXARGS];
   Cardinal        argcount;
#if !defined (_TOGGLED_PROC_SELECTION) || defined (_WITH_SEARCH_FUNCTION)
   XT_PTR_TYPE     i;
   Widget          option_menu_w,
                   pane_w;
   XmString        label;
#endif
   XmFontListEntry entry;
   uid_t           euid, /* Effective user ID. */
                   ruid; /* Real user ID. */

   CHECK_FOR_VERSION(argc, argv);

   p_work_dir = work_dir;
   init_log_file(&argc, argv, window_title);
#ifdef HAVE_SETPRIORITY
   get_afd_config_value();
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
   disable_drag_drop(appshell);

   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                       euid, strerror(errno), __FILE__, __LINE__);
      }
   }

   line_counter = 0;
   wpr_position = 0;
   total_length = 0;

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
   form = XmCreateForm(appshell, "form", NULL, 0);

   /* Prepare the font. */
   entry = XmFontListEntryLoad(XtDisplay(appshell), font_name,
                               XmFONT_IS_FONT, "TAG1");
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

   /* Create managing widget for the toggle buttons. */
   togglebox = XtVaCreateWidget("togglebox", xmRowColumnWidgetClass, form,
                                XmNorientation,      XmHORIZONTAL,
                                XmNspacing,          0,
                                XmNpacking,          XmPACK_TIGHT,
                                XmNnumColumns,       1,
                                XmNtopAttachment,    XmATTACH_FORM,
                                XmNleftAttachment,   XmATTACH_FORM,
                                XmNresizable,        False,
                                NULL);

   /* Create toggle button widget. */
   toggle = XtVaCreateManagedWidget(toggle_label[0],
                                    xmToggleButtonGadgetClass, togglebox,
                                    XmNfontList,               fontlist,
                                    XmNset,                    True,
                                    NULL);
   XtAddCallback(toggle, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_INFO);
   if ((log_type_flag == SYSTEM_LOG_TYPE) ||
#ifdef _MAINTAINER_LOG
       (log_type_flag == MAINTAINER_LOG_TYPE) ||
#endif
       (log_type_flag == MONITOR_LOG_TYPE) ||
       (log_type_flag == MON_SYSTEM_LOG_TYPE))
   {
      toggle = XtVaCreateManagedWidget(toggle_label[1],
                                       xmToggleButtonGadgetClass, togglebox,
                                       XmNfontList,               fontlist,
                                       XmNset,                    True,
                                       NULL);
      XtAddCallback(toggle, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_CONFIG);
   }
   toggle = XtVaCreateManagedWidget(toggle_label[2],
                                    xmToggleButtonGadgetClass, togglebox,
                                    XmNfontList,               fontlist,
                                    XmNset,                    True,
                                    NULL);
   XtAddCallback(toggle, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_WARN);
   toggle = XtVaCreateManagedWidget(toggle_label[3],
                                    xmToggleButtonGadgetClass, togglebox,
                                    XmNfontList,               fontlist,
                                    XmNset,                    True,
                                    NULL);
   XtAddCallback(toggle, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_ERROR);
   toggle = XtVaCreateManagedWidget(toggle_label[4],
                                    xmToggleButtonGadgetClass, togglebox,
                                    XmNfontList,               fontlist,
                                    XmNset,                    True,
                                    NULL);
   XtAddCallback(toggle, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_OFFLINE);
   if (log_type_flag == TRANS_DB_LOG_TYPE)
   {
      toggle = XtVaCreateManagedWidget(toggle_label[5],
                                       xmToggleButtonGadgetClass, togglebox,
                                       XmNfontList,               fontlist,
                                       XmNset,                    True,
                                       NULL);
      XtAddCallback(toggle, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_DEBUG);
      toggle = XtVaCreateManagedWidget(toggle_label[6],
                                       xmToggleButtonGadgetClass, togglebox,
                                       XmNfontList,               fontlist,
                                       XmNset,                    True,
                                       NULL);
      XtAddCallback(toggle, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_TRACE);
      toggles_set = SHOW_INFO | SHOW_CONFIG | SHOW_WARN | SHOW_ERROR |
                    SHOW_FATAL | SHOW_OFFLINE | SHOW_DEBUG | SHOW_TRACE;
   }
   else
   {
      toggle = XtVaCreateManagedWidget(toggle_label[5],
                                       xmToggleButtonGadgetClass, togglebox,
                                       XmNfontList,               fontlist,
                                       XmNset,                    False,
                                       NULL);
      XtAddCallback(toggle, XmNvalueChangedCallback,
                    (XtCallbackProc)toggled, (XtPointer)SHOW_DEBUG);
      toggles_set = SHOW_INFO | SHOW_CONFIG | SHOW_WARN | SHOW_ERROR |
                    SHOW_OFFLINE| SHOW_FATAL;
   }
   XtManageChild(togglebox);

   /* Create the first horizontal separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,         XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,             togglebox);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator1 = XmCreateSeparator(form, "h_separator1", args, argcount);
   XtManageChild(h_separator1);

   /* Create the first vertical separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       togglebox);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator1);
   argcount++;
   v_separator1 = XmCreateSeparator(form, "v_separator1", args, argcount);
   XtManageChild(v_separator1);

   if ((log_type_flag == TRANSFER_LOG_TYPE) ||
       (log_type_flag == TRANS_DB_LOG_TYPE))
   {
#ifdef _TOGGLED_PROC_SELECTION
      int  i;
      char label[MAX_INT_LENGTH];

      /* Create managing widget for the second toggle buttons. */
      togglebox = XtVaCreateWidget("togglebox2",
                             xmRowColumnWidgetClass, form,
                             XmNorientation,      XmHORIZONTAL,
                             XmNpacking,          XmPACK_TIGHT,
                             XmNnumColumns,       1,
                             XmNtopAttachment,    XmATTACH_FORM,
                             XmNleftAttachment,   XmATTACH_WIDGET,
                             XmNleftWidget,       v_separator1,
                             XmNresizable,        False,
                             NULL);

      toggles_set_parallel_jobs = 0; /* Default to 'all'. */
      for (i = 0; i < MAX_NO_PARALLEL_JOBS; i++)
      {
         /* Create toggle button widget. */
         (void)sprintf(label, "%d", i);
         toggle = XtVaCreateManagedWidget(label,
                             xmToggleButtonGadgetClass, togglebox,
                             XmNfontList,               fontlist,
                             XmNset,                    True,
                             NULL);
         XtAddCallback(toggle, XmNvalueChangedCallback,
                       (XtCallbackProc)toggled_jobs, (XtPointer)i);
      }
      XtManageChild(togglebox);

      /* Create the second vertical separator. */
      argcount = 0;
      XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNleftWidget,       togglebox);
      argcount++;
      XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNbottomWidget,     h_separator1);
      argcount++;
      v_separator1 = XmCreateSeparator(form, "v_separator1", args, argcount);
      XtManageChild(v_separator1);
#else
      Widget box_w;

      argcount = 0;
      XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNleftWidget,       v_separator1);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNbottomWidget,     h_separator1);
      argcount++;
      box_w = XmCreateForm(form, "button_box", args, argcount);

      /* Create a pulldown pane and attach it to the option menu. */
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList,         fontlist);
      argcount++;
      pane_w = XmCreatePulldownMenu(box_w, "pane", args, argcount);

      label = XmStringCreateLocalized("Proc");
      argcount = 0;
      XtSetArg(args[argcount], XmNsubMenuId,        pane_w);
      argcount++;
      XtSetArg(args[argcount], XmNlabelString,      label);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNbottomOffset,     -2);
      argcount++;
      option_menu_w = XmCreateOptionMenu(box_w, "proc_selection",
                                         args, argcount);
      XtManageChild(option_menu_w);
      XmStringFree(label);

      argcount = 0;
      XtSetArg(args[argcount], XmNfontList,         fontlist);
      argcount++;
      XtSetValues(XmOptionLabelGadget(option_menu_w), args, argcount);

      /* Add all possible buttons. */
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList, fontlist);
      argcount++;
      button = XtCreateManagedWidget("all", xmPushButtonWidgetClass,
                                     pane_w, args, argcount);
      XtAddCallback(button, XmNactivateCallback, toggled_jobs, (XtPointer)0);
      for (i = 1; i < (MAX_NO_PARALLEL_JOBS + 1); i++)
      {
         argcount = 0;
         XtSetArg(args[argcount], XmNfontList, fontlist);
         argcount++;
#if SIZEOF_LONG == 4
         (void)sprintf(str_number, "%d", i - 1);
#else
         (void)sprintf(str_number, "%ld", i - 1);
#endif
         button = XtCreateManagedWidget(str_number, xmPushButtonWidgetClass,
                                        pane_w, args, argcount);

         /* Add callback handler. */
         XtAddCallback(button, XmNactivateCallback, toggled_jobs, (XtPointer)i);
      }
      toggles_set_parallel_jobs = 0; /* Default to 'all'. */
      XtManageChild(box_w);

      /* Create the second vertical separator. */
      argcount = 0;
      XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNleftWidget,       box_w);
      argcount++;
      XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNbottomWidget,     h_separator1);
      argcount++;
      v_separator1 = XmCreateSeparator(form, "v_separator1", args, argcount);
      XtManageChild(v_separator1);
#endif
   }

#ifdef _WITH_SEARCH_FUNCTION
   /* Create search box. */
   label_w = XtVaCreateManagedWidget("Search:",
                        xmLabelGadgetClass,       form,
                        XmNtopAttachment,         XmATTACH_FORM,
                        XmNtopOffset,             6,
                        XmNleftAttachment,        XmATTACH_WIDGET,
                        XmNleftWidget,            v_separator1,
                        XmNfontList,              fontlist,
                        XmNalignment,             XmALIGNMENT_BEGINNING,
                        NULL);
   searchbox = XtVaCreateWidget("searchbox",
                        xmTextWidgetClass,        form,
                        XmNleftAttachment,        XmATTACH_WIDGET,
                        XmNleftWidget,            label_w,
                        XmNtopAttachment,         XmATTACH_FORM,
                        XmNtopOffset,             6,
                        XmNfontList,              fontlist,
                        XmNrows,                  1,
                        XmNcolumns,               12,
                        XmNeditable,              True,
                        XmNcursorPositionVisible, True,
                        XmNmarginHeight,          1,
                        XmNmarginWidth,           1,
                        XmNshadowThickness,       1,
                        XmNhighlightThickness,    0,
                        NULL);
   XtManageChild(searchbox);
   XtAddCallback(searchbox, XmNactivateCallback, search_text, NULL);

   /* Create another vertical separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       searchbox);
   argcount++;
   XtSetArg(args[argcount], XmNleftOffset,       5);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator1);
   argcount++;
   v_separator1 = XmCreateSeparator(form, "v_separator1", args, argcount);
   XtManageChild(v_separator1);
#endif

   /* Create line counter box. */
   counterbox = XtVaCreateWidget("counterbox",
                        xmTextWidgetClass,        form,
                        XmNtopAttachment,         XmATTACH_FORM,
                        XmNrightAttachment,       XmATTACH_FORM,
                        XmNtopOffset,             6,
                        XmNrightOffset,           5,
                        XmNfontList,              fontlist,
                        XmNrows,                  1,
                        XmNcolumns,               MAX_LINE_COUNTER_DIGITS,
                        XmNeditable,              False,
                        XmNcursorPositionVisible, False,
                        XmNmarginHeight,          1,
                        XmNmarginWidth,           1,
                        XmNshadowThickness,       1,
                        XmNhighlightThickness,    0,
                        NULL);
   XtManageChild(counterbox);

   /* Create the second vertical separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNrightWidget,      counterbox);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,      2);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator1);
   argcount++;
   v_separator2 = XmCreateSeparator(form, "v_separator2", args, argcount);
   XtManageChild(v_separator2);

   /* Create scale widget for selecting the log file number. */
   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       v_separator1);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNrightWidget,      v_separator2);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,      2);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,     h_separator1);
   argcount++;
   scalebox = XmCreateForm(form, "scalebox", args, argcount);

#ifdef _WITH_SEARCH_FUNCTION
   /* Create a pulldown pane and attach it to the option menu. */
   argcount = 0;
   XtSetArg(args[argcount], XmNfontList,         fontlist);
   argcount++;
   pane_w = XmCreatePulldownMenu(scalebox, "pane", args, argcount);

   label = XmStringCreateLocalized("Log file:");
   argcount = 0;
   XtSetArg(args[argcount], XmNsubMenuId,        pane_w);
   argcount++;
   XtSetArg(args[argcount], XmNlabelString,      label);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomOffset,     -2);
   argcount++;
   option_menu_w = XmCreateOptionMenu(scalebox, "proc_selection",
                                      args, argcount);
   XtManageChild(option_menu_w);
   XmStringFree(label);

   argcount = 0;
   XtSetArg(args[argcount], XmNfontList,         fontlist);
   argcount++;
   XtSetValues(XmOptionLabelGadget(option_menu_w), args, argcount);

   /* Add all possible buttons. */
   for (i = 0; i < (max_log_number + 1); i++)
   {
      argcount = 0;
      XtSetArg(args[argcount], XmNfontList, fontlist);
      argcount++;
#if SIZEOF_LONG == 4
      (void)sprintf(str_number, "%d", i);
#else
      (void)sprintf(str_number, "%ld", i);
#endif
      button = XtCreateManagedWidget(str_number, xmPushButtonWidgetClass,
                                     pane_w, args, argcount);

      /* Add callback handler. */
      XtAddCallback(button, XmNactivateCallback, toggled_log_no, (XtPointer)i);
   }
   argcount = 0;
   XtSetArg(args[argcount], XmNfontList, fontlist);
   argcount++;
   button = XtCreateManagedWidget("all", xmPushButtonWidgetClass,
                                  pane_w, args, argcount);
   XtAddCallback(button, XmNactivateCallback, toggled_log_no, (XtPointer)-1);
   current_log_number = 0;
#else
# ifdef _WITH_SCROLLBAR
   label_w = XtVaCreateManagedWidget("Log file:",
                        xmLabelGadgetClass, scalebox,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNleftOffset,       2,
                        XmNfontList,         fontlist,
                        XmNalignment,        XmALIGNMENT_BEGINNING,
                        NULL);
   str_number[0] = '0';
   str_number[1] = '\0';
   selectlog = XtVaCreateWidget(str_number,
                        xmLabelGadgetClass,       scalebox,
                        XmNtopAttachment,         XmATTACH_FORM,
                        XmNtopOffset,             1,
                        XmNrightAttachment,       XmATTACH_FORM,
                        XmNrightOffset,           2,
                        XmNleftAttachment,        XmATTACH_WIDGET,
                        XmNleftWidget,            label_w,
                        XmNfontList,              fontlist,
                        XmNalignment,             XmALIGNMENT_END,
                        NULL);
   XtManageChild(selectlog);
   if ((slider_size = ((max_log_number + 1) / 10)) < 1)
   {
      slider_size = 1;
   }
   selectscroll = XtVaCreateManagedWidget("selectscroll",
                        xmScrollBarWidgetClass, scalebox,
                        XmNmaximum,         max_log_number + slider_size,
                        XmNminimum,         0,
                        XmNsliderSize,      slider_size,
                        XmNvalue,           0,
                        XmNincrement,       1,
                        XmNfontList,        fontlist,
                        XmNheight,          10,
                        XmNtopOffset,       1,
                        XmNorientation,     XmHORIZONTAL,
                        XmNtopAttachment,   XmATTACH_WIDGET,
                        XmNtopWidget,       selectlog,
                        XmNleftAttachment,  XmATTACH_WIDGET,
                        XmNleftWidget,      label_w,
                        NULL);
   XtAddCallback(selectscroll, XmNvalueChangedCallback, slider_moved,
                 (XtPointer)&current_log_number);
   XtAddCallback(selectscroll, XmNdragCallback, slider_moved,
                 (XtPointer)&current_log_number);
# else
   label_w = XtVaCreateManagedWidget("Log file:",
                        xmLabelGadgetClass,  scalebox,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNleftOffset,       2,
                        XmNfontList,         fontlist,
                        XmNalignment,        XmALIGNMENT_BEGINNING,
                        NULL);
   selectscroll = XtVaCreateManagedWidget("selectscroll",
                        xmScaleWidgetClass,    scalebox,
                        XmNmaximum,            max_log_number,
                        XmNminimum,            0,
                        XmNvalue,              0,
                        XmNshowValue,          True,
                        XmNorientation,        XmHORIZONTAL,
                        XmNfontList,           fontlist,
                        XmNhighlightThickness, 0,
                        XmNscaleHeight,        10,
                        XmNtopAttachment,      XmATTACH_FORM,
                        XmNtopOffset,          3,
                        XmNbottomAttachment,   XmATTACH_FORM,
                        XmNbottomOffset,       4,
                        XmNrightAttachment,    XmATTACH_FORM,
                        XmNleftAttachment,     XmATTACH_WIDGET,
                        XmNleftWidget,         label_w,
                        XmNleftOffset,         2,
                        NULL);
# endif /* _WITH_SCROLLBAR */
#endif
   XtManageChild(scalebox);

   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase, 21);
   argcount++;
   buttonbox = XmCreateForm(form, "buttonbox", args, argcount);
   button = XtVaCreateManagedWidget("Update",
                        xmPushButtonWidgetClass, buttonbox,
                        XmNfontList,         fontlist,
                        XmNtopAttachment,    XmATTACH_POSITION,
                        XmNtopPosition,      1,
                        XmNleftAttachment,   XmATTACH_POSITION,
                        XmNleftPosition,     1,
                        XmNrightAttachment,  XmATTACH_POSITION,
                        XmNrightPosition,    10,
                        XmNbottomAttachment, XmATTACH_POSITION,
                        XmNbottomPosition,   20,
                        NULL);
   XtAddCallback(button, XmNactivateCallback,
                 (XtCallbackProc)update_button, (XtPointer)0);
   button = XtVaCreateManagedWidget("Close",
                        xmPushButtonWidgetClass, buttonbox,
                        XmNfontList,         fontlist,
                        XmNtopAttachment,    XmATTACH_POSITION,
                        XmNtopPosition,      1,
                        XmNleftAttachment,   XmATTACH_POSITION,
                        XmNleftPosition,     11,
                        XmNrightAttachment,  XmATTACH_POSITION,
                        XmNrightPosition,    20,
                        XmNbottomAttachment, XmATTACH_POSITION,
                        XmNbottomPosition,   20,
                        NULL);
   XtAddCallback(button, XmNactivateCallback,
                 (XtCallbackProc)close_button, (XtPointer)0);
   XtManageChild(buttonbox);

   /* Create the second horizontal separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,      XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,          buttonbox);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator2 = XmCreateSeparator(form, "h_separator2", args, argcount);
   XtManageChild(h_separator2);

   /* Create log_text as a ScrolledText window. */
   argcount = 0;
   XtSetArg(args[argcount], XmNrows,                   9);
   argcount++;
   if (log_type_flag == TRANS_DB_LOG_TYPE)
   {
      XtSetArg(args[argcount], XmNcolumns,             TRANS_DB_LOG_WIDTH);
   }
   else
   {
      XtSetArg(args[argcount], XmNcolumns,             DEFAULT_SHOW_LOG_WIDTH);
   }
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
   XtSetArg(args[argcount], XmNtopWidget,              h_separator1);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,           h_separator2);
   argcount++;
   log_output = XmCreateScrolledText(form, "log_output", args, argcount);
   XtManageChild(log_output);
   XtAddCallback(log_output, XmNgainPrimaryCallback,
                 (XtCallbackProc)check_selection, (XtPointer)0);
   XtManageChild(form);

#ifdef WITH_EDITRES
   XtAddEventHandler(appshell, (EventMask)0, True, _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets. */
   XtRealizeWidget(appshell);
   wait_visible(appshell);


   /* Set some signal handlers. */
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(WARN_DIALOG, "Failed to set signal handler's for %s : %s",
                 SHOW_LOG, strerror(errno));
   }

   if (atexit(mshow_log_exit) != 0)
   {
      (void)xrec(WARN_DIALOG, "Failed to set exit handler for %s : %s",
                 SHOW_LOG, strerror(errno));                          
   }

   /* Create pixmaps for cursor. */
   create_cursors();

   /*
    * Get widget id of the scroll bar in the scrolled text widget.
    */
   XtVaGetValues(XtParent(log_output), XmNverticalScrollBar,
                 &log_scroll_bar, NULL);

   init_text();

   /* Call check_log() after LOG_START_TIMEOUT ms. */
   interval_id_host = XtAppAddTimeOut(app, LOG_START_TIMEOUT,
                                      (XtTimerCallbackProc)check_log,
                                      log_output);

   /* Show line_counter if necessary. */
   if (line_counter != 0)
   {
      (void)sprintf(str_number, "%*d",
                    MAX_LINE_COUNTER_DIGITS, line_counter);
      XmTextSetString(counterbox, str_number);
   }

   /* We want the keyboard focus on the log output. */
   XmProcessTraversal(log_output, XmTRAVERSE_CURRENT);

   /* Start the main event-handling loop. */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++ init_log_file() +++++++++++++++++++++++++++*/
static void
init_log_file(int *argc, char *argv[], char *window_title)
{
   int  max_alias_length;
   char log_file[MAX_PATH_LENGTH + 1 + MAX_FILENAME_LENGTH + 2];

   if ((get_arg(argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
      exit(SUCCESS);
   }
   if (get_afd_path(argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }

   if (get_arg(argc, argv, "-l", log_type, MAX_FILENAME_LENGTH) == INCORRECT)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }

   /* Check if title is specified. */
   if (get_arg(argc, argv, "-t", font_name, 40) == INCORRECT)
   {
      char hostname[MAX_AFD_NAME_LENGTH];

      (void)strcpy(window_title, log_type);
      (void)strcat(window_title, " Log ");
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
      (void)snprintf(window_title, MAX_FILENAME_LENGTH + 5 + 40,
                     "%s Log %s", log_type, font_name);
   }

   if (get_arg(argc, argv, "-p", profile, MAX_PROFILE_NAME_LENGTH) == INCORRECT)
   {
      profile[0] = '\0';
   }
   if (get_arg(argc, argv, "-f", font_name, 40) == INCORRECT)
   {
      (void)strcpy(font_name, DEFAULT_FONT);
   }
   if (get_arg(argc, argv, "-n", log_file, MAX_INT_LENGTH) == SUCCESS)
   {
      alias_name_length = atoi(log_file);
   }
   else
   {
      alias_name_length = 0;
   }
   check_fake_user(argc, argv, AFD_CONFIG_FILE, fake_user);

   /* Initialise log directory. */
   (void)strcpy(log_dir, work_dir);
   (void)strcat(log_dir, LOG_DIR);
   if (my_strcmp(log_type, SYSTEM_STR) == 0)
   {
      (void)strcpy(log_name, SYSTEM_LOG_NAME);
      max_log_number = MAX_SYSTEM_LOG_FILES;
      max_alias_length = MAX_DIR_ALIAS_LENGTH;
      max_logfile_size = MAX_SYS_LOGFILE_SIZE;
      get_max_log_values(&max_log_number, MAX_SYSTEM_LOG_FILES_DEF,
                         MAX_SYSTEM_LOG_FILES, &max_logfile_size,
                         MAX_SYS_LOGFILE_SIZE_DEF, MAX_SYS_LOGFILE_SIZE,
                         AFD_CONFIG_FILE);
      max_log_number--;
      log_type_flag = SYSTEM_LOG_TYPE;
   }
#ifdef _MAINTAINER_LOG
   else if (my_strcmp(log_type, MAINTAINER_STR) == 0)
        {
           (void)strcpy(log_name, MAINTAINER_LOG_NAME);
           max_log_number = MAX_MAINTAINER_LOG_FILES;
           max_alias_length = MAX_DIR_ALIAS_LENGTH;
           max_logfile_size = MAX_MAINTAINER_LOGFILE_SIZE;
           get_max_log_values(&max_log_number, MAX_MAINTAINER_LOG_FILES_DEF,
                              MAX_MAINTAINER_LOG_FILES, &max_logfile_size,
                              MAX_MAINTAINER_LOGFILE_SIZE_DEF,
                              MAX_MAINTAINER_LOGFILE_SIZE,
                              AFD_CONFIG_FILE);
           max_log_number--;
           log_type_flag = MAINTAINER_LOG_TYPE;
        }
#endif
   else if (my_strcmp(log_type, RECEIVE_STR) == 0)
        {
           (void)strcpy(log_name, RECEIVE_LOG_NAME);
           max_log_number = MAX_RECEIVE_LOG_FILES;
           max_alias_length = MAX_DIR_ALIAS_LENGTH;
           if ((alias_name_length < 1) ||
               (alias_name_length > MAX_DIR_ALIAS_LENGTH))
           {
              alias_name_length = DEFAULT_DIR_ALIAS_DISPLAY_LENGTH;
           }
           get_max_log_values(&max_log_number, MAX_RECEIVE_LOG_FILES_DEF,
                              MAX_RECEIVE_LOG_FILES, NULL, NULL, 0,
                              AFD_CONFIG_FILE);
           max_log_number--;
           log_type_flag = RECEIVE_LOG_TYPE;
        }
   else if (my_strcmp(log_type, TRANSFER_STR) == 0)
        {
           (void)strcpy(log_name, TRANSFER_LOG_NAME);
           max_log_number = MAX_TRANSFER_LOG_FILES;
           max_alias_length = MAX_HOSTNAME_LENGTH;
           if ((alias_name_length < 1) ||
               (alias_name_length > MAX_HOSTNAME_LENGTH))
           {
              alias_name_length = DEFAULT_HOSTNAME_DISPLAY_LENGTH;
           }
           get_max_log_values(&max_log_number, MAX_TRANSFER_LOG_FILES_DEF,
                              MAX_TRANSFER_LOG_FILES, NULL, NULL, 0,
                              AFD_CONFIG_FILE);
           max_log_number--;
           log_type_flag = TRANSFER_LOG_TYPE;
        }
   else if (my_strcmp(log_type, TRANS_DB_STR) == 0)
        {
           (void)strcpy(log_name, TRANS_DB_LOG_NAME);
           max_log_number = MAX_TRANS_DB_LOG_FILES;
           max_alias_length = MAX_HOSTNAME_LENGTH;
           max_logfile_size = MAX_TRANS_DB_LOGFILE_SIZE;
           if ((alias_name_length < 1) ||
               (alias_name_length > MAX_HOSTNAME_LENGTH))
           {
              alias_name_length = DEFAULT_HOSTNAME_DISPLAY_LENGTH;
           }
           get_max_log_values(&max_log_number, MAX_TRANS_DB_LOG_FILES_DEF,
                              MAX_TRANS_DB_LOG_FILES, &max_logfile_size,
                              MAX_TRANS_DB_LOGFILE_SIZE_DEF,
                              MAX_TRANS_DB_LOGFILE_SIZE,
                              AFD_CONFIG_FILE);
           max_log_number--;
           log_type_flag = TRANS_DB_LOG_TYPE;
        }
   else if (my_strcmp(log_type, MON_SYSTEM_STR) == 0)
        {
           (void)strcpy(log_name, MON_SYS_LOG_NAME);
           max_log_number = MAX_MON_SYS_LOG_FILES;
           max_alias_length = MAX_DIR_ALIAS_LENGTH;
           get_max_log_values(&max_log_number, MAX_MON_SYS_LOG_FILES_DEF,
                              MAX_MON_SYS_LOG_FILES, NULL, NULL, 0,
                              MON_CONFIG_FILE);
           max_log_number--;
           log_type_flag = MON_SYSTEM_LOG_TYPE;
        }
   else if (my_strcmp(log_type, MONITOR_STR) == 0)
        {
           (void)strcpy(log_name, MON_LOG_NAME);
           max_log_number = MAX_MON_LOG_FILES;
           max_alias_length = MAX_AFDNAME_LENGTH;
           if ((alias_name_length < 1) ||
               (alias_name_length > MAX_AFDNAME_LENGTH))
           {
              alias_name_length = DEFAULT_AFD_ALIAS_DISPLAY_LENGTH;
           }
           get_max_log_values(&max_log_number, MAX_MON_LOG_FILES_DEF,
                              MAX_MON_LOG_FILES, NULL, NULL, 0,
                              MON_CONFIG_FILE);
           max_log_number--;
           log_type_flag = MONITOR_LOG_TYPE;
        }
        else
        {
            (void)fprintf(stderr, "ERROR   : Unknown log type %s (%s %d)\n",
                          log_type, __FILE__, __LINE__);
            exit(INCORRECT);
        }

   max_alias_length += 4; /* In case there is switching information. */
   (void)sprintf(log_file, "%s/%s0", log_dir, log_name);
   current_log_number = 0;

   if ((p_log_file = fopen(log_file, "r")) == NULL)
   {
      (void)fprintf(stderr, "ERROR   : Could not fopen() %s : %s (%s %d)\n",
                    log_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Collect all hostnames. */
   no_of_hosts = *argc - 1;
   if (no_of_hosts > 0)
   {
      int i = 0;

      RT_ARRAY(hosts, no_of_hosts, (max_alias_length + 1), char);
      while (*argc > 1)
      {
         (void)my_strncpy(hosts[i], argv[1], (max_alias_length + 1));
         (*argc)--; argv++;
         i++;
      }
   }

   return;
}


#ifdef HAVE_SETPRIORITY
/*+++++++++++++++++++++++++ get_afd_config_value() ++++++++++++++++++++++*/
static void
get_afd_config_value(void)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH];

   (void)sprintf(config_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_file, F_OK) == 0) &&
       (read_file_no_cr(config_file, &buffer, YES, __FILE__, __LINE__) != INCORRECT))
   {
      char value[MAX_INT_LENGTH];

      if (get_definition(buffer, SHOW_LOG_PRIORITY_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         if (setpriority(PRIO_PROCESS, 0, atoi(value)) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to set priority to %d : %s",
                       atoi(value), strerror(errno));
         }
      }
      free(buffer);
   }

   return;
}
#endif


/*---------------------------------- usage() ----------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 "Usage: %s [-w <work_dir>] [-f <font name>] [-n <alias length>] -l System|Receive|Transfer|Debug|Monitor|Monsystem [hostname 1..n] [X arguments]\n",
                 progname);
   return;
}


/*+++++++++++++++++++++++++++ create_cursors() ++++++++++++++++++++++++++*/
static void
create_cursors(void)
{
   XColor fg,
          bg;
   Pixmap cursorsrc,
          cursormask;

   cursorsrc = XCreateBitmapFromData(display,
                                     XtWindow(appshell),
                                     (char *)cursor1_bits,
                                     cursor1_width, cursor1_height);
   cursormask = XCreateBitmapFromData(display,
                                      XtWindow(appshell),
                                      (char *)cursormask1_bits,
                                      cursormask1_width, cursormask1_height);
   XParseColor(display, DefaultColormap(display, DefaultScreen(display)),
               "black", &fg);
   XParseColor(display, DefaultColormap(display, DefaultScreen(display)),
               "white", &bg);
   cursor1 = XCreatePixmapCursor(display, cursorsrc, cursormask, &fg, &bg,
                                 cursor1_x_hot, cursor1_y_hot);
   XFreePixmap(display, cursorsrc); XFreePixmap(display, cursormask);

   cursorsrc = XCreateBitmapFromData(display,
                                     XtWindow(appshell),
                                     (char *)cursor2_bits,
                                     cursor2_width, cursor2_height);
   cursormask = XCreateBitmapFromData(display,
                                      XtWindow(appshell),
                                      (char *)cursormask2_bits,
                                      cursormask2_width, cursormask2_height);
   XParseColor(display, DefaultColormap(display, DefaultScreen(display)),
               "black", &fg);
   XParseColor(display, DefaultColormap(display, DefaultScreen(display)),
               "white", &bg);
   cursor2 = XCreatePixmapCursor(display, cursorsrc, cursormask, &fg, &bg,
                                 cursor2_x_hot, cursor2_y_hot);
   XFreePixmap(display, cursorsrc); XFreePixmap(display, cursormask);

   return;
}


/*++++++++++++++++++++++++++ mshow_log_exit() +++++++++++++++++++++++++++*/
static void
mshow_log_exit(void)
{                   
   int i;

   for (i = 0; i < no_of_active_process; i++)
   {
      if (apps_list[i].pid > 0)
      {
         if (kill(apps_list[i].pid, SIGINT) < 0)
         {
#if SIZEOF_PID_T == 4
            (void)xrec(WARN_DIALOG, "Failed to kill() process %s (%d) : %s",
#else                                                                       
            (void)xrec(WARN_DIALOG, "Failed to kill() process %s (%lld) : %s",
#endif                                                                        
                       apps_list[i].progname,
                       (pri_pid_t)apps_list[i].pid, strerror(errno));
         }
      }
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
