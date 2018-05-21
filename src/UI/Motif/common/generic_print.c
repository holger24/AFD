/*
 *  print_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2018 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   generic_print - generic printer interface
 **
 ** SYNOPSIS
 **   void print_data(Widget w, XtPointer client_data, XtPointer call_data)
 **   int  prepare_printer(int *)
 **   int  prepare_file(int *, int)
 **   void prepare_tmp_name(void)
 **   void send_mail_cmd(char *)
 **   void send_print_cmd(char *)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.03.2000 H.Kiehl Created
 **   11.08.2003 H.Kiehl Added option to send data as mail.
 **   08.08.2007 H.Kiehl For printing write to file first, then print file.
 **   02.06.2016 H.Kiehl Added mail server when using mail.
 **
 */
DESCR__E_M3

#include <stdio.h>               /* snprintf(), popen()                  */
#include <string.h>              /* strcpy(), strcat(), strerror()       */
#include <stdlib.h>              /* exit(), free()                       */
#include <signal.h>              /* signal()                             */
#include <sys/types.h>
#include <unistd.h>              /* unlink(), getpid()                   */
#include <sys/stat.h>
#include <fcntl.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/LabelG.h>
#include <Xm/Frame.h>
#include <Xm/Text.h>
#include <Xm/List.h>
#include <Xm/Separator.h>
#include <Xm/PushB.h>
#include <Xm/ToggleBG.h>
#include <X11/keysym.h>
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif
#include <errno.h>
#include "mafd_ctrl.h"

/* Local definitions. */
#define DEFAULT_SUBJECT          "AFD log data"
#define MAX_PRINT_SUBJECT_LENGTH 256

/* Global variables. */
Widget         printshell = (Widget)NULL;
XT_PTR_TYPE    range_type,
               device_type;
char           file_name[MAX_PATH_LENGTH],
               subject[MAX_PRINT_SUBJECT_LENGTH];
FILE           *fp;             /* File pointer for printer command.     */

/* External global variables. */
extern Widget  appshell;
extern char    font_name[];

/* Local global variables. */
static Widget  printer_text_w,
               printer_radio_w,
               file_text_w,
               file_radio_w,
               mail_text_w,
               mail_radio_w,
               subject_label_w,
               subject_w;
static char    mailserver[MAX_REAL_HOSTNAME_LENGTH + 1 + MAX_INT_LENGTH + 1],
               mailto[MAX_RECIPIENT_LENGTH],
               printer_cmd[PRINTER_INFO_LENGTH + 1],
               printer_name[PRINTER_INFO_LENGTH + 1];
static int     mailserverport;

/* Local function prototypes. */
static void    cancel_print_button(Widget, XtPointer, XtPointer),
               device_select(Widget, XtPointer, XtPointer),
               range_select(Widget, XtPointer, XtPointer),
               save_file_name(Widget, XtPointer, XtPointer),
               save_printer_name(Widget, XtPointer, XtPointer),
               save_mail_address(Widget, XtPointer, XtPointer),
               save_mail_subject(Widget, XtPointer, XtPointer);


/*############################# print_data() ############################*/
void
print_data(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
   {
      (void)fprintf(stderr, "signal() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
   }

   /*
    * First, see if the window has already been created. If
    * no create a new window.
    */
   if ((printshell == (Widget)NULL) || (XtIsRealized(printshell) == False) ||
       (XtIsSensitive(printshell) != True))
   {
      Widget          main_form_w,
                      form_w,
                      frame_w,
                      radio_w,
                      criteriabox_w,
                      radiobox_w,
                      separator_w,
                      buttonbox_w,
                      button_w,
                      inputline_w;
      Arg             args[MAXARGS];
      Cardinal        argcount;
      XmFontList      p_fontlist;
      XmFontListEntry entry;
      XT_PTR_TYPE     select_all = (XT_PTR_TYPE)client_data;

      /* Get default values from AFD_CONFIG file. */
      get_printer_cmd(printer_cmd, printer_name, mailserver, &mailserverport);

      printshell = XtVaCreatePopupShell("Print Data", topLevelShellWidgetClass,
                                        appshell, NULL);

      /* Create managing widget. */
      main_form_w = XmCreateForm(printshell, "main_form", NULL, 0);

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

      /* Create Print Button. */
      button_w = XtVaCreateManagedWidget("Print",
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
                    (XtCallbackProc)print_data_button, (XtPointer)0);

      /* Create Cancel Button. */
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
                    (XtCallbackProc)cancel_print_button, (XtPointer)0);
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

      if (select_all == 0)
      {
         /*---------------------------------------------------------------*/
         /*                         Range Box                             */
         /*---------------------------------------------------------------*/
         /* Frame for Range Box. */
         frame_w = XtVaCreateManagedWidget("range_frame",
                           xmFrameWidgetClass,  criteriabox_w,
                           XmNshadowType,       XmSHADOW_ETCHED_IN,
                           XmNtopAttachment,    XmATTACH_FORM,
                           XmNtopOffset,        5,
                           XmNleftAttachment,   XmATTACH_FORM,
                           XmNleftOffset,       5,
                           XmNbottomAttachment, XmATTACH_FORM,
                           XmNbottomOffset,     5,
                           NULL);

         /* Label for the frame. */
         XtVaCreateManagedWidget("Range",
                           xmLabelGadgetClass,        frame_w,
                           XmNchildType,              XmFRAME_TITLE_CHILD,
                           XmNchildVerticalAlignment, XmALIGNMENT_CENTER,
                           NULL);

         /* Manager widget for the actual range stuff. */
         radiobox_w = XtVaCreateWidget("radiobox",
                           xmRowColumnWidgetClass, frame_w,
                           XmNradioBehavior,       True,
                           XmNorientation,         XmVERTICAL,
                           XmNpacking,             XmPACK_COLUMN,
                           XmNnumColumns,          1,
                           XmNresizable,           False,
                           NULL);

         radio_w = XtVaCreateManagedWidget("Selection",
                           xmToggleButtonGadgetClass, radiobox_w,
                           XmNfontList,               p_fontlist,
                           XmNset,                    True,
                           NULL);
         XtAddCallback(radio_w, XmNarmCallback, (XtCallbackProc)range_select,
                       (XtPointer)SELECTION_TOGGLE);
         radio_w = XtVaCreateManagedWidget("All",
                           xmToggleButtonGadgetClass, radiobox_w,
                           XmNfontList,               p_fontlist,
                           XmNset,                    False,
                           NULL);
         XtAddCallback(radio_w, XmNarmCallback,
                       (XtCallbackProc)range_select, (XtPointer)ALL_TOGGLE);
         XtManageChild(radiobox_w);
         range_type = SELECTION_TOGGLE;

         /* Frame for Device Box. */
         frame_w = XtVaCreateManagedWidget("device_frame",
                           xmFrameWidgetClass,   criteriabox_w,
                           XmNshadowType,        XmSHADOW_ETCHED_IN,
                           XmNtopAttachment,     XmATTACH_FORM,
                           XmNtopOffset,         5,
                           XmNleftAttachment,    XmATTACH_WIDGET,
                           XmNleftWidget,        frame_w,
                           XmNleftOffset,        5,
                           XmNrightAttachment,   XmATTACH_FORM,
                           XmNrightOffset,       5,
                           XmNbottomAttachment,  XmATTACH_FORM,
                           XmNbottomOffset,      5,
                           NULL);
      }
      else
      {
         /* Frame for Device Box. */
         frame_w = XtVaCreateManagedWidget("device_frame",
                           xmFrameWidgetClass,   criteriabox_w,
                           XmNshadowType,        XmSHADOW_ETCHED_IN,
                           XmNtopAttachment,     XmATTACH_FORM,
                           XmNtopOffset,         5,
                           XmNleftAttachment,    XmATTACH_FORM,
                           XmNleftOffset,        5,
                           XmNrightAttachment,   XmATTACH_FORM,
                           XmNrightOffset,       5,
                           XmNbottomAttachment,  XmATTACH_FORM,
                           XmNbottomOffset,      5,
                           NULL);
      }

      /*---------------------------------------------------------------*/
      /*                        Device Box                             */
      /*---------------------------------------------------------------*/
      /* Label for the frame. */
      XtVaCreateManagedWidget("Device",
                        xmLabelGadgetClass,          frame_w,
                        XmNchildType,                XmFRAME_TITLE_CHILD,
                        XmNchildVerticalAlignment,   XmALIGNMENT_CENTER,
                        NULL);

      form_w = XtVaCreateWidget("device_form",
                        xmFormWidgetClass, frame_w,
                        NULL);

      /* Create selection line to select a printer. */
      inputline_w = XtVaCreateWidget("input_line",
                        xmFormWidgetClass,  form_w,
                        XmNtopAttachment,   XmATTACH_FORM,
                        XmNrightAttachment, XmATTACH_FORM,
                        XmNleftAttachment,  XmATTACH_FORM,
                        NULL);

      printer_radio_w = XtVaCreateManagedWidget("Printer",
                        xmToggleButtonGadgetClass, inputline_w,
                        XmNindicatorType,          XmONE_OF_MANY,
                        XmNfontList,               p_fontlist,
                        XmNset,                    True,
                        XmNtopAttachment,          XmATTACH_FORM,
                        XmNleftAttachment,         XmATTACH_FORM,
                        XmNbottomAttachment,       XmATTACH_FORM,
                        NULL);
      XtAddCallback(printer_radio_w, XmNvalueChangedCallback,
                    (XtCallbackProc)device_select, (XtPointer)PRINTER_TOGGLE);

      /* A text box to enter the printers name. */
      printer_text_w = XtVaCreateManagedWidget("printer_name",
                        xmTextWidgetClass,   inputline_w,
                        XmNfontList,         p_fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNcolumns,          20,
                        XmNmaxLength,        PRINTER_INFO_LENGTH,
                        XmNvalue,            printer_name,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       printer_radio_w,
                        XmNrightAttachment,  XmATTACH_FORM,
                        XmNrightOffset,      5,
                        XmNbottomAttachment, XmATTACH_FORM,
                        NULL);
      XtAddCallback(printer_text_w, XmNlosingFocusCallback,
                    save_printer_name, (XtPointer)0);
      XtManageChild(inputline_w);
      device_type = PRINTER_TOGGLE;

      /* Create selection line to select a file to store the data. */
      inputline_w = XtVaCreateWidget("input_line",
                        xmFormWidgetClass,  form_w,
                        XmNtopAttachment,   XmATTACH_WIDGET,
                        XmNtopWidget,       inputline_w,
                        XmNtopOffset,       5,
                        XmNrightAttachment, XmATTACH_FORM,
                        XmNleftAttachment,  XmATTACH_FORM,
                        NULL);

      file_radio_w = XtVaCreateManagedWidget("File   ",
                        xmToggleButtonGadgetClass, inputline_w,
                        XmNindicatorType,          XmONE_OF_MANY,
                        XmNfontList,               p_fontlist,
                        XmNset,                    False,
                        XmNtopAttachment,          XmATTACH_FORM,
                        XmNleftAttachment,         XmATTACH_FORM,
                        XmNbottomAttachment,       XmATTACH_FORM,
                        NULL);
      XtAddCallback(file_radio_w, XmNvalueChangedCallback,
                    (XtCallbackProc)device_select, (XtPointer)FILE_TOGGLE);

      /* A text box to enter the files name. */
      file_text_w = XtVaCreateManagedWidget("file_name",
                        xmTextWidgetClass,   inputline_w,
                        XmNfontList,         p_fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNcolumns,          20,
                        XmNmaxLength,        MAX_PATH_LENGTH - 1,
                        XmNvalue,            file_name,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       file_radio_w,
                        XmNrightAttachment,  XmATTACH_FORM,
                        XmNrightOffset,      5,
                        XmNbottomAttachment, XmATTACH_FORM,
                        NULL);
      XtAddCallback(file_text_w, XmNlosingFocusCallback,
                    save_file_name, (XtPointer)0);
      XtSetSensitive(file_text_w, False);
      XtManageChild(inputline_w);

      /* Create selection line to mail the data. */
      inputline_w = XtVaCreateWidget("input_line",
                        xmFormWidgetClass,  form_w,
                        XmNtopAttachment,   XmATTACH_WIDGET,
                        XmNtopWidget,       inputline_w,
                        XmNtopOffset,       5,
                        XmNrightAttachment, XmATTACH_FORM,
                        XmNleftAttachment,  XmATTACH_FORM,
                        NULL);

      mail_radio_w = XtVaCreateManagedWidget("Mailto ",
                        xmToggleButtonGadgetClass, inputline_w,
                        XmNindicatorType,          XmONE_OF_MANY,
                        XmNfontList,               p_fontlist,
                        XmNset,                    False,
                        XmNtopAttachment,          XmATTACH_FORM,
                        XmNleftAttachment,         XmATTACH_FORM,
                        XmNbottomAttachment,       XmATTACH_FORM,
                        NULL);
      XtAddCallback(mail_radio_w, XmNvalueChangedCallback,
                    (XtCallbackProc)device_select, (XtPointer)MAIL_TOGGLE);

      /* A text box to enter the mailto address. */
      mail_text_w = XtVaCreateManagedWidget("mailto",
                        xmTextWidgetClass,   inputline_w,
                        XmNfontList,         p_fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNcolumns,          20,
                        XmNmaxLength,        MAX_RECIPIENT_LENGTH - 1,
                        XmNvalue,            mailto,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       mail_radio_w,
                        XmNrightAttachment,  XmATTACH_FORM,
                        XmNrightOffset,      5,
                        XmNbottomAttachment, XmATTACH_FORM,
                        NULL);
      XtAddCallback(mail_text_w, XmNlosingFocusCallback,
                    save_mail_address, (XtPointer)0);
      XtSetSensitive(mail_text_w, False);
      XtManageChild(inputline_w);

      /* Create selection line to enter a mail subject. */
      (void)strncpy(subject, DEFAULT_SUBJECT, MAX_PRINT_SUBJECT_LENGTH);
      inputline_w = XtVaCreateWidget("input_line",
                        xmFormWidgetClass,  form_w,
                        XmNtopAttachment,   XmATTACH_WIDGET,
                        XmNtopWidget,       inputline_w,
                        XmNtopOffset,       5,
                        XmNrightAttachment, XmATTACH_FORM,
                        XmNleftAttachment,  XmATTACH_FORM,
                        NULL);

      subject_label_w = XtVaCreateManagedWidget("Subject: ",
                        xmLabelGadgetClass,  inputline_w,
                        XmNfontList,         p_fontlist,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNalignment,        XmALIGNMENT_END,
                        NULL);
      subject_w = XtVaCreateManagedWidget("",
                        xmTextWidgetClass,   inputline_w,
                        XmNfontList,         p_fontlist,
                        XmNmarginHeight,     1,
                        XmNmarginWidth,      1,
                        XmNshadowThickness,  1,
                        XmNcolumns,          20,
                        XmNmaxLength,        MAX_RECIPIENT_LENGTH - 1,
                        XmNvalue,            subject,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNleftAttachment,   XmATTACH_WIDGET,
                        XmNleftWidget,       subject_label_w,
                        XmNrightAttachment,  XmATTACH_FORM,
                        XmNrightOffset,      5,
                        XmNbottomAttachment, XmATTACH_FORM,
                        NULL);
      XtAddCallback(subject_w, XmNlosingFocusCallback,
                    save_mail_subject, (XtPointer)0);
      XtSetSensitive(subject_label_w, False);
      XtSetSensitive(subject_w, False);
      XtManageChild(inputline_w);

      XtManageChild(form_w);
      XtManageChild(criteriabox_w);
      XtManageChild(main_form_w);

      XmFontListFree(p_fontlist);

#ifdef WITH_EDITRES
      XtAddEventHandler(printshell, (EventMask)0, True, _XEditResCheckMessages, NULL);
#endif
   }
   XtPopup(printshell, XtGrabNone);

   return;
}


/*########################## prepare_printer() ##########################*/
int
prepare_printer(int *fd)
{
   char cmd[PRINTER_INFO_LENGTH + PRINTER_INFO_LENGTH + 1];

   (void)strcpy(cmd, printer_cmd);
   (void)strcat(cmd, printer_name);
   (void)strcat(cmd, " > /dev/null");

   if ((fp = popen(cmd, "w")) == NULL)
   {
      (void)xrec(ERROR_DIALOG, "Failed to send printer command %s : %s (%s %d)",
                 cmd, strerror(errno), __FILE__, __LINE__);
      XtPopdown(printshell);

      return(INCORRECT);
   }

   *fd = fileno(fp);

   return(SUCCESS);
}


/*######################### prepare_tmp_name() ##########################*/
void
prepare_tmp_name(void)
{
#if SIZEOF_PID_T == 4
   (void)snprintf(file_name, MAX_PATH_LENGTH, "/tmp/mail_log_file_%d",
#else
   (void)snprintf(file_name, MAX_PATH_LENGTH, "/tmp/mail_log_file_%lld",
#endif
                  (pri_pid_t)getpid());

   return;
}


/*########################### prepare_file() ############################*/
int
prepare_file(int *fd, int show_error)
{
   if ((*fd = open(file_name, (O_RDWR | O_CREAT | O_TRUNC), FILE_MODE)) < 0)
   {
      if (show_error)
      {
         (void)xrec(ERROR_DIALOG, "Failed to open() %s : %s (%s %d)",
                    file_name, strerror(errno), __FILE__, __LINE__);
         XtPopdown(printshell);
      }

      return(INCORRECT);
   }

   return(SUCCESS);
}


/*########################## send_print_cmd() ###########################*/
void
send_print_cmd(char *message, int max_msg_length)
{
   int  ret;
   char *buffer = NULL,
        cmd[PRINTER_INFO_LENGTH + 1 + PRINTER_INFO_LENGTH + 1 + MAX_PATH_LENGTH];

   (void)snprintf(cmd,
                  PRINTER_INFO_LENGTH + 1 + PRINTER_INFO_LENGTH + 1 + MAX_PATH_LENGTH,
                  "%s%s %s", printer_cmd, printer_name, file_name);
   if ((ret = exec_cmd(cmd, &buffer, -1, NULL, 0,
#ifdef HAVE_SETPRIORITY
                       NO_PRIORITY,
#endif
                       "", NULL, NULL, 0, 0L, YES, YES)) != 0)
   {
      if (buffer == NULL)
      {
         (void)xrec(ERROR_DIALOG,
                    "Failed to send printer command `%s' [%d] (%s %d)",
                    cmd, ret, __FILE__, __LINE__);
      }
      else
      {
         (void)xrec(ERROR_DIALOG,
                    "Failed to send printer command `%s' [%d] : %s (%s %d)",
                    cmd, ret, buffer, __FILE__, __LINE__);
      }
      XtPopdown(printshell);
      if (message != NULL)
      {
         (void)snprintf(message, max_msg_length,
                        "Failed to send data to printer (%d).", ret);
      }
   }
   else
   {
      if (message != NULL)
      {
         (void)snprintf(message, max_msg_length, "Send data to printer.");
      }
   }
   (void)unlink(file_name);
   free(buffer);

   return;
}


/*########################## send_mail_cmd() ############################*/
void
send_mail_cmd(char *message, int max_msg_length)
{
   if (mailto[0] == '\0')
   {
      if (message != NULL)
      {
         (void)snprintf(message, max_msg_length,
                        "ERROR: No mail address specified.");
      }
      (void)xrec(ERROR_DIALOG, "Please, enter a mail address for `Mailto`");
   }
   else
   {
      int  ret;
      char *buffer = NULL,
           cmd[ASMTP_LENGTH + 4 + MAX_REAL_HOSTNAME_LENGTH + 1 + MAX_INT_LENGTH + 1 + 4 + MAX_INT_LENGTH + 4 + MAX_RECIPIENT_LENGTH + 5 + MAX_PRINT_SUBJECT_LENGTH + 8 + MAX_PATH_LENGTH];

      (void)snprintf(cmd,
                     ASMTP_LENGTH + 4 + MAX_REAL_HOSTNAME_LENGTH + 1 + MAX_INT_LENGTH + 1 + 4 + MAX_INT_LENGTH + 4 + MAX_RECIPIENT_LENGTH + 5 + MAX_PRINT_SUBJECT_LENGTH + 8 + MAX_PATH_LENGTH,
                     "%s -m %s -p %d -a %s -s \"%s\" -t 20 %s",
                     ASMTP, mailserver, mailserverport, mailto, subject,
                     file_name);
      if ((ret = exec_cmd(cmd, &buffer, -1, NULL, 0,
#ifdef HAVE_SETPRIORITY
                          NO_PRIORITY,
#endif
                          "", NULL, NULL, 0, 0L, YES, YES)) != 0)
      {
         if (buffer == NULL)
         {
            (void)xrec(ERROR_DIALOG,
                       "Failed to send mail command `%s' [%d] (%s %d)",
                       cmd, ret, __FILE__, __LINE__);
         }
         else
         {
            (void)xrec(ERROR_DIALOG,
                       "Failed to send mail command `%s' [%d] : %s (%s %d)",
                       cmd, ret, buffer, __FILE__, __LINE__);
         }
         XtPopdown(printshell);
         if (message != NULL)
         {
            (void)snprintf(message, max_msg_length,
                           "Failed to send mail (%d).", ret);
         }
      }
      else
      {
         if (message != NULL)
         {
            (void)snprintf(message, max_msg_length, "Send mail to %s.", mailto);
         }
      }
      (void)unlink(file_name);
      free(buffer);
   }

   return;
}


/*+++++++++++++++++++++++ cancel_print_button() +++++++++++++++++++++++++*/
static void
cancel_print_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   XtPopdown(printshell);

   return;
}


/*+++++++++++++++++++++++++++ range_select() ++++++++++++++++++++++++++++*/
static void
range_select(Widget w, XtPointer client_data, XtPointer call_data)
{
   range_type = (XT_PTR_TYPE)client_data;

   return;
}


/*+++++++++++++++++++++++++++ device_select() +++++++++++++++++++++++++++*/
static void
device_select(Widget w, XtPointer client_data, XtPointer call_data)
{
   device_type = (XT_PTR_TYPE)client_data;

   if (device_type == PRINTER_TOGGLE)
   {
      XtVaSetValues(mail_radio_w, XmNset, False, NULL);
      XtSetSensitive(mail_text_w, False);
      XtSetSensitive(subject_label_w, False);
      XtSetSensitive(subject_w, False);
      XtVaSetValues(file_radio_w, XmNset, False, NULL);
      XtSetSensitive(file_text_w, False);
      XtVaSetValues(printer_radio_w, XmNset, True, NULL);
      XtSetSensitive(printer_text_w, True);
   }
   else if (device_type == FILE_TOGGLE)
        {
           XtVaSetValues(mail_radio_w, XmNset, False, NULL);
           XtSetSensitive(mail_text_w, False);
           XtSetSensitive(subject_label_w, False);
           XtSetSensitive(subject_w, False);
           XtVaSetValues(printer_radio_w, XmNset, False, NULL);
           XtSetSensitive(printer_text_w, False);
           XtVaSetValues(file_radio_w, XmNset, True, NULL);
           XtSetSensitive(file_text_w, True);
        }
        else
        {
           XtVaSetValues(file_radio_w, XmNset, False, NULL);
           XtSetSensitive(file_text_w, False);
           XtVaSetValues(printer_radio_w, XmNset, False, NULL);
           XtSetSensitive(printer_text_w, False);
           XtVaSetValues(mail_radio_w, XmNset, True, NULL);
           XtSetSensitive(mail_text_w, True);
           XtSetSensitive(subject_label_w, True);
           XtSetSensitive(subject_w, True);
        }

   return;
}


/*+++++++++++++++++++++++++ save_printer_name() +++++++++++++++++++++++++*/
static void
save_printer_name(Widget w, XtPointer client_data, XtPointer call_data)
{
   char *value = XmTextGetString(w);

   (void)strcpy(printer_name, value);
   XtFree(value);

   return;
}


/*++++++++++++++++++++++++++ save_file_name() +++++++++++++++++++++++++++*/
static void
save_file_name(Widget w, XtPointer client_data, XtPointer call_data)
{
   char *value = XmTextGetString(w);

   (void)strcpy(file_name, value);
   XtFree(value);

   return;
}


/*++++++++++++++++++++++++ save_mail_address() ++++++++++++++++++++++++++*/
static void
save_mail_address(Widget w, XtPointer client_data, XtPointer call_data)
{
   char *value = XmTextGetString(w);

   (void)strcpy(mailto, value);
   XtFree(value);
#if SIZEOF_PID_T == 4
   (void)snprintf(file_name, MAX_PATH_LENGTH, "mail_log_file_%d",
#else
   (void)snprintf(file_name, MAX_PATH_LENGTH, "mail_log_file_%lld",
#endif
                  (pri_pid_t)getpid());

   return;
}


/*++++++++++++++++++++++++ save_mail_subject() ++++++++++++++++++++++++++*/
static void
save_mail_subject(Widget w, XtPointer client_data, XtPointer call_data)
{
   char *value = XmTextGetString(w);

   (void)strncpy(subject, value, MAX_PRINT_SUBJECT_LENGTH);
   XtFree(value);

   return;
}
