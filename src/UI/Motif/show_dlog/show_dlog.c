/*
 *  show_dlog.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_dlog - displays the delete log file from the AFD
 **
 ** SYNOPSIS
 **   show_dlog [--version]
 **                 OR
 **   show_dlog [-w <AFD working directory>] [-f <fontname>] [hostname 1..n]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.02.1998 H.Kiehl Created
 **   17.02.1999 H.Kiehl Multiple recipients.
 **   23.11.2003 H.Kiehl Disallow user to change window width even if
 **                      window manager allows this, but allow to change
 **                      height.
 **   15.06.2005 H.Kiehl Added showing deletions of duplicates.
 **   15.08.2005 H.Kiehl Added Exec type.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>            /* strcpy(), strcat(), strerror()         */
#include <ctype.h>             /* toupper()                              */
#include <signal.h>            /* signal()                               */
#include <stdlib.h>            /* free(), atexit()                       */
#ifdef HAVE_SETPRIORITY
# include <sys/time.h>
# include <sys/resource.h>     /* setpriority()                          */
#endif
#include <sys/types.h>
#include <unistd.h>            /* gethostname()                          */
#include <errno.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/List.h>
#include <Xm/ToggleBG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/Separator.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include "mafd_ctrl.h"
#include "show_dlog.h"
#include "logdefs.h"
#include "permission.h"
#include "version.h"
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif

/* Global variables. */
Display                    *display;
XtAppContext               app;
Widget                     appshell,
                           start_time_w,
                           end_time_w,
                           file_name_w,
                           directory_w,
                           file_length_w,
                           recipient_w,
                           headingbox_w,
                           listbox_w,
                           select_all_button_w,
                           statusbox_w,
                           summarybox_w,
                           scrollbar_w,
                           special_button_w;
Window                     main_window;
XmFontList                 fontlist;
int                        char_width,
                           file_name_toggle_set,
                           file_name_length,
                           fra_fd = -1,
                           fra_id,
                           log_date_length = LOG_DATE_LENGTH,
                           max_delete_log_files = MAX_DELETE_LOG_FILES,
                           max_hostname_length = MAX_HOSTNAME_LENGTH,
                           no_of_active_process = 0,
                           no_of_dirs = 0,
                           no_of_log_files,
                           no_of_search_dirs,
                           no_of_search_dirids,
                           no_of_search_file_names = 0,
                           no_of_search_hosts,
                           *search_dir_length = NULL,
                           special_button_flag,
                           sum_line_length,
                           sys_log_fd = STDERR_FILENO;
unsigned int               all_list_items = 0,
                           *search_dirid = NULL;
XT_PTR_TYPE                dr_toggles_set;
#ifdef HAVE_MMAP
off_t                      fra_size;
#endif
Dimension                  button_height;
time_t                     start_time_val,
                           end_time_val;
size_t                     search_file_size;
char                       *p_work_dir,
                           font_name[40],
                           header_line[MAX_OUTPUT_LINE_LENGTH + SHOW_LONG_FORMAT + 2],
                           multi_search_separator = DEFAULT_MULTI_SEARCH_SEPARATOR,
                           **search_file_name = NULL,
                           **search_dir = NULL,
                           *search_dir_filter = NULL,
                           **search_recipient;
struct item_list           *il;
struct sol_perm            perm;
struct fileretrieve_status *fra = NULL;
struct apps_list           *apps_list;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void                eval_permissions(char *),
#ifdef HAVE_SETPRIORITY
                           get_afd_config_value(void),
#endif
                           init_show_dlog(int *, char **, char *),
                           show_dlog_exit(void),
                           sig_bus(int),
                           sig_segv(int),
                           usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char            ms_label_str[MAX_MS_LABEL_STR_LENGTH],
                   window_title[MAX_WNINDOW_TITLE_LENGTH],
                   work_dir[MAX_PATH_LENGTH],
                   *radio_label[] = {"Short", "Med", "Long"};
   static String   fallback_res[] =
                   {
                      ".show_dlog*background : NavajoWhite2",
                      ".show_dlog.mainform*background : NavajoWhite2",
                      ".show_dlog.mainform*XmText.background : NavajoWhite1",
                      ".show_dlog.mainform*listbox.background : NavajoWhite1",
                      ".show_dlog.mainform.buttonbox*background : PaleVioletRed2",
                      ".show_dlog.mainform.buttonbox*foreground : Black",
                      ".show_dlog.mainform.buttonbox*highlightColor : Black",
                      ".show_dlog.show_info*mwmDecorations : 10",
                      ".show_dlog.show_info*mwmFunctions : 4",
                      ".show_dlog.show_info*background : NavajoWhite2",
                      ".show_dlog.show_info*XmText.background : NavajoWhite1",
                      ".show_dlog.show_info.infoform.buttonbox*background : PaleVioletRed2",
                      ".show_dlog.show_info.infoform.buttonbox*foreground : Black",
                      ".show_dlog.show_info.infoform.buttonbox*highlightColor : Black",
                      ".show_dlog.Print Data*background : NavajoWhite2",
                      ".show_dlog.Print Data*XmText.background : NavajoWhite1",
                      ".show_dlog.Print Data.main_form.buttonbox*background : PaleVioletRed2",
                      ".show_dlog.Print Data.main_form.buttonbox*foreground : Black",
                      ".show_dlog.Print Data.main_form.buttonbox*highlightColor : Black",
                      ".show_dlog.Select Delete Reason.main_form.buttonbox*background : PaleVioletRed2",
                      ".show_dlog.Select Delete Reason.main_form.buttonbox*foreground : Black",
                      ".show_dlog.Select Delete Reason.main_form.buttonbox*highlightColor : Black",
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
                   radio_w,
                   radiobox_w,
                   rowcol_w,
                   selectionbox_w,
                   separator_w,
                   timebox_w;
   Arg             args[MAXARGS];
   Cardinal        argcount;
   XmFontListEntry entry;
   XFontStruct     *font_struct;
   XmFontType      dummy;
   uid_t           euid, /* Effective user ID. */
                   ruid; /* Real user ID. */

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values. */
   p_work_dir = work_dir;
   init_show_dlog(&argc, argv, window_title);
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
/* Start and end time to search ouput log file. If no time is entered it */
/* means we should search through all log files.                         */
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
   rowcol_w = XtVaCreateWidget("rowcol",
                           xmRowColumnWidgetClass, entertime_w,
                           XmNorientation, XmHORIZONTAL,
                           NULL);
   block_w = XmCreateForm(rowcol_w, "rowcol", NULL, 0);
   label_w = XtVaCreateManagedWidget("Start time :",
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
/*                          Criteria Box                                 */
/*                          ------------                                 */
/* Here more search parameters can be entered, such as: file name,       */
/* size of the file, directory from which the file had its origion,      */
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
   criteriabox_w = XmCreateForm(mainform_w, "criteriabox", args, argcount);

   (void)snprintf(ms_label_str, MAX_MS_LABEL_STR_LENGTH,
                  "File name (%c):", multi_search_separator);
   label_w = XtVaCreateManagedWidget(ms_label_str,
                           xmLabelGadgetClass,  criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   51,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     0,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    16,
                           XmNalignment,        XmALIGNMENT_END,
                           NULL);
   file_name_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   51,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    61,
                           NULL);
   XtAddCallback(file_name_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)FILE_NAME_NO_ENTER);
   XtAddCallback(file_name_w, XmNactivateCallback, save_input,
                 (XtPointer)FILE_NAME);

   XtVaCreateManagedWidget("Directory (,):",
                           xmLabelGadgetClass,  criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNalignment,        XmALIGNMENT_END,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      53,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   103,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     0,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    16,
                           NULL);
   directory_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      53,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   103,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    61,
                           NULL);
   XtAddCallback(directory_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)DIRECTORY_NAME_NO_ENTER);
   XtAddCallback(directory_w, XmNactivateCallback, save_input,
                 (XtPointer)DIRECTORY_NAME);

   label_w = XtVaCreateManagedWidget("File size    :",
                           xmLabelGadgetClass,  criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNalignment,        XmALIGNMENT_END,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   51,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     62,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    77,
                           NULL);
   file_length_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   51,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    103,
                           NULL);
   XtAddCallback(file_length_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)FILE_LENGTH_NO_ENTER);
   XtAddCallback(file_length_w, XmNactivateCallback, save_input,
                 (XtPointer)FILE_LENGTH);

   XtVaCreateManagedWidget("Recipient (,):",
                           xmLabelGadgetClass,  criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNalignment,        XmALIGNMENT_END,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      53,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   103,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     62,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    77,
                           NULL);
   recipient_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      53,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   103,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    103,
                           NULL);
   XtAddCallback(recipient_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)RECIPIENT_NAME_NO_ENTER);
   XtAddCallback(recipient_w, XmNactivateCallback, save_input,
                 (XtPointer)RECIPIENT_NAME);
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
/*                          Selection Box                                */
/*                          -------------                                */
/* Let user select the deletion type: AGE, USER or OTHER. It also allows */
/* the user to increase or decrease the file name length.                */
/*-----------------------------------------------------------------------*/
   /* Create managing widget for selection box. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   selectionbox_w = XmCreateForm(mainform_w, "selectionbox", args, argcount);

/*-----------------------------------------------------------------------*/
/*                      Delete Reason Button                             */
/*                      --------------------                             */
/* Let user select the delete reason.                                    */
/*-----------------------------------------------------------------------*/
   button_w = XtVaCreateManagedWidget("Delete reason",
                        xmPushButtonWidgetClass, selectionbox_w,
                        XmNfontList,             fontlist,
                        XmNtopAttachment,        XmATTACH_FORM,
                        XmNleftAttachment,       XmATTACH_FORM,
                        XmNleftOffset,           10,
                        XmNbottomAttachment,     XmATTACH_FORM,
                        NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)select_delete_reason, (XtPointer)0);

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
   XtSetArg(args[argcount], XmNleftWidget,       button_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftOffset,       10);
   argcount++;
   separator_w = XmCreateSeparator(selectionbox_w, "separator",
                                   args, argcount);
   XtManageChild(separator_w);

/*-----------------------------------------------------------------------*/
/*                             Radio Box                                 */
/*                             ---------                                 */
/* To select if the output in the list widget should be in long or short */
/* format. Default is short, since this is the fastest form.             */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,          XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNnumColumns,       1);
   argcount++;
   radiobox_w = XmCreateRadioBox(selectionbox_w, "radiobox", args, argcount);
   radio_w = XtVaCreateManagedWidget(radio_label[0],
                                   xmToggleButtonGadgetClass, radiobox_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    False,
                                   NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)radio_button, (XtPointer)SHOW_SHORT_FORMAT);
   radio_w = XtVaCreateManagedWidget(radio_label[1],
                                   xmToggleButtonGadgetClass, radiobox_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    True,
                                   NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)radio_button, (XtPointer)SHOW_MEDIUM_FORMAT);
   radio_w = XtVaCreateManagedWidget(radio_label[2],
                                   xmToggleButtonGadgetClass, radiobox_w,
                                   XmNfontList,               fontlist,
                                   XmNset,                    False,
                                   NULL);
   XtAddCallback(radio_w, XmNdisarmCallback,
                 (XtCallbackProc)radio_button, (XtPointer)SHOW_LONG_FORMAT);
   XtManageChild(radiobox_w);
   file_name_length = SHOW_MEDIUM_FORMAT;

   XtManageChild(selectionbox_w);

   /* Label radiobox_w. */
   XtVaCreateManagedWidget("File name :",
                        xmLabelGadgetClass,  selectionbox_w,
                        XmNfontList,         fontlist,
                        XmNalignment,        XmALIGNMENT_END,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNrightAttachment,  XmATTACH_WIDGET,
                        XmNrightWidget,      radiobox_w,
                        XmNbottomAttachment, XmATTACH_FORM,
                        NULL);

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
/*                           Heading Box                                 */
/*                           -----------                                 */
/* Shows a heading for the list box.                                     */
/*-----------------------------------------------------------------------*/
   headingbox_w = XtVaCreateWidget("headingbox",
                        xmTextWidgetClass,        mainform_w,
                        XmNfontList,              fontlist,
                        XmNleftAttachment,        XmATTACH_FORM,
                        XmNleftOffset,            2,
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
                        XmNcolumns,               MAX_OUTPUT_LINE_LENGTH + file_name_length + 1,
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
   XtSetArg(args[argcount], XmNfractionBase, 41);
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
                        XmNbottomPosition,       40,
                        NULL);
   XtAddCallback(special_button_w, XmNactivateCallback,
                 (XtCallbackProc)search_button, (XtPointer)0);
   select_all_button_w = XtVaCreateManagedWidget("Select All",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,             fontlist,
                        XmNtopAttachment,        XmATTACH_POSITION,
                        XmNtopPosition,          1,                
                        XmNleftAttachment,       XmATTACH_POSITION,
                        XmNleftPosition,         11,               
                        XmNrightAttachment,      XmATTACH_POSITION,
                        XmNrightPosition,        20,               
                        XmNbottomAttachment,     XmATTACH_POSITION,
                        XmNbottomPosition,       40,               
                        NULL);
   XtAddCallback(select_all_button_w, XmNactivateCallback,
                 (XtCallbackProc)select_all_button, (XtPointer)0);
   button_w = XtVaCreateManagedWidget("Print",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,             fontlist,
                        XmNtopAttachment,        XmATTACH_POSITION,
                        XmNtopPosition,          1,
                        XmNleftAttachment,       XmATTACH_POSITION,
                        XmNleftPosition,         21,
                        XmNrightAttachment,      XmATTACH_POSITION,
                        XmNrightPosition,        30,
                        XmNbottomAttachment,     XmATTACH_POSITION,
                        XmNbottomPosition,       40,
                        NULL);
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)print_button, (XtPointer)0);
   button_w = XtVaCreateManagedWidget("Close",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,             fontlist,
                        XmNtopAttachment,        XmATTACH_POSITION,
                        XmNtopPosition,          1,
                        XmNleftAttachment,       XmATTACH_POSITION,
                        XmNleftPosition,         31,
                        XmNrightAttachment,      XmATTACH_POSITION,
                        XmNrightPosition,        40,
                        XmNbottomAttachment,     XmATTACH_POSITION,
                        XmNbottomPosition,       40,
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
   separator_w = XmCreateSeparator(mainform_w, "separator", args, argcount);
   XtManageChild(separator_w);

/*-----------------------------------------------------------------------*/
/*                           Summary Box                                 */
/*                           -----------                                 */
/* Summary of what has been selected. If none is slected in listbox a    */
/* summary of all items is made.                                         */
/*-----------------------------------------------------------------------*/
   summarybox_w = XtVaCreateManagedWidget(" ",
                           xmLabelWidgetClass,  mainform_w,
                           XmNfontList,         fontlist,
                           XmNleftAttachment,   XmATTACH_FORM,
                           XmNleftOffset,       3,
                           XmNrightAttachment,  XmATTACH_FORM,
                           XmNbottomAttachment, XmATTACH_WIDGET,
                           XmNbottomWidget,     separator_w,
                           NULL);

/*-----------------------------------------------------------------------*/
/*                             List Box                                  */
/*                             --------                                  */
/* This scrolled list widget shows the contents of the output log,       */
/* either in short or long form. Default is short.                       */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,              headingbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,           summarybox_w);
   argcount++;
   XtSetArg(args[argcount], XmNvisibleItemCount,       NO_OF_VISIBLE_LINES);
   argcount++;
   XtSetArg(args[argcount], XmNselectionPolicy,        XmEXTENDED_SELECT);
   argcount++;
   XtSetArg(args[argcount], XmNscrollBarDisplayPolicy, XmSTATIC);
   argcount++;
   XtSetArg(args[argcount], XmNfontList,               fontlist);
   argcount++;
   listbox_w = XmCreateScrolledList(mainform_w, "listbox", args, argcount);
   XtManageChild(listbox_w);
   XtAddEventHandler(listbox_w, ButtonPressMask, False,
                     (XtEventHandler)info_click, (XtPointer)NULL);
   XtAddCallback(listbox_w, XmNextendedSelectionCallback,
                 (XtCallbackProc)item_selection, (XtPointer)0);
   XtManageChild(mainform_w);

   /* Disallow user to change window width. */
   XtVaSetValues(appshell,
                 XmNminWidth, char_width * (MAX_OUTPUT_LINE_LENGTH + file_name_length + 6),
                 XmNmaxWidth, char_width * (MAX_OUTPUT_LINE_LENGTH + file_name_length + 6),
                 NULL);

#ifdef WITH_EDITRES
   XtAddEventHandler(appshell, (EventMask)0, True,
                     _XEditResCheckMessages, NULL);
#endif

   /* Start clock. */
   update_time((XtPointer)currenttime_w, (XtIntervalId)NULL);

   /* Realize all widgets. */
   XtRealizeWidget(appshell);

   /* Set some signal handlers. */
   if ((signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(WARN_DIALOG, "Failed to set signal handler's for %s : %s",
                 SHOW_DLOG, strerror(errno));
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
   XtVaGetValues(XtParent(listbox_w), XmNverticalScrollBar,
                 &scrollbar_w, NULL);
   XtAddCallback(scrollbar_w, XmNdragCallback,
                 (XtCallbackProc)scrollbar_moved, (XtPointer)0);
   XtVaGetValues(buttonbox_w, XmNheight, &button_height, NULL);

   /* Write heading. */
   sum_line_length = sprintf(header_line, "%s%-*s %s%-*s %-*s %-*s",
                             DATE_TIME_HEADER, file_name_length,
                             FILE_NAME_HEADER, FILE_SIZE_HEADER,
                             HOST_NAME_LENGTH, HOST_NAME_HEADER,
                             (int)MAX_REASON_LENGTH, REASON_HEADER,
                             MAX_PROC_USER_LENGTH, PROCESS_USER_HEADER);
   XmTextSetString(headingbox_w, header_line);

   if ((no_of_search_dirs > 0) || (no_of_search_dirids > 0))
   {
      size_t str_length;
      char   *str;

      str_length = (no_of_search_dirs * MAX_PATH_LENGTH) +
                   (no_of_search_dirids * (1 + MAX_INT_HEX_LENGTH + 2)) +
                   ((no_of_search_dirs + no_of_search_dirids) * 2) + 1;
      if ((str = malloc(str_length)) != NULL)
      {
         int    i;
         size_t length = 0;
         char   *ptr;

         for (i = 0; i < no_of_search_dirs; i++)
         {
            length += snprintf(&str[length], str_length - length,
                               "%s, ", search_dir[i]);
            if (length > str_length)
            {
#if SIZEOF_SIZE_T == 4
               (void)fprintf(stderr, "Buffer to small %d > %d (%s %d)\n",
#else
               (void)fprintf(stderr, "Buffer to small %lld > %lld (%s %d)\n",
#endif
                             (pri_size_t)length, (pri_size_t)str_length,
                             __FILE__, __LINE__);
               exit(INCORRECT);
            }
            search_dir_filter[i] = NO;
            ptr = str;
            while (*ptr != '\0')
            {
               if (((*ptr == '?') || (*ptr == '*') || (*ptr == '[')) &&
                   ((ptr == str) || (*(ptr - 1) != '\\')))
               {
                  search_dir_filter[i] = YES;
                  break;
               }
               ptr++;
            }
            if (search_dir_filter[i] == YES)
            {
               search_dir_length[i] = 0;
            }
            else
            {
               search_dir_length[i] = strlen(search_dir[i]);
            }
         }
         for (i = 0; i < no_of_search_dirids; i++)
         {
            length += snprintf(&str[length], str_length - length,
                               "#%x, ", search_dirid[i]);
            if (length > str_length)
            {
#if SIZEOF_SIZE_T == 4
               (void)fprintf(stderr, "Buffer to small %d > %d (%s %d)\n",
#else
               (void)fprintf(stderr, "Buffer to small %lld > %lld (%s %d)\n",
#endif
                             (pri_size_t)length, (pri_size_t)str_length,
                             __FILE__, __LINE__);
               exit(INCORRECT);
            }
         }
         str[length - 2] = '\0';
         XtVaSetValues(directory_w, XmNvalue, str, NULL);
         free(str);
      }
   }
   if (no_of_search_hosts > 0)
   {
      char *str;

      if ((str = malloc(MAX_RECIPIENT_LENGTH * no_of_search_hosts)) != NULL)
      {
         int length,
             i;

         length = sprintf(str, "%s", search_recipient[0]);
         for (i = 1; i < no_of_search_hosts; i++)
         {
            length += sprintf(&str[length], ", %s", search_recipient[i]);
         }
         XtVaSetValues(recipient_w, XmNvalue, str, NULL);
         free(str);
      }
   }

   if (atexit(show_dlog_exit) != 0)
   {
      (void)xrec(WARN_DIALOG, "Failed to set exit handler for %s : %s",
                 SHOW_DLOG, strerror(errno));
   }

   /* Get Window for resizing the main window. */
   main_window = XtWindow(appshell);

   /* Start the main event-handling loop. */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++ init_show_dlog() ++++++++++++++++++++++++++*/
static void
init_show_dlog(int *argc, char *argv[], char *window_title)
{
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
      (void)strcpy(window_title, "Delete Log ");
      if (get_afd_name(&window_title[11]) == INCORRECT)
      {
         if (gethostname(&window_title[11], MAX_AFD_NAME_LENGTH) == 0)
         {
            window_title[11] = toupper((int)window_title[11]);
         }
      }
   }
   else
   {
      (void)snprintf(window_title, MAX_WNINDOW_TITLE_LENGTH,
                     "Delete Log %s", font_name);
   }

   if (get_arg(argc, argv, "-p", profile, MAX_PROFILE_NAME_LENGTH) == INCORRECT)
   {
      profile[0] = '\0';
   }
   if (get_arg(argc, argv, "-f", font_name, 40) == INCORRECT)
   {
      (void)strcpy(font_name, DEFAULT_FONT);
   }
   if (get_arg_int_array(argc, argv, "-d", &search_dirid,
                         &no_of_search_dirids) == INCORRECT)
   {
      no_of_search_dirids = 0;
   }
   if (get_arg_array(argc, argv, "-D", &search_dir,
                     &no_of_search_dirs) == INCORRECT)
   {
      no_of_search_dirs = 0;
   }
   else
   {
      if (no_of_search_dirs > 0)
      {
         if ((search_dir_filter = malloc(no_of_search_dirs)) == NULL)
         {
            (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         if ((search_dir_length = malloc((no_of_search_dirs * sizeof(int)))) == NULL)
         {
            (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
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

      case SUCCESS : /* Lets evaluate the permissions and see what */
                     /* the user may do.                           */
         eval_permissions(perm_buffer);
         free(perm_buffer);
         break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
         perm.view_passwd  = NO;
         perm.list_limit   = NO_LIMIT;
         break;

      default :
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   /* Collect all hostnames. */
   no_of_search_hosts = *argc - 1;
   if (no_of_search_hosts > 0)
   {
      int i = 0;

      RT_ARRAY(search_recipient, no_of_search_hosts,
               (MAX_RECIPIENT_LENGTH + 1), char);
      while (*argc > 1)
      {
         (void)my_strncpy(search_recipient[i], argv[1], MAX_RECIPIENT_LENGTH + 1);
         if (strlen(search_recipient[i]) == MAX_HOSTNAME_LENGTH)
         {
            (void)strcat(search_recipient[i], "*");
         }
         (*argc)--; argv++;
         i++;
      }
   }

   start_time_val = -1;
   end_time_val = -1;
   search_file_size = -1;
   special_button_flag = SEARCH_BUTTON;
   no_of_log_files = 0;
   dr_toggles_set = ((1 << (AGE_OUTPUT + 1)) |
                     (1 << (AGE_INPUT + 1)) |
                     (1 << (USER_DEL + 1)) |
                     (1 << (EXEC_FAILED_DEL + 1)) |
                     (1 << (NO_MESSAGE_FILE_DEL + 1)) |
#ifdef WITH_DUP_CHECK
                     (1 << (DUP_INPUT + 1)) |
                     (1 << (DUP_OUTPUT + 1)) |
#endif
                     (1 << (DEL_UNKNOWN_FILE + 1)) |
                     (1 << (JID_LOOKUP_FAILURE_DEL + 1)) |
                     (1 << (DEL_OLD_LOCKED_FILE + 1)) |
                     (1 << (DEL_QUEUED_FILE + 1)) |
                     (1 << (DELETE_OPTION + 1)) |
                     (1 << (DELETE_STALE_ERROR_JOBS + 1)) |
                     (1 << (CLEAR_STALE_MESSAGES + 1)) |
                     (1 << (FILE_CURRENTLY_TRANSMITTED + 1)) |
                     (1 << (DELETE_UNKNOWN_POOL_DIR + 1)) |
                     (1 << (EXEC_FAILED_STORED + 1)) |
                     (1 << (DELETE_HOST_DISABLED + 1)) |
                     (1 << (CONVERSION_FAILED + 1)) |
                     (1 << (RENAME_OVERWRITE + 1)) |
                     (1 << (RECIPIENT_REJECTED + 1)) |
                     (1 << (MIRROR_REMOVE + 1)) |
                     (1 << (MKDIR_QUEUE_ERROR + 1)) |
                     (1 << (INTERNAL_LINK_FAILED + 1)) |
                     (1 << (DEL_UNREADABLE_FILE + 1)) |
                     (1 << (DEL_UNKNOWN_FILE_GLOB + 1)) |
                     (1 << (DEL_OLD_LOCKED_FILE_GLOB + 1)) |
                     (1 << (DEL_OLD_RLOCKED_FILE_GLOB + 1)) |
                     (1 << (DEL_QUEUED_FILE_GLOB + 1))) + 1;

   /* Get the maximum number of logfiles we keep for history. */
   get_max_log_values(&max_delete_log_files, MAX_DELETE_LOG_FILES_DEF,
                      MAX_DELETE_LOG_FILES, NULL, NULL, 0, AFD_CONFIG_FILE);

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


/*-------------------------------- usage() ------------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "Usage : %s [options] [host name 1..n]\n", progname);
   (void)fprintf(stderr,
                 "        Options:\n");
   (void)fprintf(stderr,
                 "           -d <dir identifier 1> ... <dir identifier n>\n");
   (void)fprintf(stderr,
                 "           -D <directory 1> ... <directory n>\n");
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
      perm.list_limit   = NO_LIMIT;
      perm.view_passwd  = YES;
   }
   else
   {
      /*
       * First of all check if the user may use this program
       * at all.
       */
      if ((ptr = posi(perm_buffer, SHOW_RLOG_PERM)) == NULL)
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
      if ((ptr = posi(perm_buffer, VIEW_PASSWD_PERM)) == NULL)
      {
         /* The user may NOT view the password. */
         perm.view_passwd = NO;
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


/*++++++++++++++++++++++++++ show_dlog_exit() +++++++++++++++++++++++++++*/
static void
show_dlog_exit(void)
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
