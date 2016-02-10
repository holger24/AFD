/*
 *  show_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2015 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   show_info - displays more detailed information on a single
 **               log entry
 **
 ** SYNOPSIS
 **   void show_info(char *text, int with_search_function)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   16.04.1997 H.Kiehl Created
 **   16.01.1999 H.Kiehl Add scrollbars when list gets too long.
 **   05.01.2011 H.Kiehl Added optional search button.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Text.h>
#include <Xm/Separator.h>
#include <Xm/PushB.h>
#include <X11/keysym.h>
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif
#include <errno.h>
#include "motif_common_defs.h"

/* External global variables. */
extern Display   *display;
extern Widget    appshell;
extern char      font_name[];
extern int       max_x,
                 max_y;
extern Dimension button_height;

/* Local global variables. */
static int       glyph_height,
                 glyph_width;
static Widget    infoshell = (Widget)NULL,
                 searchbox_w,
                 text_w;

/* Local function prototypes. */
static void      close_info_button(Widget, XtPointer, XtPointer),
                 search_button(Widget, XtPointer, XtPointer);


/*############################## show_info() ############################*/
void
show_info(char *text, int with_search_function)
{
   static Window win = (Window)NULL;
   static int    button_lines,
                 max_vertical_lines;

   /*
    * First, see if the window has already been created. If
    * no create a new window.
    */
   if ((infoshell == (Widget)NULL) || (XtIsRealized(infoshell) == False) ||
       (XtIsSensitive(infoshell) != True))
   {
      Widget          form_w,
                      buttonbox_w,
                      button_w,
                      h_separator_w;
      Arg             args[MAXARGS];
      Cardinal        argcount;
      XmFontList      i_fontlist;
      XmFontListEntry entry;
      XFontStruct     *font_struct;
      XmFontType      dummy;

      infoshell = XtVaCreatePopupShell("show_info", topLevelShellWidgetClass,
                                       appshell, NULL);

      /* Create managing widget. */
      form_w = XmCreateForm(infoshell, "infoform", NULL, 0);

      /* Prepare font. */
      if ((entry = XmFontListEntryLoad(XtDisplay(form_w), font_name,
                                       XmFONT_IS_FONT, "TAG1")) == NULL)
      {
         if ((entry = XmFontListEntryLoad(XtDisplay(form_w), DEFAULT_FONT,
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
      glyph_height = font_struct->ascent + font_struct->descent;
      glyph_width  = font_struct->per_char->width;
      i_fontlist = XmFontListAppendEntry(NULL, entry);
      XmFontListEntryFree(&entry);

      if (with_search_function == YES)
      {
         argcount = 0;
         XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
         argcount++;
         XtSetArg(args[argcount], XmNtopOffset,        1);
         argcount++;
         XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
         argcount++;
         XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
         argcount++;
         XtSetArg(args[argcount], XmNfractionBase,     31);
         argcount++;
         buttonbox_w = XmCreateForm(form_w, "buttonbox2", args, argcount);

         searchbox_w = XtVaCreateWidget("searchbox",
                                        xmTextWidgetClass,        buttonbox_w,
                                        XmNtopAttachment,         XmATTACH_POSITION,
                                        XmNtopPosition,           5,
                                        XmNbottomAttachment,      XmATTACH_POSITION,
                                        XmNbottomPosition,        26,
                                        XmNleftAttachment,        XmATTACH_POSITION,
                                        XmNleftPosition,          1,
                                        XmNrightAttachment,       XmATTACH_POSITION,
                                        XmNrightPosition,         20,
                                        XmNfontList,              i_fontlist,
                                        XmNrows,                  1,
                                        XmNeditable,              True,
                                        XmNcursorPositionVisible, True,
                                        XmNmarginHeight,          1,
                                        XmNmarginWidth,           1,
                                        XmNshadowThickness,       1,
                                        XmNhighlightThickness,    0,
                                        NULL);
         XtManageChild(searchbox_w);
         button_w = XtVaCreateManagedWidget("Search",
                                            xmPushButtonWidgetClass, buttonbox_w,
                                            XmNleftAttachment,       XmATTACH_POSITION,
                                            XmNleftPosition,         22,
                                            XmNrightAttachment,      XmATTACH_POSITION,
                                            XmNrightPosition,        28,
                                            XmNtopAttachment,        XmATTACH_FORM,
                                            XmNfontList,             i_fontlist,
                                            NULL);
         XtAddCallback(button_w, XmNactivateCallback,
                       (XtCallbackProc)search_button, (XtPointer)0);
         XtManageChild(buttonbox_w);

         /* Create a horizontal separator. */
         argcount = 0;
         XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
         argcount++;
         XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
         argcount++;
         XtSetArg(args[argcount], XmNtopWidget,       buttonbox_w);
         argcount++;
         XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
         argcount++;
         XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
         argcount++;
         h_separator_w = XmCreateSeparator(form_w, "h_separator",
                                           args, argcount);
         XtManageChild(h_separator_w);

         argcount = 0;
         XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
         argcount++;
         XtSetArg(args[argcount], XmNtopWidget,       h_separator_w);
         argcount++;
         XtSetArg(args[argcount], XmNtopOffset,       1);

         button_lines = 2;
      }
      else
      {
         argcount = 0;
         XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_FORM);

         button_lines = 1;
      }

      /*
       * Lets determine the maximum number of lines that we can
       * display on this screen.
       */
      max_vertical_lines = (8 * (DisplayHeight(display, DefaultScreen(display)) / glyph_height)) / 10;

      /* A small text box. */
      argcount++;
      XtSetArg(args[argcount], XmNcursorPositionVisible, False);
      argcount++;
      XtSetArg(args[argcount], XmNhighlightThickness,    0);
      argcount++;
      XtSetArg(args[argcount], XmNeditable,              False);
      argcount++;
      XtSetArg(args[argcount], XmNeditMode,              XmMULTI_LINE_EDIT);
      argcount++;
      XtSetArg(args[argcount], XmNcolumns,               max_x + 2);
      argcount++;
      if (max_y > max_vertical_lines)
      {
         XtSetArg(args[argcount], XmNrows,                  max_vertical_lines + 2);
      }
      else
      {
         XtSetArg(args[argcount], XmNrows,                  max_y);
      }
      argcount++;
      XtSetArg(args[argcount], XmNscrollVertical,        True);
      argcount++;
      XtSetArg(args[argcount], XmNscrollHorizontal,      False);
      argcount++;
      XtSetArg(args[argcount], XmNfontList,              i_fontlist);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
      argcount++;
      text_w = XmCreateScrolledText(form_w, "info_text", args, argcount);
      XtManageChild(text_w);

      argcount = 0;
      XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNtopWidget,        text_w);
      argcount++;
      XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
      argcount++;
      XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
      argcount++;
      buttonbox_w = XmCreateForm(form_w, "buttonbox", args, argcount);

      /* Create close button. */
      button_w = XtVaCreateManagedWidget("Close",
                        xmPushButtonWidgetClass, buttonbox_w,
                        XmNfontList,             i_fontlist,
                        XmNtopAttachment,        XmATTACH_FORM,
                        XmNleftAttachment,       XmATTACH_FORM,
                        XmNrightAttachment,      XmATTACH_FORM,
                        XmNbottomAttachment,     XmATTACH_FORM,
                        NULL);
      XtAddCallback(button_w, XmNactivateCallback,
                    (XtCallbackProc)close_info_button, (XtPointer)0);
      XtManageChild(buttonbox_w);
      XtManageChild(form_w);

#ifdef WITH_EDITRES
      XtAddEventHandler(infoshell, (EventMask)0, True,
                        _XEditResCheckMessages, NULL);
#endif
   }
   XtPopup(infoshell, XtGrabNone);

   while (win == (Window)NULL)
   {
      win = XtWindow(infoshell);
   }

   /* Display the info text. */
   if (max_y > max_vertical_lines)
   {
      XResizeWindow(display, win,
                    glyph_width * (max_x + 3 + 2),
                    (glyph_height * (max_vertical_lines + 1)) +
                    (button_lines * button_height));
      XtVaSetValues(text_w,
                    XmNcolumns, max_x + 2,
                    XmNrows,    max_vertical_lines,
                    NULL);
   }
   else
   {
      XResizeWindow(display, win,
                    glyph_width * (max_x + 3 + 2),
                    (glyph_height * (max_y + 1)) +
                    (button_lines * button_height));
      XtVaSetValues(text_w,
                    XmNcolumns, max_x,
                    XmNrows,    max_y,
                    NULL);
   }
   XmTextSetString(text_w, text);
   XSync(display, 0);
   XmUpdateDisplay(text_w);

   return;
}


/*++++++++++++++++++++++++ close_info_button() ++++++++++++++++++++++++++*/
static void
close_info_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   XtPopdown(infoshell);

   return;
}


/*+++++++++++++++++++++++++++ search_button() +++++++++++++++++++++++++++*/
static void
search_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   char                  *ptr,
                         *search_str,
                         *text_str;
   static char           *last_search_str = NULL;
   static XmTextPosition last_pos = 0;

   if (last_pos != 0)
   {
      XmTextClearSelection(text_w, 0);
   }
   if (!(search_str = XmTextGetString(searchbox_w)) || (!*search_str))
   {
      XtFree(search_str);
      return;
   }
   else
   {
      if (last_search_str == NULL)
      {
         size_t length;

         length = strlen(search_str) + 1;
         if ((last_search_str = malloc(length)) == NULL)
         {
            (void)xrec(FATAL_DIALOG,
                       "Could not malloc() %d Bytes : %s (%s %d)",
                       length, strerror(errno), __FILE__, __LINE__);
            return;
         }
         (void)memcpy(last_search_str, search_str, length);
      }
      else
      {
         if (my_strcmp(last_search_str, search_str) != 0)
         {
            size_t length;

            length = strlen(search_str) + 1;
            last_pos = 0;
            free(last_search_str);
            if ((last_search_str = malloc(length)) == NULL)
            {
               (void)xrec(FATAL_DIALOG,
                          "Could not malloc() %d Bytes : %s (%s %d)",
                          length, strerror(errno), __FILE__, __LINE__);
               return;
            }
            (void)memcpy(last_search_str, search_str, length);
         }
      }
   }
   if (!(text_str = XmTextGetString(text_w)) || (!*text_str))
   {
      XtFree(text_str);
      XtFree(search_str);
      return;
   }
   if ((ptr = posi(text_str + last_pos, search_str)) != NULL)
   {
      size_t         length;
      XmTextPosition pos;

      length = strlen(search_str);
      pos = (XmTextPosition)(ptr - text_str - length - 1);
      XmTextShowPosition(text_w, pos);
      XmTextSetSelection(text_w, pos, pos + length, 0);
      last_pos = pos + length;
   }
   else
   {
      if (last_pos != 0)
      {
         XmTextClearSelection(text_w, 0);
         last_pos = 0;
      }
   }

   return;
}
