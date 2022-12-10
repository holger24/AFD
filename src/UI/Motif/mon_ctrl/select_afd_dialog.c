/*
 *  select_afd_dialog.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   select_afd_dialog - searches for an AFD in the mon_ctrl dialog
 **
 ** SYNOPSIS
 **   void select_afd_dialog(Widget    w,
 **                          XtPointer client_data,
 **                          XtPointer call_data)
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
 **   20.12.2003 H.Kiehl Created
 **   21.03.2022 H.Kiehl Added case insensitive search.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>                      /* malloc()                     */
#include <unistd.h>                      /* close()                      */
#include <ctype.h>                       /* tolower()                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                       /* O_RDONLY                     */
#include <sys/mman.h>                    /* munmap()                     */
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
#include "mon_ctrl.h"

/* Global variables. */
Widget                        findshell = (Widget)NULL;

/* External global variables. */
extern Display                *display;
extern Widget                 appshell;
extern int                    no_of_afds,
                              no_of_afds_invisible,
                              no_of_afds_visible,
                              no_selected,
                              no_selected_static,
                              *vpl,
                              window_width;
extern char                   font_name[],
                              *p_work_dir;
extern struct mon_line        *connect_data;
extern struct mon_status_area *msa;

/* Local global variables. */
static Widget                 alias_toggle_w,
                              find_text_w;
static int                    case_sensitive,
                              deselect,
                              name_class,
                              name_type,
                              redraw_counter,
                              *redraw_line,
                              static_select;

/* Local function prototypes. */
static void                   done_button(Widget, XtPointer, XtPointer),
                              draw_selections(void),
                              search_select_afd(Widget, XtPointer, XtPointer),
                              select_callback(Widget, XtPointer, XtPointer),
                              select_line(int);

#define STATIC_SELECT_CB      1
#define DESELECT_CB           2
#define AFD_NAME_CLASS_CB     3
#define HOST_NAME_CLASS_CB    4
#define ALIAS_AFDNAME_CB      5
#define REAL_AFDNAME_CB       6
#define CASE_SENSITIVE_CB     7
#define ALIAS_NAME            1
#define REAL_NAME             2
#define AFD_NAME_CLASS        1
#define HOST_NAME_CLASS       2


/*######################### select_afd_dialog() #########################*/
void
select_afd_dialog(Widget w, XtPointer client_data, XtPointer call_data)
{
   /*
    * First, see if the window has already been created. If
    * no create a new window.
    */
   if ((findshell == (Widget)NULL) || (XtIsRealized(findshell) == False) ||
       (XtIsSensitive(findshell) != True))
   {
      Widget          buttonbox_w,
                      button_w,
                      case_sensitive_toggle_w,
                      criteriabox_w,
                      dialog_w,
                      main_form_w,
                      radiobox_w,
                      separator_w,
                      togglebox_w;
      Arg             args[MAXARGS];
      Cardinal        argcount;
      XmFontList      p_fontlist;
      XmFontListEntry entry;

      findshell = XtVaCreatePopupShell("Search AFD", topLevelShellWidgetClass,
                                       appshell, NULL);

      /* Create managing widget. */
      main_form_w = XmCreateForm(findshell, "main_form", NULL, 0);

      /* Prepare font. */
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
                    (XtCallbackProc)search_select_afd, (XtPointer)0);

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
      /*                        Enter AFD name                         */
      /*---------------------------------------------------------------*/
      dialog_w = XtVaCreateManagedWidget("Search AFD/Host name:",
                        xmLabelGadgetClass,  criteriabox_w,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNleftOffset,       5,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        5,
                        XmNrightAttachment,  XmATTACH_FORM,
                        XmNleftOffset,       2,
                        XmNfontList,         p_fontlist,
                        XmNalignment,        XmALIGNMENT_BEGINNING,
                        NULL);
      find_text_w = XtVaCreateWidget("find_afdname",
                        xmTextWidgetClass,   criteriabox_w,
                        XmNtopAttachment,    XmATTACH_WIDGET,
                        XmNtopWidget,        dialog_w,
                        XmNtopOffset,        5,
                        XmNrightAttachment,  XmATTACH_FORM,
                        XmNrightOffset,      5,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNleftOffset,       5,
                        XmNfontList,         p_fontlist,
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
      case_sensitive_toggle_w = XtVaCreateManagedWidget("Case Sensitive",
                                xmToggleButtonGadgetClass, togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    False,
                                NULL);
      XtAddCallback(case_sensitive_toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)select_callback,
                    (XtPointer)CASE_SENSITIVE_CB);
      XtManageChild(togglebox_w);
      case_sensitive = NO;

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

      togglebox_w = XtVaCreateWidget("togglebox",
                                xmRowColumnWidgetClass, criteriabox_w,
                                XmNorientation,         XmHORIZONTAL,
                                XmNpacking,             XmPACK_TIGHT,
                                XmNnumColumns,          1,
                                XmNtopAttachment,       XmATTACH_WIDGET,
                                XmNtopWidget,           separator_w,
                                XmNleftAttachment,      XmATTACH_WIDGET,
                                XmNleftWidget,          dialog_w,
                                XmNbottomAttachment,    XmATTACH_FORM,
                                XmNresizable,           False,
                                NULL);
      alias_toggle_w = XtVaCreateManagedWidget("Static",
                                xmToggleButtonGadgetClass, togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    False,
                                NULL);
      XtAddCallback(alias_toggle_w, XmNvalueChangedCallback,
                    (XtCallbackProc)select_callback, (XtPointer)STATIC_SELECT_CB);
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
      dialog_w = XtVaCreateWidget("Name :",
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
      radiobox_w = XmCreateRadioBox(criteriabox_w, "radiobox", args, argcount);
      dialog_w = XtVaCreateManagedWidget("AFD",
                                   xmToggleButtonGadgetClass, radiobox_w,
                                   XmNfontList,               p_fontlist,
                                   XmNset,                    True,
                                   NULL);
      XtAddCallback(dialog_w, XmNdisarmCallback,
                    (XtCallbackProc)select_callback, (XtPointer)AFD_NAME_CLASS_CB);
      dialog_w = XtVaCreateManagedWidget("Host",
                                   xmToggleButtonGadgetClass, radiobox_w,
                                   XmNfontList,               p_fontlist,
                                   XmNset,                    False,
                                   NULL);
      XtAddCallback(dialog_w, XmNdisarmCallback,
                    (XtCallbackProc)select_callback, (XtPointer)HOST_NAME_CLASS_CB);
      name_class = AFD_NAME_CLASS;
      XtManageChild(radiobox_w);
      argcount = 0;
      XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNtopWidget,        separator_w);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNleftWidget,       radiobox_w);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNorientation,      XmHORIZONTAL);
      argcount++;
      XtSetArg(args[argcount], XmNpacking,          XmPACK_TIGHT);
      argcount++;
      XtSetArg(args[argcount], XmNnumColumns,       1);
      argcount++;
      radiobox_w = XmCreateRadioBox(criteriabox_w, "radiobox", args, argcount);
      dialog_w = XtVaCreateManagedWidget("Alias",
                                   xmToggleButtonGadgetClass, radiobox_w,
                                   XmNfontList,               p_fontlist,
                                   XmNset,                    True,
                                   NULL);
      XtAddCallback(dialog_w, XmNdisarmCallback,
                    (XtCallbackProc)select_callback, (XtPointer)ALIAS_AFDNAME_CB);
      dialog_w = XtVaCreateManagedWidget("Real",
                                   xmToggleButtonGadgetClass, radiobox_w,
                                   XmNfontList,               p_fontlist,
                                   XmNset,                    False,
                                   NULL);
      XtAddCallback(dialog_w, XmNdisarmCallback,
                    (XtCallbackProc)select_callback, (XtPointer)REAL_AFDNAME_CB);
      name_type = ALIAS_NAME;
      XtManageChild(radiobox_w);
      XtManageChild(criteriabox_w);
      XtManageChild(main_form_w);

      XmFontListFree(p_fontlist);

#ifdef WITH_EDITRES
      XtAddEventHandler(findshell, (EventMask)0, True, _XEditResCheckMessages, NULL);
#endif
   }
   XtPopup(findshell, XtGrabNone);

   /* We want the keyboard focus on the text field. */
   XmProcessTraversal(find_text_w, XmTRAVERSE_CURRENT);

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

      case AFD_NAME_CLASS_CB :
         name_class = AFD_NAME_CLASS;
         break;

      case HOST_NAME_CLASS_CB :
         name_class = HOST_NAME_CLASS;
         break;

      case ALIAS_AFDNAME_CB :
         name_type = ALIAS_NAME;
         break;

      case REAL_AFDNAME_CB :
         name_type = REAL_NAME;
         break;

      case CASE_SENSITIVE_CB :
         if (case_sensitive == YES)
         {
            case_sensitive = NO;
         }
         else
         {
            case_sensitive = YES;
         }
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


/*++++++++++++++++++++++++ search_select_afd() ++++++++++++++++++++++++++*/
static void
search_select_afd(Widget w, XtPointer client_data, XtPointer call_data)
{
   char *p_check,
        *p_store,
        *ptr,
        *text = XmTextGetString(find_text_w),
        *tmp_store = NULL;
   int  i;

   if (case_sensitive == NO)
   {
      ptr = text;
      while (*ptr != '\0')
      {
         *ptr = tolower((int)*ptr);
         ptr++;
      }
      if ((tmp_store = malloc(MAX_REAL_HOSTNAME_LENGTH + 20)) == NULL)
      {
         (void)fprintf(stderr,
                       "ERROR : Failed to malloc() memory : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   redraw_counter = 0;
   if ((redraw_line = malloc((no_of_afds * sizeof(int)))) == NULL)
   {
      (void)fprintf(stderr,
                    "ERROR : Failed to malloc() memory : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (name_class == AFD_NAME_CLASS)
   {
      int match;

      for (i = 0; i < no_of_afds; i++)
      {
         if (name_type == ALIAS_NAME)
         {
            if (case_sensitive == NO)
            {
               ptr = connect_data[i].afd_alias;
               p_check = p_store = tmp_store;
               while (*ptr != '\0')
               {
                  *p_store = tolower((int)*ptr);
                  ptr++; p_store++;
               }
               *p_store = '\0';
            }
            else
            {
               p_check = connect_data[i].afd_alias;
            }
            match = pmatch((text[0] == '\0') ? "*" : text, p_check, NULL);
         }
         else
         {
            if (case_sensitive == NO)
            {
               ptr = msa[i].hostname[0];
               p_check = p_store = tmp_store;
               while (*ptr != '\0')
               {
                  *p_store = tolower((int)*ptr);
                  ptr++; p_store++;
               }
               *p_store = '\0';
            }
            else
            {
               p_check = msa[i].hostname[0];
            }
            match = pmatch((text[0] == '\0') ? "*" : text, p_check, NULL);
            if ((match != 0) && (msa[i].hostname[1][0] != '\0') &&
                (my_strcmp(msa[i].hostname[0], msa[i].hostname[1]) != 0))
            {
               if (case_sensitive == NO)
               {
                  ptr = msa[i].hostname[1];
                  p_check = p_store = tmp_store;
                  while (*ptr != '\0')
                  {
                     *p_store = tolower((int)*ptr);
                     ptr++; p_store++;
                  }
                  *p_store = '\0';
               }
               else
               {
                  p_check = msa[i].hostname[1];
               }
               match = pmatch((text[0] == '\0') ? "*" : text, p_check, NULL);
            }
         }
         if (match == 0)
         {
            select_line(i);
         }
      }
   }
   else
   {
      int                  ahl_fd,
                           j;
      char                 ahl_file[MAX_PATH_LENGTH],
                           *p_ahl_file,
                           *ptr;
#ifdef HAVE_STATX
      struct statx         stat_buf;
#else
      struct stat          stat_buf;
#endif
      struct afd_host_list **ahl;

      p_ahl_file = ahl_file +
                   sprintf(ahl_file, "%s%s%s", p_work_dir, FIFO_DIR, AHL_FILE_NAME);
      if ((ahl = malloc(no_of_afds * sizeof(struct afd_host_list *))) == NULL)
      {
         (void)fprintf(stderr,
                       "ERROR   : Failed to malloc() memory : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      for (i = 0; i < no_of_afds; i++)
      {
         (void)sprintf(p_ahl_file, "%s", msa[i].afd_alias);
#ifdef HAVE_STATX
         if ((statx(0, ahl_file, AT_STATX_SYNC_AS_STAT,
                    STATX_SIZE, &stat_buf) == 0) && (stat_buf.stx_size > 0))
#else
         if ((stat(ahl_file, &stat_buf) == 0) && (stat_buf.st_size > 0))
#endif
         {
            if ((ptr = map_file(ahl_file, &ahl_fd, NULL, &stat_buf,
                                O_RDONLY)) == (caddr_t) -1)
            {
               (void)fprintf(stderr,
                             "ERROR : Failed to mmap() to %s : %s (%s %d)\n",
                             ahl_file, strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            ahl[i] = (struct afd_host_list *)ptr;
            if (close(ahl_fd) == -1)
            {
               (void)fprintf(stderr, "DEBUG : close() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
            }
         }
         else
         {
            ahl[i] = NULL;
         }
      }
      if (name_type == ALIAS_NAME)
      {
         for (i = 0; i < no_of_afds; i++)
         {
            if ((msa[i].rcmd[0] != '\0') && (ahl[i] != NULL))
            {
               for (j = 0; j < msa[i].no_of_hosts; j++)
               {
                  if (case_sensitive == NO)
                  {
                     ptr = ahl[i][j].host_alias;
                     p_check = p_store = tmp_store;
                     while (*ptr != '\0')
                     {
                        *p_store = tolower((int)*ptr);
                        ptr++; p_store++;
                     }
                     *p_store = '\0';
                  }
                  else
                  {
                     p_check = ahl[i][j].host_alias;
                  }
                  if (pmatch((text[0] == '\0') ? "*" : text, p_check, NULL) == 0)
                  {
                     select_line(i);
                  }
               }
            }
         }
      }
      else
      {
         for (i = 0; i < no_of_afds; i++)
         {
            if ((msa[i].rcmd[0] != '\0') && (ahl[i] != NULL))
            {
               for (j = 0; j < msa[i].no_of_hosts; j++)
               {
                  if (ahl[i][j].real_hostname[0][0] != GROUP_IDENTIFIER)
                  {
                     if (case_sensitive == NO)
                     {
                        ptr = ahl[i][j].real_hostname[0];
                        p_check = p_store = tmp_store;
                        while (*ptr != '\0')
                        {
                           *p_store = tolower((int)*ptr);
                           ptr++; p_store++;
                        }
                        *p_store = '\0';
                     }
                     else
                     {
                        p_check = ahl[i][j].real_hostname[0];
                     }
                     if (pmatch((text[0] == '\0') ? "*" : text, p_check, NULL) == 0)
                     {
                        select_line(i);
                     }
                     else if ((ahl[i][j].real_hostname[1][0] != '\0') &&
                              (my_strcmp(ahl[i][j].real_hostname[0], ahl[i][j].real_hostname[1]) != 0))
                          {
                             if (case_sensitive == NO)
                             {
                                ptr = ahl[i][j].real_hostname[1];
                                p_check = p_store = tmp_store;
                                while (*ptr != '\0')
                                {
                                   *p_store = tolower((int)*ptr);
                                   ptr++; p_store++;
                                }
                                *p_store = '\0';
                             }
                             else
                             {
                                p_check = ahl[i][j].real_hostname[1];
                             }
                             if (pmatch((text[0] == '\0') ? "*" : text, p_check, NULL) == 0)
                             {
                                select_line(i);
                             }
                          }
                  }
               }
            }
         }
      }

      /* Now unmap and free everything. */
      for (i = 0; i < no_of_afds; i++)
      {
         if (ahl[i] != NULL)
         {
            (void)sprintf(p_ahl_file, "%s", msa[i].afd_alias);
#ifdef HAVE_STATX
            if (statx(0, ahl_file, AT_STATX_SYNC_AS_STAT,
                      STATX_SIZE, &stat_buf) == 0)
#else
            if (stat(ahl_file, &stat_buf) == 0)
#endif
            {
#ifdef HAVE_STATX
               if (munmap((char *)ahl[i], stat_buf.stx_size) == -1)
#else
               if (munmap((char *)ahl[i], stat_buf.st_size) == -1)
#endif
               {
                  (void)fprintf(stderr,
                                "ERROR : Failed to munmap() from %s : %s (%s %d)\n",
                                ahl_file, strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
            }
         }
      }
      (void)free(ahl);
   }
   draw_selections();
   XFlush(display);
   XtFree(text);
   free(redraw_line);
   redraw_line = NULL;
   if (case_sensitive == NO)
   {
      free(tmp_store);
   }

   return;
}


/*---------------------------- select_line() ----------------------------*/
static void
select_line(int i)
{
   if (connect_data[i].rcmd != '\0')
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
                                   (connect_data[j].rcmd != '\0')); j--)
         {
#ifdef _WITH_DEBUG
            (void)fprintf(stderr, "Opening (%d) %s\n",
                          j, connect_data[j].afd_alias);
#endif
            connect_data[j].plus_minus = PM_OPEN_STATE;
            no_of_afds_visible++;
            no_of_afds_invisible--;
         }
#ifdef _WITH_DEBUG
         (void)fprintf(stderr, "!Opening Group! (%d) %s\n",
                       j, connect_data[j].afd_alias);
#endif
         connect_data[j].plus_minus = PM_OPEN_STATE;
         for (j = redraw_line[i] + 1; ((j < no_of_afds) &&
                                       (connect_data[j].rcmd != '\0')); j++)
         {
#ifdef _WITH_DEBUG
            (void)fprintf(stderr, "Opening (%d) %s\n",
                          j, connect_data[j].afd_alias);
#endif
            connect_data[j].plus_minus = PM_OPEN_STATE;
            no_of_afds_visible++;
            no_of_afds_invisible--;
         }
         redraw_everything = YES;
      }
   }

   if (redraw_everything == YES)
   {
      /* First lets redo the visible position list (vpl). */
      j = 0;
      for (i = 0; i < no_of_afds; i++)
      {
         if ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
             (connect_data[i].rcmd == '\0'))
         {
            vpl[j] = i;
            j++;
         }
      }

      /* Resize and redraw window. */
      if (resize_mon_window() == YES)
      {
         calc_mon_but_coord(window_width);
      }
      redraw_all();
   }
   else
   {
      int x,
          y;

      for (i = 0; i < redraw_counter; i++)
      {
         for (j = 0; j < no_of_afds; j++)
         {
            if (redraw_line[i] == vpl[j])
            {
               locate_xy(j, &x, &y);
               draw_mon_line_status(redraw_line[i], 1, x, y);
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
