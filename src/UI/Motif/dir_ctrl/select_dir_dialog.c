/*
 *  select_dir_dialog.c - Part of AFD, an automatic file distribution program.
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
 **   select_dir_dialog - searches for a directory in the dir_ctrl dialog
 **
 ** SYNOPSIS
 **   void select_dir_dialog(Widget    w,
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
 **   21.03.2022 H.Kiehl Added choice for protocol.
 **
 */
DESCR__E_M3

#include <stdio.h>
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
#include "dir_ctrl.h"

/* Global variables. */
Widget                            findshell = (Widget)NULL;

/* External global variables. */
extern Display                    *display;
extern Widget                     appshell;
extern int                        no_of_dirs,
                                  no_selected,
                                  no_selected_static;
extern char                       font_name[];
extern struct dir_line            *connect_data;
extern struct fileretrieve_status *fra;

/* Local global variables. */
static Widget                     alias_toggle_w,
                                  find_text_w,
                                  proto_togglebox_w;
static int                        deselect,
                                  dirname_type,
                                  static_select;
static XT_PTR_TYPE                toggles_set;

/* Local function prototypes. */
static void                       done_button(Widget, XtPointer, XtPointer),
                                  search_select_dir(Widget, XtPointer, XtPointer),
                                  select_callback(Widget, XtPointer, XtPointer),
                                  toggled(Widget, XtPointer, XtPointer);

#define STATIC_SELECT_CB          1
#define DESELECT_CB               2
#define ALIAS_DIRNAME_CB          3
#define REAL_DIRNAME_CB           4
#define ALIAS_NAME                1
#define REAL_NAME                 2


/*######################### select_dir_dialog() #########################*/
void
select_dir_dialog(Widget w, XtPointer client_data, XtPointer call_data)
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

      findshell = XtVaCreatePopupShell("Search Directory",
                                       topLevelShellWidgetClass,
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
                    (XtCallbackProc)search_select_dir, (XtPointer)0);

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
      /*                        Enter dirname                          */
      /*---------------------------------------------------------------*/
      dialog_w = XtVaCreateManagedWidget("Search dirname:",
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
      find_text_w = XtVaCreateWidget("find_directory",
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
      toggle_w = XtVaCreateManagedWidget("HTTP",
                                xmToggleButtonGadgetClass, proto_togglebox_w,
                                XmNfontList,               p_fontlist,
                                XmNset,                    True,
                                NULL);
      XtAddCallback(toggle_w, XmNvalueChangedCallback,
                 (XtCallbackProc)toggled, (XtPointer)SHOW_HTTP);
      toggle_w = XtVaCreateManagedWidget("LOC",
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
      XtManageChild(proto_togglebox_w);

      toggles_set = SHOW_FTP |
                    SHOW_HTTP |
                    SHOW_SFTP |
                    SHOW_FILE |
                    SHOW_EXEC;

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
      dialog_w = XtVaCreateWidget("Dirname :",
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
      dialog_w = XtVaCreateManagedWidget("Alias",
                                   xmToggleButtonGadgetClass, radiobox_w,
                                   XmNfontList,               p_fontlist,
                                   XmNset,                    True,
                                   NULL);
      XtAddCallback(dialog_w, XmNdisarmCallback,
                    (XtCallbackProc)select_callback, (XtPointer)ALIAS_DIRNAME_CB);
      dialog_w = XtVaCreateManagedWidget("Real",
                                   xmToggleButtonGadgetClass, radiobox_w,
                                   XmNfontList,               p_fontlist,
                                   XmNset,                    False,
                                   NULL);
      XtAddCallback(dialog_w, XmNdisarmCallback,
                    (XtCallbackProc)select_callback, (XtPointer)REAL_DIRNAME_CB);
      dirname_type = ALIAS_NAME;
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

      case ALIAS_DIRNAME_CB :
         dirname_type = ALIAS_NAME;
         break;

      case REAL_DIRNAME_CB :
         dirname_type = REAL_NAME;
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


/*++++++++++++++++++++++++ search_select_dir() ++++++++++++++++++++++++++*/
static void
search_select_dir(Widget w, XtPointer client_data, XtPointer call_data)
{
   char *text = XmTextGetString(find_text_w);
   int  draw_selection,
        i,
        match;

   for (i = 0; i < no_of_dirs; i++)
   {
      if (((fra[i].protocol == LOC) && (toggles_set & SHOW_FILE)) ||
          ((fra[i].protocol == FTP) && (toggles_set & SHOW_FTP)) ||
          ((fra[i].protocol == SFTP) && (toggles_set & SHOW_SFTP)) ||
          ((fra[i].protocol == HTTP) && (toggles_set & SHOW_HTTP)) ||
          ((fra[i].protocol == EXEC) && (toggles_set & SHOW_EXEC)))
      {
         if (dirname_type == ALIAS_NAME)
         {
            match = pmatch((text[0] == '\0') ? "*" : text, connect_data[i].dir_alias, NULL);
         }
         else
         {
            match = pmatch((text[0] == '\0') ? "*" : text, fra[i].url, NULL);
         }
         if (match == 0)
         {
            if (deselect == YES)
            {
               if (connect_data[i].inverse == STATIC)
               {
                  no_selected_static--;
                  draw_selection = YES;
               }
               else if (connect_data[i].inverse == ON)
                    {
                       no_selected--;
                       draw_selection = YES;
                    }
                    else
                    {
                       draw_selection = NO;
                    }
               connect_data[i].inverse = OFF;
            }
            else
            {
               if (static_select == YES)
               {
                  if (connect_data[i].inverse == STATIC)
                  {
                     draw_selection = NO;
                  }
                  else
                  {
                     if (connect_data[i].inverse == ON)
                     {
                        no_selected--;
                     }
                     no_selected_static++;
                     connect_data[i].inverse = STATIC;
                     draw_selection = YES;
                  }
               }
               else
               {
                  if (connect_data[i].inverse == ON)
                  {
                     draw_selection = NO;
                  }
                  else
                  {
                     if (connect_data[i].inverse == STATIC)
                     {
                        no_selected_static--;
                     }
                     no_selected++;
                     connect_data[i].inverse = ON;
                     draw_selection = YES;
                  }
               }
            }
            if (draw_selection == YES)
            {
               draw_dir_line_status(i, 1);
            }
         }
      }
   }
   XFlush(display);
   XtFree(text);

   return;
}


/*++++++++++++++++++++++++++++ done_button() ++++++++++++++++++++++++++++*/
static void
done_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   XtPopdown(findshell);

   return;
}
