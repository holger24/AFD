/*
 *  afd_load.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2016 Deutscher Wetterdienst (DWD),
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

DESCR__S_M1
/*
 ** NAME
 **   afd_load - shows load of the AFD
 **
 ** SYNOPSIS
 **   afd_load
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   13.03.1998 H.Kiehl Created
 **   25.07.1998 H.Kiehl Added number of active transfers.
 **
 */
DESCR__E_M1

#include <stdio.h>                 /* fprintf()                          */
#include <string.h>                /* strerror(), strcpy()               */
#include <ctype.h>                 /* toupper()                          */
#include <stdlib.h>                /* malloc(), free()                   */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                /* gethostname()                      */
#include <errno.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Text.h>
#include <Xm/PushB.h>
#include <Xm/LabelG.h>
#include <Xm/Separator.h>
#include <X11/Intrinsic.h>
#include <X11/Xaw/StripChart.h>
#include "afd_load.h"
#include "mafd_ctrl.h"
#include "permission.h"
#include "version.h"

/* Global variables. */
Widget                     current_value_w;
int                        fsa_fd,
                           fsa_id,
                           no_of_hosts,
                           sys_log_fd = STDERR_FILENO;
#ifdef HAVE_MMAP
off_t                      fsa_size;
#endif
double                     prev_value = 0.0,
                           update_interval = DEFAULT_UPDATE_INTERVAL;
char                       *p_work_dir;
struct filetransfer_status *fsa;
struct afd_status          *p_afd_status;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local global variables. */
static char                chart_type;

/* Local function prototypes. */
static void                init_afd_load(int *, char **, char *, char *),
                           usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char            font_name[256],
                   *heading_str[4] =
                   {
                      "FILE LOAD",
                      "KBYTE LOAD",
                      "CONNECTION LOAD",
                      "ACTIVE TRANSFERS",
                   },
                   *unit_str[4] =
                   {
                      "Files/s",
                      "KBytes/s",
                      "Connections/s",
                      " "
                   },
                   window_title[100],
                   work_dir[MAX_PATH_LENGTH];
   XtAppContext    app_context;
   Widget          appshell,
                   bottom_separator_w,
                   buttonbox_w,
                   button_w,
                   chart_w,
                   headingbox_w,
                   label_w,
                   mainform_w,
                   top_separator_w;
   static String   fallback_res[] =
                   {
                      ".afd_load*mwmDecorations : 110",
                      ".afd_load*mwmFunctions : 30",
                      ".afd_load.mainform*background : NavajoWhite2",
                      ".afd_load.mainform.headingbox.current_value*background : NavajoWhite1",
                      ".afd_load.mainform.chart*background : NavajoWhite1",
                      ".afd_load.mainform.buttonbox*background : PaleVioletRed2",
                      ".afd_load.mainform.buttonbox*foreground : Black",
                      ".afd_load.mainform.buttonbox*highlightColor : Black",
                      NULL
                   };
   Arg             args[MAXARGS];
   Cardinal        argcount;
   XmFontList      fontlist;
   XmFontListEntry entry;
   uid_t           euid, /* Effective user ID. */
                   ruid; /* Real user ID. */

   CHECK_FOR_VERSION(argc, argv);

   /* Initialize global variables. */
   p_work_dir = work_dir;
   init_afd_load(&argc, argv, font_name, window_title);

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
   appshell = XtAppInitialize(&app_context, "afd_load", NULL, 0,
                              &argc, argv, fallback_res, args, argcount);

   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                       euid, strerror(errno), __FILE__, __LINE__);
      }
   }

#ifdef HAVE_XPM
   /* Setup AFD logo as icon. */
   setup_icon(XtDisplay(appshell), appshell);
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
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

/*-----------------------------------------------------------------------*/
/*                             Heading Box                               */
/*                             -----------                               */
/*-----------------------------------------------------------------------*/
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   headingbox_w = XmCreateForm(mainform_w, "headingbox", args, argcount);

   label_w = XtVaCreateManagedWidget(heading_str[(int)chart_type],
                        xmLabelGadgetClass,  headingbox_w,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        5,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNbottomOffset,     5,
                        XmNleftAttachment,   XmATTACH_FORM,
                        XmNleftOffset,       5,
                        XmNfontList,         fontlist,
                        XmNalignment,        XmALIGNMENT_BEGINNING,
                        NULL);
   label_w = XtVaCreateManagedWidget(unit_str[(int)chart_type],
                        xmLabelGadgetClass,  headingbox_w,
                        XmNtopAttachment,    XmATTACH_FORM,
                        XmNtopOffset,        5,
                        XmNbottomAttachment, XmATTACH_FORM,
                        XmNbottomOffset,     5,
                        XmNrightAttachment,  XmATTACH_FORM,
                        XmNrightOffset,      5,
                        XmNfontList,         fontlist,
                        XmNalignment,        XmALIGNMENT_BEGINNING,
                        NULL);
   current_value_w = XtVaCreateWidget("current_value",
                        xmTextWidgetClass,        headingbox_w,
                        XmNtopAttachment,         XmATTACH_FORM,
                        XmNtopOffset,             5,
                        XmNrightAttachment,       XmATTACH_WIDGET,
                        XmNrightWidget,           label_w,
                        XmNrightOffset,           5,
                        XmNbottomAttachment,      XmATTACH_FORM,
                        XmNbottomOffset,          5,
                        XmNfontList,              fontlist,
                        XmNrows,                  1,
                        XmNcolumns,               MAX_CURRENT_VALUE_DIGIT,
                        XmNeditable,              False,
                        XmNcursorPositionVisible, False,
                        XmNmarginHeight,          1,
                        XmNmarginWidth,           1,
                        XmNshadowThickness,       1,
                        XmNhighlightThickness,    0,
                        NULL);
   XtManageChild(current_value_w);
   XtManageChild(headingbox_w);

/*-----------------------------------------------------------------------*/
/*                         Horizontal Separator                          */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,     XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,       headingbox_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   top_separator_w = XmCreateSeparator(mainform_w, "top separator",
                                       args, argcount);
   XtManageChild(top_separator_w);

/*-----------------------------------------------------------------------*/
/*                             Button Box                                */
/*-----------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   buttonbox_w = XmCreateForm(mainform_w, "buttonbox", args, argcount);
   button_w = XtVaCreateManagedWidget("Close",
                                     xmPushButtonWidgetClass, buttonbox_w,
                                     XmNfontList,         fontlist,
                                     XmNtopAttachment,    XmATTACH_FORM,
                                     XmNleftAttachment,   XmATTACH_FORM,
                                     XmNrightAttachment,  XmATTACH_FORM,
                                     XmNbottomAttachment, XmATTACH_FORM,
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
   bottom_separator_w = XmCreateSeparator(mainform_w, "bottom separator",
                                          args, argcount);
   XtManageChild(bottom_separator_w);

/*-----------------------------------------------------------------------*/
/*                             Chart Box                                 */
/*-----------------------------------------------------------------------*/
   chart_w = XtVaCreateManagedWidget("chart",
                                     stripChartWidgetClass, mainform_w,
                                     XmNtopAttachment,      XmATTACH_WIDGET,
                                     XmNtopWidget,          top_separator_w,
                                     XmNleftAttachment,     XmATTACH_FORM,
                                     XmNrightAttachment,    XmATTACH_FORM,
                                     XmNbottomAttachment,   XmATTACH_WIDGET,
                                     XmNbottomWidget,       bottom_separator_w,
                                     XtNupdate,             (int)update_interval,
                                     XtNjumpScroll,         1,
                                     XtNheight,             100,
                                     XtNwidth,              260,
                                     NULL);
   if (chart_type == FILE_CHART)
   {
      XtAddCallback(chart_w, XtNgetValue, get_file_value, (XtPointer)0);
   }
   else if (chart_type == KBYTE_CHART)
        {
           XtAddCallback(chart_w, XtNgetValue, get_kbyte_value, (XtPointer)0);
        }
   else if (chart_type == CONNECTION_CHART)
        {
           XtAddCallback(chart_w, XtNgetValue, get_connection_value,
                         (XtPointer)0);
        }
        else
        {
           XtAddCallback(chart_w, XtNgetValue, get_transfer_value,
                         (XtPointer)0);
        }

   XtManageChild(mainform_w);

   /* Free font list. */
   XmFontListFree(fontlist);

   /* Realize all widgets. */
   XtRealizeWidget(appshell);

   XmTextSetString(current_value_w, "      0.00");

   /* Start the main event-handling loop. */
   XtAppMainLoop(app_context);

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++ init_afd_load() +++++++++++++++++++++++++++*/
static void
init_afd_load(int  *argc,
              char *argv[],
              char *font_name,
              char *window_title)
{
   register int i;
   char         fake_user[MAX_FULL_USER_ID_LENGTH],
                *perm_buffer,
                hostname[MAX_AFD_NAME_LENGTH];

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

   /* Now lets see if user may use this program. */
   check_fake_user(argc, argv, AFD_CONFIG_FILE, fake_user);
   switch (get_permissions(&perm_buffer, fake_user, NULL))
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

      case SUCCESS : /* The user may use this program. */
         free(perm_buffer);
         break;

      case INCORRECT : /* Hmm. Something did go wrong. Since we want to */
                       /* be able to disable permission checking let    */
                       /* the user have all permissions.                */
         break;

      default :
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   /* Attach to FSA to get values for chart. */
   if ((i = fsa_attach_passive(NO, AFD_LOAD)) != SUCCESS)
   {
      if (i == INCORRECT_VERSION)
      {
         (void)fprintf(stderr, "This program is not able to attach to the FSA due to incorrect version.\n");
      }
      else
      {
         if (i < 0)
         {
            (void)fprintf(stderr, "Failed to attach to FSA.\n");
         }
         else
         {
            (void)fprintf(stderr, "Failed to attach to FSA : %s\n",
                          strerror(i));
         }
      }
      exit(INCORRECT);
   }

   if (get_arg(argc, argv, SHOW_FILE_LOAD, NULL, 0) == SUCCESS)
   {
      chart_type = FILE_CHART;
      for (i = 0; i < no_of_hosts; i++)
      {
         prev_value += (double)fsa[i].file_counter_done;
      }
   }
   else if (get_arg(argc, argv, SHOW_KBYTE_LOAD, NULL, 0) == SUCCESS)
        {
           chart_type = KBYTE_CHART;
           for (i = 0; i < no_of_hosts; i++)
           {
              prev_value += (double)fsa[i].bytes_send;
           }
           prev_value /= 1024;
        }
   else if (get_arg(argc, argv, SHOW_CONNECTION_LOAD, NULL, 0) == SUCCESS)
        {
           chart_type = CONNECTION_CHART;
           for (i = 0; i < no_of_hosts; i++)
           {
              prev_value += (double)fsa[i].connections;
           }
        }
   else if (get_arg(argc, argv, SHOW_TRANSFER_LOAD, NULL, 0) == SUCCESS)
        {
           /* Attach to the AFD Status Area. */
           if (attach_afd_status(NULL, WAIT_AFD_STATUS_ATTACH) < 0)
           {
              (void)fprintf(stderr,
                            "Failed to map to AFD status area. (%s %d)\n",
                            __FILE__, __LINE__);
              exit(INCORRECT);
           }
           chart_type = TRANSFER_CHART;
           prev_value = p_afd_status->no_of_transfers;
        }
        else
        {
           (void)fsa_detach(NO);
           usage(argv[0]);
           exit(INCORRECT);
        }

   if (get_arg(argc, argv, "-f", font_name, 256) == INCORRECT)
   {
      (void)strcpy(font_name, DEFAULT_FONT);
   }

   /* Prepare title of this window. */
   (void)strcpy(window_title, "AFD Load ");
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

   return;
}


/*-------------------------------- usage() ------------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 "Usage : %s [--version] [-w <working directory>] [-f <font name>] <%s|%s|%s|%s>\n",
                 progname, SHOW_FILE_LOAD, SHOW_KBYTE_LOAD,
                 SHOW_CONNECTION_LOAD, SHOW_TRANSFER_LOAD);
   return;
}
