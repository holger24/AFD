/*
 *  show_plog.c - Part of AFD, an automatic file distribution program.
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
 **   show_plog - displays the production log file from the AFD
 **
 ** SYNOPSIS
 **   show_plog [--version]
 **                  OR
 **   show_plog [-w <AFD working directory>] [fontname] [hostname 1..n]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.09.2016 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>            /* strcpy(), strcat(), strerror()         */
#include <ctype.h>             /* toupper()                              */
#include <signal.h>            /* signal(), kill()                       */
#include <stdlib.h>            /* malloc(), free(), atexit()             */
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
#include "show_plog.h"
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
                           command_w,
                           cont_togglebox_w,
                           cpu_time_w,
                           directory_w,
                           end_time_w,
                           headingbox_w,
                           job_id_w,
                           listbox_w,
                           new_file_name_w,
                           new_file_size_w,
                           orig_file_name_w,
                           orig_file_size_w,
                           prod_time_w,
                           print_button_w,
                           recipient_w,
                           return_code_w,
                           scrollbar_w,
                           select_all_button_w,
                           selectionbox_w,
                           special_button_w,
                           start_time_w,
                           statusbox_w,
                           summarybox_w;
Window                     main_window;
XmFontList                 fontlist;
int                        char_width,
                           continues_toggle_set,
                           file_name_length,
                           fra_fd = -1,
                           fra_id,
                           items_selected = NO,
                           log_date_length = LOG_DATE_LENGTH,
                           max_hostname_length, /* Not used. */
                           max_production_log_files = MAX_PRODUCTION_LOG_FILES,
                           no_of_active_process = 0,
                           no_of_dirs = 0,
                           no_of_log_files,
                           no_of_search_dirs,
                           no_of_search_dirids,
                           no_of_search_hosts,
                           no_of_search_jobids,
                           no_of_search_new_file_names = 0,
                           no_of_search_orig_file_names = 0,
                           no_of_search_production_cmd,
                           no_of_view_modes,
                           ratio_mode,
                           *search_dir_length = NULL,
                           search_return_code,
                           special_button_flag,
                           sum_line_length,
                           sys_log_fd = STDERR_FILENO;
unsigned int               all_list_items = 0,
                           *search_dirid = NULL,
                           *search_jobid = NULL;
XT_PTR_TYPE                toggles_set = 0;
#ifdef HAVE_MMAP
off_t                      fra_size;
#endif
Dimension                  button_height;
time_t                     start_time_val,
                           end_time_val;
size_t                     search_new_file_size,
                           search_orig_file_size;
double                     search_cpu_time = -1.0,
                           search_prod_time = -1.0;
char                       font_name[40],
                           header_line[MAX_PRODUCTION_LINE_LENGTH + SHOW_LONG_FORMAT + 1 + SHOW_LONG_FORMAT + 1],
                           multi_search_separator = DEFAULT_MULTI_SEARCH_SEPARATOR,
                           *p_work_dir,
                           **search_new_file_name,
                           **search_orig_file_name,
                           **search_production_cmd = NULL,
                           **search_dir = NULL,
                           *search_dir_filter = NULL,
                           search_return_code_str[4],
                           **search_recipient,
                           **search_user;
struct item_list           *il = NULL;
struct sol_perm            perm;
struct fileretrieve_status *fra = NULL;
struct apps_list           *apps_list;
struct view_modes          *vm;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void                eval_permissions(char *),
                           get_afd_config_value(void),
                           init_show_plog(int *, char **, char *),
                           show_plog_exit(void),
                           sig_bus(int),
                           sig_exit(int),
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
                      ".show_plog*background : NavajoWhite2",
                      ".show_plog.mainform*background : NavajoWhite2",
                      ".show_plog.mainform*XmText.background : NavajoWhite1",
                      ".show_plog.mainform*listbox.background : NavajoWhite1",
                      ".show_plog.mainform.buttonbox*background : PaleVioletRed2",
                      ".show_plog.mainform.buttonbox*foreground : Black",
                      ".show_plog.mainform.buttonbox*highlightColor : Black",
                      ".show_plog.show_info*mwmDecorations : 10",
                      ".show_plog.show_info*mwmFunctions : 4",
                      ".show_plog.show_info*background : NavajoWhite2",
                      ".show_plog.show_info*XmText.background : NavajoWhite1",
                      ".show_plog.show_info.infoform.buttonbox*background : PaleVioletRed2",
                      ".show_plog.show_info.infoform.buttonbox*foreground : Black",
                      ".show_plog.show_info.infoform.buttonbox*highlightColor : Black",
                      ".show_plog.Print Data*background : NavajoWhite2",
                      ".show_plog.Print Data*XmText.background : NavajoWhite1",
                      ".show_plog.Print Data.main_form.buttonbox*background : PaleVioletRed2",
                      ".show_plog.Print Data.main_form.buttonbox*foreground : Black",
                      ".show_plog.Print Data.main_form.buttonbox*highlightColor : Black",
                      NULL
                   };
   Widget          block_w,
                   buttonbox_w,
                   button_w,
                   criteriabox_w,
                   currenttime_w,
                   enter_xx_w,
                   label_w,
                   mainform_w,
                   pane_w,
                   radio_w,
                   radiobox_w,
                   ratio_menu_w,
                   rowcol_w,
                   separator_w,
                   timebox_w,
                   toggle_w,
                   xx_togglebox_w;
   Arg             args[MAXARGS];
   Cardinal        argcount;
   XmString        label;
   XmFontListEntry entry;
   XFontStruct     *font_struct;
   XmFontType      dummy;
   uid_t           euid, /* Effective user ID. */
                   ruid; /* Real user ID. */

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values. */
   p_work_dir = work_dir;
   init_show_plog(&argc, argv, window_title);
   get_afd_config_value();

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
         (void)fprintf(stderr,
                       "Failed to seteuid() to %d (from %d) : %s (%s %d)\n",
                       ruid, euid, strerror(errno), __FILE__, __LINE__);
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
            (void)fprintf(stderr,
                          "Failed to seteuid() to %d (from %d) : %s (%s %d)\n",
                          euid, ruid, strerror(errno), __FILE__, __LINE__);
#ifdef WITH_SETUID_PROGS
         }
#endif
      }
   }
   display = XtDisplay(appshell);

#ifdef _X_DEBUG
   XSynchronize(display, 1);
#endif

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
   enter_xx_w = XmCreateForm(timebox_w, "entertime", args, argcount);
   rowcol_w = XtVaCreateWidget("rowcol", xmRowColumnWidgetClass, enter_xx_w,
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
   XtManageChild(enter_xx_w);

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
   XtSetArg(args[argcount], XmNleftWidget,       enter_xx_w);
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
/*                          Criteria Box                                 */
/*                          ------------                                 */
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
   XtSetArg(args[argcount], XmNfractionBase,    208);
   argcount++;
   criteriabox_w = XmCreateForm(mainform_w, "criteriabox", args, argcount);

   (void)snprintf(ms_label_str, MAX_MS_LABEL_STR_LENGTH,
                  "Orig File name (%c):", multi_search_separator);
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
                           XmNrightPosition,    32,
                           XmNalignment,        XmALIGNMENT_END,
                           NULL);
   orig_file_name_w = XtVaCreateManagedWidget("",
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
                           XmNrightPosition,    121,
                           NULL);
   XtAddCallback(orig_file_name_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)ORIG_FILE_NAME_NO_ENTER);
   XtAddCallback(orig_file_name_w, XmNactivateCallback, save_input,
                 (XtPointer)ORIG_FILE_NAME);

   (void)snprintf(ms_label_str, MAX_MS_LABEL_STR_LENGTH,
                  "New File name  (%c):", multi_search_separator);
   label_w = XtVaCreateManagedWidget(ms_label_str,
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
                           XmNrightPosition,    32,
                           NULL);
   new_file_name_w = XtVaCreateManagedWidget("",
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
                           XmNrightPosition,    121,
                           NULL);
   XtAddCallback(new_file_name_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)NEW_FILE_NAME_NO_ENTER);
   XtAddCallback(new_file_name_w, XmNactivateCallback, save_input,
                 (XtPointer)NEW_FILE_NAME);

   label_w = XtVaCreateManagedWidget("Directory      (,):",
                           xmLabelGadgetClass,  criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNalignment,        XmALIGNMENT_END,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      105,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   155,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     0,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    32,
                           NULL);
   directory_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      105,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   155,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    121,
                           NULL);
   XtAddCallback(directory_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)DIRECTORY_NAME_NO_ENTER);
   XtAddCallback(directory_w, XmNactivateCallback, save_input,
                 (XtPointer)DIRECTORY_NAME);

   label_w = XtVaCreateManagedWidget("Command        (,):",
                           xmLabelGadgetClass,  criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNalignment,        XmALIGNMENT_END,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      157,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   207,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     0,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    32,
                           NULL);
   command_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      157,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   207,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    121,
                           NULL);
   XtAddCallback(command_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)COMMAND_NAME_NO_ENTER);
   XtAddCallback(command_w, XmNactivateCallback, save_input,
                 (XtPointer)COMMAND_NAME);

   label_w = XtVaCreateManagedWidget("Orig File Size   :",
                           xmLabelGadgetClass,  criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNalignment,        XmALIGNMENT_END,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   51,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     122,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    153,
                           NULL);
   orig_file_size_w = XtVaCreateManagedWidget("",
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
                           XmNrightPosition,    207,
                           NULL);
   XtAddCallback(orig_file_size_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)ORIG_FILE_SIZE_NO_ENTER);
   XtAddCallback(orig_file_size_w, XmNactivateCallback, save_input,
                 (XtPointer)ORIG_FILE_SIZE);

   label_w = XtVaCreateManagedWidget("New File Size    :",
                           xmLabelGadgetClass,  criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNalignment,        XmALIGNMENT_END,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      53,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   103,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     122,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    153,
                           NULL);
   new_file_size_w = XtVaCreateManagedWidget("",
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
                           XmNrightPosition,    207,
                           NULL);
   XtAddCallback(new_file_size_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)NEW_FILE_SIZE_NO_ENTER);
   XtAddCallback(new_file_size_w, XmNactivateCallback, save_input,
                 (XtPointer)NEW_FILE_SIZE);

   label_w = XtVaCreateManagedWidget("Recipient (,):",
                           xmLabelGadgetClass,  criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNalignment,        XmALIGNMENT_END,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      105,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   155,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     122,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    153,
                           NULL);
   recipient_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      105,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   155,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    207,
                           NULL);
   XtAddCallback(recipient_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)RECIPIENT_NAME_NO_ENTER);
   XtAddCallback(recipient_w, XmNactivateCallback, save_input,
                 (XtPointer)RECIPIENT_NAME);

   (void)snprintf(ms_label_str, MAX_MS_LABEL_STR_LENGTH,
                  "Job ID    (%c):", multi_search_separator);
   label_w = XtVaCreateManagedWidget("Job ID    (,):",
                           xmLabelGadgetClass,  criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNalignment,        XmALIGNMENT_END,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      157,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   207,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     122,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    153,
                           NULL);
   job_id_w = XtVaCreateManagedWidget("",
                           xmTextWidgetClass,   criteriabox_w,
                           XmNfontList,         fontlist,
                           XmNmarginHeight,     1,
                           XmNmarginWidth,      1,
                           XmNshadowThickness,  1,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      157,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   207,
                           XmNleftAttachment,   XmATTACH_WIDGET,
                           XmNleftWidget,       label_w,
                           XmNrightAttachment,  XmATTACH_POSITION,
                           XmNrightPosition,    207,
                           NULL);
   XtAddCallback(job_id_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)JOB_ID_NO_ENTER);
   XtAddCallback(job_id_w, XmNactivateCallback, save_input,
                 (XtPointer)JOB_ID);
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
/*                      Selection Length Box                             */
/*                      --------------------                             */
/* Let user select the length of the file name and if the file name is   */
/* local or remote.                                                      */
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
   selectionbox_w = XmCreateForm(mainform_w, "selectionboxlength",
                                 args, argcount);

/*-----------------------------------------------------------------------*/
/*                             Radio Box                                 */
/*                             ---------                                 */
/* To select if the output in the list widget should be in long or short */
/* format. Default is short, since this is the fastest form.             */
/*-----------------------------------------------------------------------*/

   /* Option menu for ratio. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   xx_togglebox_w = XmCreateForm(selectionbox_w, "option_box", args, argcount);

   argcount = 0;
   XtSetArg(args[argcount], XmNfontList,         fontlist);
   argcount++;
   pane_w = XmCreatePulldownMenu(xx_togglebox_w, "pane", args, argcount);

   label = XmStringCreateLocalized("Ratio");
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
   ratio_menu_w = XmCreateOptionMenu(xx_togglebox_w, "ratio_selection",
                                     args, argcount);
   XtManageChild(ratio_menu_w);
   XmStringFree(label);

   argcount = 0;
   XtSetArg(args[argcount], XmNfontList,         fontlist);
   argcount++;
   XtSetValues(XmOptionLabelGadget(ratio_menu_w), args, argcount);

   /* Add all possible view mode buttons. */
   argcount = 0;
   XtSetArg(args[argcount], XmNfontList, fontlist);
   argcount++;
   button_w = XtCreateManagedWidget("Any", xmPushButtonWidgetClass,
                                    pane_w, args, argcount);
   XtAddCallback(button_w, XmNactivateCallback, set_ratio_mode,
                 (XtPointer)ANY_RATIO);
   button_w = XtCreateManagedWidget("1 - 1", xmPushButtonWidgetClass,
                                    pane_w, args, argcount);
   XtAddCallback(button_w, XmNactivateCallback, set_ratio_mode,
                 (XtPointer)ONE_TO_ONE_RATIO);
   button_w = XtCreateManagedWidget("1 - 0", xmPushButtonWidgetClass,
                                    pane_w, args, argcount);
   XtAddCallback(button_w, XmNactivateCallback, set_ratio_mode,
                 (XtPointer)ONE_TO_NONE_RATIO);
   button_w = XtCreateManagedWidget("1 - n", xmPushButtonWidgetClass,
                                    pane_w, args, argcount);
   XtAddCallback(button_w, XmNactivateCallback, set_ratio_mode,
                 (XtPointer)ONE_TO_N_RATIO);
   button_w = XtCreateManagedWidget("n - 1", xmPushButtonWidgetClass,
                                    pane_w, args, argcount);
   XtAddCallback(button_w, XmNactivateCallback, set_ratio_mode,
                 (XtPointer)N_TO_ONE_RATIO);
   button_w = XtCreateManagedWidget("n - n", xmPushButtonWidgetClass,
                                    pane_w, args, argcount);
   XtAddCallback(button_w, XmNactivateCallback, set_ratio_mode,
                 (XtPointer)N_TO_N_RATIO);

   ratio_mode = ANY_RATIO; /* Default to 'any'. */
   XtManageChild(xx_togglebox_w);

   /* Vertical Separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       xx_togglebox_w);
   argcount++;
   separator_w = XmCreateSeparator(selectionbox_w, "separator", args, argcount);
   XtManageChild(separator_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   enter_xx_w = XmCreateForm(selectionbox_w, "enter_rc", args, argcount);
   rowcol_w = XtVaCreateWidget("rowcol", xmRowColumnWidgetClass, enter_xx_w,
                               XmNorientation, XmHORIZONTAL,
                               NULL);
   block_w = XmCreateForm(rowcol_w, "rowcol", NULL, 0);
   label_w = XtVaCreateManagedWidget("Return Code :",
                           xmLabelGadgetClass,  block_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNleftAttachment,   XmATTACH_FORM,
                           XmNalignment,        XmALIGNMENT_END,
                           NULL);
   return_code_w = XtVaCreateManagedWidget("return_code",
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
                           XmNcolumns,          5,
                           XmNmaxLength,        5,
                           NULL);
   XtAddCallback(return_code_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)RETURN_CODE_NO_ENTER);
   XtAddCallback(return_code_w, XmNactivateCallback, save_input,
                 (XtPointer)RETURN_CODE);
   XtManageChild(block_w);
   XtManageChild(rowcol_w);
   XtManageChild(enter_xx_w);

   /* Vertical Separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       enter_xx_w);
   argcount++;
   separator_w = XmCreateSeparator(selectionbox_w, "separator", args, argcount);
   XtManageChild(separator_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   enter_xx_w = XmCreateForm(selectionbox_w, "enter_pt", args, argcount);
   rowcol_w = XtVaCreateWidget("rowcol", xmRowColumnWidgetClass, enter_xx_w,
                               XmNorientation, XmHORIZONTAL, NULL);
   block_w = XmCreateForm(rowcol_w, "rowcol", NULL, 0);
   label_w = XtVaCreateManagedWidget("Prod time :",
                           xmLabelGadgetClass,  block_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNleftAttachment,   XmATTACH_FORM,
                           XmNalignment,        XmALIGNMENT_END,
                           NULL);
   prod_time_w = XtVaCreateManagedWidget("production_time",
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
                           XmNcolumns,          MAX_DISPLAYED_PROD_TIME + 1,
                           XmNmaxLength,        MAX_DISPLAYED_PROD_TIME + 1,
                           NULL);
   XtAddCallback(prod_time_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)PROD_TIME_NO_ENTER);
   XtAddCallback(prod_time_w, XmNactivateCallback, save_input,
                 (XtPointer)PROD_TIME);
   XtManageChild(block_w);
   XtManageChild(rowcol_w);
   XtManageChild(enter_xx_w);

   /* Vertical Separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       enter_xx_w);
   argcount++;
   separator_w = XmCreateSeparator(selectionbox_w, "separator", args, argcount);
   XtManageChild(separator_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       separator_w);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   enter_xx_w = XmCreateForm(selectionbox_w, "enter_ct", args, argcount);
   rowcol_w = XtVaCreateWidget("rowcol", xmRowColumnWidgetClass, enter_xx_w,
                               XmNorientation, XmHORIZONTAL, NULL);
   block_w = XmCreateForm(rowcol_w, "rowcol", NULL, 0);
   label_w = XtVaCreateManagedWidget("CPU time :",
                           xmLabelGadgetClass,  block_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNleftAttachment,   XmATTACH_FORM,
                           XmNalignment,        XmALIGNMENT_END,
                           NULL);
   cpu_time_w = XtVaCreateManagedWidget("cpu_time",
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
                           XmNcolumns,          MAX_DISPLAYED_PROD_TIME + 1,
                           XmNmaxLength,        MAX_DISPLAYED_PROD_TIME + 1,
                           NULL);
   XtAddCallback(cpu_time_w, XmNlosingFocusCallback, save_input,
                 (XtPointer)CPU_TIME_NO_ENTER);
   XtAddCallback(cpu_time_w, XmNactivateCallback, save_input,
                 (XtPointer)CPU_TIME);
   XtManageChild(block_w);
   XtManageChild(rowcol_w);
   XtManageChild(enter_xx_w);

   /* Vertical Separator */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       enter_xx_w);
   argcount++;
   separator_w = XmCreateSeparator(selectionbox_w, "separator", args, argcount);
   XtManageChild(separator_w);

   /* Label radiobox_w */
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
   label_w = XtVaCreateManagedWidget("File name length:",
                           xmLabelGadgetClass,  selectionbox_w,
                           XmNfontList,         fontlist,
                           XmNalignment,        XmALIGNMENT_END,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNrightAttachment,  XmATTACH_WIDGET,
                           XmNrightWidget,      radiobox_w,
                           XmNbottomAttachment, XmATTACH_FORM,
                           NULL);
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
                           XmNcolumns,               MAX_PRODUCTION_LINE_LENGTH + file_name_length + 1 + file_name_length + 1,
                           NULL);
   XtManageChild(headingbox_w);

/*-----------------------------------------------------------------------*/
/*                            Button Box                                 */
/*                            ----------                                 */
/* The status of the production log is shown here. If eg. no files are   */
/* found it will be shown here.                                          */
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
   print_button_w = XtVaCreateManagedWidget("Print",
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
   XtAddCallback(print_button_w, XmNactivateCallback,
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
/* The status of the production log is shown here. If eg. no files are   */
/* found it will be shown here.                                          */
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
/* This scrolled list widget shows the contents of the production log,   */
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
   XtSetArg(args[argcount], XmNmatchBehavior,          XmNONE);
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
                 XmNminWidth, char_width * (MAX_PRODUCTION_LINE_LENGTH + file_name_length + 1 + file_name_length + 5),
                 XmNmaxWidth, char_width * (MAX_PRODUCTION_LINE_LENGTH + file_name_length + 1 + file_name_length + 5),
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
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(WARN_DIALOG, "Failed to set signal handler's for %s : %s",
                 SHOW_PLOG, strerror(errno));
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
   XtVaGetValues(XtParent(listbox_w), XmNverticalScrollBar, &scrollbar_w, NULL);
   XtAddCallback(scrollbar_w, XmNdragCallback,
                 (XtCallbackProc)scrollbar_moved, (XtPointer)0);
   XtVaGetValues(buttonbox_w, XmNheight, &button_height, NULL);

   /* Write heading. */
   sum_line_length = snprintf(header_line,
                              MAX_PRODUCTION_LINE_LENGTH + SHOW_LONG_FORMAT + 1 + SHOW_LONG_FORMAT + 1,
                              "%s%-*s %*s %-*s %s",
                              DATE_TIME_HEADER,
                              file_name_length, ORIG_FILE_NAME_HEADER,
                              MAX_DISPLAYED_FILE_SIZE, ORIG_FILE_SIZE_HEADER,
                              file_name_length, NEW_FILE_NAME_HEADER,
                              REST_HEADER);
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
      int  str_length = MAX_RECIPIENT_LENGTH * no_of_search_hosts;
      char *str;

      if ((str = malloc(str_length)) != NULL)
      {
         int length,
             i;

         length = snprintf(str, str_length, "%s", search_recipient[0]);
         for (i = 1; i < no_of_search_hosts; i++)
         {
            length += snprintf(&str[length], str_length - length,
                               ", %s", search_recipient[i]);
         }
         XtVaSetValues(recipient_w, XmNvalue, str, NULL);
         free(str);
      }
   }

   if (atexit(show_plog_exit) != 0)
   {
      (void)xrec(WARN_DIALOG, "Failed to set exit handler for %s : %s",
                 SHOW_PLOG, strerror(errno));            
   }

   /* Get Window for resizing the main window. */
   main_window = XtWindow(appshell);

   /* Start the main event-handling loop. */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++ init_show_plog() ++++++++++++++++++++++++++*/
static void
init_show_plog(int *argc, char *argv[], char *window_title)
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
   (void)strcpy(window_title, "Production Log ");
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
                     "Production Log %s", font_name);
   }
   if (get_arg(argc, argv, "-p", profile, MAX_PROFILE_NAME_LENGTH) == INCORRECT)
   {
      profile[0] = '\0';
   }
#ifdef WITH_SETUID_PROGS
   set_afd_euid(p_work_dir);
#endif
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
      RT_ARRAY(search_user, no_of_search_hosts,
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
      for (i = 0; i < no_of_search_hosts; i++)
      {
         search_user[i][0] = '\0';
      }
   }

   start_time_val = -1;
   end_time_val = -1;
   search_orig_file_size = -1;
   search_new_file_size = -1;
   search_return_code = -1;
   special_button_flag = SEARCH_BUTTON;
   no_of_log_files = 0;

   /* So that the directories are created with the correct */
   /* permissions (see man 2 mkdir), we need to set umask  */
   /* to zero.                                             */
   umask(0);

   /* Get the maximum number of logfiles we keep for history. */
   get_max_log_values(&max_production_log_files, MAX_PRODUCTION_LOG_FILES_DEF,
                      MAX_PRODUCTION_LOG_FILES, NULL, NULL, 0, AFD_CONFIG_FILE);

   return;
}


/*+++++++++++++++++++++++++ get_afd_config_value() ++++++++++++++++++++++*/
static void
get_afd_config_value(void)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH];

   (void)snprintf(config_file, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_file, F_OK) == 0) &&
       (read_file_no_cr(config_file, &buffer, YES, __FILE__, __LINE__) != INCORRECT))
   {
      char value[MAX_INT_LENGTH];

#ifdef HAVE_SETPRIORITY
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
#endif

      free(buffer);
   }

   return;
}


/*------------------------------- usage() -------------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 "Usage : %s [options] [host name 1..n]\n",
                 progname);
   (void)fprintf(stderr,
                 "        Options:\n");
   (void)fprintf(stderr,
                 "           -d <dir identifier 1> ... <dir identifier n>\n");
   (void)fprintf(stderr,
                 "           -D <directory 1> ... <directory n>\n");
   (void)fprintf(stderr,
                 "           -f <font name>\n");
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
      if ((ptr = posi(perm_buffer, SHOW_PLOG_PERM)) == NULL)
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


/*++++++++++++++++++++++++++ show_plog_exit() +++++++++++++++++++++++++++*/
static void                                                                
show_plog_exit(void)
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
