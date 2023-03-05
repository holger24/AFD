/*
 *  show_elog.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M1
/*
 ** NAME
 **   show_elog - displays all AFD events
 **
 ** SYNOPSIS
 **   show_elog [--version]
 **                  OR
 **   show_elog [-w <AFD working directory>] [fontname] [alias 1..n]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   01.07.2007 H.Kiehl Created
 **   03.01.2008 H.Kiehl Added warn time for host.
 **   04.05.2009 H.Kiehl Added success actions.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>            /* strcpy(), strcat(), strerror()         */
#include <ctype.h>             /* toupper()                              */
#include <signal.h>            /* signal(), kill()                       */
#include <stdlib.h>            /* free(), atexit()                       */
#ifdef HAVE_SETPRIORITY
# include <sys/time.h>
# include <sys/resource.h>     /* setpriority()                          */
#endif
#include <sys/types.h>
#include <sys/stat.h>          /* umask()                                */
#include <unistd.h>            /* gethostname()                          */
#include <errno.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/ToggleBG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/Separator.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif
#include "mafd_ctrl.h"
#include "show_elog.h"
#include "logdefs.h"
#include "permission.h"
#include "version.h"

/* Global variables. */
Display          *display;
XtAppContext     app;
Widget           appshell,
                 class_togglebox_w,
                 cont_togglebox_w,
                 dir_alias_w,
                 dir_label_w,
                 end_time_w,
                 headingbox_w,
                 host_alias_w,
                 host_label_w,
                 outputbox_w,
                 print_button_w,
                 scrollbar_w,
                 search_w,
                 selectionbox_w,
                 special_button_w,
                 start_time_w,
                 statusbox_w,
                 type_togglebox_w;
XmTextPosition   wpr_position;
Window           main_window;
XmFontList       fontlist;
int              char_width,
                 continues_toggle_set,
                 event_log_fd = STDERR_FILENO,
                 items_selected = NO,
                 max_event_log_files,
                 no_of_log_files,
                 no_of_search_dir_alias,
                 no_of_search_host_alias,
                 special_button_flag,
                 sys_log_fd = STDERR_FILENO;
unsigned int     ea_toggles_set_1,
                 ea_toggles_set_2,
                 ea_toggles_set_3;
Dimension        button_height;
XT_PTR_TYPE      toggles_set;
time_t           start_time_val,
                 end_time_val;
char             *p_work_dir,
                 font_name[40],
                 heading_line[MAX_OUTPUT_LINE_LENGTH + 1],
                 search_add_info[MAX_EVENT_REASON_LENGTH + 1],
                 **search_host_alias = NULL,
                 **search_dir_alias = NULL,
                 summary_str[MAX_OUTPUT_LINE_LENGTH + 1 + 5],
                 sum_sep_line[MAX_OUTPUT_LINE_LENGTH + 1],
                 user[MAX_FULL_USER_ID_LENGTH];
struct apps_list *apps_list;
struct sol_perm  perm;
const char       *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void      eval_permissions(char *),
#ifdef HAVE_SETPRIORITY
                 get_afd_config_value(void),
#endif
                 init_show_elog(int *, char **, char *),
                 sig_bus(int),
                 sig_exit(int),
                 sig_segv(int),
                 usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char            window_title[14 + 40 + 1],
                   work_dir[MAX_PATH_LENGTH];
   static String   fallback_res[] =
                   {
                      ".show_elog*background : NavajoWhite2",
                      ".show_elog.mainform*background : NavajoWhite2",
                      ".show_elog.mainform*XmText.background : NavajoWhite1",
                      ".show_elog.mainform*listbox.background : NavajoWhite1",
                      ".show_elog.mainform.buttonbox*background : PaleVioletRed2",
                      ".show_elog.mainform.buttonbox*foreground : Black",
                      ".show_elog.mainform.buttonbox*highlightColor : Black",
                      ".show_elog.Print Data*background : NavajoWhite2",
                      ".show_elog.Print Data*XmText.background : NavajoWhite1",
                      ".show_elog.Print Data.main_form.buttonbox*background : PaleVioletRed2",
                      ".show_elog.Print Data.main_form.buttonbox*foreground : Black",
                      ".show_elog.Print Data.main_form.buttonbox*highlightColor : Black",
                      ".show_elog.Select Event Actions.main_form.buttonbox*background : PaleVioletRed2",
                      ".show_elog.Select Event Actions.main_form.buttonbox*foreground : Black",
                      ".show_elog.Select Event Actions.main_form.buttonbox*highlightColor : Black",
                      NULL
                   };
   Widget          block_w,
                   buttonbox_w,
                   button_w,
                   criteriabox_w,
                   currenttime_w,
                   entertime_w,
                   label_w,
                   mainform_w,
                   rowcol_w,
                   separator_w,
                   timebox_w,
                   toggle_w;
   Arg             args[16];
   Cardinal        argcount;
   XmFontListEntry entry;
   XFontStruct     *font_struct;
   XmFontType      dummy;
   uid_t           euid, /* Effective user ID. */
                   ruid; /* Real user ID. */

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values. */
   p_work_dir = work_dir;
   init_show_elog(&argc, argv, window_title);
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
   display = XtDisplay(appshell);

#ifdef HAVE_XPM
   /* Setup AFD logo as icon. */
   setup_icon(display, appshell);
#endif

   /* Create managing widget. */
   mainform_w = XmCreateForm(appshell, "mainform", NULL, 0);

   /* Prepare font. */
   if ((entry = XmFontListEntryLoad(XtDisplay(mainform_w), font_name,
                                    XmFONT_IS_FONT, "TAG1")) == NULL)
   {
       if ((entry = XmFontListEntryLoad(XtDisplay(mainform_w), DEFAULT_FONT,
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
   font_struct = (XFontStruct *)XmFontListEntryGetFont(entry, &dummy);
   char_width  = font_struct->per_char->width;
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

/*-----------------------------------------------------------------------*/
/*                           Time Box                                    */
/*                           --------                                    */
/* Start and end time to search for files in the queue. If no time is    */
/* entered it means we should search through all files in the queue.     */
/*-----------------------------------------------------------------------*/
   /* Create managing widget to enter the time interval. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   timebox_w = XmCreateForm(mainform_w, "timebox", args, argcount);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   entertime_w = XmCreateForm(timebox_w, "entertime", args, argcount);
   rowcol_w = XtVaCreateWidget("rowcol", xmRowColumnWidgetClass, entertime_w,
                               XmNorientation, XmHORIZONTAL,
                               NULL);
   block_w = XmCreateForm(rowcol_w, "rowcol", NULL, 0);
   label_w = XtVaCreateManagedWidget(" Start time :",
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
   label_w = XtVaCreateManagedWidget("End time :",
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

/*-----------------------------------------------------------------------*/
/*                          Vertical Separator                           */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       entertime_w);
   argcount++;
   separator_w = XmCreateSeparator(timebox_w, "separator", args, argcount);
   XtManageChild(separator_w);

/*-----------------------------------------------------------------------*/
/*                        Continues Toggle Box                           */
/*                        --------------------                           */
/* Let user select the if he wants toe run this dialog in continues mode.*/
/*-----------------------------------------------------------------------*/
   cont_togglebox_w = XtVaCreateWidget("cont_togglebox",
                                xmRowColumnWidgetClass, timebox_w,
                                XmNorientation,      XmHORIZONTAL,
                                XmNpacking,          XmPACK_TIGHT,
                                XmNnumColumns,       1,
                                XmNtopAttachment,    XmATTACH_FORM,
                                XmNleftAttachment,   XmATTACH_WIDGET,
                                XmNleftWidget,       separator_w,
                                XmNbottomAttachment, XmATTACH_FORM,
                                XmNresizable,        False,
                                NULL);                     

   toggle_w = XtVaCreateManagedWidget("Cont. ",
                                xmToggleButtonGadgetClass, cont_togglebox_w,
                                XmNfontList,               fontlist,
                                XmNset,                    False,   
                                NULL);
   XtAddCallback(toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)continues_toggle, NULL);
   continues_toggle_set = NO;
   XtManageChild(cont_togglebox_w);

/*-----------------------------------------------------------------------*/
/*                          Vertical Separator                           */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       cont_togglebox_w);
   argcount++;
   separator_w = XmCreateSeparator(timebox_w, "separator", args, argcount);
   XtManageChild(separator_w);

   currenttime_w = XtVaCreateManagedWidget("",
                           xmLabelWidgetClass,  timebox_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNrightAttachment,  XmATTACH_FORM,
                           XmNrightOffset,      10,
                           NULL);
   XtManageChild(timebox_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       timebox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(mainform_w, "separator", args, argcount);
   XtManageChild(separator_w);

/*-----------------------------------------------------------------------*/
/*                          Selection Box                                */
/*                          -------------                                */
/* Here more search parameters can be entered, such as: file name,       */
/* length of the file, directory from which the file had its origion,    */
/* recipient of the file.                                                */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for other criteria. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,    104);
   argcount++;
   selectionbox_w = XmCreateForm(mainform_w, "selectionbox", args, argcount);

   host_label_w = XtVaCreateManagedWidget("Host (,):",
                           xmLabelGadgetClass,  selectionbox_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   103,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     0,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    9,
                           XmNalignment,        XmALIGNMENT_END,
                           NULL);
   host_alias_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   selectionbox_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   103,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       host_label_w,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    31,
                           NULL);
   XtAddCallback(host_alias_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)HOST_ALIAS_NO_ENTER);
   XtAddCallback(host_alias_w, XmNactivateCallback, save_input,
                 (XtPointer)HOST_ALIAS);

   dir_label_w = XtVaCreateManagedWidget("Dir (,):",
                           xmLabelGadgetClass,  selectionbox_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   103,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     32,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    40,
                           XmNalignment,        XmALIGNMENT_END,
                           NULL);
   dir_alias_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   selectionbox_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   103,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       dir_label_w,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    60,
                           NULL);
   XtAddCallback(dir_alias_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)DIR_ALIAS_NO_ENTER);
   XtAddCallback(dir_alias_w, XmNactivateCallback, save_input,
                 (XtPointer)DIR_ALIAS);

   label_w = XtVaCreateManagedWidget("Add. Info :",
                           xmLabelGadgetClass,  selectionbox_w,
                           XmNfontList,         fontlist,
                           XmNalignment,        XmALIGNMENT_END,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   103,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     60,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    71,
                           NULL);
   search_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   selectionbox_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   103,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    103,
                           NULL);
   XtAddCallback(search_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)SEARCH_ADD_INFO_NO_ENTER);
   XtAddCallback(search_w, XmNactivateCallback, save_input,
                 (XtPointer)SEARCH_ADD_INFO);
   XtManageChild(selectionbox_w);


/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       selectionbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(mainform_w, "separator", args, argcount);
   XtManageChild(separator_w);

/*-----------------------------------------------------------------------*/
/*                         Criteria Box 1                                */
/*                         --------------                                */
/* Let user select the event class (Global, Directory, Production and    */
/* Host), event type (External, Manual and Auto) and event action.       */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for criteria box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   criteriabox_w = XmCreateForm(mainform_w, "criteriabox", args, argcount);

/*-----------------------------------------------------------------------*/
/*                           Toggle Box                                  */
/*                           ----------                                  */
/* Let user select the event class: Global, Directory, Production and    */
/* Host.                                                                 */
/*-----------------------------------------------------------------------*/
   label_w = XtVaCreateManagedWidget("Event Class :",
                                xmLabelGadgetClass,  criteriabox_w,
                                XmNfontList,         fontlist,
                                XmNalignment,        XmALIGNMENT_END,
                                XmNtopAttachment,    XmATTACH_FORM,
                                XmNleftAttachment,   XmATTACH_FORM,
                                XmNleftOffset,       10,
                                XmNbottomAttachment, XmATTACH_FORM,
                                NULL);
   class_togglebox_w = XtVaCreateWidget("togglebox",
                                xmRowColumnWidgetClass, criteriabox_w,
                                XmNorientation,         XmHORIZONTAL,
                                XmNpacking,             XmPACK_TIGHT,
                                XmNnumColumns,          1,
                                XmNtopAttachment,       XmATTACH_FORM,
                                XmNleftAttachment,      XmATTACH_WIDGET,
                                XmNleftWidget,          label_w,
                                XmNbottomAttachment,    XmATTACH_FORM,
                                XmNresizable,           False,
                                NULL);
   toggle_w = XtVaCreateManagedWidget("Global",
                                xmToggleButtonGadgetClass, class_togglebox_w,
                                XmNfontList,               fontlist,
                                XmNset,                    (toggles_set & SHOW_CLASS_GLOBAL) ? True : False,
                                NULL);
   XtAddCallback(toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_CLASS_GLOBAL);
   toggle_w = XtVaCreateManagedWidget("Directory",
                                xmToggleButtonGadgetClass, class_togglebox_w,
                                XmNfontList,               fontlist,
                                XmNset,                    (toggles_set & SHOW_CLASS_DIRECTORY) ? True : False,
                                NULL);
   XtAddCallback(toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_CLASS_DIRECTORY);
   toggle_w = XtVaCreateManagedWidget("Production",
                                xmToggleButtonGadgetClass, class_togglebox_w,
                                XmNfontList,               fontlist,
                                XmNset,                    (toggles_set & SHOW_CLASS_PRODUCTION) ? True : False,
                                NULL);
   XtAddCallback(toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_CLASS_PRODUCTION);
   toggle_w = XtVaCreateManagedWidget("Host",
                                xmToggleButtonGadgetClass, class_togglebox_w,
                                XmNfontList,               fontlist,
                                XmNset,                    (toggles_set & SHOW_CLASS_HOST) ? True : False,
                                NULL);
   XtAddCallback(toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_CLASS_HOST);
   XtManageChild(class_togglebox_w);
   XtManageChild(criteriabox_w);


/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       criteriabox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(mainform_w, "separator", args, argcount);
   XtManageChild(separator_w);

/*-----------------------------------------------------------------------*/
/*                         Criteria Box 2                                */
/*                         --------------                                */
/* Let user select the event type (External, Manual and Auto) and        */
/* event action.                                                         */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for criteria box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   criteriabox_w = XmCreateForm(mainform_w, "criteriabox", args, argcount);

/*-----------------------------------------------------------------------*/
/*                           Toggle Box                                  */
/*                           ----------                                  */
/* Let user select the event type: External, Manual and/or Auto.         */
/*-----------------------------------------------------------------------*/
   label_w = XtVaCreateManagedWidget("Event Type :",
                                xmLabelGadgetClass,  criteriabox_w,
                                XmNfontList,         fontlist,
                                XmNalignment,        XmALIGNMENT_END,
                                XmNtopAttachment,    XmATTACH_FORM,
                                XmNleftAttachment,   XmATTACH_FORM,
                                XmNleftOffset,       10,
                                XmNbottomAttachment, XmATTACH_FORM,
                                NULL);
   type_togglebox_w = XtVaCreateWidget("togglebox",
                                xmRowColumnWidgetClass, criteriabox_w,
                                XmNorientation,         XmHORIZONTAL,
                                XmNpacking,             XmPACK_TIGHT,
                                XmNnumColumns,          1,
                                XmNtopAttachment,       XmATTACH_FORM,
                                XmNleftAttachment,      XmATTACH_WIDGET,
                                XmNleftWidget,          label_w,
                                XmNbottomAttachment,    XmATTACH_FORM,
                                XmNresizable,           False,
                                NULL);
   toggle_w = XtVaCreateManagedWidget("External",
                                xmToggleButtonGadgetClass, type_togglebox_w,
                                XmNfontList,               fontlist,
                                XmNset,                    True,
                                NULL);
   XtAddCallback(toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_TYPE_EXTERNAL);
   toggle_w = XtVaCreateManagedWidget("Manual",
                                xmToggleButtonGadgetClass, type_togglebox_w,
                                XmNfontList,               fontlist,
                                XmNset,                    True,
                                NULL);
   XtAddCallback(toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_TYPE_MANUAL);
   toggle_w = XtVaCreateManagedWidget("Auto",
                                xmToggleButtonGadgetClass, type_togglebox_w,
                                XmNfontList,               fontlist,
                                XmNset,                    True,
                                NULL);
   XtAddCallback(toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_TYPE_AUTO);
   XtManageChild(type_togglebox_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       type_togglebox_w);
   argcount++;
   separator_w = XmCreateSeparator(criteriabox_w, "separator", args, argcount);
   XtManageChild(separator_w);

   button_w = XtVaCreateManagedWidget("Event actions",
                        xmPushButtonWidgetClass, criteriabox_w,
                        XmNfontList,             fontlist,
                        XmNtopAttachment,        XmATTACH_FORM,
                        XmNrightAttachment,      XmATTACH_FORM,
                        XmNrightOffset,          10,
                        XmNbottomAttachment,     XmATTACH_FORM,
                        NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)select_event_actions, (XtPointer)0);

   XtManageChild(criteriabox_w);


/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       criteriabox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   separator_w = XmCreateSeparator(mainform_w, "separator", args, argcount);
   XtManageChild(separator_w);

/*-----------------------------------------------------------------------*/
/*                           Heading Box                                 */
/*                           -----------                                 */
/* Shows a heading for the list box.                                     */
/*-----------------------------------------------------------------------*/
   headingbox_w = XtVaCreateWidget("headingbox",
                           xmTextWidgetClass,        mainform_w,
                           XmNfontList,              fontlist,
                           XmNleftAttachment,        XmATTACH_FORM,
                           XmNleftOffset,            5,
                           XmNrightAttachment,       XmATTACH_FORM,
                           XmNrightOffset,           20,
                           XmNtopAttachment,         XmATTACH_WIDGET,
                           XmNtopWidget,             separator_w,
                           XmNmarginHeight,          1,
                           XmNmarginWidth,           2,
                           XmNshadowThickness,       1,
                           XmNrows,                  1,
                           XmNeditable,              False,
                           XmNcursorPositionVisible, False,
                           XmNhighlightThickness,    0,
                           XmNcolumns,               MAX_OUTPUT_LINE_LENGTH,
                           NULL);
   XtManageChild(headingbox_w);

/*-----------------------------------------------------------------------*/
/*                            Button Box                                 */
/*                            ----------                                 */
/* The status of the output log is shown here. If eg. no files are found */
/* it will be shown here.                                                */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase, 31);
   argcount++;
   buttonbox_w = XmCreateForm(mainform_w, "buttonbox", args, argcount);
   special_button_w = XtVaCreateManagedWidget("Search",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,             fontlist,
                        XmNtopAttachment,        XmATTACH_POSITION,
                        XmNtopPosition,          1,
                        XmNleftAttachment,       XmATTACH_POSITION,
                        XmNleftPosition,         1,
                        XmNrightAttachment,      XmATTACH_POSITION,
                        XmNrightPosition,        10,
                        XmNbottomAttachment,     XmATTACH_POSITION,
                        XmNbottomPosition,       30,
                        NULL);
   XtAddCallback(special_button_w, XmNactivateCallback,
                 (XtCallbackProc)search_button, (XtPointer)0);
   print_button_w = XtVaCreateManagedWidget("Print",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,             fontlist,
                        XmNtopAttachment,        XmATTACH_POSITION,
                        XmNtopPosition,          1,
                        XmNleftAttachment,       XmATTACH_POSITION,
                        XmNleftPosition,         11,
                        XmNrightAttachment,      XmATTACH_POSITION,
                        XmNrightPosition,        20,
                        XmNbottomAttachment,     XmATTACH_POSITION,
                        XmNbottomPosition,       30,
                        NULL);
   XtAddCallback(print_button_w, XmNactivateCallback,
                 (XtCallbackProc)print_button, (XtPointer)1);
   button_w = XtVaCreateManagedWidget("Close",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,             fontlist,
                        XmNtopAttachment,        XmATTACH_POSITION,
                        XmNtopPosition,          1,
                        XmNleftAttachment,       XmATTACH_POSITION,
                        XmNleftPosition,         21,
                        XmNrightAttachment,      XmATTACH_POSITION,
                        XmNrightPosition,        30,
                        XmNbottomAttachment,     XmATTACH_POSITION,
                        XmNbottomPosition,       30,
                        NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)close_button, (XtPointer)0);
   XtManageChild(buttonbox_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
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
   separator_w = XmCreateSeparator(mainform_w, "separator", args, argcount);
   XtManageChild(separator_w);

/*-----------------------------------------------------------------------*/
/*                            Status Box                                 */
/*                            ----------                                 */
/* The status of the output log is shown here. If eg. no files are found */
/* it will be shown here.                                                */
/*-----------------------------------------------------------------------*/
   statusbox_w = XtVaCreateManagedWidget(" ",
                           xmLabelWidgetClass,  mainform_w,
                           XmNfontList,         fontlist,
                           XmNleftAttachment,   XmATTACH_FORM,
                           XmNrightAttachment,  XmATTACH_FORM,
                           XmNbottomAttachment, XmATTACH_WIDGET,
                           XmNbottomWidget,     separator_w,
                           NULL);

/*-----------------------------------------------------------------------*/
/*                            Output Box                                 */
/*                            ----------                                 */
/* This scrolled text widget shows the contents of the event log,        */
/* either in short or long form. Default is short.                       */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNrows,                   NO_OF_VISIBLE_LINES);
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
   XtSetArg(args[argcount], XmNtopWidget,              headingbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,           statusbox_w);
   argcount++;
   outputbox_w = XmCreateScrolledText(mainform_w, "outputbox", args, argcount);
   XtManageChild(outputbox_w);
   XtManageChild(mainform_w);

   /* Disallow user to change window width. */
   XtVaSetValues(appshell,
                 XmNminWidth, char_width * (MAX_OUTPUT_LINE_LENGTH + 6),
                 XmNmaxWidth, char_width * (MAX_OUTPUT_LINE_LENGTH + 6),
                 NULL);

#ifdef WITH_EDITRES
   XtAddEventHandler(appshell, (EventMask)0, True,
                     (XtEventHandler)_XEditResCheckMessages, NULL);
#endif

   /* Start clock. */
   update_time((XtPointer)currenttime_w, (XtIntervalId)NULL);

   /* Realize all widgets. */
   XtRealizeWidget(appshell);

   /* Set some signal handlers. */
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(WARN_DIALOG, "Failed to set signal handler's for %s : %s",
                 SHOW_QUEUE, strerror(errno));
   }

   /* We want the keyboard focus on the start time. */
   XmProcessTraversal(start_time_w, XmTRAVERSE_CURRENT);

#ifdef _WITH_FANCY_TRAVERSE
   /*
    * Only now my we activate the losing focus callback. If we
    * do it earlier, the start time will always be filled with
    * the current time. This is NOT what we want.
    */
   XtAddCallback(start_time_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)START_TIME);
#endif

   /* Get widget ID of the scrollbar. */
   XtVaGetValues(XtParent(outputbox_w), XmNverticalScrollBar, &scrollbar_w, NULL);
   XtAddCallback(scrollbar_w, XmNdragCallback,
                 (XtCallbackProc)scrollbar_moved, (XtPointer)0);
   XtVaGetValues(buttonbox_w, XmNheight, &button_height, NULL);

   /* Write selected dir and host alias names. */
   XmTextSetString(headingbox_w, heading_line);

   if (no_of_search_dir_alias > 0)
   {
      size_t length;
      char   *str;  

      length = (no_of_search_dir_alias * (MAX_DIR_ALIAS_LENGTH + 2 + 1));
      if ((str = malloc(length)) != NULL)                          
      {
         int i;

         length = 0;
         for (i = 0; i < no_of_search_dir_alias; i++)
         {
            length += sprintf(&str[length], "%s, ", search_dir_alias[i]);
         }
         str[length - 2] = '\0';
         XtVaSetValues(dir_alias_w, XmNvalue, str, NULL);
         free(str);
      }
   }
   if (no_of_search_host_alias > 0)
   {
      size_t length;
      char   *str;  

      length = (no_of_search_host_alias * (MAX_HOSTNAME_LENGTH + 2 + 1));
      if ((str = malloc(length)) != NULL)                          
      {
         int i;

         length = 0;
         for (i = 0; i < no_of_search_host_alias; i++)
         {
            length += sprintf(&str[length], "%s, ", search_host_alias[i]);
         }
         str[length - 2] = '\0';
         XtVaSetValues(host_alias_w, XmNvalue, str, NULL);
         free(str);
      }
   }

   if (toggles_set & SHOW_CLASS_DIRECTORY)
   {
      XtSetSensitive(dir_label_w, True);
      XtSetSensitive(dir_alias_w, True);
   }
   else
   {
      XtSetSensitive(dir_label_w, False);
      XtSetSensitive(dir_alias_w, False);
   }
   if (toggles_set & SHOW_CLASS_HOST)
   {
      XtSetSensitive(host_label_w, True);
      XtSetSensitive(host_alias_w, True);
   }
   else
   {
      XtSetSensitive(host_label_w, False);
      XtSetSensitive(host_alias_w, False);
   }

   /* Get Window for resizing the main window. */
   main_window = XtWindow(appshell);

   /* Start the main event-handling loop. */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++ init_show_elog() +++++++++++++++++++++++++++*/
static void
init_show_elog(int *argc, char *argv[], char *window_title)
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
      (void)strcpy(window_title, "AFD Event Log ");
      if (get_afd_name(&window_title[14]) == INCORRECT)
      {
         if (gethostname(&window_title[14], MAX_AFD_NAME_LENGTH) == 0)
         {
            window_title[14] = toupper((int)window_title[14]);
         }
      }
   }
   else
   {
      (void)snprintf(window_title, 14 + 40 + 1, "AFD Event Log %s", font_name);
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
   toggles_set = SHOW_TYPE_EXTERNAL | SHOW_TYPE_MANUAL | SHOW_TYPE_AUTO;
   if (get_arg_array(argc, argv, "-d", &search_dir_alias,
                     &no_of_search_dir_alias) == INCORRECT)
   {
      no_of_search_dir_alias = 0;
   }
   else
   {
      toggles_set |= SHOW_CLASS_DIRECTORY;
   }
   if (get_arg_array(argc, argv, "-h", &search_host_alias,
                     &no_of_search_host_alias) == INCORRECT)
   {
      no_of_search_host_alias = 0;
   }
   else
   {
      toggles_set |= SHOW_CLASS_HOST;
   }

   if ((no_of_search_dir_alias == 0) && (no_of_search_host_alias == 0))
   {
      toggles_set |= SHOW_CLASS_GLOBAL | SHOW_CLASS_DIRECTORY | \
                     SHOW_CLASS_PRODUCTION | SHOW_CLASS_HOST;
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

      case SUCCESS : /* Lets evaluate the permissions and see what */
                     /* the user may do.                           */
         eval_permissions(perm_buffer);
         free(perm_buffer);
         break;

      case INCORRECT : /* Hmm. Something did go wrong. Since we want to */
                       /* be able to disable permission checking let    */
                       /* the user have all permissions.                */
         perm.view_passwd  = NO;
         perm.view_data    = NO;
         perm.list_limit   = NO_LIMIT;
         break;

      default :
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   get_user(user, fake_user, user_offset);
   start_time_val = -1;
   end_time_val = -1;
   special_button_flag = SEARCH_BUTTON;
   no_of_log_files = 0;
   search_add_info[0] = '*';
   search_add_info[1] = '\0';

   /* So that the directories are created with the correct */
   /* permissions (see man 2 mkdir), we need to set umask  */
   /* to zero.                                             */
   umask(0);

   (void)sprintf(heading_line, "dd.mm.yyyy HH:MM:SS C T %-*s %-*s %-*s",
                 MAX_ALIAS_LENGTH, "Alias",
                 (int)MAX_EVENT_ACTION_LENGTH, "Action",
                 ADDITIONAL_INFO_LENGTH,
                 "Additional information");
   (void)memset(sum_sep_line, '=', MAX_OUTPUT_LINE_LENGTH - 1);
   sum_sep_line[MAX_OUTPUT_LINE_LENGTH] = '\0';

   /* Get the maximum number of event logfiles. */
   max_event_log_files = MAX_EVENT_LOG_FILES;
   get_max_log_values(&max_event_log_files, MAX_EVENT_LOG_FILES_DEF,
                      MAX_EVENT_LOG_FILES, NULL, NULL, 0, AFD_CONFIG_FILE);

   ea_toggles_set_1 = (1 << EA_REREAD_DIR_CONFIG) |
                      (1 << EA_REREAD_HOST_CONFIG) |
                      (1 << EA_REREAD_RENAME_RULE) |
                      (1 << EA_AFD_CONFIG_CHANGE) |
                      (1 << EA_ENABLE_RETRIEVE) |
                      (1 << EA_DISABLE_RETRIEVE) |
                      (1 << EA_ENABLE_ARCHIVE) |
                      (1 << EA_DISABLE_ARCHIVE) |
                      (1 << EA_ENABLE_CREATE_TARGET_DIR) |
                      (1 << EA_DISABLE_CREATE_TARGET_DIR) |
                      (1 << EA_ENABLE_DIR_WARN_TIME) |
                      (1 << EA_DISABLE_DIR_WARN_TIME) |
                      (1 << EA_AMG_STOP) |
                      (1 << EA_AMG_START) |
                      (1 << EA_FD_STOP) |
                      (1 << EA_FD_START) |
                      (1 << EA_AFD_STOP) |
                      (1 << EA_AFD_START) |
                      (1 << EA_PRODUCTION_ERROR) |
                      (1 << EA_ERROR_START) |
                      (1 << EA_ERROR_END) |
                      (1 << EA_ENABLE_DIRECTORY) |
                      (1 << EA_DISABLE_DIRECTORY) |
                      (1 << EA_RESCAN_DIRECTORY) |
                      (1 << EA_EXEC_ERROR_ACTION_START) |
                      (1 << EA_EXEC_ERROR_ACTION_STOP) |
                      (1 << EA_OFFLINE) |
                      (1 << EA_ACKNOWLEDGE) |
                      (1 << EA_ENABLE_HOST) |
                      (1 << EA_DISABLE_HOST);
   ea_toggles_set_2 = (1 << (EA_START_TRANSFER - EA_DISABLE_HOST)) |
                      (1 << (EA_STOP_TRANSFER - EA_DISABLE_HOST)) |
                      (1 << (EA_START_QUEUE - EA_DISABLE_HOST)) |
                      (1 << (EA_STOP_QUEUE - EA_DISABLE_HOST)) |
                      (1 << (EA_START_ERROR_QUEUE - EA_DISABLE_HOST)) |
                      (1 << (EA_STOP_ERROR_QUEUE - EA_DISABLE_HOST)) |
                      (1 << (EA_SWITCH_HOST - EA_DISABLE_HOST)) |
                      (1 << (EA_RETRY_HOST - EA_DISABLE_HOST)) |
                      (1 << (EA_ENABLE_DEBUG_HOST - EA_DISABLE_HOST)) |
                      (1 << (EA_ENABLE_TRACE_HOST - EA_DISABLE_HOST)) |
                      (1 << (EA_ENABLE_FULL_TRACE_HOST - EA_DISABLE_HOST)) |
                      (1 << (EA_DISABLE_DEBUG_HOST - EA_DISABLE_HOST)) |
                      (1 << (EA_DISABLE_TRACE_HOST - EA_DISABLE_HOST)) |
                      (1 << (EA_DISABLE_FULL_TRACE_HOST - EA_DISABLE_HOST)) |
                      (1 << (EA_UNSET_ACK_OFFL - EA_DISABLE_HOST)) |
                      (1 << (EA_WARN_TIME_SET - EA_DISABLE_HOST)) |
                      (1 << (EA_WARN_TIME_UNSET - EA_DISABLE_HOST)) |
                      (1 << (EA_ENABLE_HOST_WARN_TIME - EA_DISABLE_HOST)) |
                      (1 << (EA_DISABLE_HOST_WARN_TIME - EA_DISABLE_HOST)) |
                      (1 << (EA_ENABLE_DELETE_DATA - EA_DISABLE_HOST)) |
                      (1 << (EA_DISABLE_DELETE_DATA - EA_DISABLE_HOST)) |
                      (1 << (EA_EXEC_WARN_ACTION_START - EA_DISABLE_HOST)) |
                      (1 << (EA_EXEC_WARN_ACTION_STOP - EA_DISABLE_HOST)) |
                      (1 << (EA_EXEC_SUCCESS_ACTION_START - EA_DISABLE_HOST)) |
                      (1 << (EA_EXEC_SUCCESS_ACTION_STOP - EA_DISABLE_HOST)) |
                      (1 << (EA_START_DIRECTORY - EA_DISABLE_HOST)) |
                      (1 << (EA_STOP_DIRECTORY - EA_DISABLE_HOST)) |
                      (1 << (EA_CHANGE_INFO - EA_DISABLE_HOST)) |
                      (1 << (EA_ENABLE_CREATE_SOURCE_DIR - EA_DISABLE_HOST)) |
                      (1 << (EA_DISABLE_CREATE_SOURCE_DIR - EA_DISABLE_HOST));
   ea_toggles_set_3 = (1 << (EA_INFO_TIME_SET - EA_DISABLE_CREATE_SOURCE_DIR)) |
                      (1 << (EA_INFO_TIME_UNSET - EA_DISABLE_CREATE_SOURCE_DIR)) |
                      (1 << (EA_EXEC_INFO_ACTION_START - EA_DISABLE_CREATE_SOURCE_DIR)) |
                      (1 << (EA_EXEC_INFO_ACTION_STOP - EA_DISABLE_CREATE_SOURCE_DIR)) |
                      (1 << (EA_ENABLE_SIMULATE_SEND_MODE - EA_DISABLE_CREATE_SOURCE_DIR)) |
                      (1 << (EA_DISABLE_SIMULATE_SEND_MODE - EA_DISABLE_CREATE_SOURCE_DIR)) |
                      (1 << (EA_ENABLE_SIMULATE_SEND_HOST - EA_DISABLE_CREATE_SOURCE_DIR)) |
                      (1 << (EA_DISABLE_SIMULATE_SEND_HOST - EA_DISABLE_CREATE_SOURCE_DIR)) |
                      (1 << (EA_MODIFY_ERRORS_OFFLINE - EA_DISABLE_CREATE_SOURCE_DIR)) |
                      (1 << (EA_CHANGE_REAL_HOSTNAME - EA_DISABLE_CREATE_SOURCE_DIR));

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
   (void)fprintf(stderr, "Usage : %s [options] -d <alias>[...<alias n>] | -h <alias>[...<alias n>]\n", progname);
   (void)fprintf(stderr,
                 "        Options:\n");
   (void)fprintf(stderr,
                 "           -d <dir alias 1> ... <dir alias n>\n");
   (void)fprintf(stderr,
                 "           -h <host alias 1> ... <host alias n>\n");
   (void)fprintf(stderr,
                 "           -f <font name>\n");
   (void)fprintf(stderr,
                 "           -p <user profile>\n");
   (void)fprintf(stderr,
                 "           -u [<fake user>]\n");
   (void)fprintf(stderr,
                 "           -w <working directory>\n");
   (void)fprintf(stderr,
                 "           --version\n");
   return;
}


/*-------------------------- eval_permissions() -------------------------*/
static void
eval_permissions(char *perm_buffer)
{
   char *ptr;

   /*
    * If we find 'all' right at the beginning, no further evaluation
    * is needed, since the user has all permissions.
    */
   if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
       (perm_buffer[2] == 'l') &&
       ((perm_buffer[3] == '\0') || (perm_buffer[3] == ',') ||
        (perm_buffer[3] == ' ') || (perm_buffer[3] == '\t')))
   {
      perm.list_limit  = NO_LIMIT;
      perm.view_passwd = YES;
      perm.view_data   = YES;
   }
   else
   {
      /*
       * First of all check if the user may use this program
       * at all.
       */
      if ((ptr = posi(perm_buffer, SHOW_ELOG_PERM)) == NULL)
      {
         (void)fprintf(stderr, "%s (%s %d)\n",
                       PERMISSION_DENIED_STR, __FILE__, __LINE__);
         free(perm_buffer);
         exit(INCORRECT);
      }
      else
      {
         /*
          * For future use. Allow to limit for host names
          * as well.
          */
         ptr--;
      }

      /* May he see the password when using info click? */
      if (posi(perm_buffer, VIEW_PASSWD_PERM) == NULL)
      {
         /* The user may NOT view the password. */
         perm.view_passwd = NO;
      }

      /* May he see the data being distributed? */
      if (posi(perm_buffer, VIEW_DATA_PERM) == NULL)
      {
         /* The user may NOT view the data. */
         perm.view_data = NO;
      }

      /* Is there a limit on how many items the user may view? */
      if ((ptr = posi(perm_buffer, LIST_LIMIT)) == NULL)
      {
         /* There is no limit. */
         perm.list_limit = NO_LIMIT;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            char *p_start = ptr + 1,
                 tmp_char;

            ptr++;
            while ((*ptr != ',') && (*ptr != ' ') && (*ptr != '\t') &&
                   (*ptr != '\n') && (*ptr != '\0'))
            {
               ptr++;
            }
            tmp_char = *ptr;
            *ptr = '\0';
            perm.list_limit = atoi(p_start);
            *ptr = tmp_char;
         }
         else
         {
            perm.list_limit = NO_LIMIT;
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
