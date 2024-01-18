/*
 *  dir_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   dir_info - displays information on a single AFD
 **
 ** SYNOPSIS
 **   dir_info [--version] [-w <work dir>] [-f <font name>] -d <directory-name>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.08.2000 H.Kiehl Created
 **   20.07.2001 H.Kiehl Show if queued and/or unknown files are deleted.
 **   22.05.2002 H.Kiehl Separate old file times for unknown and queued files.
 **   27.09.2005 H.Kiehl Updated info to 1.3.x.
 **   14.03.2016 H.Kiehl Added information dialog.
 **
 */
DESCR__E_M1

#include <stdio.h>               /* fopen(), NULL                        */
#include <string.h>              /* strcpy(), strcat()                   */
#include <time.h>                /* strftime(), localtime()              */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>              /* getcwd(), gethostname()              */
#include <sys/mman.h>
#include <stdlib.h>              /* getenv()                             */
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
#include <Xm/LabelG.h>
#include <Xm/Separator.h>
#include <Xm/ScrollBar.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif
#include <errno.h>
#include "dir_info.h"
#include "version.h"
#include "permission.h"

/* Global variables. */
Display                    *display;
XtAppContext               app;
XtIntervalId               interval_id_dir;
Widget                     appshell,
                           dirname_text_w,
#ifdef WITH_DUP_CHECK
                           dup_check_w,
#endif
                           info_w,
                           text_wl[NO_OF_LABELS_PER_ROW],
                           text_wr[NO_OF_LABELS_PER_ROW],
                           label_l_widget[NO_OF_LABELS_PER_ROW],
                           label_r_widget[NO_OF_LABELS_PER_ROW],
                           url_text_w;
XmFontList                 fontlist;
int                        editable = NO,
                           event_log_fd = STDERR_FILENO,
                           fra_pos = -1,
                           fra_id,
                           fra_fd = -1,
                           no_of_dirs,
                           sys_log_fd = STDERR_FILENO,
                           view_passwd;
off_t                      fra_size;
char                       dir_alias[MAX_DIR_ALIAS_LENGTH + 1],
#ifdef WITH_DUP_CHECK
                           dupcheck_label_str[72 + MAX_INT_LENGTH],
#endif
                           font_name[40],
                           *info_data = NULL,
                           *p_work_dir,
                           label_l[NO_OF_LABELS_PER_ROW][22] =
                           {
                              "Alias directory name:",
                              "Store retrieve list :",
                              "Force reread        :",
                              "Accumulate          :",
                              "Delete unknown files:",
                              "Delete queued files :",
                              "Ignore file time    :",
                              "End character       :",
                              "Bytes received      :",
                              "Last retrieval      :"
                           },
                           label_r[NO_OF_LABELS_PER_ROW][22] =
                           {
                              "Directory ID        :",
                              "Remove files (input):",
                              "Wait for filename   :",
                              "Accumulate size     :",
                              "Report unknown files:",
                              "Delete locked files :",
                              "Ignore size         :",
                              "Max copied files    :",
                              "Files received      :",
                              "Next check time     :"
                           },
                           user[MAX_FULL_USER_ID_LENGTH];
struct fileretrieve_status *fra;
struct prev_values         prev;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void                dir_info_exit(void),
                           eval_permissions(char *),
                           init_dir_info(int *, char **),
                           usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int             i;
#ifdef WITH_DUP_CHECK
   size_t          length;
#endif
   char            window_title[100],
                   work_dir[MAX_PATH_LENGTH],
                   str_line[MAX_PATH_LENGTH + 1],
                   tmp_str_line[MAX_PATH_LENGTH + 1];
   static String   fallback_res[] =
                   {
                      "*mwmDecorations : 42",
                      "*mwmFunctions : 12",
                      ".dir_info.form*background : NavajoWhite2",
                      ".dir_info.form.dir_box.?.?.?.text_wl.background : NavajoWhite1",
                      ".dir_info.form.dir_box.?.?.?.text_wr.background : NavajoWhite1",
                      ".dir_info.form.dir_box.?.?.dirname_text_w.background : NavajoWhite1",
                      ".dir_info.form.dir_box.?.?.url_text_w.background : NavajoWhite1",
                      ".dir_info.form.buttonbox*background : PaleVioletRed2",
                      ".dir_info.form.buttonbox*foreground : Black",
                      ".dir_info.form.buttonbox*highlightColor : Black",
                      NULL
                   };
   Widget          form_w,
                   dir_box_w,
                   dir_box1_w,
                   dir_box2_w,
                   dir_text_w,
                   button_w,
                   buttonbox_w,
                   rowcol1_w,
                   rowcol2_w,
                   h_separator1_w,
                   h_separator2_w,
                   v_separator_w;
   XmFontListEntry entry;
   Arg             args[MAXARGS];
   Cardinal        argcount;
   uid_t           euid, /* Effective user ID. */
                   ruid; /* Real user ID. */

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values. */
   p_work_dir = work_dir;
   init_dir_info(&argc, argv);

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

   (void)strcpy(window_title, dir_alias);
   (void)strcat(window_title, " Info");
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
   form_w = XmCreateForm(appshell, "form", NULL, 0);

   entry = XmFontListEntryLoad(XtDisplay(form_w), font_name,
                               XmFONT_IS_FONT, "TAG1");
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

   /*---------------------------------------------------------------*/
   /*          Real directory name and if required URL              */
   /*---------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   dir_box_w = XmCreateForm(form_w, "dir_box", args, argcount);
   XtManageChild(dir_box_w);

   rowcol1_w = XtVaCreateWidget("rowcol1", xmRowColumnWidgetClass,
                                 dir_box_w, NULL);
   dir_text_w = XtVaCreateWidget("dir_text", xmFormWidgetClass,
                                  rowcol1_w,
                                  XmNfractionBase, 41,
                                  NULL);
   XtVaCreateManagedWidget("Real directory name :",
                           xmLabelGadgetClass,  dir_text_w,
                           XmNfontList,         fontlist,
                           XmNtopAttachment,    XmATTACH_POSITION,
                           XmNtopPosition,      1,
                           XmNbottomAttachment, XmATTACH_POSITION,
                           XmNbottomPosition,   40,
                           XmNleftAttachment,   XmATTACH_POSITION,
                           XmNleftPosition,     1,
                           XmNalignment,        XmALIGNMENT_END,
                           NULL);
   dirname_text_w = XtVaCreateManagedWidget("dirname_text_w",
                           xmTextWidgetClass,        dir_text_w,
                           XmNfontList,              fontlist,
                           XmNcolumns,               MAX_DIR_INFO_STRING_LENGTH,
                           XmNtraversalOn,           False,
                           XmNeditable,              False,
                           XmNcursorPositionVisible, False,
                           XmNmarginHeight,          1,
                           XmNmarginWidth,           1,
                           XmNshadowThickness,       1,
                           XmNhighlightThickness,    0,
                           XmNrightAttachment,       XmATTACH_FORM,
                           XmNleftAttachment,        XmATTACH_POSITION,
                           XmNleftPosition,          12,
                           NULL);
   XtManageChild(dir_text_w);
   if (prev.host_alias[0] != '\0')
   {
      dir_text_w = XtVaCreateWidget("dir_text", xmFormWidgetClass,
                                     rowcol1_w,
                                     XmNfractionBase, 41,
                                     NULL);
      XtVaCreateManagedWidget("URL                 :",
                              xmLabelGadgetClass,  dir_text_w,
                              XmNfontList,         fontlist,
                              XmNtopAttachment,    XmATTACH_POSITION,
                              XmNtopPosition,      1,
                              XmNbottomAttachment, XmATTACH_POSITION,
                              XmNbottomPosition,   40,
                              XmNleftAttachment,   XmATTACH_POSITION,
                              XmNleftPosition,     1,
                              XmNalignment,        XmALIGNMENT_END,
                              NULL);
      url_text_w = XtVaCreateManagedWidget("url_text_w",
                              xmTextWidgetClass,        dir_text_w,
                              XmNfontList,              fontlist,
                              XmNcolumns,               MAX_DIR_INFO_STRING_LENGTH,
                              XmNtraversalOn,           False,
                              XmNeditable,              False,
                              XmNcursorPositionVisible, False,
                              XmNmarginHeight,          1,
                              XmNmarginWidth,           1,
                              XmNshadowThickness,       1,
                              XmNhighlightThickness,    0,
                              XmNrightAttachment,       XmATTACH_FORM,
                              XmNleftAttachment,        XmATTACH_POSITION,
                              XmNleftPosition,          12,
                              NULL);
      XtManageChild(dir_text_w);
      (void)sprintf(str_line, "%*s", MAX_DIR_INFO_STRING_LENGTH, prev.display_url);
      XmTextSetString(url_text_w, str_line);
   }
   XtManageChild(rowcol1_w);
   (void)sprintf(str_line, "%s", prev.real_dir_name);
   XmTextSetString(dirname_text_w, str_line);

   /* Create the horizontal separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       dir_box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   h_separator1_w = XmCreateSeparator(form_w, "h_separator1_w", args, argcount);
   XtManageChild(h_separator1_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,      h_separator1_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   dir_box_w = XmCreateForm(form_w, "dir_box", args, argcount);
   XtManageChild(dir_box_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   dir_box1_w = XmCreateForm(dir_box_w, "dir_box1", args, argcount);
   XtManageChild(dir_box1_w);

   rowcol1_w = XtVaCreateWidget("rowcol1", xmRowColumnWidgetClass,
                                 dir_box1_w, NULL);
   for (i = 0; i < NO_OF_LABELS_PER_ROW; i++)
   {
      dir_text_w = XtVaCreateWidget("dir_text", xmFormWidgetClass,
                                     rowcol1_w,
                                     XmNfractionBase, 41,
                                     NULL);
      label_l_widget[i] = XtVaCreateManagedWidget(label_l[i],
                              xmLabelGadgetClass,  dir_text_w,
                              XmNfontList,         fontlist,
                              XmNtopAttachment,    XmATTACH_POSITION,
                              XmNtopPosition,      1,
                              XmNbottomAttachment, XmATTACH_POSITION,
                              XmNbottomPosition,   40,
                              XmNleftAttachment,   XmATTACH_POSITION,
                              XmNleftPosition,     1,
                              XmNalignment,        XmALIGNMENT_END,
                              NULL);
      text_wl[i] = XtVaCreateManagedWidget("text_wl",
                              xmTextWidgetClass,        dir_text_w,
                              XmNfontList,              fontlist,
                              XmNcolumns,               DIR_INFO_LENGTH_L,
                              XmNtraversalOn,           False,
                              XmNeditable,              False,
                              XmNcursorPositionVisible, False,
                              XmNmarginHeight,          1,
                              XmNmarginWidth,           1,
                              XmNshadowThickness,       1,
                              XmNhighlightThickness,    0,
                              XmNrightAttachment,       XmATTACH_FORM,
                              XmNleftAttachment,        XmATTACH_WIDGET,
                              XmNleftWidget,            label_l_widget[i],
                              NULL);
      XtManageChild(dir_text_w);
   }
   XtManageChild(rowcol1_w);

   /* Fill up the text widget with some values. */
   (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, prev.dir_alias);
   XmTextSetString(text_wl[ALIAS_DIR_NAME_POS], str_line);
   if (prev.stupid_mode == YES)
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Yes");
   }
   else if (prev.stupid_mode == NOT_EXACT)
        {
           (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Not exact");
        }
   else if (prev.stupid_mode == GET_ONCE_ONLY)
        {
           (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Once only");
        }
   else if (prev.stupid_mode == GET_ONCE_NOT_EXACT)
        {
           (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Once not exact");
        }
   else if (prev.stupid_mode == APPEND_ONLY)
        {
           (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Append");
        }
        else
        {
           (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "No");
        }
   XmTextSetString(text_wl[STUPID_MODE_POS], str_line);
   if (prev.force_reread == YES)
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Yes");
   }
   else
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "No");
   }
   XmTextSetString(text_wl[FORCE_REREAD_POS], str_line);
   if (prev.accumulate == 0)
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Not set");
   }
   else
   {
      (void)sprintf(str_line, "%*u", DIR_INFO_LENGTH_L, prev.accumulate);
   }
   XmTextSetString(text_wl[ACCUMULATE_POS], str_line);
   if ((prev.delete_files_flag & UNKNOWN_FILES) == 0)
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Not set");
   }
   else
   {
      (void)sprintf(str_line, "%*d", DIR_INFO_LENGTH_L,
                    prev.unknown_file_time / 3600);
   }
   XmTextSetString(text_wl[DELETE_UNKNOWN_POS], str_line);
   if ((prev.delete_files_flag & QUEUED_FILES) == 0)
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Not set");
   }
   else
   {
      (void)sprintf(str_line, "%*d", DIR_INFO_LENGTH_L,
                    prev.queued_file_time / 3600);
   }
   XmTextSetString(text_wl[DELETE_QUEUED_POS], str_line);
   if (prev.ignore_file_time == 0)
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Not set");
   }
   else
   {
      char sign_char,
           str_value[MAX_INT_LENGTH + 1];

      if (prev.gt_lt_sign & IFTIME_LESS_THEN)
      {
         sign_char = '<';
      }
      else if (prev.gt_lt_sign & IFTIME_GREATER_THEN)
           {
              sign_char = '>';
           }
           else
           {
              sign_char = ' ';
           }
      (void)sprintf(str_value, "%c%u", sign_char, prev.ignore_file_time);
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, str_value);
   }
   XmTextSetString(text_wl[IGNORE_FILE_TIME_POS], str_line);
   if (prev.end_character == -1)
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, "Not set");
   }
   else
   {
      (void)sprintf(str_line, "%*d", DIR_INFO_LENGTH_L, prev.end_character);
   }
   XmTextSetString(text_wl[END_CHARACTER_POS], str_line);
#if SIZEOF_OFF_T == 4
   (void)sprintf(str_line, "%*lu", DIR_INFO_LENGTH_L, prev.bytes_received);
#else
   (void)sprintf(str_line, "%*llu", DIR_INFO_LENGTH_L, prev.bytes_received);
#endif
   XmTextSetString(text_wl[BYTES_RECEIVED_POS], str_line);
   (void)strftime(tmp_str_line, MAX_DIR_INFO_STRING_LENGTH, "%d.%m.%Y  %H:%M:%S",
                  localtime(&prev.last_retrieval));
   (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_L, tmp_str_line);
   XmTextSetString(text_wl[LAST_RETRIEVAL_POS], str_line);

   /* Create the horizontal separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       dir_box_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   h_separator1_w = XmCreateSeparator(form_w, "h_separator1_w", args, argcount);
   XtManageChild(h_separator1_w);

   /* Create the vertical separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       dir_box1_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   v_separator_w = XmCreateSeparator(dir_box_w, "v_separator", args, argcount);
   XtManageChild(v_separator_w);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator_w);
   argcount++;
   dir_box2_w = XmCreateForm(dir_box_w, "dir_box2", args, argcount);
   XtManageChild(dir_box2_w);

   rowcol2_w = XtVaCreateWidget("rowcol2", xmRowColumnWidgetClass,
                                dir_box2_w, NULL);
   for (i = 0; i < NO_OF_LABELS_PER_ROW; i++)
   {
      dir_text_w = XtVaCreateWidget("dir_text", xmFormWidgetClass,
                                    rowcol2_w,
                                    XmNfractionBase, 41,
                                    NULL);
      label_r_widget[i] = XtVaCreateManagedWidget(label_r[i],
                              xmLabelGadgetClass,  dir_text_w,
                              XmNfontList,         fontlist,
                              XmNtopAttachment,    XmATTACH_POSITION,
                              XmNtopPosition,      1,
                              XmNbottomAttachment, XmATTACH_POSITION,
                              XmNbottomPosition,   40,
                              XmNleftAttachment,   XmATTACH_POSITION,
                              XmNleftPosition,     1,
                              XmNalignment,        XmALIGNMENT_END,
                              NULL);
      text_wr[i] = XtVaCreateManagedWidget("text_wr",
                              xmTextWidgetClass,        dir_text_w,
                              XmNfontList,              fontlist,
                              XmNcolumns,               DIR_INFO_LENGTH_R,
                              XmNtraversalOn,           False,
                              XmNeditable,              False,
                              XmNcursorPositionVisible, False,
                              XmNmarginHeight,          1,
                              XmNmarginWidth,           1,
                              XmNshadowThickness,       1,
                              XmNhighlightThickness,    0,
                              XmNrightAttachment,       XmATTACH_FORM,
                              XmNleftAttachment,        XmATTACH_WIDGET,
                              XmNleftWidget,            label_r_widget[i],
                              NULL);
      XtManageChild(dir_text_w);
   }
   XtManageChild(rowcol2_w);

   /* Fill up the text widget with some values. */
   (void)sprintf(str_line, "%*x", DIR_INFO_LENGTH_R, prev.dir_id);
   XmTextSetString(text_wr[DIRECTORY_ID_POS], str_line);
   if (prev.remove == YES)
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "Yes");
   }
   else
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "No");
   }
   XmTextSetString(text_wr[REMOVE_FILES_POS], str_line);
   if (prev.wait_for_filename[0] == '\0')
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "Not set");
   }
   else
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, prev.wait_for_filename);
   }
   XmTextSetString(text_wr[WAIT_FOR_FILENAME_POS], str_line);
   if (prev.accumulate_size == 0)
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "Not set");
   }
   else
   {
#if SIZEOF_OFF_T == 4
      (void)sprintf(str_line, "%*ld",
#else
      (void)sprintf(str_line, "%*lld",
#endif
                    DIR_INFO_LENGTH_R, (pri_off_t)prev.accumulate_size);
   }
   XmTextSetString(text_wr[ACCUMULATE_SIZE_POS], str_line);
   if (prev.report_unknown_files == YES)
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "Yes");
   }
   else
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "No");
   }
   XmTextSetString(text_wr[REPORT_UNKNOWN_FILES_POS], str_line);
   if ((prev.delete_files_flag & OLD_LOCKED_FILES) == 0)
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "Not set");
   }
   else
   {
      (void)sprintf(str_line, "%*d", DIR_INFO_LENGTH_R,
                    prev.locked_file_time / 3600);
   }
   XmTextSetString(text_wr[DELETE_LOCKED_FILES_POS], str_line);
   if (prev.ignore_size == -1)
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "Not set");
   }
   else
   {
      char sign_char,
           str_value[MAX_INT_LENGTH + MAX_INT_LENGTH + 1];

      if (prev.gt_lt_sign & ISIZE_LESS_THEN)
      {
         sign_char = '<';
      }
      else if (prev.gt_lt_sign & ISIZE_GREATER_THEN)
           {
              sign_char = '>';
           }
           else
           {
              sign_char = ' ';
           }
#if SIZEOF_OFF_T == 4
       (void)sprintf(str_value, "%c%ld",
#else
       (void)sprintf(str_value, "%c%lld",
#endif
                     sign_char, (pri_off_t)prev.ignore_size);
       (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, str_value);
   }
   XmTextSetString(text_wr[IGNORE_SIZE_POS], str_line);
   (void)sprintf(str_line, "%*u", DIR_INFO_LENGTH_R, prev.max_copied_files);
   XmTextSetString(text_wr[MAX_COPIED_FILES_POS], str_line);
   (void)sprintf(str_line, "%*u", DIR_INFO_LENGTH_R, prev.files_received);
   XmTextSetString(text_wr[FILES_RECEIVED_POS], str_line);
   if (prev.no_of_time_entries > 0)
   {
#if SIZEOF_TIME_T == 4
      if (prev.next_check_time == LONG_MAX)
#else
# ifdef LLONG_MAX
      if (prev.next_check_time == LLONG_MAX)
# else
      if (prev.next_check_time == LONG_MAX)
# endif
#endif
      {
         (void)strcpy(tmp_str_line, "<external>");
      }
      else
      {
         (void)strftime(tmp_str_line, MAX_DIR_INFO_STRING_LENGTH,
                        "%d.%m.%Y  %H:%M:%S", localtime(&prev.next_check_time));
      }
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, tmp_str_line);
   }
   else
   {
      (void)sprintf(str_line, "%*s", DIR_INFO_LENGTH_R, "No time entry.");
   }
   XmTextSetString(text_wr[NEXT_CHECK_TIME_POS], str_line);

#ifdef WITH_DUP_CHECK
   if (prev.dup_check_flag == 0)
   {
      length = sprintf(dupcheck_label_str, "Duplicate check : Not set.");
   }
   else
   {
      length = sprintf(dupcheck_label_str, "Duplicate check : ");
      if (prev.dup_check_flag & DC_FILENAME_ONLY)
      {
         length += sprintf(&dupcheck_label_str[length], "Filename");
      }
      else if (prev.dup_check_flag & DC_FILENAME_AND_SIZE)
           {
              length += sprintf(&dupcheck_label_str[length], "Filename + size");
           }
      else if (prev.dup_check_flag & DC_NAME_NO_SUFFIX)
           {
              length += sprintf(&dupcheck_label_str[length], "Filename no suffix");
           }
      else if (prev.dup_check_flag & DC_FILE_CONTENT)
           {
              length += sprintf(&dupcheck_label_str[length], "File content");
           }
      else if (prev.dup_check_flag & DC_FILE_CONT_NAME)
           {
              length += sprintf(&dupcheck_label_str[length],
                                "File content and name");
           }
           else
           {
              length += sprintf(&dupcheck_label_str[length], "Unknown");
           }
      if (prev.dup_check_flag & DC_CRC32)
      {
         length += sprintf(&dupcheck_label_str[length], ", CRC32");
      }
      else if (prev.dup_check_flag & DC_CRC32C)
           {
              length += sprintf(&dupcheck_label_str[length], ", CRC32C");
           }
      else if (prev.dup_check_flag & DC_MURMUR3)
           {
              length += sprintf(&dupcheck_label_str[length], ", MURMUR3");
           }
           else
           {
              length += sprintf(&dupcheck_label_str[length], "Unknown");
           }
      if ((prev.dup_check_flag & DC_DELETE) || (prev.dup_check_flag & DC_STORE))
      {
         if (prev.dup_check_flag & DC_DELETE)
         {
            length += sprintf(&dupcheck_label_str[length], ", Delete");
         }
         else
         {
            length += sprintf(&dupcheck_label_str[length], ", Store");
         }
         if (prev.dup_check_flag & DC_WARN)
         {
            length += sprintf(&dupcheck_label_str[length], " + Warn");
         }
      }
      else
      {
         if (prev.dup_check_flag & DC_WARN)
         {
            length += sprintf(&dupcheck_label_str[length], ", Warn");
         }
      }
      if (prev.dup_check_timeout != 0)
      {
#if SIZEOF_TIME_T == 4
         length += sprintf(&dupcheck_label_str[length], ", timeout=%ld",
#else
         length += sprintf(&dupcheck_label_str[length], ", timeout=%lld",
#endif
                           (pri_time_t)prev.dup_check_timeout);
      }
   }
   dup_check_w = XtVaCreateManagedWidget(dupcheck_label_str,
                                         xmLabelGadgetClass, form_w,
                                         XmNfontList,        fontlist,
                                         XmNtopAttachment,   XmATTACH_WIDGET,
                                         XmNtopWidget,       h_separator1_w,
                                         XmNleftAttachment,  XmATTACH_FORM,
                                         XmNrightAttachment, XmATTACH_FORM,
                                         NULL);

   /* Create the horizontal separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       dup_check_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   h_separator1_w = XmCreateSeparator(form_w, "h_separator1_w", args, argcount);
   XtManageChild(h_separator1_w);
#endif /* WITH_DUP_CHECK */

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

   /* Create the horizontal separator. */
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
   h_separator2_w = XmCreateSeparator(form_w, "h_separator2_w", args, argcount);
   XtManageChild(h_separator2_w);

   if (editable == YES)
   {
      button_w = XtVaCreateManagedWidget("Save",
                                         xmPushButtonWidgetClass, buttonbox_w,
                                         XmNfontList,         fontlist,
                                         XmNtopAttachment,    XmATTACH_POSITION,
                                         XmNtopPosition,      2,
                                         XmNbottomAttachment, XmATTACH_POSITION,
                                         XmNbottomPosition,   19,
                                         XmNleftAttachment,   XmATTACH_POSITION,
                                         XmNleftPosition,     1,
                                         XmNrightAttachment,  XmATTACH_POSITION,
                                         XmNrightPosition,    9,
                                         NULL);
      XtAddCallback(button_w, XmNactivateCallback, 
                    (XtCallbackProc)save_button, (XtPointer)0);
      button_w = XtVaCreateManagedWidget("Close",
                                         xmPushButtonWidgetClass, buttonbox_w,
                                         XmNfontList,         fontlist,
                                         XmNtopAttachment,    XmATTACH_POSITION,
                                         XmNtopPosition,      2,
                                         XmNbottomAttachment, XmATTACH_POSITION,
                                         XmNbottomPosition,   19,
                                         XmNleftAttachment,   XmATTACH_POSITION,
                                         XmNleftPosition,     10,
                                         XmNrightAttachment,  XmATTACH_POSITION,
                                         XmNrightPosition,    20,
                                         NULL);
   }
   else
   {
      button_w = XtVaCreateManagedWidget("Close",
                                         xmPushButtonWidgetClass, buttonbox_w,
                                         XmNfontList,         fontlist,
                                         XmNtopAttachment,    XmATTACH_POSITION,
                                         XmNtopPosition,      2,
                                         XmNbottomAttachment, XmATTACH_POSITION,
                                         XmNbottomPosition,   19,
                                         XmNleftAttachment,   XmATTACH_POSITION,
                                         XmNleftPosition,     1,
                                         XmNrightAttachment,  XmATTACH_POSITION,
                                         XmNrightPosition,    20,
                                         NULL);
   }
   XtAddCallback(button_w, XmNactivateCallback,
                 (XtCallbackProc)close_button, (XtPointer)0);
   XtManageChild(buttonbox_w);

   /* Create log_text as a ScrolledText window. */
   argcount = 0;
   XtSetArg(args[argcount], XmNfontList,               fontlist);
   argcount++;
   XtSetArg(args[argcount], XmNrows,                   10);
   argcount++;
   XtSetArg(args[argcount], XmNcolumns,                80);
   argcount++;
   if (editable == YES)
   {  
      XtSetArg(args[argcount], XmNeditable,               True);
      argcount++;
      XtSetArg(args[argcount], XmNcursorPositionVisible,  True);
      argcount++;
      XtSetArg(args[argcount], XmNautoShowCursorPosition, True);
   }
   else
   {  
      XtSetArg(args[argcount], XmNeditable,               False);
      argcount++;
      XtSetArg(args[argcount], XmNcursorPositionVisible,  False);
      argcount++;
      XtSetArg(args[argcount], XmNautoShowCursorPosition, False);
   }
   argcount++;
   XtSetArg(args[argcount], XmNeditMode,               XmMULTI_LINE_EDIT);
   argcount++;
   XtSetArg(args[argcount], XmNwordWrap,               False);
   argcount++;
   XtSetArg(args[argcount], XmNscrollHorizontal,       False);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,              h_separator1_w);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,              3);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftOffset,             3);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,            3);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,           h_separator2_w);
   argcount++;
   XtSetArg(args[argcount], XmNbottomOffset,           3);
   argcount++;
   info_w = XmCreateScrolledText(form_w, "host_info", args, argcount);
   XtManageChild(info_w);
   XtManageChild(form_w);

#ifdef WITH_EDITRES
   XtAddEventHandler(appshell, (EventMask)0, True,
                     _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets. */
   XtRealizeWidget(appshell);
   wait_visible(appshell);

   /* Read and display the information file. */
   (void)check_info_file(dir_alias, DIR_INFO_FILE, YES);
   XmTextSetString(info_w, NULL);  /* Clears old entry. */
   XmTextSetString(info_w, info_data);

   /* Call update_info() after UPDATE_INTERVAL ms. */
   interval_id_dir = XtAppAddTimeOut(app, UPDATE_INTERVAL,
                                      (XtTimerCallbackProc)update_info,
                                      form_w);

   /* We want the keyboard focus on the Done button. */
   XmProcessTraversal(button_w, XmTRAVERSE_CURRENT);

   /* Write window ID, so dir_ctrl can set focus if it is called again. */
   write_window_id(XtWindow(appshell), getpid(), DIR_INFO);

   /* Start the main event-handling loop. */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ init_dir_info() ++++++++++++++++++++++++++*/
static void
init_dir_info(int *argc, char *argv[])
{
   int                 count = 0,
                       dnb_fd,
                       i,
                       no_of_dir_names,
                       user_offset;
   char                dir_name_file[MAX_PATH_LENGTH],
                       fake_user[MAX_FULL_USER_ID_LENGTH],
                       *perm_buffer,
                       profile[MAX_PROFILE_NAME_LENGTH + 1],
                       *ptr;
#ifdef HAVE_STATX
   struct statx        stat_buf;
#else
   struct stat         stat_buf;
#endif
   struct dir_name_buf *dnb;

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
   if (get_arg(argc, argv, "-d", dir_alias,
               MAX_DIR_ALIAS_LENGTH + 1) == INCORRECT)
   {
      usage(argv[0]);
      exit(INCORRECT);
   }
   while (count--)
   {
      argc++;
   }

   /* Do not start if binary dataset matches the one stort on disk. */
   if (check_typesize_data(NULL, NULL, NO) > 0)
   {
      (void)fprintf(stderr,
                    "The compiled binary does not match stored database.\n");
      (void)fprintf(stderr,
                    "Initialize database with the command : afd -i\n");
      exit(INCORRECT);
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
         view_passwd  = NO;
         editable = NO;
         break;

      default :
         (void)fprintf(stderr,
                       "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   get_user(user, fake_user, user_offset);

   /* Attach to the FRA. */
   if ((i = fra_attach_passive()) != SUCCESS)
   {
      if (i == INCORRECT_VERSION)
      {
         (void)fprintf(stderr, "This program is not able to attach to the FRA due to incorrect version. (%s %d)\n",
                       __FILE__, __LINE__);
      }
      else
      {
         if (i < 0)
         {
            (void)fprintf(stderr, "Failed to attach to FRA. (%s %d)\n",
                          __FILE__, __LINE__);
         }
         else
         {
            (void)fprintf(stderr, "Failed to attach to FRA : %s (%s %d)\n",
                          strerror(i), __FILE__, __LINE__);
         }
      }
      exit(INCORRECT);
   }
   for (i = 0; i < no_of_dirs; i++)
   {
      if (my_strcmp(fra[i].dir_alias, dir_alias) == 0)
      {
         fra_pos = i;
         break;
      }
   }
   if (fra_pos < 0)
   {
      (void)fprintf(stderr,
                    "WARNING : Could not find directory %s in FRA. (%s %d)\n",
                    dir_alias, __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Map to directory name buffer. */
   (void)sprintf(dir_name_file, "%s%s%s", p_work_dir, FIFO_DIR,
                 DIR_NAME_FILE);
   if ((dnb_fd = open(dir_name_file, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr, "Failed to open() %s : %s (%s %d)\n",
                    dir_name_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   if (statx(dnb_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(dnb_fd, &stat_buf) == -1)
#endif
   {
      (void)fprintf(stderr, "Failed to access %s : %s (%s %d)\n",
                    dir_name_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   if (stat_buf.stx_size > 0)
#else
   if (stat_buf.st_size > 0)
#endif
   {
      if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                      stat_buf.stx_size, PROT_READ,
#else
                      stat_buf.st_size, PROT_READ,
#endif
                      MAP_SHARED, dnb_fd, 0)) == (caddr_t) -1)
      {
         (void)fprintf(stderr, "Failed to mmap() to %s : %s (%s %d)\n",
                       dir_name_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      no_of_dir_names = *(int *)ptr;
      ptr += AFD_WORD_OFFSET;
      dnb = (struct dir_name_buf *)ptr;
   }
   else
   {
      (void)fprintf(stderr, "Job ID database file is empty. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   prev.dir_pos = -1;
   for (i = 0; i < no_of_dir_names; i++)
   {
      if (fra[fra_pos].dir_id == dnb[i].dir_id)
      {
         prev.dir_pos = i;
         break;
      }
   }
   if (prev.dir_pos == -1)
   {
      (void)fprintf(stderr, "Failed to locate dir_id %x in %s. (%s %d)\n",
                    fra[fra_pos].dir_id, dir_name_file,
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialize values in FRA structure. */
   (void)strcpy(prev.real_dir_name, dnb[prev.dir_pos].dir_name);
   (void)strcpy(prev.host_alias, fra[fra_pos].host_alias);
   (void)strcpy(prev.dir_alias, fra[fra_pos].dir_alias);
   (void)strcpy(prev.url, fra[fra_pos].url);
   (void)strcpy(prev.display_url, prev.url);
   if (view_passwd == YES)
   {
      insert_passwd(prev.display_url);
   }
   prev.bytes_received       = fra[fra_pos].bytes_received;
   prev.ignore_size          = fra[fra_pos].ignore_size;
   prev.accumulate_size      = fra[fra_pos].accumulate_size;
   prev.last_retrieval       = fra[fra_pos].last_retrieval;
   prev.next_check_time      = fra[fra_pos].next_check_time;
   prev.dir_id               = fra[fra_pos].dir_id;
   prev.accumulate           = fra[fra_pos].accumulate;
   prev.ignore_file_time     = fra[fra_pos].ignore_file_time;
   prev.gt_lt_sign           = fra[fra_pos].gt_lt_sign;
   prev.files_received       = fra[fra_pos].files_received;
   prev.max_copied_files     = fra[fra_pos].max_copied_files;
   prev.unknown_file_time    = fra[fra_pos].unknown_file_time;
   prev.queued_file_time     = fra[fra_pos].queued_file_time;
   prev.locked_file_time     = fra[fra_pos].locked_file_time;
   prev.end_character        = fra[fra_pos].end_character;
   prev.no_of_time_entries   = fra[fra_pos].no_of_time_entries;
   prev.delete_files_flag    = fra[fra_pos].delete_files_flag;
   prev.stupid_mode          = fra[fra_pos].stupid_mode;
   prev.remove               = fra[fra_pos].remove;
   prev.force_reread         = fra[fra_pos].force_reread;
   prev.report_unknown_files = fra[fra_pos].report_unknown_files;
#ifdef WITH_DUP_CHECK
   prev.dup_check_flag       = fra[fra_pos].dup_check_flag;
   prev.dup_check_timeout    = fra[fra_pos].dup_check_timeout;
#endif

   if (close(dnb_fd) == -1)
   {
      (void)fprintf(stderr, "Failed to close() %s : %s (%s %d)\n",
                    dir_name_file, strerror(errno), __FILE__, __LINE__);
   }
   ptr -= AFD_WORD_OFFSET;
#ifdef HAVE_STATX
   if (munmap(ptr, stat_buf.stx_size) == -1)
#else
   if (munmap(ptr, stat_buf.st_size) == -1)
#endif
   {
      (void)fprintf(stderr, "Failed to munmap() from %s : %s (%s %d)\n",
                    dir_name_file, strerror(errno), __FILE__, __LINE__);
   }
   if (atexit(dir_info_exit) != 0)
   {
      (void)xrec(WARN_DIALOG, "Failed to set exit handler for %s : %s",
                 DIR_INFO, strerror(errno));                            
   }
   check_window_ids(DIR_INFO);

   return;
}


/*-------------------------------- usage() ------------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "Usage : %s [options] -d <dir-alias>\n", progname);
   (void)fprintf(stderr, "           --version\n");
   (void)fprintf(stderr, "           -f <font name>\n");
   (void)fprintf(stderr, "           -u[ <user>]\n");
   (void)fprintf(stderr, "           -w <working directory>\n");
   return;
}

/*-------------------------- eval_permissions() -------------------------*/
static void
eval_permissions(char *perm_buffer)
{
   /*
    * If we find 'all' right at the beginning, no further evaluation
    * is needed, since the user has all permissions.
    */
   if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
       (perm_buffer[2] == 'l') &&
       ((perm_buffer[3] == '\0') || (perm_buffer[3] == ',') ||
        (perm_buffer[3] == ' ') || (perm_buffer[3] == '\t')))
   {
      view_passwd = YES;
      editable = YES;
   }
   else
   {
      /*
       * First of all check if the user may use this program
       * at all.
       */
      if (posi(perm_buffer, DIR_INFO_PERM) == NULL)
      {
         (void)fprintf(stderr, "%s (%s %d)\n",
                       PERMISSION_DENIED_STR, __FILE__, __LINE__);
         free(perm_buffer);
         exit(INCORRECT);
      }

      /* May he see the password? */
      if (posi(perm_buffer, VIEW_PASSWD_PERM) == NULL)
      {
         /* The user may NOT view the password. */
         view_passwd = NO;
      }
      else
      {
         view_passwd = YES;
      }

      /* May the user change the information? */
      if (posi(perm_buffer, EDIT_DIR_INFO_PERM) == NULL)
      {
         /* The user may NOT change the information. */
         editable = NO;
      }
      else
      {
         /* The user may change the information. */
         editable = YES;
      }
   }

   return;
}


/*--------------------------- dir_info_exit() ---------------------------*/
static void
dir_info_exit(void)
{
   remove_window_id(getpid(), DIR_INFO);
   return;
}
