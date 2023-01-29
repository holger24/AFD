/*
 *  mafd_ctrl.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2023 Deutscher Wetterdienst (DWD),
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
 **   mafd_ctrl - controls and monitors the AFD
 **
 ** SYNOPSIS
 **   mafd_ctrl [--version]
 **             [-w <AFD working directory>]
 **             [-p <user profile>]
 **             [-u <fake user>]
 **             [-no_input]
 **             [-f <numeric font name>]
 **             [-t <title>]
 **             [-bs]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.01.1996 H.Kiehl Created
 **   14.08.1997 H.Kiehl Support for multi-selection of lines.
 **   12.01.2000 H.Kiehl Added receive log.
 **   30.07.2001 H.Kiehl Support for the show_queue dialog.
 **   09.11.2002 H.Kiehl Short line support.
 **   14.03.2003 H.Kiehl Added history log in button bar.
 **   27.03.2003 H.Kiehl Added profile option.
 **   16.02.2008 H.Kiehl All drawing areas are now also drawn to an off-line
 **                      pixmap.
 **   15.09.2014 H.Kiehl Added option for simulated send mode.
 **   05.12.2014 H.Kiehl Added parameter -t to supply window title.
 **   27.02.2016 H.Kiehl Remove long/short line code.
 **   14.09.2016 H.Kiehl Added production log.
 **   04.03.2017 H.Kiehl Added group support.
 **   17.07.2019 H.Kiehl Added parameter -bs to disable backing store
 **                      and save under.
 **   23.01.2021 H.Kiehl Added viewing rename rules.
 **   05.03.2022 H.Kiehl Added setup option to modify alias display length.
 **
 */
DESCR__E_M1

/* #define WITH_MEMCHECK */

#include <stdio.h>            /* fprintf(), stderr                       */
#include <string.h>           /* strcpy()                                */
#include <ctype.h>            /* toupper()                               */
#include <unistd.h>           /* gethostname(), getcwd(), STDERR_FILENO  */
#include <stdlib.h>           /* getenv(), calloc()                      */
#ifdef WITH_MEMCHECK
# include <mcheck.h>
#endif
#include <math.h>             /* log10()                                 */
#include <sys/times.h>        /* times(), struct tms                     */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>        /* mmap()                                  */
#endif
#include <signal.h>           /* kill(), signal(), SIGINT                */
#include <fcntl.h>            /* O_RDWR                                  */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include "mafd_ctrl.h"
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/Form.h>
#include <Xm/PushBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/CascadeBG.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/DrawingA.h>
#include <Xm/CascadeB.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif
#include <errno.h>
#include "version.h"
#include "permission.h"

/* Global variables. */
Display                    *display;
XtAppContext               app;
XtIntervalId               interval_id_status,
                           interval_id_tv;
XtInputId                  db_update_cmd_id;
GC                         letter_gc,
                           normal_letter_gc,
                           locked_letter_gc,
                           color_letter_gc,
                           default_bg_gc,
                           normal_bg_gc,
                           locked_bg_gc,
                           label_bg_gc,
                           button_bg_gc,
                           tr_bar_gc,
                           color_gc,
                           black_line_gc,
                           unset_led_bg_gc,
                           white_line_gc,
                           led_gc;
Colormap                   default_cmap;
XFontStruct                *font_struct = NULL;
XmFontList                 fontlist = NULL;
Widget                     mw[5],          /* Main menu */
                           ow[13],         /* Host menu */
                           tw[2],          /* Test (ping, traceroute) */
                           vw[15],         /* View menu */
                           cw[8],          /* Control menu */
                           sw[8],          /* Setup menu */
                           hw[3],          /* Help menu */
                           lw[4],          /* AFD load */
                           lsw[4],         /* Line style */
                           ptw[3],         /* Process type. */
                           oow[3],         /* Other options */
                           fw[NO_OF_FONTS],/* Select font */
                           rw[NO_OF_ROWS], /* Select rows */
                           adl[MAX_HOSTNAME_LENGTH - MIN_ALIAS_DISPLAY_LENGTH + 2], /* Alias display length */
                           pw[13],         /* Popup menu */
                           dprw[3],        /* Debug pull right */
                           dprwpp[3],      /* Debug pull right in popup */
                           appshell,
                           button_window_w,
                           detailed_window_w,
                           label_window_w,
                           line_window_w,
                           pullright_debug_popup,
                           transviewshell = NULL,
                           tv_label_window_w;
Window                     button_window,
                           detailed_window,
                           label_window,
                           line_window,
                           tv_label_window;
Pixmap                     button_pixmap,
                           label_pixmap,
                           line_pixmap;
float                      max_bar_length;
int                        alias_length_set,
                           bar_thickness_2,
                           bar_thickness_3,
                           button_width,
                           depth,
                           even_height,
                           event_log_fd = STDERR_FILENO,
                           filename_display_length,
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           jid_fd = -1,
                           ft_exposure_tv_line = 0,
                           have_groups = NO,
                           hostname_display_length,
                           led_width,
                           *line_length = NULL,
                           max_line_length,
                           max_parallel_jobs_columns,
                           line_height = 0,
                           magic_value,
                           log_angle,
                           no_backing_store,
                           no_of_current_jobs,
                           no_of_dirs,
                           no_of_his_log,
                           no_of_job_ids = 0,
                           no_input,
                           no_selected,
                           no_selected_static,
                           no_of_active_process = 0,
                           no_of_columns,
                           no_of_rows,
                           no_of_rows_set,
                           no_of_hosts,
                           no_of_hosts_invisible,
                           no_of_hosts_visible,
                           no_of_jobs_selected,
                           sys_log_fd = STDERR_FILENO,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           sys_log_readfd,
#endif
                           tv_line_length,
                           tv_no_of_columns,
                           tv_no_of_rows,
                           *vpl,           /* Visible position list. */
                           window_width,
                           window_height,
                           x_center_receive_log,
                           x_center_sys_log,
                           x_center_trans_log,
                           x_offset_led,
                           x_offset_debug_led,
                           x_offset_proc,
                           x_offset_bars,
                           x_offset_characters,
                           x_offset_stat_leds,
                           x_offset_receive_log,
                           x_offset_sys_log,
                           x_offset_trans_log,
                           x_offset_log_history_left,
                           x_offset_log_history_right,
                           x_offset_tv_bars,
                           x_offset_tv_characters,
                           x_offset_tv_file_name,
                           x_offset_tv_job_number,
                           x_offset_tv_priority,
                           x_offset_tv_rotating_dash,
                           y_center_log,
                           y_offset_led;
XT_PTR_TYPE                current_alias_length = -1,
                           current_font = -1,
                           current_row = -1;
long                       danger_no_of_jobs,
                           link_max;
Dimension                  tv_window_height,
                           tv_window_width;
#ifdef HAVE_MMAP
off_t                      afd_active_size,
                           fra_size,
                           fsa_size,
                           jid_size;
#endif
time_t                     afd_active_time;
unsigned short             step_size;
unsigned long              color_pool[COLOR_POOL_SIZE],
                           redraw_time_host,
                           redraw_time_status;
unsigned int               *current_jid_list = NULL,
                           glyph_height,
                           glyph_width,
                           text_offset;
char                       work_dir[MAX_PATH_LENGTH],
                           *p_work_dir,
                           *pid_list,
                           afd_active_file[MAX_PATH_LENGTH],
                           *db_update_reply_fifo = NULL,
                           line_style,
                           other_options,
                           fake_user[MAX_FULL_USER_ID_LENGTH],
                           font_name[20],
                           title[MAX_AFD_NAME_LENGTH],
                           *info_data = NULL,
                           tv_window = OFF,
                           blink_flag,
                           profile[MAX_PROFILE_NAME_LENGTH + 1],
                           *ping_cmd = NULL,
                           *ptr_ping_cmd,
                           *traceroute_cmd = NULL,
                           *ptr_traceroute_cmd,
                           user[MAX_FULL_USER_ID_LENGTH];
unsigned char              *p_feature_flag,
                           saved_feature_flag;
clock_t                    clktck;
struct apps_list           *apps_list = NULL;
struct coord               coord[3][LOG_FIFO_SIZE];
struct line                *connect_data;
struct job_data            *jd = NULL;
struct afd_status          *p_afd_status,
                           prev_afd_status;
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;
struct job_id_data         *jid;
struct afd_control_perm    acp;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void                mafd_ctrl_exit(void),
                           create_pullright_alias_length(Widget),
                           create_pullright_debug(Widget),
                           create_pullright_font(Widget),
                           create_pullright_load(Widget),
                           create_pullright_other(Widget),
                           create_pullright_row(Widget),
                           create_pullright_style(Widget),
                           create_pullright_test(Widget),
                           eval_permissions(char *),
                           init_menu_bar(Widget, Widget *),
                           init_mafd_ctrl(int *, char **, char *),
                           init_popup_menu(Widget),
                           sig_bus(int),
                           sig_exit(int),
                           sig_segv(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char          window_title[100];
   static String fallback_res[] =
                 {
                    "*mwmDecorations : 42",
                    "*mwmFunctions : 12",
                    ".afd_ctrl*background : NavajoWhite2",
                    ".mafd_ctrl*background : NavajoWhite2",
                    ".afd_ctrl.Search Host.main_form.buttonbox*background : PaleVioletRed2",
                    ".mafd_ctrl.Search Host.main_form.buttonbox*background : PaleVioletRed2",
                    ".afd_ctrl.Search Host.main_form.buttonbox*foreground : Black",
                    ".mafd_ctrl.Search Host.main_form.buttonbox*foreground : Black",
                    ".afd_ctrl.Search Host.main_form.buttonbox*highlightColor : Black",
                    ".mafd_ctrl.Search Host.main_form.buttonbox*highlightColor : Black",
                    ".afd_ctrl.Search Host*background : NavajoWhite2",
                    ".mafd_ctrl.Search Host*background : NavajoWhite2",
                    ".afd_ctrl.Search Host*XmText.background : NavajoWhite1",
                    ".mafd_ctrl.Search Host*XmText.background : NavajoWhite1",
                    NULL
                 };
   Widget        mainform_w,
                 mainwindow,
                 menu_w;
   Screen        *screen;
   Arg           args[MAXARGS];
   Cardinal      argcount;
   uid_t         euid, /* Effective user ID. */
                 ruid; /* Real user ID. */

#ifdef WITH_MEMCHECK
   mtrace();
#endif
   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values. */
   init_mafd_ctrl(&argc, argv, window_title);

   /*
    * SSH wants to look at .Xauthority and with setuid flag
    * set we cannot do that. So when we initialize X lets temporaly
    * disable it. After XtAppInitialize() we set it back.
    */
   euid = geteuid();
   ruid = getuid();
   if (euid != ruid)
   {
      if (seteuid(ruid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d (from %d) : %s (%s %d)\n",
                       ruid, euid, strerror(errno), __FILE__, __LINE__);
      }
   }

   /* Create the top-level shell widget and initialise the toolkit. */
   argcount = 0;
   XtSetArg(args[argcount], XmNtitle, window_title); argcount++;
   appshell = XtAppInitialize(&app, "AFD", NULL, 0, &argc, argv,
                              fallback_res, args, argcount);
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
                                euid, ruid, strerror(errno), __FILE__, __LINE__);
               }
            }
         }
         else
         {
#endif
            (void)fprintf(stderr, "Failed to seteuid() to %d (from %d) : %s (%s %d)\n",
                          euid, ruid, strerror(errno), __FILE__, __LINE__);
#ifdef WITH_SETUID_PROGS
         }
#endif
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

#ifdef _X_DEBUG
   XSynchronize(display, 1);
#endif

   mainwindow = XtVaCreateManagedWidget("Main_window",
                                        xmMainWindowWidgetClass, appshell,
                                        NULL);

   /* Setup and determine window parameters. */
   setup_window(font_name, YES);

#ifdef HAVE_XPM
   /* Setup AFD logo as icon. */
   setup_icon(display, appshell);
#endif

   /* Get window size. */
   (void)window_size(&window_width, &window_height);

   /* Create managing widget for label, line and button widget. */
   mainform_w = XmCreateForm(mainwindow, "mainform_w", NULL, 0);
   XtManageChild(mainform_w);

   if (no_input == False)
   {
      init_menu_bar(mainform_w, &menu_w);
   }

   /* Setup colors. */
   default_cmap = DefaultColormap(display, DefaultScreen(display));
   init_color(XtDisplay(appshell));

   /* Create the label_window_w. */
   argcount = 0;
   XtSetArg(args[argcount], XmNheight, (Dimension) line_height);
   argcount++;
   XtSetArg(args[argcount], XmNwidth, (Dimension) window_width);
   argcount++;
   XtSetArg(args[argcount], XmNbackground, color_pool[LABEL_BG]);
   argcount++;
   if (no_input == False)
   {
      XtSetArg(args[argcount], XmNtopAttachment, XmATTACH_WIDGET);
      argcount++;
      XtSetArg(args[argcount], XmNtopWidget,     menu_w);
      argcount++;
   }
   else
   {
      XtSetArg(args[argcount], XmNtopAttachment, XmATTACH_FORM);
      argcount++;
   }
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   label_window_w = XmCreateDrawingArea(mainform_w, "label_window_w", args,
                                        argcount);
   XtManageChild(label_window_w);

   /* Get background color from the widget's resources. */
   argcount = 0;
   XtSetArg(args[argcount], XmNbackground, &color_pool[LABEL_BG]);
   argcount++;
   XtGetValues(label_window_w, args, argcount);

   /* Create the line_window_w. */
   argcount = 0;
   XtSetArg(args[argcount], XmNheight, (Dimension)(line_height * no_of_rows));
   argcount++;
   XtSetArg(args[argcount], XmNwidth, (Dimension)window_width);
   argcount++;
   XtSetArg(args[argcount], XmNbackground, color_pool[DEFAULT_BG]);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget, label_window_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   line_window_w = XmCreateDrawingArea(mainform_w, "line_window_w", args,
                                       argcount);
   XtManageChild(line_window_w);

   /* Initialise the GC's. */
   init_gcs();

   /* Get foreground color from the widget's resources. */
   argcount = 0;
   XtSetArg(args[argcount], XmNforeground, &color_pool[FG]); argcount++;
   XtGetValues(line_window_w, args, argcount);

   /* Create the button_window_w. */
   argcount = 0;
   XtSetArg(args[argcount], XmNheight, (Dimension)line_height);
   argcount++;
   XtSetArg(args[argcount], XmNwidth, (Dimension)window_width);
   argcount++;
   XtSetArg(args[argcount], XmNbackground, color_pool[LABEL_BG]);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment, XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget, line_window_w);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   button_window_w = XmCreateDrawingArea(mainform_w, "button_window_w", args,
                                         argcount);
   XtManageChild(button_window_w);

   /* Get background color from the widget's resources. */
   argcount = 0;
   XtSetArg(args[argcount], XmNbackground, &color_pool[LABEL_BG]);
   argcount++;
   XtGetValues(button_window_w, args, argcount);

   /* Add call back to handle expose events for the drawing areas. */
   XtAddCallback(label_window_w, XmNexposeCallback,
                 (XtCallbackProc)expose_handler_label, (XtPointer)0);
   XtAddCallback(line_window_w, XmNexposeCallback,
                 (XtCallbackProc)expose_handler_line, NULL);
   XtAddCallback(button_window_w, XmNexposeCallback,
                 (XtCallbackProc)expose_handler_button, NULL);

   if (no_input == False)
   {
      XtAddEventHandler(line_window_w,
                        EnterWindowMask | KeyPressMask | ButtonPressMask |
                        Button1MotionMask,
                        False, (XtEventHandler)input, NULL);

      /* Set toggle button for font|row|style. */
      XtVaSetValues(fw[current_font], XmNset, True, NULL);
      XtVaSetValues(rw[current_row], XmNset, True, NULL);
      XtVaSetValues(adl[current_alias_length - MIN_ALIAS_DISPLAY_LENGTH],
                    XmNset, True, NULL);
      if (line_style & SHOW_LEDS)
      {
         XtVaSetValues(lsw[LEDS_STYLE_W], XmNset, True, NULL);
      }
      if (line_style & SHOW_JOBS)
      {
         XtVaSetValues(ptw[0], XmNset, True, NULL);
         XtVaSetValues(ptw[1], XmNset, False, NULL);
         XtVaSetValues(ptw[2], XmNset, False, NULL);
      }
      else if (line_style & SHOW_JOBS_COMPACT)
           {
              XtVaSetValues(ptw[0], XmNset, False, NULL);
              XtVaSetValues(ptw[1], XmNset, True, NULL);
              XtVaSetValues(ptw[2], XmNset, False, NULL);
           }
           else
           {
              XtVaSetValues(ptw[0], XmNset, False, NULL);
              XtVaSetValues(ptw[1], XmNset, False, NULL);
              XtVaSetValues(ptw[2], XmNset, True, NULL);
           }
      if (line_style & SHOW_CHARACTERS)
      {
         XtVaSetValues(lsw[CHARACTERS_STYLE_W], XmNset, True, NULL);
      }
      if (line_style & SHOW_BARS)
      {
         XtVaSetValues(lsw[BARS_STYLE_W], XmNset, True, NULL);
      }
      if (other_options & FORCE_SHIFT_SELECT)
      {
         XtVaSetValues(oow[FORCE_SHIFT_SELECT_W], XmNset, True, NULL);
      }
      if (other_options & AUTO_SAVE)
      {
         XtVaSetValues(oow[AUTO_SAVE_W], XmNset, True, NULL);
      }
      if (other_options & FRAMED_GROUPS)
      {
         XtVaSetValues(oow[FRAMED_GROUPS_W], XmNset, True, NULL);
      }

      /* Setup popup menu. */
      init_popup_menu(line_window_w);

      XtAddEventHandler(line_window_w, EnterWindowMask | LeaveWindowMask,
                        False, (XtEventHandler)focus, NULL);
   }

#ifdef WITH_EDITRES
   XtAddEventHandler(appshell, (EventMask)0, True, _XEditResCheckMessages, NULL);
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
      (void)xrec(WARN_DIALOG, "Failed to set signal handlers for mafd_ctrl : %s",
                 strerror(errno));
   }

   /* Exit handler so we can close applications that the user started. */
   if (atexit(mafd_ctrl_exit) != 0)
   {
      (void)xrec(WARN_DIALOG,
                 "Failed to set exit handler for %s : %s\n\nWill not be able to close applications when terminating.",
                 AFD_CTRL, strerror(errno));
   }

   /* Get window ID of four main windows. */
   label_window = XtWindow(label_window_w);
   line_window = XtWindow(line_window_w);
   button_window = XtWindow(button_window_w);

   /* Create off-screen pixmaps. */
   screen = DefaultScreenOfDisplay(display);
   depth = DefaultDepthOfScreen(screen);
   label_pixmap = XCreatePixmap(display, label_window, window_width,
                                line_height, depth);
   line_pixmap = XCreatePixmap(display, line_window, window_width,
                               (line_height * no_of_rows), depth);
   button_pixmap = XCreatePixmap(display, button_window, window_width,
                                 line_height, depth);

   /* Start the main event-handling loop. */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++ init_mafd_ctrl() ++++++++++++++++++++++++++*/
static void
init_mafd_ctrl(int *argc, char *argv[], char *window_title)
{
   int          fd,
                gotcha,
                i,
                j,
                no_of_invisible_members = 0,
                prev_plus_minus,
                user_offset;
   unsigned int new_bar_length;
   time_t       current_time,
                start_time;
   char         afd_file_dir[MAX_PATH_LENGTH],
                *buffer,
                config_file[MAX_PATH_LENGTH],
                hostname[MAX_AFD_NAME_LENGTH],
                **hosts = NULL,
                **invisible_members = NULL,
                *perm_buffer;
   struct tms   tmsdummy;
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
                    "Usage: %s [-w <work_dir>] [-p <user profile>] [-u[ <fake user>]] [-no_input] [-f <numeric font name>] [-t <title>] [-bs]\n",
                    argv[0]);
      exit(SUCCESS);
   }

   /*
    * Determine the working directory. If it is not specified
    * in the command line try read it from the environment else
    * just take the default.
    */
   if (get_afd_path(argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;
#ifdef WITH_SETUID_PROGS
   set_afd_euid(work_dir);
#endif

   /* Do not start if binary dataset matches the one stort on disk. */
   if (check_typesize_data(NULL, stdout, NO) > 0)
   {
      (void)fprintf(stderr,
                    "The compiled binary does not match stored database.\n");
      (void)fprintf(stderr,
                    "Initialize database with the command : afd -i\n");
      exit(INCORRECT);
   }

   /* Disable all input? */
   if (get_arg(argc, argv, "-no_input", NULL, 0) == SUCCESS)
   {
      no_input = True;
   }
   else
   {
      no_input = False;
   }

   /* Disable backing store and save under? */
   if (get_arg(argc, argv, "-bs", NULL, 0) == SUCCESS)
   {
      no_backing_store = True;
   }
   else
   {
      no_backing_store = False;
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
   if (get_arg(argc, argv, "-f", font_name, 20) == INCORRECT)
   {
      (void)strcpy(font_name, DEFAULT_FONT);
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

      case NONE : /* User is not allowed to use this program. */
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

      case SUCCESS : /* Lets evaluate the permissions and see what */
                     /* the user may do.                           */
         eval_permissions(perm_buffer);
         free(perm_buffer);
         break;

      case INCORRECT : /* Hmm. Something did go wrong. Since we want to */
                       /* be able to disable permission checking let    */
                       /* the user have all permissions.                */
         acp.afd_ctrl_list            = NULL;
         acp.amg_ctrl                 = YES; /* Start/Stop the AMG    */
         acp.fd_ctrl                  = YES; /* Start/Stop the FD     */
         acp.rr_dc                    = YES; /* Reread DIR_CONFIG     */
         acp.rr_hc                    = YES; /* Reread HOST_CONFIG    */
         acp.startup_afd              = YES; /* Startup the AFD       */
         acp.shutdown_afd             = YES; /* Shutdown the AFD      */
         acp.handle_event             = YES;
         acp.handle_event_list        = NULL;
         acp.ctrl_transfer            = YES; /* Start/Stop a transfer */
         acp.ctrl_transfer_list       = NULL;
         acp.ctrl_queue               = YES; /* Start/Stop the input  */
         acp.ctrl_queue_list          = NULL;
         acp.ctrl_queue_transfer      = YES;
         acp.ctrl_queue_transfer_list = NULL;
         acp.switch_host              = YES;
         acp.switch_host_list         = NULL;
         acp.disable                  = YES;
         acp.disable_list             = NULL;
         acp.info                     = YES;
         acp.info_list                = NULL;
         acp.debug                    = YES;
         acp.debug_list               = NULL;
         acp.trace                    = YES;
         acp.full_trace               = YES;
         acp.simulation               = YES;
         acp.retry                    = YES;
         acp.retry_list               = NULL;
         acp.show_slog                = YES; /* View the system log   */
         acp.show_slog_list           = NULL;
         acp.show_elog                = YES; /* View the event log   */
         acp.show_elog_list           = NULL;
         acp.show_mlog                = YES; /* View the maintainer log */
         acp.show_mlog_list           = NULL;
         acp.show_rlog                = YES; /* View the receive log  */
         acp.show_rlog_list           = NULL;
         acp.show_tlog                = YES; /* View the transfer log */
         acp.show_tlog_list           = NULL;
         acp.show_tdlog               = YES; /* View the transfer debug log */
         acp.show_tdlog_list          = NULL;
         acp.show_ilog                = YES; /* View the input log    */
         acp.show_ilog_list           = NULL;
         acp.show_plog                = YES; /* View the production log */
         acp.show_plog_list           = NULL;
         acp.show_olog                = YES; /* View the output log   */
         acp.show_olog_list           = NULL;
         acp.show_dlog                = YES; /* View the delete log   */
         acp.show_dlog_list           = NULL;
         acp.afd_load                 = YES;
         acp.afd_load_list            = NULL;
         acp.view_jobs                = YES; /* View jobs             */
         acp.view_jobs_list           = NULL;
         acp.edit_hc                  = YES; /* Edit Host Config      */
         acp.edit_hc_list             = NULL;
         acp.view_dc                  = YES; /* View DIR_CONFIG entry */
         acp.view_dc_list             = NULL;
         acp.view_rr                  = YES; /* View DIR_CONFIG entry */
         acp.view_rr_list             = NULL;
         acp.dir_ctrl                 = YES;
         break;

      default : /* Impossible! */
         (void)fprintf(stderr,
                      "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   (void)strcpy(afd_active_file, p_work_dir);
   (void)strcat(afd_active_file, FIFO_DIR);
   (void)strcat(afd_active_file, AFD_ACTIVE_FILE);

   /* Prepare title for mafd_ctrl window. */
   window_title[0] = 'A'; window_title[1] = 'F'; window_title[2] = 'D';
   window_title[3] = '_'; window_title[4] = 'C'; window_title[5] = 'T';
   window_title[6] = 'R'; window_title[7] = 'L'; window_title[8] = ' ';
   window_title[9] = '\0';
   if (get_arg(argc, argv, "-t", title, MAX_AFD_NAME_LENGTH) == INCORRECT)
   {
      title[0] = '\0';
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
   }
   else
   {
      (void)strcat(window_title, title);
   }

   get_user(user, fake_user, user_offset);

   /*
    * Attach to the FSA and get the number of host
    * and the fsa_id of the FSA.
    */
   if ((i = fsa_attach("mafd_ctrl")) != SUCCESS)
   {
      if (i == INCORRECT_VERSION)
      {
         (void)fprintf(stderr,
                       "ERROR   : This program is not able to attach to the FSA due to incorrect version. (%s %d)\n",
                       __FILE__, __LINE__);
      }
      else
      {
         if (i < 0)
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to attach to FSA! (%s %d)\n",
                          __FILE__, __LINE__);
         }
         else
         {
            (void)fprintf(stderr,
                          "ERROR   : Failed to attach to FSA : %s (%s %d)\n",
                          strerror(i), __FILE__, __LINE__);
         }
      }
      exit(INCORRECT);
   }
   p_feature_flag = (unsigned char *)fsa - AFD_FEATURE_FLAG_OFFSET_END;
   saved_feature_flag = *p_feature_flag;

   if ((vpl = malloc((no_of_hosts * sizeof(int)))) == NULL)
   {
      (void)fprintf(stderr, "Failed to malloc() %ld bytes : %s (%s %d)\n",
                    (no_of_hosts * sizeof(int)), strerror(errno),
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Attach to the AFD Status Area.
    */
   if (attach_afd_status(NULL, WAIT_AFD_STATUS_ATTACH) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to attach to AFD status area. (%s %d)\n",
                    __FILE__, __LINE__);
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Failed to attach to AFD status area.");
      exit(INCORRECT);
   }

   if ((clktck = sysconf(_SC_CLK_TCK)) <= 0)
   {
      (void)fprintf(stderr, "Could not get clock ticks per second.\n");
      exit(INCORRECT);
   }

   (void)strcpy(afd_file_dir, work_dir);
   (void)strcat(afd_file_dir, AFD_FILE_DIR);
#ifdef _LINK_MAX_TEST
   link_max = LINKY_MAX;
#else
# ifdef REDUCED_LINK_MAX
   link_max = REDUCED_LINK_MAX;
# else                          
   if ((link_max = pathconf(afd_file_dir, _PC_LINK_MAX)) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "pathconf() _PC_LINK_MAX error, setting to %d : %s",
                 _POSIX_LINK_MAX, strerror(errno));
      link_max = _POSIX_LINK_MAX;
   }
# endif
#endif
   danger_no_of_jobs = link_max / 2;
   if (MAX_NO_PARALLEL_JOBS % 3)
   {
      max_parallel_jobs_columns = (MAX_NO_PARALLEL_JOBS / 3) + 1;
   }
   else
   {
      max_parallel_jobs_columns = MAX_NO_PARALLEL_JOBS / 3;
   }

   /*
    * Map to AFD_ACTIVE file, to check if all process are really
    * still alive.
    */
   if ((fd = open(afd_active_file, O_RDWR)) < 0)
   {
      pid_list = NULL;
   }
   else
   {
#ifdef HAVE_STATX
      if (statx(fd, "", AT_STATX_SYNC_AS_STAT | AT_EMPTY_PATH,
                STATX_SIZE | STATX_MTIME, &stat_buf) == -1)
#else
      if (fstat(fd, &stat_buf) == -1)
#endif
      {
         (void)fprintf(stderr, "WARNING : access error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         pid_list = NULL;
      }
      else
      {
#ifdef HAVE_MMAP
# ifdef HAVE_STATX
         afd_active_size = stat_buf.stx_size;
# else
         afd_active_size = stat_buf.st_size;
# endif
         if ((pid_list = mmap(0, afd_active_size, (PROT_READ | PROT_WRITE),
                              MAP_SHARED, fd, 0)) == (caddr_t) -1)
#else
         if ((pid_list = mmap_emu(0,
# ifdef HAVE_STATX
                                  stat_buf.st_size, (PROT_READ | PROT_WRITE),
# else
                                  stat_buf.st_size, (PROT_READ | PROT_WRITE),
# endif
                                  MAP_SHARED,
                                  afd_active_file, 0)) == (caddr_t) -1)
#endif
         {
            (void)fprintf(stderr, "WARNING : mmap() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            pid_list = NULL;
         }
#ifdef HAVE_STATX
         afd_active_time = stat_buf.stx_mtime.tv_sec;
#else
         afd_active_time = stat_buf.st_mtime;
#endif

         if (close(fd) == -1)
         {
            (void)fprintf(stderr, "WARNING : close() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
         }
      }
   }

   /* Allocate memory for local 'FSA'. */
   if ((connect_data = calloc(no_of_hosts + 1, sizeof(struct line))) == NULL)
   {
      (void)fprintf(stderr,
                    "Failed to calloc() %d bytes for %d hosts : %s (%s %d)\n",
                    (no_of_hosts * (int)sizeof(struct line)), no_of_hosts,
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Read setup file of this user.
    */
   other_options = DEFAULT_OTHER_OPTIONS;
   line_style = SHOW_LEDS | SHOW_JOBS | SHOW_CHARACTERS | SHOW_BARS;
   no_of_rows_set = DEFAULT_NO_OF_ROWS;
   alias_length_set = MAX_HOSTNAME_LENGTH;
   filename_display_length = DEFAULT_FILENAME_DISPLAY_LENGTH;
   hostname_display_length = DEFAULT_HOSTNAME_DISPLAY_LENGTH;
   RT_ARRAY(hosts, no_of_hosts, (MAX_REAL_HOSTNAME_LENGTH + 4 + 1), char);
   read_setup(AFD_CTRL, profile, &hostname_display_length,
              &filename_display_length, NULL,
              &no_of_invisible_members, &invisible_members);
   current_alias_length = hostname_display_length;

   /* Determine the default bar length. */
   max_bar_length  = 6 * BAR_LENGTH_MODIFIER;
   step_size = MAX_INTENSITY / max_bar_length;

   /* Initialise all display data for each host. */
   start_time = times(&tmsdummy);
   current_time = time(NULL);
   for (i = 0; i < no_of_hosts; i++)
   {
      (void)strcpy(connect_data[i].hostname, fsa[i].host_alias);
      connect_data[i].host_id = fsa[i].host_id;
      if (fsa[i].real_hostname[0][0] == GROUP_IDENTIFIER)
      {
         connect_data[i].type = GROUP_IDENTIFIER;
         have_groups = YES;
      }
      else
      {
         connect_data[i].type = NORMAL_IDENTIFIER;
      }
      if (no_of_invisible_members > 0)
      {
         if (connect_data[i].type == GROUP_IDENTIFIER)
         {
            gotcha = NO;
            for (j = 0; j < no_of_invisible_members; j++)
            {
               if (strcmp(connect_data[i].hostname, invisible_members[j]) == 0)
               {
                  connect_data[i].plus_minus = PM_CLOSE_STATE;
                  gotcha = YES;
                  break;
               }
            }
            if (gotcha == NO)
            {
               connect_data[i].plus_minus = PM_OPEN_STATE;
            }
         }
         else
         {
            connect_data[i].plus_minus = PM_OPEN_STATE;
         }
      }
      else
      {
         connect_data[i].plus_minus = PM_OPEN_STATE;
      }
      (void)snprintf(connect_data[i].host_display_str, MAX_HOSTNAME_LENGTH + 2,
                     "%-*s",
                     MAX_HOSTNAME_LENGTH + 1, fsa[i].host_dsp_name);
      connect_data[i].host_toggle = fsa[i].host_toggle;
      if (fsa[i].host_toggle_str[0] != '\0')
      {
         connect_data[i].host_toggle_display = fsa[i].host_toggle_str[(int)connect_data[i].host_toggle];
      }
      else
      {
         connect_data[i].host_toggle_display = fsa[i].host_dsp_name[(int)fsa[i].toggle_pos];
      }
      connect_data[i].start_time = start_time;
      connect_data[i].total_file_counter = fsa[i].total_file_counter;
      CREATE_FC_STRING(connect_data[i].str_tfc, connect_data[i].total_file_counter);
      connect_data[i].debug = fsa[i].debug;
      connect_data[i].host_status = fsa[i].host_status;
      connect_data[i].protocol = fsa[i].protocol;
      connect_data[i].special_flag = fsa[i].special_flag;
      connect_data[i].start_event_handle = fsa[i].start_event_handle;
      connect_data[i].end_event_handle = fsa[i].end_event_handle;
      if (connect_data[i].special_flag & HOST_DISABLED)
      {
         connect_data[i].stat_color_no = WHITE;
      }
      else if ((connect_data[i].special_flag & HOST_IN_DIR_CONFIG) == 0)
           {
              connect_data[i].stat_color_no = DEFAULT_BG;
           }
      else if (fsa[i].error_counter >= fsa[i].max_errors)
           {
              if ((connect_data[i].host_status & HOST_ERROR_OFFLINE) ||
                  ((connect_data[i].host_status & HOST_ERROR_OFFLINE_T) &&
                   ((connect_data[i].start_event_handle == 0) ||
                    (current_time >= connect_data[i].start_event_handle)) &&
                   ((connect_data[i].end_event_handle == 0) ||
                    (current_time <= connect_data[i].end_event_handle))) ||
                  (connect_data[i].host_status & HOST_ERROR_OFFLINE_STATIC))
              {
                 connect_data[i].stat_color_no = ERROR_OFFLINE_ID;
              }
              else if ((connect_data[i].host_status & HOST_ERROR_ACKNOWLEDGED) ||
                       ((connect_data[i].host_status & HOST_ERROR_ACKNOWLEDGED_T) &&
                        ((connect_data[i].start_event_handle == 0) ||
                         (current_time >= connect_data[i].start_event_handle)) &&
                        ((connect_data[i].end_event_handle == 0) ||
                         (current_time <= connect_data[i].end_event_handle))))
                   {
                      connect_data[i].stat_color_no = ERROR_ACKNOWLEDGED_ID;
                   }
                   else
                   {
                      connect_data[i].stat_color_no = NOT_WORKING2;
                   }
           }
      else if (connect_data[i].host_status & HOST_WARN_TIME_REACHED)
           {
              if ((connect_data[i].host_status & HOST_ERROR_OFFLINE) ||
                  ((connect_data[i].host_status & HOST_ERROR_OFFLINE_T) &&
                   ((connect_data[i].start_event_handle == 0) ||
                    (current_time >= connect_data[i].start_event_handle)) &&
                   ((connect_data[i].end_event_handle == 0) ||
                    (current_time <= connect_data[i].end_event_handle))) ||
                  (connect_data[i].host_status & HOST_ERROR_OFFLINE_STATIC))
              {
                 connect_data[i].stat_color_no = ERROR_OFFLINE_ID;
              }
              else if ((connect_data[i].host_status & HOST_ERROR_ACKNOWLEDGED) ||
                       ((connect_data[i].host_status & HOST_ERROR_ACKNOWLEDGED_T) &&
                        ((connect_data[i].start_event_handle == 0) ||
                         (current_time >= connect_data[i].start_event_handle)) &&
                        ((connect_data[i].end_event_handle == 0) ||
                         (current_time <= connect_data[i].end_event_handle))))
                   {
                      connect_data[i].stat_color_no = ERROR_ACKNOWLEDGED_ID;
                   }
                   else
                   {
                      connect_data[i].stat_color_no = WARNING_ID;
                   }
           }
      else if (fsa[i].active_transfers > 0)
           {
              connect_data[i].stat_color_no = TRANSFER_ACTIVE;
           }
           else
           {
              connect_data[i].stat_color_no = NORMAL_STATUS;
           }
      if (connect_data[i].host_status & PAUSE_QUEUE_STAT)
      {
         connect_data[i].status_led[0] = PAUSE_QUEUE;
      }
      else if ((connect_data[i].host_status & AUTO_PAUSE_QUEUE_STAT) ||
               (connect_data[i].host_status & DANGER_PAUSE_QUEUE_STAT))
           {
              connect_data[i].status_led[0] = AUTO_PAUSE_QUEUE;
           }
#ifdef WITH_ERROR_QUEUE
      else if (connect_data[i].host_status & ERROR_QUEUE_SET)
           {
              connect_data[i].status_led[0] = JOBS_IN_ERROR_QUEUE;
           }
#endif
           else
           {
              connect_data[i].status_led[0] = NORMAL_STATUS;
           }
      if (connect_data[i].host_status & STOP_TRANSFER_STAT)
      {
         connect_data[i].status_led[1] = STOP_TRANSFER;
      }
      else if (connect_data[i].host_status & SIMULATE_SEND_MODE)
           {
              connect_data[i].status_led[1] = SIMULATE_MODE;
           }
           else
           {
              connect_data[i].status_led[1] = NORMAL_STATUS;
           }
      connect_data[i].status_led[2] = (connect_data[i].protocol >> 30);
      connect_data[i].total_file_size = fsa[i].total_file_size;
      CREATE_FS_STRING(connect_data[i].str_tfs, connect_data[i].total_file_size);
      connect_data[i].bytes_per_sec = 0;
      (void)strcpy(connect_data[i].str_tr, "  0B");
      connect_data[i].average_tr = 0.0;
      connect_data[i].max_average_tr = 0.0;
      connect_data[i].max_errors = fsa[i].max_errors;
      connect_data[i].error_counter = fsa[i].error_counter;
      CREATE_EC_STRING(connect_data[i].str_ec, connect_data[i].error_counter);
      if (connect_data[i].max_errors < 1)
      {
         connect_data[i].scale = (double)max_bar_length;
      }
      else
      {
         connect_data[i].scale = max_bar_length / connect_data[i].max_errors;
      }
      if ((new_bar_length = (connect_data[i].error_counter * connect_data[i].scale)) > 0)
      {
         if (new_bar_length >= max_bar_length)
         {
            connect_data[i].bar_length[ERROR_BAR_NO] = max_bar_length;
            connect_data[i].red_color_offset = MAX_INTENSITY;
            connect_data[i].green_color_offset = 0;
         }
         else
         {
            connect_data[i].bar_length[ERROR_BAR_NO] = new_bar_length;
            connect_data[i].red_color_offset = new_bar_length * step_size;
            connect_data[i].green_color_offset = MAX_INTENSITY - connect_data[i].red_color_offset;
         }
      }
      else
      {
         connect_data[i].bar_length[ERROR_BAR_NO] = 0;
         connect_data[i].red_color_offset = 0;
         connect_data[i].green_color_offset = MAX_INTENSITY;
      }
      connect_data[i].bar_length[TR_BAR_NO] = 0;
      connect_data[i].inverse = OFF;
      connect_data[i].allowed_transfers = fsa[i].allowed_transfers;
      for (j = 0; j < connect_data[i].allowed_transfers; j++)
      {
         connect_data[i].no_of_files[j] = fsa[i].job_status[j].no_of_files - fsa[i].job_status[j].no_of_files_done;
         connect_data[i].bytes_send[j] = fsa[i].job_status[j].bytes_send;
         connect_data[i].connect_status[j] = fsa[i].job_status[j].connect_status;
         connect_data[i].detailed_selection[j] = NO;
      }
   }
   FREE_RT_ARRAY(hosts);

   if (invisible_members != NULL)
   {
      FREE_RT_ARRAY(invisible_members);
   }

   prev_plus_minus = PM_OPEN_STATE;
   no_of_hosts_visible = 0;
   for (i = 0; i < no_of_hosts; i++)
   {
      if (connect_data[i].type == GROUP_IDENTIFIER)
      {
         prev_plus_minus = connect_data[i].plus_minus;
      }
      else
      {
         connect_data[i].plus_minus = prev_plus_minus;
      }

      if ((connect_data[i].plus_minus == PM_OPEN_STATE) ||
          (connect_data[i].type == GROUP_IDENTIFIER))
      {
         vpl[no_of_hosts_visible] = i;
         no_of_hosts_visible++;
      }
   }
   no_of_hosts_invisible = no_of_hosts - no_of_hosts_visible;

   /*
    * Initialise all data for AFD status area.
    */
   prev_afd_status.amg = p_afd_status->amg;
   prev_afd_status.fd = p_afd_status->fd;
   prev_afd_status.archive_watch = p_afd_status->archive_watch;
   prev_afd_status.afdd = p_afd_status->afdd;
   if ((prev_afd_status.fd == OFF) ||
       (prev_afd_status.amg == OFF) ||
       (prev_afd_status.archive_watch == OFF))
   {
      blink_flag = ON;
   }
   else
   {
      blink_flag = OFF;
   }
   prev_afd_status.sys_log = p_afd_status->sys_log;
   prev_afd_status.receive_log = p_afd_status->receive_log;
   prev_afd_status.trans_log = p_afd_status->trans_log;
   prev_afd_status.trans_db_log = p_afd_status->trans_db_log;
   prev_afd_status.receive_log_ec = p_afd_status->receive_log_ec;
   memcpy(prev_afd_status.receive_log_fifo, p_afd_status->receive_log_fifo,
          LOG_FIFO_SIZE + 1);
   prev_afd_status.sys_log_ec = p_afd_status->sys_log_ec;
   memcpy(prev_afd_status.sys_log_fifo, p_afd_status->sys_log_fifo,
          LOG_FIFO_SIZE + 1);
   prev_afd_status.trans_log_ec = p_afd_status->trans_log_ec;
   memcpy(prev_afd_status.trans_log_fifo, p_afd_status->trans_log_fifo,
          LOG_FIFO_SIZE + 1);
   prev_afd_status.jobs_in_queue = p_afd_status->jobs_in_queue;
   (void)memcpy(prev_afd_status.receive_log_history,
                p_afd_status->receive_log_history, MAX_LOG_HISTORY);
   (void)memcpy(prev_afd_status.sys_log_history,
                p_afd_status->sys_log_history, MAX_LOG_HISTORY);
   (void)memcpy(prev_afd_status.trans_log_history,
                p_afd_status->trans_log_history, MAX_LOG_HISTORY);

   log_angle = 360 / LOG_FIFO_SIZE;
   no_selected = no_selected_static = 0;
   redraw_time_host = redraw_time_status = STARTING_REDRAW_TIME;

   (void)snprintf(config_file, MAX_PATH_LENGTH, "%s%s%s",
                  p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_file, F_OK) == 0) &&
       (read_file_no_cr(config_file, &buffer, YES, __FILE__, __LINE__) != INCORRECT))
   {
      int  str_length;
      char value[MAX_PATH_LENGTH];

      if (get_definition(buffer, PING_CMD_DEF,
                         value, MAX_PATH_LENGTH) != NULL)
      {
         str_length = strlen(value);
         if (str_length > 0)
         {
            if ((ping_cmd = malloc(str_length + 4 +
                                   MAX_REAL_HOSTNAME_LENGTH + 1 +
                                   MAX_HOSTNAME_LENGTH + 2)) == NULL)
            {
               (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            ping_cmd[0] = '"';
            (void)strcpy(&ping_cmd[1], value);
            ping_cmd[str_length + 1] = ' ';
            ptr_ping_cmd = ping_cmd + str_length + 2;
         }
      }
      if (get_definition(buffer, TRACEROUTE_CMD_DEF,
                         value, MAX_PATH_LENGTH) != NULL)
      {
         str_length = strlen(value);
         if (str_length > 0)
         {
            if ((traceroute_cmd = malloc(str_length + 4 +
                                         MAX_REAL_HOSTNAME_LENGTH + 1 +
                                         MAX_HOSTNAME_LENGTH + 2)) == NULL)
            {
               (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                             strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
            traceroute_cmd[0] = '"';
            (void)strcpy(&traceroute_cmd[1], value);
            traceroute_cmd[str_length + 1] = ' ';
            ptr_traceroute_cmd = traceroute_cmd + str_length + 2;
         }
      }
      free(buffer);
   }

   return;
}


/*+++++++++++++++++++++++++++ init_menu_bar() +++++++++++++++++++++++++++*/
static void
init_menu_bar(Widget mainform_w, Widget *menu_w)
{
   Arg      args[MAXARGS];
   Cardinal argcount;
   Widget   pull_down_w,
            pullright_alias_length,
            pullright_font,
            pullright_load,
            pullright_row,
            pullright_line_style,
            pullright_other_options;

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNpacking,         XmPACK_TIGHT);
   argcount++;
   XtSetArg(args[argcount], XmNmarginHeight,    0);
   argcount++;
   XtSetArg(args[argcount], XmNmarginWidth,     0);
   argcount++;
   *menu_w = XmCreateSimpleMenuBar(mainform_w, "Menu Bar", args, argcount);

   /**********************************************************************/
   /*                           Host Menu                                */
   /**********************************************************************/
   argcount = 0;
   XtSetArg(args[argcount], XmNtearOffModel,   XmTEAR_OFF_ENABLED);
   argcount++;
   pull_down_w = XmCreatePulldownMenu(*menu_w, "Host Pulldown", args, argcount);
   mw[HOST_W] = XtVaCreateManagedWidget("Host",
                           xmCascadeButtonWidgetClass, *menu_w,
                           XmNfontList,                fontlist,
#ifdef WHEN_WE_KNOW_HOW_TO_FIX_THIS
   /* Enabling this causes on some system the following error code:        */
   /* Illegal mnemonic character;  Could not convert X KEYSYM to a keycode */
   /* Don't know how to fix it, so just leave it away.                     */
                           XmNmnemonic,                't',
#endif
                           XmNsubMenuId,               pull_down_w,
                           NULL);
   if ((acp.handle_event != NO_PERMISSION) ||
       (acp.ctrl_queue != NO_PERMISSION) ||
       (acp.ctrl_transfer != NO_PERMISSION) ||
       (acp.ctrl_queue_transfer != NO_PERMISSION) ||
       (acp.disable != NO_PERMISSION) ||
       (acp.switch_host != NO_PERMISSION) ||
       (acp.retry != NO_PERMISSION) ||
       (acp.debug != NO_PERMISSION) ||
       (acp.trace != NO_PERMISSION) ||
       (acp.full_trace != NO_PERMISSION) ||
       (acp.simulation != NO_PERMISSION) ||
       (ping_cmd != NULL) ||
       (traceroute_cmd != NULL) ||
       (acp.afd_load != NO_PERMISSION))
   {
      if (acp.handle_event != NO_PERMISSION)
      {
         ow[HANDLE_EVENT_W] = XtVaCreateManagedWidget("Handle event",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(ow[HANDLE_EVENT_W], XmNactivateCallback, popup_cb,
                       (XtPointer)EVENT_SEL);
      }
      if (acp.ctrl_queue != NO_PERMISSION)
      {
         ow[QUEUE_W] = XtVaCreateManagedWidget("Start/Stop input queue",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(ow[QUEUE_W], XmNactivateCallback, popup_cb,
                       (XtPointer)QUEUE_SEL);
      }
      if (acp.ctrl_transfer != NO_PERMISSION)
      {
         ow[TRANSFER_W] = XtVaCreateManagedWidget("Start/Stop transfer",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(ow[TRANSFER_W], XmNactivateCallback, popup_cb,
                       (XtPointer)TRANS_SEL);
      }
      if (acp.ctrl_queue_transfer != NO_PERMISSION)
      {
         ow[QUEUE_TRANSFER_W] = XtVaCreateManagedWidget("Start/Stop host",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(ow[QUEUE_TRANSFER_W], XmNactivateCallback, popup_cb,
                       (XtPointer)QUEUE_TRANS_SEL);
      }
      if (acp.disable != NO_PERMISSION)
      {
         ow[DISABLE_W] = XtVaCreateManagedWidget("Enable/Disable host",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(ow[DISABLE_W], XmNactivateCallback, popup_cb,
                       (XtPointer)DISABLE_SEL);
      }
      if (acp.switch_host != NO_PERMISSION)
      {
         ow[SWITCH_W] = XtVaCreateManagedWidget("Switch host",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(ow[SWITCH_W], XmNactivateCallback, popup_cb,
                       (XtPointer)SWITCH_SEL);
      }
      if (acp.retry != NO_PERMISSION)
      {
#ifdef WITH_CTRL_ACCELERATOR
         ow[RETRY_W] = XtVaCreateManagedWidget("Retry                (Ctrl+r)",
#else
         ow[RETRY_W] = XtVaCreateManagedWidget("Retry                (Alt+r)",
#endif
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
#ifdef WHEN_WE_KNOW_HOW_TO_FIX_THIS
                                 XmNmnemonic,             'R',
#endif
#ifdef WITH_CTRL_ACCELERATOR
                                 XmNaccelerator,          "Ctrl<Key>R",
#else
                                 XmNaccelerator,          "Alt<Key>R",
#endif
                                 NULL);
         XtAddCallback(ow[RETRY_W], XmNactivateCallback, popup_cb,
                       (XtPointer)RETRY_SEL);
      }
      if ((acp.debug != NO_PERMISSION) ||
          (acp.trace != NO_PERMISSION) ||
          (acp.full_trace != NO_PERMISSION))
      {
         Widget pullright_debug_menu;

         pullright_debug_menu = XmCreateSimplePulldownMenu(pull_down_w,
                                                           "pullright_debug_menu",
                                                           NULL, 0);
         ow[DEBUG_W] = XtVaCreateManagedWidget("Debug",
                                 xmCascadeButtonWidgetClass, pull_down_w,
                                 XmNfontList,                fontlist,
                                 XmNsubMenuId,               pullright_debug_menu,
                                 NULL);
         create_pullright_debug(pullright_debug_menu);
      }
      if (acp.simulation != NO_PERMISSION)
      {
         ow[SIMULATION_W] = XtVaCreateManagedWidget("Simulate mode",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(ow[SIMULATION_W], XmNactivateCallback, popup_cb,
                       (XtPointer)SIMULATION_SEL);
      }
#ifdef WITH_CTRL_ACCELERATOR
      ow[SELECT_W] = XtVaCreateManagedWidget("Search + (De)Select  (Ctrl+s)",
#else
      ow[SELECT_W] = XtVaCreateManagedWidget("Search + (De)Select  (Alt+s)",
#endif
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
#ifdef WHEN_WE_KNOW_HOW_TO_FIX_THIS
                                 XmNmnemonic,             'S',
#endif
#ifdef WITH_CTRL_ACCELERATOR
                                 XmNaccelerator,          "Ctrl<Key>S",
#else
                                 XmNaccelerator,          "Alt<Key>S",
#endif
                                 NULL);
      XtAddCallback(ow[SELECT_W], XmNactivateCallback, select_host_dialog,
                    (XtPointer)0);
      if ((traceroute_cmd != NULL) || (ping_cmd != NULL))
      {
         Widget pullright_test;

         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, pull_down_w,
                                 NULL);
         pullright_test = XmCreateSimplePulldownMenu(pull_down_w,
                                                     "pullright_test", NULL, 0);
         ow[TEST_W] = XtVaCreateManagedWidget("Test",
                                 xmCascadeButtonWidgetClass, pull_down_w,
                                 XmNfontList,                fontlist,
                                 XmNsubMenuId,               pullright_test,
                                 NULL);
         create_pullright_test(pullright_test);
      }
      if (acp.afd_load != NO_PERMISSION)
      {
         pullright_load = XmCreateSimplePulldownMenu(pull_down_w,
                                                     "pullright_load", NULL, 0);
         ow[VIEW_LOAD_W] = XtVaCreateManagedWidget("Load",
                                 xmCascadeButtonWidgetClass, pull_down_w,
                                 XmNfontList,                fontlist,
                                 XmNsubMenuId,               pullright_load,
                                 NULL);
         create_pullright_load(pullright_load);
      }
      XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, pull_down_w,
                              XmNseparatorType,       XmDOUBLE_LINE,
                              NULL);
   }

#ifdef WITH_CTRL_ACCELERATOR
   ow[EXIT_W] = XtVaCreateManagedWidget("Exit                 (Ctrl+x)",
#else
   ow[EXIT_W] = XtVaCreateManagedWidget("Exit                 (Alt+x)",
#endif
                           xmPushButtonWidgetClass, pull_down_w,
                           XmNfontList,             fontlist,
#ifdef WHEN_WE_KNOW_HOW_TO_FIX_THIS
                           XmNmnemonic,             'x',
#endif
#ifdef WITH_CTRL_ACCELERATOR
                           XmNaccelerator,          "Ctrl<Key>x",
#else
                           XmNaccelerator,          "Alt<Key>x",
#endif
                           NULL);
   XtAddCallback(ow[EXIT_W], XmNactivateCallback, popup_cb,
                 (XtPointer)EXIT_SEL);

   /**********************************************************************/
   /*                           View Menu                                */
   /**********************************************************************/
   if ((acp.show_slog != NO_PERMISSION) || (acp.show_mlog != NO_PERMISSION) ||
       (acp.show_elog != NO_PERMISSION) || (acp.show_rlog != NO_PERMISSION) ||
       (acp.show_tlog != NO_PERMISSION) || (acp.show_dlog != NO_PERMISSION) ||
       (acp.show_ilog != NO_PERMISSION) || (acp.show_plog != NO_PERMISSION) ||
       (acp.show_olog != NO_PERMISSION) || (acp.show_dlog != NO_PERMISSION) ||
       (acp.show_queue != NO_PERMISSION) || (acp.info != NO_PERMISSION) ||
       (acp.view_dc != NO_PERMISSION) || (acp.view_rr != NO_PERMISSION) ||
       (acp.view_jobs != NO_PERMISSION))
   {
      pull_down_w = XmCreatePulldownMenu(*menu_w,
                                              "View Pulldown", NULL, 0);
      XtVaSetValues(pull_down_w,
                    XmNtearOffModel, XmTEAR_OFF_ENABLED,
                    NULL);
      mw[LOG_W] = XtVaCreateManagedWidget("View",
                              xmCascadeButtonWidgetClass, *menu_w,
                              XmNfontList,                fontlist,
#ifdef WHEN_WE_KNOW_HOW_TO_FIX_THIS
                              XmNmnemonic,                'V',
#endif
                              XmNsubMenuId,               pull_down_w,
                              NULL);
      if (acp.show_slog != NO_PERMISSION)
      {
         vw[SYSTEM_W] = XtVaCreateManagedWidget("System Log",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(vw[SYSTEM_W], XmNactivateCallback, popup_cb,
                       (XtPointer)S_LOG_SEL);
      }
#ifdef _MAINTAINER_LOG
      if (acp.show_mlog != NO_PERMISSION)
      {
         vw[MAINTAINER_W] = XtVaCreateManagedWidget("Maintainer Log",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(vw[MAINTAINER_W], XmNactivateCallback, popup_cb,
                       (XtPointer)M_LOG_SEL);
      }
#endif
      if (acp.show_elog != NO_PERMISSION)
      {
         vw[EVENT_W] = XtVaCreateManagedWidget("Event Log",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(vw[EVENT_W], XmNactivateCallback, popup_cb,
                       (XtPointer)E_LOG_SEL);
      }
      if (acp.show_rlog != NO_PERMISSION)
      {
         vw[RECEIVE_W] = XtVaCreateManagedWidget("Receive Log",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(vw[RECEIVE_W], XmNactivateCallback, popup_cb,
                       (XtPointer)R_LOG_SEL);
      }
      if (acp.show_tlog != NO_PERMISSION)
      {
         vw[TRANS_W] = XtVaCreateManagedWidget("Transfer Log",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(vw[TRANS_W], XmNactivateCallback, popup_cb,
                       (XtPointer)T_LOG_SEL);
      }
      if (acp.show_tdlog != NO_PERMISSION)
      {
         vw[TRANS_DEBUG_W] = XtVaCreateManagedWidget("Transfer Debug Log",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(vw[TRANS_DEBUG_W], XmNactivateCallback, popup_cb,
                       (XtPointer)TD_LOG_SEL);
      }
      if ((acp.show_ilog != NO_PERMISSION) ||
          (acp.show_plog != NO_PERMISSION) ||
          (acp.show_olog != NO_PERMISSION) ||
          (acp.show_dlog != NO_PERMISSION))
      {
#if defined (_INPUT_LOG) || defined (_PRODUCTION_LOG) || defined (_OUTPUT_LOG) || defined (_DELETE_LOG)
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, pull_down_w,
                                 NULL);
#endif
#ifdef _INPUT_LOG
         if (acp.show_ilog != NO_PERMISSION)
         {
            vw[INPUT_W] = XtVaCreateManagedWidget("Input Log",
                                    xmPushButtonWidgetClass, pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(vw[INPUT_W], XmNactivateCallback, popup_cb,
                          (XtPointer)I_LOG_SEL);
         }
#endif
#ifdef _PRODUCTION_LOG
         if (acp.show_plog != NO_PERMISSION)
         {
            vw[PRODUCTION_W] = XtVaCreateManagedWidget("Production Log",
                                    xmPushButtonWidgetClass, pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(vw[PRODUCTION_W], XmNactivateCallback, popup_cb,
                          (XtPointer)P_LOG_SEL);
         }
#endif
#ifdef _OUTPUT_LOG
         if (acp.show_olog != NO_PERMISSION)
         {
            vw[OUTPUT_W] = XtVaCreateManagedWidget("Output Log",
                                    xmPushButtonWidgetClass, pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(vw[OUTPUT_W], XmNactivateCallback, popup_cb,
                          (XtPointer)O_LOG_SEL);
         }
#endif
#ifdef _DELETE_LOG
         if (acp.show_dlog != NO_PERMISSION)
         {
            vw[DELETE_W] = XtVaCreateManagedWidget("Delete Log",
                                    xmPushButtonWidgetClass, pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(vw[DELETE_W], XmNactivateCallback, popup_cb,
                          (XtPointer)D_LOG_SEL);
         }
#endif
      }
      if (acp.show_queue != NO_PERMISSION)
      {
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, pull_down_w,
                                 NULL);
         vw[SHOW_QUEUE_W] = XtVaCreateManagedWidget("Queue",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(vw[SHOW_QUEUE_W], XmNactivateCallback, popup_cb,
                       (XtPointer)SHOW_QUEUE_SEL);
      }
      if ((acp.info != NO_PERMISSION) || (acp.view_dc != NO_PERMISSION) ||
          (acp.view_rr != NO_PERMISSION))
      {
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, pull_down_w,
                                 NULL);
         if (acp.info != NO_PERMISSION)
         {
            vw[INFO_W] = XtVaCreateManagedWidget("Info",
                                    xmPushButtonWidgetClass, pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(vw[INFO_W], XmNactivateCallback, popup_cb,
                          (XtPointer)INFO_SEL);
         }
         if (acp.view_dc != NO_PERMISSION)
         {
            vw[VIEW_DC_W] = XtVaCreateManagedWidget("Configuration",
                                    xmPushButtonWidgetClass, pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(vw[VIEW_DC_W], XmNactivateCallback, popup_cb,
                          (XtPointer)VIEW_DC_SEL);
         }
         if (acp.view_rr != NO_PERMISSION)
         {
            vw[VIEW_RR_W] = XtVaCreateManagedWidget("Rename rules",
                                    xmPushButtonWidgetClass, pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(vw[VIEW_RR_W], XmNactivateCallback, popup_cb,
                          (XtPointer)VIEW_RR_SEL);
         }
      }
      if (acp.view_jobs != NO_PERMISSION)
      {
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, pull_down_w,
                                 NULL);
         vw[VIEW_JOB_W] = XtVaCreateManagedWidget("Job details",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(vw[VIEW_JOB_W], XmNactivateCallback, popup_cb,
                       (XtPointer)VIEW_JOB_SEL);
      }
   }

   /**********************************************************************/
   /*                          Control Menu                              */
   /**********************************************************************/
   if ((acp.amg_ctrl != NO_PERMISSION) || (acp.fd_ctrl != NO_PERMISSION) ||
       (acp.rr_dc != NO_PERMISSION) || (acp.rr_hc != NO_PERMISSION) ||
       (acp.edit_hc != NO_PERMISSION) || (acp.startup_afd != NO_PERMISSION) ||
       (acp.shutdown_afd != NO_PERMISSION) || (acp.dir_ctrl != NO_PERMISSION))
   {
      pull_down_w = XmCreatePulldownMenu(*menu_w, "Control Pulldown", NULL, 0);
      XtVaSetValues(pull_down_w, XmNtearOffModel, XmTEAR_OFF_ENABLED, NULL);
      mw[CONTROL_W] = XtVaCreateManagedWidget("Control",
                              xmCascadeButtonWidgetClass, *menu_w,
                              XmNfontList,                fontlist,
#ifdef WHEN_WE_KNOW_HOW_TO_FIX_THIS
                              XmNmnemonic,                'C',
#endif
                              XmNsubMenuId,               pull_down_w,
                              NULL);
      if (acp.amg_ctrl != NO_PERMISSION)
      {
         cw[AMG_CTRL_W] = XtVaCreateManagedWidget("Start/Stop AMG",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(cw[AMG_CTRL_W], XmNactivateCallback,
                       control_cb, (XtPointer)CONTROL_AMG_SEL);
      }
      if (acp.fd_ctrl != NO_PERMISSION)
      {
         cw[FD_CTRL_W] = XtVaCreateManagedWidget("Start/Stop FD",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(cw[FD_CTRL_W], XmNactivateCallback,
                       control_cb, (XtPointer)CONTROL_FD_SEL);
      }
      if ((acp.rr_dc != NO_PERMISSION) || (acp.rr_hc != NO_PERMISSION))
      {
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, pull_down_w,
                                 NULL);
         if (acp.rr_dc != NO_PERMISSION)
         {
            cw[RR_DC_W] = XtVaCreateManagedWidget("Reread DIR_CONFIG",
                                    xmPushButtonWidgetClass, pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(cw[RR_DC_W], XmNactivateCallback,
                          control_cb, (XtPointer)REREAD_DIR_CONFIG_SEL);
         }
         if (acp.rr_hc != NO_PERMISSION)
         {
            cw[RR_HC_W] = XtVaCreateManagedWidget("Reread HOST_CONFIG",
                                    xmPushButtonWidgetClass, pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(cw[RR_HC_W], XmNactivateCallback,
                          control_cb, (XtPointer)REREAD_HOST_CONFIG_SEL);
         }
      }
      if (acp.edit_hc != NO_PERMISSION)
      {
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, pull_down_w,
                                 NULL);
         cw[EDIT_HC_W] = XtVaCreateManagedWidget("Edit HOST_CONFIG",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(cw[EDIT_HC_W], XmNactivateCallback,
                       popup_cb, (XtPointer)EDIT_HC_SEL);
      }
      if (acp.dir_ctrl != NO_PERMISSION)
      {
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, pull_down_w,
                                 NULL);
         cw[DIR_CTRL_W] = XtVaCreateManagedWidget("Directory Control",
                                 xmPushButtonWidgetClass, pull_down_w,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(cw[DIR_CTRL_W], XmNactivateCallback,
                       popup_cb, (XtPointer)DIR_CTRL_SEL);
      }

      /* Startup/Shutdown of AFD. */
      if ((acp.startup_afd != NO_PERMISSION) ||
          (acp.shutdown_afd != NO_PERMISSION))
      {
         XtVaCreateManagedWidget("Separator",
                                 xmSeparatorWidgetClass, pull_down_w,
                                 NULL);
         if (acp.startup_afd != NO_PERMISSION)
         {
            cw[STARTUP_AFD_W] = XtVaCreateManagedWidget("Startup AFD",
                                    xmPushButtonWidgetClass, pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(cw[STARTUP_AFD_W], XmNactivateCallback,
                          control_cb, (XtPointer)STARTUP_AFD_SEL);
         }
         if (acp.shutdown_afd != NO_PERMISSION)
         {
            cw[SHUTDOWN_AFD_W] = XtVaCreateManagedWidget("Shutdown AFD",
                                    xmPushButtonWidgetClass, pull_down_w,
                                    XmNfontList,             fontlist,
                                    NULL);
            XtAddCallback(cw[SHUTDOWN_AFD_W], XmNactivateCallback,
                          control_cb, (XtPointer)SHUTDOWN_AFD_SEL);
         }
      }
   }

   /**********************************************************************/
   /*                           Setup Menu                               */
   /**********************************************************************/
   pull_down_w = XmCreatePulldownMenu(*menu_w, "Setup Pulldown", NULL, 0);
   XtVaSetValues(pull_down_w, XmNtearOffModel, XmTEAR_OFF_ENABLED, NULL);
   pullright_font = XmCreateSimplePulldownMenu(pull_down_w,
                                               "pullright_font", NULL, 0);
   pullright_row = XmCreateSimplePulldownMenu(pull_down_w,
                                              "pullright_row", NULL, 0);
   pullright_alias_length = XmCreateSimplePulldownMenu(pull_down_w,
                                              "pullright_alias_length", NULL, 0);
   pullright_line_style = XmCreateSimplePulldownMenu(pull_down_w,
                                              "pullright_line_style", NULL, 0);
   pullright_other_options = XmCreateSimplePulldownMenu(pull_down_w,
                                              "pullright_other_options", NULL, 0);
   mw[CONFIG_W] = XtVaCreateManagedWidget("Setup",
                           xmCascadeButtonWidgetClass, *menu_w,
                           XmNfontList,                fontlist,
#ifdef WHEN_WE_KNOW_HOW_TO_FIX_THIS
                           XmNmnemonic,                'p',
#endif
                           XmNsubMenuId,               pull_down_w,
                           NULL);
   sw[AFD_FONT_W] = XtVaCreateManagedWidget("Font size",
                           xmCascadeButtonWidgetClass, pull_down_w,
                           XmNfontList,                fontlist,
                           XmNsubMenuId,               pullright_font,
                           NULL);
   create_pullright_font(pullright_font);
   sw[AFD_ROWS_W] = XtVaCreateManagedWidget("Number of rows",
                           xmCascadeButtonWidgetClass, pull_down_w,
                           XmNfontList,                fontlist,
                           XmNsubMenuId,               pullright_row,
                           NULL);
   create_pullright_row(pullright_row);
   sw[AFD_ALIAS_LENGTH_W] = XtVaCreateManagedWidget("Alias length",
                           xmCascadeButtonWidgetClass, pull_down_w,
                           XmNfontList,                fontlist,
                           XmNsubMenuId,               pullright_alias_length,
                           NULL);
   create_pullright_alias_length(pullright_alias_length);
   sw[AFD_STYLE_W] = XtVaCreateManagedWidget("Line Style",
                           xmCascadeButtonWidgetClass, pull_down_w,
                           XmNfontList,                fontlist,
                           XmNsubMenuId,               pullright_line_style,
                           NULL);
   create_pullright_style(pullright_line_style);
   sw[AFD_OTHER_W] = XtVaCreateManagedWidget("Other options",
                           xmCascadeButtonWidgetClass, pull_down_w,
                           XmNfontList,                fontlist,
                           XmNsubMenuId,               pullright_other_options,
                           NULL);
   create_pullright_other(pullright_other_options);

   if (have_groups == YES)
   {
      XtVaCreateManagedWidget("Separator",
                              xmSeparatorWidgetClass, pull_down_w,
                              NULL);
#ifdef WITH_CTRL_ACCELERATOR
      sw[AFD_OPEN_ALL_GROUPS_W] = XtVaCreateManagedWidget("Open Groups   (Ctrl+o)",
#else
      sw[AFD_OPEN_ALL_GROUPS_W] = XtVaCreateManagedWidget("Open Groups   (Alt+o)",
#endif
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
#ifdef WHEN_WE_KNOW_HOW_TO_FIX_THIS
                              XmNmnemonic,             'o',
#endif
#ifdef WITH_CTRL_ACCELERATOR
                              XmNaccelerator,          "Ctrl<Key>o",
#else
                              XmNaccelerator,          "Alt<Key>o",
#endif
                              NULL);
      XtAddCallback(sw[AFD_OPEN_ALL_GROUPS_W], XmNactivateCallback,
                    open_close_all_groups, (XtPointer)OPEN_ALL_GROUPS_SEL);

#ifdef WITH_CTRL_ACCELERATOR
      sw[AFD_CLOSE_ALL_GROUPS_W] = XtVaCreateManagedWidget("Close Groups (Ctrl+c)",
#else
      sw[AFD_CLOSE_ALL_GROUPS_W] = XtVaCreateManagedWidget("Close Groups (Alt+c)",
#endif
                              xmPushButtonWidgetClass, pull_down_w,
                              XmNfontList,             fontlist,
#ifdef WHEN_WE_KNOW_HOW_TO_FIX_THIS
                              XmNmnemonic,             'c',
#endif
#ifdef WITH_CTRL_ACCELERATOR
                              XmNaccelerator,          "Ctrl<Key>c",
#else
                              XmNaccelerator,          "Alt<Key>c",
#endif
                              NULL);
      XtAddCallback(sw[AFD_CLOSE_ALL_GROUPS_W], XmNactivateCallback,
                    open_close_all_groups, (XtPointer)CLOSE_ALL_GROUPS_SEL);
   }


   XtVaCreateManagedWidget("Separator",
                           xmSeparatorWidgetClass, pull_down_w,
                           NULL);
   sw[AFD_SAVE_W] = XtVaCreateManagedWidget("Save Setup",
                           xmPushButtonWidgetClass, pull_down_w,
                           XmNfontList,             fontlist,
#ifdef WHEN_WE_KNOW_HOW_TO_FIX_THIS
                           XmNmnemonic,             'a',
#endif
#ifdef WITH_CTRL_ACCELERATOR
                           XmNaccelerator,          "Ctrl<Key>a",
#else
                           XmNaccelerator,          "Alt<Key>a",
#endif
                           NULL);
   XtAddCallback(sw[AFD_SAVE_W], XmNactivateCallback, save_setup_cb, (XtPointer)0);

#ifdef _WITH_HELP_PULLDOWN
   /**********************************************************************/
   /*                            Help Menu                               */
   /**********************************************************************/
   pull_down_w = XmCreatePulldownMenu(*menu_w, "Help Pulldown", NULL, 0);
   XtVaSetValues(pull_down_w, XmNtearOffModel, XmTEAR_OFF_ENABLED, NULL);
   mw[HELP_W] = XtVaCreateManagedWidget("Help",
                           xmCascadeButtonWidgetClass, *menu_w,
                           XmNfontList,                fontlist,
#ifdef WHEN_WE_KNOW_HOW_TO_FIX_THIS
                           XmNmnemonic,                'H',
#endif
                           XmNsubMenuId,               pull_down_w,
                           NULL);
   hw[ABOUT_W] = XtVaCreateManagedWidget("About AFD",
                           xmPushButtonWidgetClass, pull_down_w,
                           XmNfontList,             fontlist,
                           NULL);
   hw[HYPER_W] = XtVaCreateManagedWidget("Hyper Help",
                           xmPushButtonWidgetClass, pull_down_w,
                           XmNfontList,             fontlist,
                           NULL);
   hw[VERSION_W] = XtVaCreateManagedWidget("Version",
                           xmPushButtonWidgetClass, pull_down_w,
                           XmNfontList,             fontlist,
                           NULL);
#endif /* _WITH_HELP_PULLDOWN */

   XtManageChild(*menu_w);
   XtVaSetValues(*menu_w, XmNmenuHelpWidget, mw[HELP_W], NULL);

   return;
}


/*++++++++++++++++++++++++++ init_popup_menu() ++++++++++++++++++++++++++*/
static void
init_popup_menu(Widget w)
{
   Arg      args[1];
   Cardinal argcount;
   Widget   popupmenu;

   argcount = 0;
   XtSetArg(args[argcount], XmNtearOffModel, XmTEAR_OFF_ENABLED); argcount++;
   popupmenu = XmCreateSimplePopupMenu(w, "popup", args, argcount);

   if ((acp.handle_event != NO_PERMISSION) ||
       (acp.ctrl_queue != NO_PERMISSION) ||
       (acp.ctrl_transfer != NO_PERMISSION) ||
       (acp.ctrl_queue_transfer != NO_PERMISSION) ||
       (acp.disable != NO_PERMISSION) ||
       (acp.switch_host != NO_PERMISSION) ||
       (acp.retry != NO_PERMISSION) ||
       (acp.debug != NO_PERMISSION) ||
       (acp.trace != NO_PERMISSION) ||
       (acp.full_trace != NO_PERMISSION) ||
       (acp.info != NO_PERMISSION) ||
       (acp.view_dc != NO_PERMISSION) ||
       (ping_cmd != NULL) ||
       (traceroute_cmd != NULL))
   {
      if (acp.handle_event != NO_PERMISSION)
      {
         pw[0] = XtVaCreateManagedWidget("Handle event",
                                 xmPushButtonWidgetClass, popupmenu,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(pw[0], XmNactivateCallback,
                       popup_cb, (XtPointer)EVENT_SEL);
      }
      if (acp.ctrl_queue != NO_PERMISSION)
      {
         pw[1] = XtVaCreateManagedWidget("Start/Stop input queue",
                                 xmPushButtonWidgetClass, popupmenu,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(pw[1], XmNactivateCallback,
                       popup_cb, (XtPointer)QUEUE_SEL);
      }
      if (acp.ctrl_transfer != NO_PERMISSION)
      {
         pw[2] = XtVaCreateManagedWidget("Start/Stop transfer",
                                 xmPushButtonWidgetClass, popupmenu,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(pw[2], XmNactivateCallback,
                       popup_cb, (XtPointer)TRANS_SEL);
      }
      if (acp.ctrl_queue_transfer != NO_PERMISSION)
      {
         pw[3] = XtVaCreateManagedWidget("Start/Stop host",
                                 xmPushButtonWidgetClass, popupmenu,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(pw[3], XmNactivateCallback,
                       popup_cb, (XtPointer)QUEUE_TRANS_SEL);
      }
      if (acp.disable != NO_PERMISSION)
      {
         pw[4] = XtVaCreateManagedWidget("Enable/Disable host",
                                 xmPushButtonWidgetClass, popupmenu,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(pw[4], XmNactivateCallback,
                       popup_cb, (XtPointer)DISABLE_SEL);
      }
      if (acp.switch_host != NO_PERMISSION)
      {
         pw[5] = XtVaCreateManagedWidget("Switch host",
                                 xmPushButtonWidgetClass, popupmenu,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(pw[5], XmNactivateCallback,
                       popup_cb, (XtPointer)SWITCH_SEL);
      }
      if (acp.retry != NO_PERMISSION)
      {
#ifdef WITH_CTRL_ACCELERATOR
         pw[6] = XtVaCreateManagedWidget("Retry (Ctrl+r)",
#else
         pw[6] = XtVaCreateManagedWidget("Retry (Alt+r)",
#endif
                                 xmPushButtonWidgetClass, popupmenu,
                                 XmNfontList,             fontlist,
#ifdef WITH_CTRL_ACCELERATOR
                                 XmNaccelerator,          "Ctrl<Key>R",
#else
                                 XmNaccelerator,          "Alt<Key>R",
#endif
#ifdef WHEN_WE_KNOW_HOW_TO_FIX_THIS
                                 XmNmnemonic,             'R',
#endif
                                 NULL);
         XtAddCallback(pw[6], XmNactivateCallback,
                       popup_cb, (XtPointer)RETRY_SEL);
      }
      if (acp.debug != NO_PERMISSION)
      {
         pullright_debug_popup = XmCreateSimplePulldownMenu(popupmenu,
                                                            "pullright_debug_popup",
                                                            NULL, 0);
         pw[7] = XtVaCreateManagedWidget("Debug",
                                 xmCascadeButtonWidgetClass, popupmenu,
                                 XmNfontList,                fontlist,
                                 XmNsubMenuId,               pullright_debug_popup,
                                 NULL);
         create_pullright_debug(pullright_debug_popup);
      }
      if (acp.info != NO_PERMISSION)
      {
         pw[8] = XtVaCreateManagedWidget("Info",
                                 xmPushButtonWidgetClass, popupmenu,
                                 XmNfontList,             fontlist,
#ifdef WITH_CTRL_ACCELERATOR
                                 XmNaccelerator,          "Ctrl<Key>I",
#else
                                 XmNaccelerator,          "Alt<Key>I",
#endif
#ifdef WHEN_WE_KNOW_HOW_TO_FIX_THIS
                                 XmNmnemonic,             'I',
#endif
                                 NULL);
         XtAddCallback(pw[8], XmNactivateCallback,
                       popup_cb, (XtPointer)INFO_SEL);
      }
      if (acp.view_dc != NO_PERMISSION)
      {
         pw[9] = XtVaCreateManagedWidget("Configuration",
                                 xmPushButtonWidgetClass, popupmenu,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(pw[9], XmNactivateCallback,
                       popup_cb, (XtPointer)VIEW_DC_SEL);
      }
      if (acp.show_elog != NO_PERMISSION)
      {
         pw[10] = XtVaCreateManagedWidget("Event Log",
                                 xmPushButtonWidgetClass, popupmenu,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(pw[10], XmNactivateCallback,
                       popup_cb, (XtPointer)E_LOG_SEL);
      }
      if (acp.show_tlog != NO_PERMISSION)
      {
         pw[11] = XtVaCreateManagedWidget("Transfer Log",
                                 xmPushButtonWidgetClass, popupmenu,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(pw[11], XmNactivateCallback,
                       popup_cb, (XtPointer)T_LOG_SEL);
      }
#ifdef _OUTPUT_LOG
      if (acp.show_olog != NO_PERMISSION)
      {
         pw[12] = XtVaCreateManagedWidget("Output Log",
                                 xmPushButtonWidgetClass, popupmenu,
                                 XmNfontList,             fontlist,
                                 NULL);
         XtAddCallback(pw[12], XmNactivateCallback,
                       popup_cb, (XtPointer)O_LOG_SEL);
      }
#endif
   }

   XtAddEventHandler(w,
                     ButtonPressMask | ButtonReleaseMask |
                     Button1MotionMask,
                     False, (XtEventHandler)popup_menu_cb, popupmenu);

   return;
}


/*------------------------ create_pullright_test() ----------------------*/
static void
create_pullright_test(Widget pullright_test)
{
   XmString x_string;
   Arg      args[2];
   Cardinal argcount;

   if (ping_cmd != NULL)
   {
      /* Create pullright for "Ping". */
      argcount = 0;
      x_string = XmStringCreateLocalized(SHOW_PING_TEST);
      XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
      XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
      tw[PING_W] = XmCreatePushButton(pullright_test, "Ping",
                                      args, argcount);
      XtAddCallback(tw[PING_W], XmNactivateCallback, popup_cb,
		    (XtPointer)PING_SEL);
      XtManageChild(tw[PING_W]);
      XmStringFree(x_string);
   }

   if (traceroute_cmd != NULL)
   {
      /* Create pullright for "Traceroute". */
      argcount = 0;
      x_string = XmStringCreateLocalized(SHOW_TRACEROUTE_TEST);
      XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
      XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
      tw[TRACEROUTE_W] = XmCreatePushButton(pullright_test, "Traceroute",
                                            args, argcount);
      XtAddCallback(tw[TRACEROUTE_W], XmNactivateCallback, popup_cb,
		    (XtPointer)TRACEROUTE_SEL);
      XtManageChild(tw[TRACEROUTE_W]);
      XmStringFree(x_string);
   }

   return;
}


/*------------------------ create_pullright_load() ----------------------*/
static void
create_pullright_load(Widget pullright_line_load)
{
   XmString x_string;
   Arg      args[2];
   Cardinal argcount;

   /* Create pullright for "Files". */
   argcount = 0;
   x_string = XmStringCreateLocalized(SHOW_FILE_LOAD);
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   lw[FILE_LOAD_W] = XmCreatePushButton(pullright_line_load, "file",
				        args, argcount);
   XtAddCallback(lw[FILE_LOAD_W], XmNactivateCallback, popup_cb,
		 (XtPointer)VIEW_FILE_LOAD_SEL);
   XtManageChild(lw[FILE_LOAD_W]);
   XmStringFree(x_string);

   /* Create pullright for "KBytes". */
   argcount = 0;
   x_string = XmStringCreateLocalized(SHOW_KBYTE_LOAD);
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   lw[KBYTE_LOAD_W] = XmCreatePushButton(pullright_line_load, "kbytes",
				        args, argcount);
   XtAddCallback(lw[KBYTE_LOAD_W], XmNactivateCallback, popup_cb,
		 (XtPointer)VIEW_KBYTE_LOAD_SEL);
   XtManageChild(lw[KBYTE_LOAD_W]);
   XmStringFree(x_string);

   /* Create pullright for "Connections". */
   argcount = 0;
   x_string = XmStringCreateLocalized(SHOW_CONNECTION_LOAD);
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   lw[CONNECTION_LOAD_W] = XmCreatePushButton(pullright_line_load,
                                              "connection",
				              args, argcount);
   XtAddCallback(lw[CONNECTION_LOAD_W], XmNactivateCallback, popup_cb,
		 (XtPointer)VIEW_CONNECTION_LOAD_SEL);
   XtManageChild(lw[CONNECTION_LOAD_W]);
   XmStringFree(x_string);

   /* Create pullright for "Active-Transfers". */
   argcount = 0;
   x_string = XmStringCreateLocalized(SHOW_TRANSFER_LOAD);
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   lw[TRANSFER_LOAD_W] = XmCreatePushButton(pullright_line_load,
                                              "active-transfers",
				              args, argcount);
   XtAddCallback(lw[TRANSFER_LOAD_W], XmNactivateCallback, popup_cb,
		 (XtPointer)VIEW_TRANSFER_LOAD_SEL);
   XtManageChild(lw[TRANSFER_LOAD_W]);
   XmStringFree(x_string);

   return;
}


/*------------------------ create_pullright_font() ----------------------*/
static void
create_pullright_font(Widget pullright_font)
{
   XT_PTR_TYPE     i;
   char            *font[NO_OF_FONTS] =
                   {
                      FONT_0, FONT_1, FONT_2, FONT_3, FONT_4, FONT_5, FONT_6,
                      FONT_7, FONT_8, FONT_9, FONT_10, FONT_11, FONT_12
                   };
   XmString        x_string;
   XmFontListEntry entry;
   XmFontList      tmp_fontlist;
   Arg             args[3];
   Cardinal        argcount;
   XFontStruct     *p_font_struct;

   for (i = 0; i < NO_OF_FONTS; i++)
   {
      if ((current_font == -1) && (CHECK_STRCMP(font_name, font[i]) == 0))
      {
         current_font = i;
      }
      if ((p_font_struct = XLoadQueryFont(display, font[i])) != NULL)
      {
         if ((entry = XmFontListEntryLoad(display, font[i], XmFONT_IS_FONT, "TAG1")) == NULL)
         {
            (void)fprintf(stderr, "Failed to load font with XmFontListEntryLoad() : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         tmp_fontlist = XmFontListAppendEntry(NULL, entry);
         XmFontListEntryFree(&entry);

         argcount = 0;
         x_string = XmStringCreateLocalized(font[i]);
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
         XtSetArg(args[argcount], XmNfontList, tmp_fontlist); argcount++;
         fw[i] = XmCreateToggleButton(pullright_font, "font_x", args, argcount);
         XtAddCallback(fw[i], XmNvalueChangedCallback, change_font_cb, (XtPointer)i);
         XtManageChild(fw[i]);
         XmFontListFree(tmp_fontlist);
         XmStringFree(x_string);
         XFreeFont(display, p_font_struct);
      }
      else
      {
         fw[i] = NULL;
      }
   }

   /*
    * It can happen that in the setup file there is a font name
    * that is not in our list. If that is the case current_font
    * is still -1! So lets check if this is the case and then
    * try set DEFAULT_FONT.
    */
   if (current_font == -1)
   {
      for (i = 0; i < NO_OF_FONTS; i++)
      {
         if ((fw[i] != NULL) && (CHECK_STRCMP(DEFAULT_FONT, font[i]) == 0))
         {
            current_font = i;
            (void)strcpy(font_name, DEFAULT_FONT);
            return;
         }
      }

      /*
       * Uurghh! What now? Lets try pick the middle font that is available.
       */
      if (current_font == -1)
      {
         int available_fonts = 0;

         for (i = 0; i < NO_OF_FONTS; i++)
         {
            if (fw[i] != NULL)
            {
               available_fonts++;
            }
         }
         if (available_fonts == 0)
         {
            (void)fprintf(stderr, "ERROR : Could not find any font.");
            exit(INCORRECT);
         }
         if (available_fonts == 1)
         {
            current_font = 0;
         }
         else
         {
            current_font = available_fonts / 2;
         }
         (void)strcpy(font_name, font[current_font]);
      }
   }

   return;
}


/*------------------------ create_pullright_row() -----------------------*/
static void
create_pullright_row(Widget pullright_row)
{
   XT_PTR_TYPE i;
   char        *row[NO_OF_ROWS] =
               {
                  ROW_0, ROW_1, ROW_2, ROW_3, ROW_4, ROW_5, ROW_6,
                  ROW_7, ROW_8, ROW_9, ROW_10, ROW_11, ROW_12, ROW_13,
                  ROW_14, ROW_15, ROW_16, ROW_17, ROW_18, ROW_19, ROW_20
               };
   XmString    x_string;
   Arg         args[3];
   Cardinal    argcount;

   for (i = 0; i < NO_OF_ROWS; i++)
   {
      if ((current_row == -1) && (no_of_rows_set == atoi(row[i])))
      {
         current_row = i;
      }
      argcount = 0;
      x_string = XmStringCreateLocalized(row[i]);
      XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
      XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
      XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
      rw[i] = XmCreateToggleButton(pullright_row, "row_x", args, argcount);
      XtAddCallback(rw[i], XmNvalueChangedCallback, change_rows_cb, (XtPointer)i);
      XtManageChild(rw[i]);
      XmStringFree(x_string);
   }

   return;
}


/*-------------------- create_pullright_alias_length() ------------------*/
static void
create_pullright_alias_length(Widget pullright_alias_length)
{
   XT_PTR_TYPE i;
   char        alias_length_str[MAX_INT_LENGTH + 1];
   XmString    x_string;
   Arg         args[3];
   Cardinal    argcount;

   for (i = MIN_ALIAS_DISPLAY_LENGTH; i < (MAX_HOSTNAME_LENGTH + 2); i++)
   {
      if ((current_alias_length == -1) && (alias_length_set == i))
      {
         current_alias_length = i;
      }
      argcount = 0;
      (void)snprintf(alias_length_str, MAX_INT_LENGTH, "%ld", i);
      x_string = XmStringCreateLocalized(alias_length_str);
      XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
      XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
      XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
      adl[i - MIN_ALIAS_DISPLAY_LENGTH] = XmCreateToggleButton(pullright_alias_length, "alias_length_x", args, argcount);
      XtAddCallback(adl[i - MIN_ALIAS_DISPLAY_LENGTH], XmNvalueChangedCallback,
                    change_alias_length_cb, (XtPointer)i);
      XtManageChild(adl[i - MIN_ALIAS_DISPLAY_LENGTH]);
      XmStringFree(x_string);
   }

   return;
}


/*------------------------ create_pullright_style() ---------------------*/
static void
create_pullright_style(Widget pullright_line_style)
{
   XmString x_string;
   Arg      args[3];
   Cardinal argcount;
   Widget   pullright_proc_style;

   /* Create pullright for "Line style". */
   argcount = 0;
   x_string = XmStringCreateLocalized("Leds");
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNindicatorType, XmN_OF_MANY); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   lsw[LEDS_STYLE_W] = XmCreateToggleButton(pullright_line_style, "style_0",
                                            args, argcount);
   XtAddCallback(lsw[LEDS_STYLE_W], XmNvalueChangedCallback, change_style_cb,
                 (XtPointer)LEDS_STYLE_W);
   XtManageChild(lsw[LEDS_STYLE_W]);
   XmStringFree(x_string);

   /* Create another pullright for process. */
   pullright_proc_style = XmCreateSimplePulldownMenu(pullright_line_style,
                                          "pullright_proc_style", NULL, 0);
   lsw[JOBS_STYLE_W] = XtVaCreateManagedWidget("Process",
                         xmCascadeButtonWidgetClass, pullright_line_style,
                         XmNfontList,                fontlist,
                         XmNsubMenuId,               pullright_proc_style,
                         NULL);

   argcount = 0;
   x_string = XmStringCreateLocalized("Normal");
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   ptw[0] = XmCreateToggleButton(pullright_proc_style, "p_s_normal", args, argcount);
   XtAddCallback(ptw[0], XmNvalueChangedCallback, change_style_cb, (XtPointer)JOB_STYLE_NORMAL);
   XtManageChild(ptw[0]);
   XmStringFree(x_string);

   argcount = 0;
   x_string = XmStringCreateLocalized("Compact");
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   ptw[1] = XmCreateToggleButton(pullright_proc_style, "p_s_compact", args, argcount);
   XtAddCallback(ptw[1], XmNvalueChangedCallback, change_style_cb, (XtPointer)JOB_STYLE_COMPACT);
   XtManageChild(ptw[1]);
   XmStringFree(x_string);

   argcount = 0;
   x_string = XmStringCreateLocalized("None");
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNindicatorType, XmONE_OF_MANY); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   ptw[2] = XmCreateToggleButton(pullright_proc_style, "p_s_none", args, argcount);
   XtAddCallback(ptw[2], XmNvalueChangedCallback, change_style_cb, (XtPointer)JOB_STYLE_NONE);
   XtManageChild(ptw[2]);
   XmStringFree(x_string);

   argcount = 0;
   x_string = XmStringCreateLocalized("Characters");
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNindicatorType, XmN_OF_MANY); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   lsw[CHARACTERS_STYLE_W] = XmCreateToggleButton(pullright_line_style,
                                                  "style_2", args, argcount);
   XtAddCallback(lsw[CHARACTERS_STYLE_W], XmNvalueChangedCallback,
                 change_style_cb, (XtPointer)CHARACTERS_STYLE_W);
   XtManageChild(lsw[CHARACTERS_STYLE_W]);
   XmStringFree(x_string);

   argcount = 0;
   x_string = XmStringCreateLocalized("Bars");
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNindicatorType, XmN_OF_MANY); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   lsw[BARS_STYLE_W] = XmCreateToggleButton(pullright_line_style, "style_3",
                                            args, argcount);
   XtAddCallback(lsw[BARS_STYLE_W], XmNvalueChangedCallback, change_style_cb,
                 (XtPointer)BARS_STYLE_W);
   XtManageChild(lsw[BARS_STYLE_W]);
   XmStringFree(x_string);

   return;
}


/*------------------------ create_pullright_other() ---------------------*/
static void
create_pullright_other(Widget pullright_other_options)
{
   XmString x_string;
   Arg      args[3];
   Cardinal argcount;

   /* Create pullright for "Other". */
   argcount = 0;
   x_string = XmStringCreateLocalized("Force shift select");
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNindicatorType, XmN_OF_MANY); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   oow[FORCE_SHIFT_SELECT_W] = XmCreateToggleButton(pullright_other_options,
                                            "other_0", args, argcount);
   XtAddCallback(oow[FORCE_SHIFT_SELECT_W], XmNvalueChangedCallback,
                 change_other_cb, (XtPointer)FORCE_SHIFT_SELECT_W);
   XtManageChild(oow[FORCE_SHIFT_SELECT_W]);
   XmStringFree(x_string);

   argcount = 0;
   x_string = XmStringCreateLocalized("Auto save");
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNindicatorType, XmN_OF_MANY); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   oow[AUTO_SAVE_W] = XmCreateToggleButton(pullright_other_options,
                                            "other_1", args, argcount);
   XtAddCallback(oow[AUTO_SAVE_W], XmNvalueChangedCallback,
                 change_other_cb, (XtPointer)AUTO_SAVE_W);
   XtManageChild(oow[AUTO_SAVE_W]);
   XmStringFree(x_string);

   argcount = 0;
   x_string = XmStringCreateLocalized("Framed groups");
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNindicatorType, XmN_OF_MANY); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   oow[FRAMED_GROUPS_W] = XmCreateToggleButton(pullright_other_options,
                                            "other_2", args, argcount);
   XtAddCallback(oow[FRAMED_GROUPS_W], XmNvalueChangedCallback,
                 change_other_cb, (XtPointer)FRAMED_GROUPS_W);
   XtManageChild(oow[FRAMED_GROUPS_W]);
   XmStringFree(x_string);

   return;
}


/*+++++++++++++++++++++++ create_pullright_debug() ++++++++++++++++++++++*/
static void
create_pullright_debug(Widget pullright_debug)
{
   XmString x_string;
   Arg      args[5];
   Cardinal argcount;

   /* Create pullright for "Debug". */
   if (acp.debug != NO_PERMISSION)
   {
      argcount = 0;                                           
      x_string = XmStringCreateLocalized("Debug");
      XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
      XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;     
#ifdef WHEN_WE_KNOW_HOW_TO_FIX_THIS
      XtSetArg(args[argcount], XmNmnemonic, 'D'); argcount++;         
#endif
#ifdef WITH_CTRL_ACCELERATOR
      XtSetArg(args[argcount], XmNaccelerator, "Ctrl<Key>D"); argcount++;
#else
      XtSetArg(args[argcount], XmNaccelerator, "Alt<Key>D"); argcount++;
#endif
      if (pullright_debug == pullright_debug_popup)
      {
         dprwpp[DEBUG_STYLE_W] = XmCreatePushButton(pullright_debug,
                                                    "debug_0", args, argcount);           
         XtAddCallback(dprwpp[DEBUG_STYLE_W], XmNactivateCallback,
                       popup_cb, (XtPointer)DEBUG_SEL);
         XtManageChild(dprwpp[DEBUG_STYLE_W]);           
      }
      else
      {
         dprw[DEBUG_STYLE_W] = XmCreatePushButton(pullright_debug,
                                                  "debug_0", args, argcount);           
         XtAddCallback(dprw[DEBUG_STYLE_W], XmNactivateCallback,
                       popup_cb, (XtPointer)DEBUG_SEL);
         XtManageChild(dprw[DEBUG_STYLE_W]);           
      }
      XmStringFree(x_string);
   }
   if (acp.trace != NO_PERMISSION)
   {
      argcount = 0;
      x_string = XmStringCreateLocalized("Trace");
      XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
      XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
      if (pullright_debug == pullright_debug_popup)
      {
         dprwpp[TRACE_STYLE_W] = XmCreatePushButton(pullright_debug,
                                                    "debug_1", args, argcount);
         XtAddCallback(dprwpp[TRACE_STYLE_W], XmNactivateCallback,
                       popup_cb, (XtPointer)TRACE_SEL);
         XtManageChild(dprwpp[TRACE_STYLE_W]);
      }
      else
      {
         dprw[TRACE_STYLE_W] = XmCreatePushButton(pullright_debug,
                                                  "debug_1", args, argcount);
         XtAddCallback(dprw[TRACE_STYLE_W], XmNactivateCallback,
                       popup_cb, (XtPointer)TRACE_SEL);
         XtManageChild(dprw[TRACE_STYLE_W]);
      }
      XmStringFree(x_string);
   }
   if (acp.full_trace != NO_PERMISSION)
   {
      argcount = 0;
      x_string = XmStringCreateLocalized("Full Trace");
      XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
      XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
      if (pullright_debug == pullright_debug_popup)
      {
         dprwpp[FULL_TRACE_STYLE_W] = XmCreatePushButton(pullright_debug,
                                                         "debug_2",
                                                         args, argcount);
         XtAddCallback(dprwpp[FULL_TRACE_STYLE_W], XmNactivateCallback,
                       popup_cb, (XtPointer)FULL_TRACE_SEL);
         XtManageChild(dprwpp[FULL_TRACE_STYLE_W]);
      }
      else
      {
         dprw[FULL_TRACE_STYLE_W] = XmCreatePushButton(pullright_debug,
                                                       "debug_2",
                                                       args, argcount);
         XtAddCallback(dprw[FULL_TRACE_STYLE_W], XmNactivateCallback,
                       popup_cb, (XtPointer)FULL_TRACE_SEL);
         XtManageChild(dprw[FULL_TRACE_STYLE_W]);
      }
      XmStringFree(x_string);
   }

   return;
}


/*-------------------------- eval_permissions() -------------------------*/
/*                           ------------------                          */
/* Description: Checks the permissions on what the user may do.          */
/* Returns    : Fills the global structure acp with data.                */
/*-----------------------------------------------------------------------*/
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
      acp.afd_ctrl_list            = NULL;
      acp.amg_ctrl                 = YES;   /* Start/Stop the AMG    */
      acp.fd_ctrl                  = YES;   /* Start/Stop the FD     */
      acp.rr_dc                    = YES;   /* Reread DIR_CONFIG     */
      acp.rr_hc                    = YES;   /* Reread HOST_CONFIG    */
      acp.startup_afd              = YES;   /* Startup AFD           */
      acp.shutdown_afd             = YES;   /* Shutdown AFD          */
      acp.handle_event             = YES;
      acp.handle_event_list        = NULL;
      acp.ctrl_transfer            = NO_PERMISSION; /* Start/Stop a transfer */
      acp.ctrl_transfer_list       = NULL;
      acp.ctrl_queue               = NO_PERMISSION; /* Start/Stop the input queue */
      acp.ctrl_queue_list          = NULL;
      acp.ctrl_queue_transfer      = YES;   /* Start/Stop host */
      acp.ctrl_queue_transfer_list = NULL;
      acp.switch_host              = YES;
      acp.switch_host_list         = NULL;
      acp.disable                  = YES;
      acp.disable_list             = NULL;
      acp.info                     = YES;
      acp.info_list                = NULL;
      acp.debug                    = YES;
      acp.debug_list               = NULL;
      acp.trace                    = YES;
      acp.full_trace               = YES;
      acp.simulation               = YES;
      acp.retry                    = YES;
      acp.retry_list               = NULL;
      acp.show_slog                = YES;   /* View the system log   */
      acp.show_slog_list           = NULL;
      acp.show_elog                = YES;   /* View the event log   */
      acp.show_elog_list           = NULL;
#ifdef _MAINTAINER_LOG
      acp.show_mlog                = NO_PERMISSION; /* View the maintainer log */
      acp.show_mlog_list           = NULL;
#endif
      acp.show_rlog                = YES;   /* View the receive log */
      acp.show_rlog_list           = NULL;
      acp.show_tlog                = YES;   /* View the transfer log */
      acp.show_tlog_list           = NULL;
      acp.show_tdlog               = YES;   /* View the transfer debug log */
      acp.show_tdlog_list          = NULL;
      acp.show_ilog                = YES;   /* View the input log    */
      acp.show_ilog_list           = NULL;
      acp.show_plog                = YES;   /* View the production log */
      acp.show_plog_list           = NULL;
      acp.show_olog                = YES;   /* View the output log   */
      acp.show_olog_list           = NULL;
      acp.show_dlog                = YES;   /* View the delete log   */
      acp.show_dlog_list           = NULL;
      acp.show_queue               = YES;   /* View the AFD queue    */
      acp.show_queue_list          = NULL;
      acp.view_jobs                = YES;   /* View jobs             */
      acp.view_jobs_list           = NULL;
      acp.edit_hc                  = YES;   /* Edit Host Configuration */
      acp.edit_hc_list             = NULL;
      acp.view_dc                  = YES;   /* View DIR_CONFIG entries */
      acp.view_dc_list             = NULL;
      acp.dir_ctrl                 = YES;

      ptr = &perm_buffer[3];
      while ((*ptr == ' ') || (*ptr == '\t'))
      {
         ptr++;
      }
      if (*ptr == ',')
      {
         char *tmp_ptr = ptr + 1;

         /* May the user start/stop the input queue? */
         if ((ptr = posi(tmp_ptr, CTRL_QUEUE_PERM)) != NULL)
         {
            ptr--;
            if ((*ptr == ' ') || (*ptr == '\t'))
            {
               acp.ctrl_queue = store_host_names(&acp.ctrl_queue_list, ptr + 1);
            }
            else
            {
               acp.ctrl_queue = NO_LIMIT;
               acp.ctrl_queue_list = NULL;
            }
         }

         /* May the user start/stop the transfer? */
         if ((ptr = posi(tmp_ptr, CTRL_TRANSFER_PERM)) != NULL)
         {
            ptr--;
            if ((*ptr == ' ') || (*ptr == '\t'))
            {
               acp.ctrl_transfer = store_host_names(&acp.ctrl_transfer_list, ptr + 1);
            }
            else
            {
               acp.ctrl_transfer = NO_LIMIT;
               acp.ctrl_transfer_list = NULL;
            }
         }

#ifdef _MAINTAINER_LOG
         /* May the user view the maintainer log? */
         if ((ptr = posi(tmp_ptr, SHOW_MLOG_PERM)) != NULL)
         {
            ptr--;
            if ((*ptr == ' ') || (*ptr == '\t'))
            {
               acp.show_mlog = store_host_names(&acp.show_mlog_list, ptr + 1);
            }
            else
            {
               acp.show_mlog = NO_LIMIT;
               acp.show_mlog_list = NULL;
            }
         }
#endif
      }
   }
   else
   {
      /*
       * First of all check if the user may use this program
       * at all.
       */
      if ((ptr = posi(perm_buffer, AFD_CTRL_PERM)) == NULL)
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
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            store_host_names(&acp.afd_ctrl_list, ptr + 1);
         }
         else
         {
            acp.afd_ctrl_list = NULL;
         }
      }

      /* May the user start/stop the AMG? */
      if ((ptr = posi(perm_buffer, AMG_CTRL_PERM)) == NULL)
      {
         /* The user may NOT do any resending. */
         acp.amg_ctrl = NO_PERMISSION;
      }
      else
      {
         acp.amg_ctrl = NO_LIMIT;
      }

      /* May the user start/stop the FD? */
      if ((ptr = posi(perm_buffer, FD_CTRL_PERM)) == NULL)
      {
         /* The user may NOT do any resending. */
         acp.fd_ctrl = NO_PERMISSION;
      }
      else
      {
         acp.fd_ctrl = NO_LIMIT;
      }

      /* May the user reread the DIR_CONFIG? */
      if ((ptr = posi(perm_buffer, RR_DC_PERM)) == NULL)
      {
         /* The user may NOT do reread the DIR_CONFIG. */
         acp.rr_dc = NO_PERMISSION;
      }
      else
      {
         acp.rr_dc = NO_LIMIT;
      }

      /* May the user reread the HOST_CONFIG? */
      if ((ptr = posi(perm_buffer, RR_HC_PERM)) == NULL)
      {
         /* The user may NOT do reread the HOST_CONFIG. */
         acp.rr_hc = NO_PERMISSION;
      }
      else
      {
         acp.rr_hc = NO_LIMIT;
      }

      /* May the user startup the AFD? */
      if ((ptr = posi(perm_buffer, STARTUP_PERM)) == NULL)
      {
         /* The user may NOT do a startup the AFD. */
         acp.startup_afd = NO_PERMISSION;
      }
      else
      {
         acp.startup_afd = NO_LIMIT;
      }

      /* May the user shutdown the AFD? */
      if ((ptr = posi(perm_buffer, SHUTDOWN_PERM)) == NULL)
      {
         /* The user may NOT do a shutdown the AFD. */
         acp.shutdown_afd = NO_PERMISSION;
      }
      else
      {
         acp.shutdown_afd = NO_LIMIT;
      }

      /* May the user use the dir_ctrl dialog? */
      if ((ptr = posi(perm_buffer, DIR_CTRL_PERM)) == NULL)
      {
         /* The user may NOT use the dir_ctrl dialog. */
         acp.dir_ctrl = NO_PERMISSION;
      }
      else
      {
         acp.dir_ctrl = NO_LIMIT;
      }

      /* May the user handle event queue? */
      if ((ptr = posi(perm_buffer, HANDLE_EVENT_PERM)) == NULL)
      {
         /* The user may NOT handle internal events. */
         acp.handle_event = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.handle_event = store_host_names(&acp.handle_event_list, ptr + 1);
         }
         else
         {
            acp.handle_event = NO_LIMIT;
            acp.handle_event_list = NULL;
         }
      }

      /* May the user start/stop the input queue? */
      if ((ptr = posi(perm_buffer, CTRL_QUEUE_PERM)) == NULL)
      {
         /* The user may NOT start/stop the input queue. */
         acp.ctrl_queue = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.ctrl_queue = store_host_names(&acp.ctrl_queue_list, ptr + 1);
         }
         else
         {
            acp.ctrl_queue = NO_LIMIT;
            acp.ctrl_queue_list = NULL;
         }
      }

      /* May the user start/stop the transfer? */
      if ((ptr = posi(perm_buffer, CTRL_TRANSFER_PERM)) == NULL)
      {
         /* The user may NOT start/stop the transfer. */
         acp.ctrl_transfer = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.ctrl_transfer = store_host_names(&acp.ctrl_transfer_list, ptr + 1);
         }
         else
         {
            acp.ctrl_transfer = NO_LIMIT;
            acp.ctrl_transfer_list = NULL;
         }
      }

      /* May the user start/stop the host? */
      if ((ptr = posi(perm_buffer, CTRL_QUEUE_TRANSFER_PERM)) == NULL)
      {
         /* The user may NOT start/stop the host. */
         acp.ctrl_queue_transfer = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.ctrl_queue_transfer = store_host_names(&acp.ctrl_queue_transfer_list, ptr + 1);
         }
         else
         {
            acp.ctrl_queue_transfer = NO_LIMIT;
            acp.ctrl_queue_transfer_list = NULL;
         }
      }

      /* May the user do a host switch? */
      if ((ptr = posi(perm_buffer, SWITCH_HOST_PERM)) == NULL)
      {
         /* The user may NOT do a host switch. */
         acp.switch_host = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.switch_host = store_host_names(&acp.switch_host_list, ptr + 1);
         }
         else
         {
            acp.switch_host = NO_LIMIT;
            acp.switch_host_list = NULL;
         }
      }

      /* May the user disable a host? */
      if ((ptr = posi(perm_buffer, DISABLE_HOST_PERM)) == NULL)
      {
         /* The user may NOT disable a host. */
         acp.disable = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.disable = store_host_names(&acp.disable_list, ptr + 1);
         }
         else
         {
            acp.disable = NO_LIMIT;
            acp.disable_list = NULL;
         }
      }

      /* May the user view the information of a host? */
      if ((ptr = posi(perm_buffer, INFO_PERM)) == NULL)
      {
         /* The user may NOT view any information. */
         acp.info = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.info = store_host_names(&acp.info_list, ptr + 1);
         }
         else
         {
            acp.info = NO_LIMIT;
            acp.info_list = NULL;
         }
      }

      /* May the user toggle the debug flag? */
      if ((ptr = posi(perm_buffer, DEBUG_PERM)) == NULL)
      {
         /* The user may NOT toggle the debug flag. */
         acp.debug = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.debug = store_host_names(&acp.debug_list, ptr + 1);
         }
         else
         {
            acp.debug = NO_LIMIT;
            acp.debug_list = NULL;
         }
      }

      /* May the user toggle the trace flag? */
      if ((ptr = posi(perm_buffer, TRACE_PERM)) == NULL)
      {
         /* The user may NOT toggle the trace flag. */
         acp.trace = NO_PERMISSION;
      }
      else
      {
         acp.trace = NO_LIMIT;
      }

      /* May the user toggle the full trace flag? */
      if ((ptr = posi(perm_buffer, FULL_TRACE_PERM)) == NULL)
      {
         /* The user may NOT toggle the full trace flag. */
         acp.full_trace = NO_PERMISSION;
      }
      else
      {
         acp.full_trace = NO_LIMIT;
      }

      /* May the user toggle the simulate transfer flag? */
      if ((ptr = posi(perm_buffer, SIMULATE_MODE_PERM)) == NULL)
      {
         /* The user may NOT toggle the full trace flag. */
         acp.simulation = NO_PERMISSION;
      }
      else
      {
         acp.simulation = NO_LIMIT;
      }

      /* May the user use the retry button for a particular host? */
      if ((ptr = posi(perm_buffer, RETRY_PERM)) == NULL)
      {
         /* The user may NOT use the retry button. */
         acp.retry = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.retry = store_host_names(&acp.retry_list, ptr + 1);
         }
         else
         {
            acp.retry = NO_LIMIT;
            acp.retry_list = NULL;
         }
      }

      /* May the user view the system log? */
      if ((ptr = posi(perm_buffer, SHOW_SLOG_PERM)) == NULL)
      {
         /* The user may NOT view the system log. */
         acp.show_slog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_slog = store_host_names(&acp.show_slog_list, ptr + 1);
         }
         else
         {
            acp.show_slog = NO_LIMIT;
            acp.show_slog_list = NULL;
         }
      }

      /* May the user view the event log? */
      if ((ptr = posi(perm_buffer, SHOW_ELOG_PERM)) == NULL)
      {
         /* The user may NOT view the event log. */
         acp.show_elog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_elog = store_host_names(&acp.show_elog_list, ptr + 1);
         }
         else
         {
            acp.show_elog = NO_LIMIT;
            acp.show_elog_list = NULL;
         }
      }

#ifdef _MAINTAINER_LOG
      /* May the user view the maintainer log? */
      if ((ptr = posi(perm_buffer, SHOW_MLOG_PERM)) == NULL)
      {
         /* The user may NOT view the maintainer log. */
         acp.show_mlog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_mlog = store_host_names(&acp.show_mlog_list, ptr + 1);
         }
         else
         {
            acp.show_mlog = NO_LIMIT;
            acp.show_mlog_list = NULL;
         }
      }
#endif

      /* May the user view the receive log? */
      if ((ptr = posi(perm_buffer, SHOW_RLOG_PERM)) == NULL)
      {
         /* The user may NOT view the receive log. */
         acp.show_rlog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_rlog = store_host_names(&acp.show_rlog_list, ptr + 1);
         }
         else
         {
            acp.show_rlog = NO_LIMIT;
            acp.show_rlog_list = NULL;
         }
      }

      /* May the user view the transfer log? */
      if ((ptr = posi(perm_buffer, SHOW_TLOG_PERM)) == NULL)
      {
         /* The user may NOT view the transfer log. */
         acp.show_tlog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_tlog = store_host_names(&acp.show_tlog_list, ptr + 1);
         }
         else
         {
            acp.show_tlog = NO_LIMIT;
            acp.show_tlog_list = NULL;
         }
      }

      /* May the user view the transfer debug log? */
      if ((ptr = posi(perm_buffer, SHOW_TDLOG_PERM)) == NULL)
      {
         /* The user may NOT view the transfer debug log. */
         acp.show_tdlog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_tdlog = store_host_names(&acp.show_tdlog_list, ptr + 1);
         }
         else
         {
            acp.show_tdlog = NO_LIMIT;
            acp.show_tdlog_list = NULL;
         }
      }

      /* May the user view the input log? */
      if ((ptr = posi(perm_buffer, SHOW_ILOG_PERM)) == NULL)
      {
         /* The user may NOT view the input log. */
         acp.show_ilog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_ilog = store_host_names(&acp.show_ilog_list, ptr + 1);
         }
         else
         {
            acp.show_ilog = NO_LIMIT;
            acp.show_ilog_list = NULL;
         }
      }

      /* May the user view the production log? */
      if ((ptr = posi(perm_buffer, SHOW_PLOG_PERM)) == NULL)
      {
         /* The user may NOT view the production log. */
         acp.show_plog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_plog = store_host_names(&acp.show_plog_list, ptr + 1);
         }
         else
         {
            acp.show_plog = NO_LIMIT;
            acp.show_plog_list = NULL;
         }
      }

      /* May the user view the output log? */
      if ((ptr = posi(perm_buffer, SHOW_OLOG_PERM)) == NULL)
      {
         /* The user may NOT view the output log. */
         acp.show_olog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_olog = store_host_names(&acp.show_olog_list, ptr + 1);
         }
         else
         {
            acp.show_olog = NO_LIMIT;
            acp.show_olog_list = NULL;
         }
      }

      /* May the user view the delete log? */
      if ((ptr = posi(perm_buffer, SHOW_DLOG_PERM)) == NULL)
      {
         /* The user may NOT view the delete log. */
         acp.show_dlog = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_dlog = store_host_names(&acp.show_dlog_list, ptr + 1);
         }
         else
         {
            acp.show_dlog = NO_LIMIT;
            acp.show_dlog_list = NULL;
         }
      }

      /* May the user view the AFD queue? */
      if ((ptr = posi(perm_buffer, SHOW_QUEUE_PERM)) == NULL)
      {
         /* The user may NOT view the AFD queue. */
         acp.show_queue = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.show_queue = store_host_names(&acp.show_queue_list, ptr + 1);
         }
         else
         {
            acp.show_queue = NO_LIMIT;
            acp.show_queue_list = NULL;
         }
      }

      /* May the user view the job details? */
      if ((ptr = posi(perm_buffer, VIEW_JOBS_PERM)) == NULL)
      {
         /* The user may NOT view the job details. */
         acp.view_jobs = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.view_jobs = store_host_names(&acp.view_jobs_list, ptr + 1);
         }
         else
         {
            acp.view_jobs = NO_LIMIT;
            acp.view_jobs_list = NULL;
         }
      }

      /* May the user edit the host configuration file? */
      if ((ptr = posi(perm_buffer, EDIT_HC_PERM)) == NULL)
      {
         /* The user may NOT edit the host configuration file. */
         acp.edit_hc = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.edit_hc = store_host_names(&acp.edit_hc_list, ptr + 1);
         }
         else
         {
            acp.edit_hc = NO_LIMIT;
            acp.edit_hc_list = NULL;
         }
      }

      /* May the user view the DIR_CONFIG file? */
      if ((ptr = posi(perm_buffer, VIEW_DIR_CONFIG_PERM)) == NULL)
      {
         /* The user may NOT view the DIR_CONFIG file. */
         acp.view_dc = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.view_dc = store_host_names(&acp.view_dc_list, ptr + 1);
         }
         else
         {
            acp.view_dc = NO_LIMIT;
            acp.view_dc_list = NULL;
         }
      }

      /* May the user view the rename rules? */
      if ((ptr = posi(perm_buffer, VIEW_RENAME_RULES_PERM)) == NULL)
      {
         /* The user may NOT view the rename rules. */
         acp.view_rr = NO_PERMISSION;
      }
      else
      {
         ptr--;
         if ((*ptr == ' ') || (*ptr == '\t'))
         {
            acp.view_rr = store_host_names(&acp.view_rr_list, ptr + 1);
         }
         else
         {
            acp.view_rr = NO_LIMIT;
            acp.view_rr_list = NULL;
         }
      }
   }

   return;
}


/*++++++++++++++++++++++++++ mafd_ctrl_exit() +++++++++++++++++++++++++++*/
static void
mafd_ctrl_exit(void)
{
   int i;

   for (i = 0; i < no_of_active_process; i++)
   {
      if (apps_list[i].pid > 0)
      {
         if (kill(apps_list[i].pid, SIGINT) < 0)
         {
            (void)xrec(WARN_DIALOG,
#if SIZEOF_PID_T == 4
                       "Failed to kill() process %s (%d) : %s",
#else
                       "Failed to kill() process %s (%lld) : %s",
#endif
                       apps_list[i].progname,
                       (pri_pid_t)apps_list[i].pid, strerror(errno));
         }
      }
   }
   if (db_update_reply_fifo != NULL)
   {
      (void)unlink(db_update_reply_fifo);
   }
   if (other_options & AUTO_SAVE)
   {
      save_setup();
   }
   free(connect_data);

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void                                                                
sig_segv(int signo)
{
   mafd_ctrl_exit();
   (void)fprintf(stderr, "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
                 __FILE__, __LINE__);
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void                                                                
sig_bus(int signo)
{
   mafd_ctrl_exit();
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
