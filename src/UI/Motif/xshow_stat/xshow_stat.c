/*
 *  xshow_stat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   xshow_stat - shows output statistics of the AFD
 **
 ** SYNOPSIS
 **   xshow_stat [--version]
 **                  OR
 **              [-w <AFD working directory>] [-f font name] [host name 1..n]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.02.1999 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>         /* struct tm                               */
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "xshow_stat.h"
#include "statdefs.h"
#include "permission.h"
#include "version.h"

#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/Form.h>
#include <Xm/Separator.h>
#include <Xm/DrawingA.h>
#include <Xm/CascadeB.h>
#include <Xm/PushB.h>
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif

#ifndef MAP_FILE    /* Required for BSD.          */
# define MAP_FILE 0 /* All others do not need it. */
#endif

/* Global variables. */
Display        *display;
XtAppContext   app;
Colormap       default_cmap;
XFontStruct    *font_struct;
XmFontList     fontlist = NULL;
Widget         appshell,
               stat_window_w;
Window         stat_window;
GC             letter_gc,
               normal_letter_gc,
               color_letter_gc,
               default_bg_gc,
               normal_bg_gc,
               button_bg_gc,
               color_gc,
               black_line_gc,
               white_line_gc;
char           font_name[20],
               **hosts,
               *p_work_dir,
               **x_data_point = NULL,
               **y_data_point = NULL;
int            data_height,
               data_length,
               first_data_pos,
               host_counter,
               no_of_chars,
               no_of_x_data_points,
               no_of_y_data_points,
               no_of_hosts,
               *stat_pos = NULL,
               stat_type,
               sys_log_fd = STDERR_FILENO,
               time_type,
               window_height = 0,
               window_width = 0,
               x_data_spacing,
               x_offset_left_xaxis,
               x_offset_right_xaxis,
               y_data_spacing,
               y_offset_top_yaxis,
               y_offset_bottom_yaxis,
               y_offset_xaxis;
unsigned int   glyph_height,
               glyph_width;
unsigned long  color_pool[COLOR_POOL_SIZE];
struct afdstat *stat_db;
const char     *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void    init_show_stat(int *, char **, char *, char *),
               sig_bus(int),
               sig_exit(int),
               sig_segv(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char          window_title[100],
                 work_dir[MAX_PATH_LENGTH];
   static String fallback_res[] =
                 {
                    ".xshow_stat*background : NavajoWhite2",
                    ".xshow_stat.mainform.buttonbox*background : PaleVioletRed2",
                    ".xshow_stat.mainform.buttonbox*foreground : Black",
                    ".xshow_stat.mainform.buttonbox*highlightColor : Black",
                    NULL
                 };
   Widget        button_w,
                 buttonbox_w,
                 mainform_w,
                 separator_w;
   Arg           args[MAXARGS];
   Cardinal      argcount;
   uid_t         euid, /* Effective user ID. */
                 ruid; /* Real user ID. */

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values. */
   p_work_dir = work_dir;
   init_show_stat(&argc, argv, font_name, window_title);

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
   appshell = XtAppInitialize(&app, "AFD", NULL, 0, &argc, argv,
                              fallback_res, args, argcount);

   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                       euid, strerror(errno), __FILE__, __LINE__);
      }
   }

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

#ifdef _X_DEBUG
   XSynchronize(display, 1);
#endif

   /* Setup and determine window parameters. */
   setup_window(font_name);

   /* Get window size. */
   (void)window_size(&window_width, &window_height, 0, 0);

   /* Create managing widget. */
   mainform_w = XmCreateForm(appshell, "mainform", NULL, 0);

   /* Setup colors. */
   default_cmap = DefaultColormap(display, DefaultScreen(display));
   init_color(XtDisplay(appshell));

   /*--------------------------------------------------------------------*/
   /*                            Button Box                              */
   /*--------------------------------------------------------------------*/
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

   /*--------------------------------------------------------------------*/
   /*                        Horizontal Separator                        */
   /*--------------------------------------------------------------------*/
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

   /*--------------------------------------------------------------------*/
   /*                            Drawing Area                            */
   /*--------------------------------------------------------------------*/
   argcount = 0;
   XtSetArg(args[argcount], XmNheight, (Dimension) window_height);
   argcount++;
   XtSetArg(args[argcount], XmNwidth, (Dimension) window_width);
   argcount++;
   XtSetArg(args[argcount], XmNbackground, color_pool[DEFAULT_BG]);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget, separator_w);
   argcount++;
   stat_window_w = XmCreateDrawingArea(mainform_w, "stat_window_w", args,
                                       argcount);
   XtManageChild(stat_window_w);
   XtAddCallback(stat_window_w, XmNexposeCallback,
                 (XtCallbackProc)expose_handler_stat, NULL);
   XtManageChild(mainform_w);

   /* Initialise the GC's. */
   init_gcs();

#ifdef WITH_EDITRES
   XtAddEventHandler(appshell, (EventMask)0, True,
                     _XEditResCheckMessages, NULL);
#endif

   /* Realize all widgets. */
   XtRealizeWidget(appshell);

   /* Set some signal handlers. */
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(WARN_DIALOG,
                 "Failed to set signal handlers for xshow_stat : %s",
                 strerror(errno));
   }

   /* Get window ID of the drawing widget. */
   stat_window = XtWindow(stat_window_w);

   /* Start the main event-handling loop. */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++ init_show_stat() +++++++++++++++++++++++++++*/
static void
init_show_stat(int *argc, char *argv[], char *font_name, char *window_title)
{
   int          stat_fd;
   time_t       now;
   char         fake_user[MAX_FULL_USER_ID_LENGTH],
                hostname[MAX_AFD_NAME_LENGTH],
                *perm_buffer,
                profile[MAX_PROFILE_NAME_LENGTH + 1],
                *ptr,
                statistic_file[MAX_PATH_LENGTH];
   struct tm    *p_ts;
#ifdef HAVE_STATX
   struct statx stat_buf;
#else
   struct stat  stat_buf;
#endif

   /* See if user wants some help. */
   if ((get_arg(argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      (void)fprintf(stdout,
                    "Usage: %s [-w <work_dir>] [-f <numeric font name>] [-[CDFHKY]]\n",
                    argv[0]);
      (void)fprintf(stdout, "       -C  View number of network connections.\n");
      (void)fprintf(stdout, "       -D  Day statistics.\n");
      (void)fprintf(stdout, "       -E  View number of errors.\n");
      (void)fprintf(stdout, "       -F  View number of files transmitted.\n");
      (void)fprintf(stdout, "       -H  Hour statistics.\n");
      (void)fprintf(stdout, "       -K  View number of bytes transmitted.\n");
      (void)fprintf(stdout, "       -Y  Your statistics.\n");
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
      profile[0] = '\0';
   }

   /*
    * If not set, set some default values.
    */
   stat_type = SHOW_FILE_STAT;
   time_type = DAY_STAT;
   if (get_arg(argc, argv, "-f", font_name, 20) == INCORRECT)
   {
      (void)strcpy(font_name, DEFAULT_FONT);
   }
   if (get_arg(argc, argv, "-K", NULL, 0) == SUCCESS)
   {
      stat_type = SHOW_KBYTE_STAT;
   }
   if (get_arg(argc, argv, "-E", NULL, 0) == SUCCESS)
   {
      stat_type = SHOW_ERROR_STAT;
   }
   if (get_arg(argc, argv, "-F", NULL, 0) == SUCCESS)
   {
      stat_type = SHOW_FILE_STAT;
   }
   if (get_arg(argc, argv, "-C", NULL, 0) == SUCCESS)
   {
      stat_type = SHOW_CONNECT_STAT;
   }
   if (get_arg(argc, argv, "-H", NULL, 0) == SUCCESS)
   {
      time_type = HOUR_STAT;
   }
   if (get_arg(argc, argv, "-D", NULL, 0) == SUCCESS)
   {
      time_type = DAY_STAT;
   }
   if (get_arg(argc, argv, "-Y", NULL, 0) == SUCCESS)
   {
      time_type = YEAR_STAT;
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

      case NONE     : /* User is not allowed to use this program. */
         {
            char *user;

            if ((user = getenv("LOGNAME")) != NULL)
            {
               (void)fprintf(stderr,
                             "User %s is not permitted to use this program.\n",
                             user);
            }
            else
            {
               (void)fprintf(stderr, "%s (%s %d)\n",
                             PERMISSION_DENIED_STR, __FILE__, __LINE__);
            }
         }
         exit(INCORRECT);

      case SUCCESS  : /* Lets evaluate the permissions and see what */
                      /* the user may do.                           */
         if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
             (perm_buffer[2] == 'l') &&
             ((perm_buffer[3] == '\0') || (perm_buffer[3] == ',') ||
              (perm_buffer[3] == ' ') || (perm_buffer[3] == '\t')))
         {
            free(perm_buffer);
            break;
         }
         else if (posi(perm_buffer, XSHOW_STAT_PERM) == NULL)
              {
                 (void)fprintf(stderr, "%s (%s %d)\n",
                               PERMISSION_DENIED_STR, __FILE__, __LINE__);
                 exit(INCORRECT);
              }
         free(perm_buffer);
         break;

      case INCORRECT: /* Hmm. Something did go wrong. Since we want to */
                      /* be able to disable permission checking let    */
                      /* the user have all permissions.                */
         break;

      default       :
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   /* Map to statistic file. */
   now = time(NULL);
   p_ts = gmtime(&now);
   (void)sprintf(statistic_file, "%s%s%s.%d", p_work_dir,
                 LOG_DIR, STATISTIC_FILE, p_ts->tm_year + 1900);
   if ((stat_fd = open(statistic_file, O_RDONLY)) == -1)
   {
      (void)fprintf(stderr, "ERROR   : Failed to open() %s : %s (%s %d)\n",
                    statistic_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef HAVE_STATX
   if (statx(stat_fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
             STATX_SIZE, &stat_buf) == -1)
#else
   if (fstat(stat_fd, &stat_buf) == -1)
#endif
   {
      (void)fprintf(stderr, "ERROR   : Failed to access %s : %s (%s %d)\n",
                    statistic_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((ptr = mmap(NULL,
#ifdef HAVE_STATX
                   stat_buf.stx_size, PROT_READ, (MAP_FILE | MAP_SHARED),
#else
                   stat_buf.st_size, PROT_READ, (MAP_FILE | MAP_SHARED),
#endif
                   stat_fd, 0)) == (caddr_t) -1)
   {
      (void)fprintf(stderr, "ERROR   : Failed to mmap() %s : %s (%s %d)\n",
                    statistic_file, strerror(errno), __FILE__, __LINE__);
      (void)close(stat_fd);
      exit(INCORRECT);
   }
   stat_db = (struct afdstat *)(ptr + AFD_WORD_OFFSET);
#ifdef HAVE_STATX
   no_of_hosts = (stat_buf.stx_size - AFD_WORD_OFFSET) / sizeof(struct afdstat);
#else
   no_of_hosts = (stat_buf.st_size - AFD_WORD_OFFSET) / sizeof(struct afdstat);
#endif
   if (close(stat_fd) == -1)
   {
      (void)fprintf(stderr, "WARN    : Failed to close() %s : %s (%s %d)\n",
                    statistic_file, strerror(errno), __FILE__, __LINE__);
   }

   /* Collect all hostnames. */
   host_counter = *argc - 1;
   if (host_counter > 0)
   {
      register int i = 0,
                   j;

      RT_ARRAY(hosts, host_counter, (MAX_RECIPIENT_LENGTH + 1), char);
      while (*argc > 1)
      {
         (void)my_strncpy(hosts[i], argv[1], (MAX_RECIPIENT_LENGTH + 1));
         (*argc)--; argv++;
         i++;
      }
      if ((stat_pos = calloc((host_counter * sizeof(int)), sizeof(int))) == NULL)
      {
         (void)fprintf(stderr, "ERROR   : calloc() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      for (i = 0; i < host_counter; i++)
      {
         for (j = 0; j < no_of_hosts; j++)
         {
            if (my_strcmp(hosts[i], stat_db[j].hostname) == 0)
            {
               stat_pos[i] = j;
               break;
            }
         }
      }
   }
   else
   {
      register int i;

      if ((stat_pos = calloc((no_of_hosts * sizeof(int)), sizeof(int))) == NULL)
      {
         (void)fprintf(stderr, "ERROR   : calloc() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      for (i = 0; i < no_of_hosts; i++)
      {
         stat_pos[i] = i;
      }
   }

   /* Prepare title of this window. */
   (void)strcpy(window_title, "Statistics ");
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
