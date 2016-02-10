/*
 *  gafd_ctrl.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   gafd_ctrl - controls and monitors the AFD
 **
 ** SYNOPSIS
 **   gafd_ctrl [--version]
 **             [-w <AFD working directory>]
 **             [-p <user profile>]
 **             [-no_input]
 **             [numeric font]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.08.2007 H.Kiehl Created
 **   16.02.2008 H.Kiehl All drawing areas are now also drawn to an off-line
 **                      pixmap.
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
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "gafd_ctrl.h"
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
XFontStruct                *font_struct;
XmFontList                 fontlist = NULL;
GtkWidget                  *mw[5],          /* Main menu */
                           *ow[13],         /* Host menu */
                           *vw[12],         /* View menu */
                           *cw[8],          /* Control menu */
                           *sw[4],          /* Setup menu */
                           *hw[3],          /* Help menu */
                           *fw[NO_OF_FONTS],/* Select font */
                           *rw[NO_OF_ROWS], /* Select rows */
                           *tw[2],          /* Test (ping, traceroute) */
                           *lw[4],          /* AFD load */
                           *lsw[4],         /* Line style */
                           *pw[10],         /* Popup menu */
                           *dprw[3],        /* Debug pull right */
                           *appshell,
                           *button_window_w,
                           *detailed_window_w,
                           *label_window_w,
                           *line_window_w,
                           *short_line_window_w,
                           *transviewshell = NULL,
                           *tv_label_window_w;
Window                     button_window,
                           detailed_window,
                           label_window,
                           line_window,
                           short_line_window,
                           tv_label_window;
Pixmap                     button_pixmap,
                           label_pixmap,
                           line_pixmap,
                           short_line_pixmap;
float                      max_bar_length;
int                        amg_flag = NO,
                           bar_thickness_2,
                           bar_thickness_3,
                           button_width,
                           depth,
                           even_height,
                           event_log_fd = STDERR_FILENO,
                           filename_display_length,
                           fsa_fd = -1,
                           fsa_id,
                           ft_exposure_short_line = 0,
                           ft_exposure_tv_line = 0,
                           hostname_display_length,
                           led_width,
                           *line_length = NULL,
                           max_line_length,
                           line_height = 0,
                           magic_value,
                           log_angle,
                           no_of_his_log,
                           no_input,
                           no_selected,
                           no_selected_static,
                           no_of_active_process = 0,
                           no_of_columns,
                           no_of_short_columns,
                           no_of_rows,
                           no_of_rows_set,
                           no_of_short_rows,
                           no_of_hosts,
                           no_of_jobs_selected,
                           no_of_long_lines = 0,
                           no_of_short_lines = 0,
                           short_line_length,
                           sys_log_fd = STDERR_FILENO,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           sys_log_readfd,
#endif
                           tv_line_length,
                           tv_no_of_columns,
                           tv_no_of_rows,
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
                           x_offset_rotating_dash,
                           x_offset_tv_characters,
                           x_offset_tv_bars,
                           x_offset_tv_file_name,
                           y_center_log,
                           y_offset_led;
XT_PTR_TYPE                current_font = -1,
                           current_row = -1;
long                       danger_no_of_jobs,
                           link_max;
Dimension                  tv_window_height,
                           tv_window_width;
#ifdef HAVE_MMAP
off_t                      fsa_size,
                           afd_active_size;
#endif
time_t                     afd_active_time;
unsigned short             step_size;
unsigned long              color_pool[COLOR_POOL_SIZE],
                           redraw_time_host,
                           redraw_time_status;
unsigned int               glyph_height,
                           glyph_width,
                           text_offset;
char                       work_dir[MAX_PATH_LENGTH],
                           *p_work_dir,
                           *pid_list,
                           afd_active_file[MAX_PATH_LENGTH],
                           *db_update_reply_fifo = NULL,
                           line_style,
                           fake_user[MAX_FULL_USER_ID_LENGTH],
                           font_name[20],
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
struct afd_control_perm    acp;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void                afd_ctrl_exit(void),
                           create_pullright_debug(GtkWidget *),
                           create_pullright_font(GtkWidget *),
                           create_pullright_load(GtkWidget *),
                           create_pullright_row(GtkWidget *),
                           create_pullright_style(GtkWidget *),
                           create_pullright_test(GtkWidget *),
                           eval_permissions(char *),
                           init_menu_bar(GtkWidget **),
                           init_popup_menu(GtkWidget *),
                           init_afd_ctrl(int *, char **, char *),
                           sig_bus(int),
                           sig_exit(int),
                           sig_segv(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char          window_title[100];
   GtkWidget     *h_draw_box,
                 *vbox,
                 *menu_w;
   uid_t         euid, /* Effective user ID. */
                 ruid; /* Real user ID. */

#ifdef WITH_MEMCHECK
   mtrace();
#endif
   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values. */
   init_afd_ctrl(&argc, argv, window_title);

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
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       ruid, strerror(errno));
      }
   }

   /* Create the top-level shell widget and initialise the toolkit. */
   gtk_init(&argc, &argv);
   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title(GTK_WINDOW(window), window_title);
   gtk_container_set_border_width(GTK_CONTAINER(window), 10);
   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s\n",
                       euid, strerror(errno));
      }
   }

   /* Get display pointer. */
   if ((display = GDK_DISPLAY()) == NULL)
   {
      (void)fprintf(stderr,
                    "ERROR   : Could not open Display : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

#ifdef _X_DEBUG
   XSynchronize(display, 1);
#endif

   /* Setup and determine window parameters. */
   setup_window(font_name, YES);

   /* Get window size. */
   (void)window_size(&window_width, &window_height);

   gtk_widget_set_size_request(window, window_width, window_height);

   vbox = gtk_vbox_new(FALSE, 5);

   /* Create managing widget for label, line and button widget. */
   h_draw_box = gtk_hbox_new(FALSE, 5);

   if (no_input == False)
   {
      init_menu_bar(&menu_w);
      gtk_box_pack_start(GTK_BOX(vbox), menu_w, TRUE, TRUE, 0);
   }

   /* Setup colors. */
   default_cmap = DefaultColormap(display, DefaultScreen(display));
   init_color(display);

   /* Create the label_window_w. */
   label_window_w = gtk_drawing_area_new();
   gtk_box_pack_start_defaults(GTK_BOX(h_draw_box), label_window_w);

   /* Create the line_window_w. */
   line_window_w = gtk_drawing_area_new();
   gtk_box_pack_start_defaults(GTK_BOX(h_draw_box), line_window_w);

   /* Create the short_line_window_w. */
   short_line_window_w = gtk_drawing_area_new();
   gtk_box_pack_start_defaults(GTK_BOX(h_draw_box), short_line_window_w);

   /* Initialise the GC's. */
   init_gcs();

   /* Create the button_window_w. */
   button_window_w = gtk_drawing_area_new();
   gtk_box_pack_start_defaults(GTK_BOX(h_draw_box), button_window_w);

   /* Add call back to handle expose events for the drawing areas. */
   XtAddCallback(label_window_w, XmNexposeCallback,
                 (XtCallbackProc)expose_handler_label, (XtPointer)0);
   XtAddCallback(line_window_w, XmNexposeCallback,
                 (XtCallbackProc)expose_handler_line, NULL);
   XtAddCallback(short_line_window_w, XmNexposeCallback,
                 (XtCallbackProc)expose_handler_short_line, NULL);
   XtAddCallback(button_window_w, XmNexposeCallback,
                 (XtCallbackProc)expose_handler_button, NULL);

   if (no_input == False)
   {
      XtAddEventHandler(line_window_w, ButtonPressMask | Button1MotionMask,
                        False, (XtEventHandler)input, NULL);
      XtAddEventHandler(short_line_window_w,
                        ButtonPressMask | ButtonReleaseMask | Button1MotionMask,
                        False, (XtEventHandler)short_input, NULL);

      /* Set toggle button for font|row|style. */
      XtVaSetValues(fw[current_font], XmNset, True, NULL);
      XtVaSetValues(rw[current_row], XmNset, True, NULL);
      if (line_style & SHOW_LEDS)
      {
         XtVaSetValues(lsw[LEDS_STYLE_W], XmNset, True, NULL);
      }
      if (line_style & SHOW_JOBS)
      {
         XtVaSetValues(lsw[JOBS_STYLE_W], XmNset, True, NULL);
      }
      if (line_style & SHOW_CHARACTERS)
      {
         XtVaSetValues(lsw[CHARACTERS_STYLE_W], XmNset, True, NULL);
      }
      if (line_style & SHOW_BARS)
      {
         XtVaSetValues(lsw[BARS_STYLE_W], XmNset, True, NULL);
      }

      /* Setup popup menu. */
      init_popup_menu(line_window_w);
      init_popup_menu(short_line_window_w);

      XtAddEventHandler(line_window_w, EnterWindowMask | LeaveWindowMask,
                        False, (XtEventHandler)focus, NULL);
      XtAddEventHandler(short_line_window_w, EnterWindowMask | LeaveWindowMask,
                        False, (XtEventHandler)focus, NULL);
   }

   /* Realize all widgets. */
   XtRealizeWidget(appshell);

   /* Set some signal handlers. */
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)xrec(WARN_DIALOG, "Failed to set signal handlers for afd_ctrl : %s",
                 strerror(errno));
   }

   /* Exit handler so we can close applications that the user started. */
   if (atexit(afd_ctrl_exit) != 0)
   {
      (void)xrec(WARN_DIALOG,
                 "Failed to set exit handler for %s : %s\n\nWill not be able to close applications when terminating.",
                 AFD_CTRL, strerror(errno));
   }

   /* Get window ID of four main windows. */
   label_window = XtWindow(label_window_w);
   line_window = XtWindow(line_window_w);
   short_line_window = XtWindow(short_line_window_w);
   button_window = XtWindow(button_window_w);

   gtk_container_add(GTK_CONTAINER(window), vbox);
   gtk_widget_show_all(window);

   /* Start the main event-handling loop. */
   gtk_main();

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ init_afd_ctrl() ++++++++++++++++++++++++++*/
static void
init_afd_ctrl(int *argc, char *argv[], char *window_title)
{
   int          i,
                j,
                fd,
                user_offset;
   unsigned int new_bar_length;
   time_t       start_time;
   char         afd_file_dir[MAX_PATH_LENGTH],
                *buffer,
                config_file[MAX_PATH_LENGTH],
                hostname[MAX_AFD_NAME_LENGTH],
                **hosts = NULL,
                *perm_buffer;
   struct tms   tmsdummy;
   struct stat  stat_buf;

   /* See if user wants some help. */
   if ((get_arg(argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      (void)fprintf(stdout,
                    "Usage: %s [-w <work_dir>] [-p <profile>] [-u[ <user>]] [-no_input] [-f <numeric font name>]\n",
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

   /* Disable all input? */
   if (get_arg(argc, argv, "-no_input", NULL, 0) == SUCCESS)
   {
      no_input = True;
   }
   else
   {
      no_input = False;
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
               (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
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
         acp.retry                    = YES;
         acp.retry_list               = NULL;
         acp.show_slog                = YES; /* View the system log   */
         acp.show_slog_list           = NULL;
         acp.show_rlog                = YES; /* View the receive log  */
         acp.show_rlog_list           = NULL;
         acp.show_tlog                = YES; /* View the transfer log */
         acp.show_tlog_list           = NULL;
         acp.show_dlog                = YES; /* View the debug log    */
         acp.show_dlog_list           = NULL;
         acp.show_ilog                = YES; /* View the input log    */
         acp.show_ilog_list           = NULL;
         acp.show_olog                = YES; /* View the output log   */
         acp.show_olog_list           = NULL;
         acp.show_elog                = YES; /* View the delete log   */
         acp.show_elog_list           = NULL;
         acp.afd_load                 = YES;
         acp.afd_load_list            = NULL;
         acp.view_jobs                = YES; /* View jobs             */
         acp.view_jobs_list           = NULL;
         acp.edit_hc                  = YES; /* Edit Host Config      */
         acp.edit_hc_list             = NULL;
         acp.view_dc                  = YES; /* View DIR_CONFIG entry */
         acp.view_dc_list             = NULL;
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

   /* Prepare title for afd_ctrl window. */
   (void)sprintf(window_title, "AFD %s ", PACKAGE_VERSION);
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

   get_user(user, fake_user, user_offset);

   /*
    * Attach to the FSA and get the number of host
    * and the fsa_id of the FSA.
    */
   if (fsa_attach("gafd_ctrl") != SUCCESS)
   {
      (void)fprintf(stderr, "ERROR   : Failed to attach to FSA. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   p_feature_flag = (unsigned char *)fsa - AFD_FEATURE_FLAG_OFFSET_END;
   saved_feature_flag = *p_feature_flag;

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
#ifdef REDUCED_LINK_MAX
   link_max = REDUCED_LINK_MAX;
#else                          
   if ((link_max = pathconf(afd_file_dir, _PC_LINK_MAX)) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "pathconf() _PC_LINK_MAX error, setting to %d : %s",
                 _POSIX_LINK_MAX, strerror(errno));
      link_max = _POSIX_LINK_MAX;
   }
#endif
#endif
   danger_no_of_jobs = link_max / 2;

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
      if (fstat(fd, &stat_buf) < 0)
      {
         (void)fprintf(stderr, "WARNING : fstat() error : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         pid_list = NULL;
      }
      else
      {
#ifdef HAVE_MMAP
         if ((pid_list = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                              MAP_SHARED, fd, 0)) == (caddr_t) -1)
#else
         if ((pid_list = mmap_emu(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                                  MAP_SHARED,
                                  afd_active_file, 0)) == (caddr_t) -1)
#endif
         {
            (void)fprintf(stderr, "WARNING : mmap() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            pid_list = NULL;
         }
#ifdef HAVE_MMAP
         afd_active_size = stat_buf.st_size;
#endif
         afd_active_time = stat_buf.st_mtime;

         if (close(fd) == -1)
         {
            (void)fprintf(stderr, "WARNING : close() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
         }
      }
   }

   /* Allocate memory for local 'FSA'. */
   if ((connect_data = calloc(no_of_hosts, sizeof(struct line))) == NULL)
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
   line_style = SHOW_LEDS | SHOW_JOBS | SHOW_CHARACTERS | SHOW_BARS;
   no_of_rows_set = DEFAULT_NO_OF_ROWS;
   filename_display_length = DEFAULT_FILENAME_DISPLAY_LENGTH;
   hostname_display_length = DEFAULT_HOSTNAME_DISPLAY_LENGTH;
   RT_ARRAY(hosts, no_of_hosts, (MAX_REAL_HOSTNAME_LENGTH + 4 + 1), char);
   read_setup(AFD_CTRL, profile, &hostname_display_length,
              &filename_display_length, NULL, hosts, MAX_REAL_HOSTNAME_LENGTH);

   /* Determine the default bar length. */
   max_bar_length  = 6 * BAR_LENGTH_MODIFIER;
   step_size = MAX_INTENSITY / max_bar_length;

   /* Initialise all display data for each host. */
   start_time = times(&tmsdummy);
   for (i = 0; i < no_of_hosts; i++)
   {
      (void)strcpy(connect_data[i].hostname, fsa[i].host_alias);
      connect_data[i].host_id = fsa[i].host_id;
      (void)sprintf(connect_data[i].host_display_str, "%-*s",
                    MAX_HOSTNAME_LENGTH, fsa[i].host_dsp_name);
      if (fsa[i].host_toggle_str[0] != '\0')
      {
         connect_data[i].host_toggle_display = fsa[i].host_toggle_str[(int)fsa[i].host_toggle];
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
      connect_data[i].short_pos = -1;
   }

   /* Locate positions for long and short version of line in dialog. */
   if (no_of_short_lines > 0)
   {
      int gotcha,
          stale_short_lines = NO;

      /*
       * We must first make sure that all hosts in our short host list
       * are still in the FSA. It could be that they have been removed.
       * In that case we must remove those hosts from the list.
       */
      for (i = 0; i < no_of_short_lines; i++)
      {
         gotcha = NO;
         for (j = 0; j < no_of_hosts; j++)
         {
            if (my_strcmp(connect_data[j].hostname, hosts[i]) == 0)
            {
               gotcha = YES;
               break;
            }
         }
         if (gotcha == NO)
         {
            if (no_of_short_lines > 1)
            {
               for (j = i; j < (no_of_short_lines - 1); j++)
               {
                  memcpy(hosts[j], hosts[j + 1], MAX_HOSTNAME_LENGTH + 1);
               }
            }
            no_of_short_lines--;
            stale_short_lines = YES;
            i--;
         }
      }
      if (stale_short_lines == YES)
      {
         /* We must update the setup file or else these stale */
         /* entries always remain there.                      */
         write_setup(hostname_display_length, filename_display_length,
                     -1, hosts, no_of_short_lines, MAX_REAL_HOSTNAME_LENGTH);
      }

      if (no_of_short_lines > 0)
      {
         int  k,
              *short_pos_list,
              tmp_short_pos_list;
         char tmp_host[MAX_REAL_HOSTNAME_LENGTH + 4 + 1];

         /*
          * Next we have to adapt the order of host to what it is currently
          * in FSA.
          */
         if ((short_pos_list = malloc(no_of_short_lines * sizeof(int))) == NULL)
         {
            (void)fprintf(stderr,
                          "Failed to malloc() %d bytes : %s (%s %d)\n",
                          (no_of_short_lines * (int)sizeof(int)),
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         for (i = 0; i < no_of_short_lines; i++)
         {
            for (j = 0; j < no_of_hosts; j++)
            {
               if (my_strcmp(connect_data[j].hostname, hosts[i]) == 0)
               {
                  short_pos_list[i] = j;
                  break;
               }
            }
         }
         for (i = 1; i < no_of_short_lines; i++)
         {
            j = i;
            tmp_short_pos_list = short_pos_list[j];
            (void)memcpy(tmp_host, hosts[j], MAX_REAL_HOSTNAME_LENGTH);
            while ((j > 0) &&
                   (tmp_short_pos_list > short_pos_list[(j - 1) / 2]))
            {
                short_pos_list[j] = short_pos_list[(j - 1) / 2];
                (void)memcpy(hosts[j], hosts[(j - 1) / 2],
                             MAX_REAL_HOSTNAME_LENGTH);
                j = (j - 1) / 2;
            }
            short_pos_list[j] = tmp_short_pos_list;
            (void)memcpy(hosts[j], tmp_host, MAX_REAL_HOSTNAME_LENGTH);
         }
         for (i = (no_of_short_lines - 1); i > 0; i--)
         {
            j = 0;
            k = 1;
            tmp_short_pos_list = short_pos_list[i];
            (void)memcpy(tmp_host, hosts[i], MAX_REAL_HOSTNAME_LENGTH);
            short_pos_list[i] = short_pos_list[0];
            (void)memcpy(hosts[i], hosts[0], MAX_REAL_HOSTNAME_LENGTH);
            while ((k < i) &&
                   ((tmp_short_pos_list < short_pos_list[k]) ||
                     (((k + 1) < i) &&
                      (tmp_short_pos_list < short_pos_list[k + 1]))))
            {
               k += (((k + 1) < i) && (short_pos_list[k + 1] > short_pos_list[k]));
               short_pos_list[j] = short_pos_list[k];
               (void)memcpy(hosts[j], hosts[k], MAX_REAL_HOSTNAME_LENGTH);
               j = k;
               k = 2 * j + 1;
            }
            short_pos_list[j] = tmp_short_pos_list;
            (void)memcpy(hosts[j], tmp_host, MAX_REAL_HOSTNAME_LENGTH);
         }
         free(short_pos_list);

         for (i = 0; i < no_of_short_lines; i++)
         {
            for (j = 0; j < no_of_hosts; j++)
            {
               if ((connect_data[j].short_pos == -1) &&
                   (my_strcmp(connect_data[j].hostname, hosts[i]) == 0))
               {
                  connect_data[j].short_pos = i;
                  connect_data[j].long_pos = -1;
                  break;
               }
            }
         }
      }
   }
   FREE_RT_ARRAY(hosts);
   for (i = 0; i < no_of_hosts; i++)
   {
      if (connect_data[i].short_pos == -1)
      {
         connect_data[i].long_pos = no_of_long_lines;
         no_of_long_lines++;
      }
   }
   if ((no_of_long_lines + no_of_short_lines) != no_of_hosts)
   {
      no_of_short_lines -= ((no_of_long_lines + no_of_short_lines) -
                            no_of_hosts);
   }

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

   (void)sprintf(config_file, "%s%s%s",
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
                                   MAX_REAL_HOSTNAME_LENGTH)) == NULL)
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
                                         MAX_REAL_HOSTNAME_LENGTH)) == NULL)
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
init_menu_bar(GtkWidget **menu_w)
{
   GtkWidget *item,
             *menu,
             *pullright_font,
             *pullright_line_style,
             *pullright_row;

   *menu_w = gtk_menu_bar_new();

   /**********************************************************************/
   /*                           Host Menu                                */
   /**********************************************************************/
   menu = gtk_menu_new();
   item = gtk_tearoff_menu_item_new();
   gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
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
       (ping_cmd != NULL) ||
       (traceroute_cmd != NULL) ||
       (acp.afd_load != NO_PERMISSION))
   {
      if (acp.handle_event != NO_PERMISSION)
      {
         
         ow[HANDLE_EVENT_W] = gtk_menu_item_new_with_label("Handle event");
         g_signal_connect(G_OBJECT(ow[HANDLE_EVENT_W]), "activate",
                          G_CALLBACK(popup_cb), (gpointer)EVENT_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), ow[HANDLE_EVENT_W]);
      }
      if (acp.ctrl_queue != NO_PERMISSION)
      {
         ow[QUEUE_W] = gtk_menu_item_new_with_label("Start/Stop input queue");
         g_signal_connect(G_OBJECT(ow[QUEUE_W]), "activate",
                          G_CALLBACK(popup_cb), (gpointer)QUEUE_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), ow[QUEUE_W]);
      }
      if (acp.ctrl_transfer != NO_PERMISSION)
      {
         ow[TRANSFER_W] = gtk_menu_item_new_with_label("Start/Stop transfer");
         g_signal_connect(G_OBJECT(ow[TRANSFER_W]), "activate",
                          G_CALLBACK(popup_cb), (gpointer)TRANS_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), ow[TRANSFER_W]);
      }
      if (acp.ctrl_queue_transfer != NO_PERMISSION)
      {
         ow[QUEUE_TRANSFER_W] = gtk_menu_item_new_with_label("Start/Stop host");
         g_signal_connect(G_OBJECT(ow[QUEUE_TRANSFER_W]), "activate",
                          G_CALLBACK(popup_cb), (gpointer)QUEUE_TRANS_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), ow[QUEUE_TRANSFER_W]);
      }
      if (acp.disable != NO_PERMISSION)
      {
         ow[DISABLE_W] = gtk_menu_item_new_with_label("Enable/Disable host");
         g_signal_connect(G_OBJECT(ow[DISABLE_W]), "activate",
                          G_CALLBACK(popup_cb), (gpointer)DISABLE_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), ow[DISABLE_W]);
      }
      if (acp.switch_host != NO_PERMISSION)
      {
         ow[SWITCH_W] = gtk_menu_item_new_with_label("Switch host");
         g_signal_connect(G_OBJECT(ow[SWITCH_W]), "activate",
                          G_CALLBACK(popup_cb), (gpointer)SWITCH_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), ow[SWITCH_W]);
      }
      if (acp.retry != NO_PERMISSION)
      {
         ow[RETRY_W] = gtk_menu_item_new_with_mnemonic("_Retry");
         g_signal_connect(G_OBJECT(ow[RETRY_W]), "activate",
                          G_CALLBACK(popup_cb), (gpointer)RETRY_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), ow[RETRY_W]);
      }
      if ((acp.debug != NO_PERMISSION) ||
          (acp.trace != NO_PERMISSION) ||
          (acp.full_trace != NO_PERMISSION))
      {
         GtkWidget *pullright_debug;

         pullright_debug = gtk_menu_new();
         ow[DEBUG_W] = gtk_menu_item_new_with_label("Debug");
         gtk_menu_item_set_submenu(GTK_MENU_ITEM(ow[DEBUG_W]), pullright_debug);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), ow[DEBUG_W]);
         create_pullright_debug(pullright_debug);
      }

      ow[SELECT_W] = gtk_menu_item_new_with_mnemonic("_Search + (De)Select");
      g_signal_connect(G_OBJECT(ow[SELECT_W]), "activate",
                       G_CALLBACK(select_host_dialog), (gpointer)0);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), ow[SELECT_W]);

      ow[LONG_SHORT_W] = gtk_menu_item_new_with_mnemonic("_Long/Short line");
      g_signal_connect(G_OBJECT(ow[LONG_SHORT_W]), "activate",
                       G_CALLBACK(popup_cb), (gpointer)LONG_SHORT_SEL);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), ow[LONG_SHORT_W]);

      if ((traceroute_cmd != NULL) || (ping_cmd != NULL))
      {
         GtkWidget *pullright_test;

         item = gtk_separator_menu_item_new();
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

         pullright_test = gtk_menu_new();
         ow[TEST_W] = gtk_menu_item_new_with_label("Test");
         gtk_menu_item_set_submenu(GTK_MENU_ITEM(ow[TEST_W]), pullright_test);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), ow[TEST_W]);
         create_pullright_test(pullright_test);
      }
      if (acp.afd_load != NO_PERMISSION)
      {
         pullright_load = gtk_menu_new();
         ow[VIEW_LOAD_W] = gtk_menu_item_new_with_label("Load");
         gtk_menu_item_set_submenu(GTK_MENU_ITEM(ow[VIEW_LOAD_W]),
                                   pullright_load);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), ow[VIEW_LOAD_W]);
         create_pullright_load(pullright_load);
      }
      item = gtk_separator_menu_item_new();
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
      item = gtk_separator_menu_item_new();
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
   }
   ow[EXIT_W] = gtk_menu_item_new_with_mnemonic("E_xit");
   g_signal_connect(G_OBJECT(ow[EXIT_W]), "activate",
                    G_CALLBACK(popup_cb), (gpointer)EXIT_SEL);
   gtk_menu_shell_append(GTK_MENU_SHELL(menu), ow[EXIT_W]);

   mw[HOST_W] = gtk_menu_item_new_with_label("Host");
   gtk_menu_item_set_submenu(GTK_MENU_ITEM(mw[HOST_W]), menu);
   gtk_menu_bar_append(GTK_MENU_BAR(*menu_w), mw[HOST_W]);

   /**********************************************************************/
   /*                           View Menu                                */
   /**********************************************************************/
   if ((acp.show_slog != NO_PERMISSION) || (acp.show_elog != NO_PERMISSION) ||
       (acp.show_rlog != NO_PERMISSION) ||
       (acp.show_tlog != NO_PERMISSION) || (acp.show_dlog != NO_PERMISSION) ||
       (acp.show_tdlog != NO_PERMISSION) || (acp.show_ilog != NO_PERMISSION) ||
       (acp.show_olog != NO_PERMISSION) || (acp.show_dlog != NO_PERMISSION) ||
       (acp.show_queue != NO_PERMISSION) || (acp.info != NO_PERMISSION) ||
       (acp.view_dc != NO_PERMISSION) || (acp.view_jobs != NO_PERMISSION))
   {
      menu = gtk_menu_new();
      item = gtk_tearoff_menu_item_new();
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
      if (acp.show_slog != NO_PERMISSION)
      {
         vw[SYSTEM_W] = gtk_menu_item_new_with_label("System Log");
         g_signal_connect(G_OBJECT(vw[SYSTEM_W]), "activate",
                          G_CALLBACK(popup_cb), (gpointer)S_LOG_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), vw[SYSTEM_W]);
      }
      if (acp.show_elog != NO_PERMISSION)
      {
         vw[EVENT_W] = gtk_menu_item_new_with_label("Event Log");
         g_signal_connect(G_OBJECT(vw[EVENT_W]), "activate",
                          G_CALLBACK(popup_cb), (gpointer)E_LOG_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), vw[EVENT_W]);
      }
      if (acp.show_rlog != NO_PERMISSION)
      {
         vw[RECEIVE_W] = gtk_menu_item_new_with_label("Receive Log");
         g_signal_connect(G_OBJECT(vw[RECEIVE_W]), "activate",
                          G_CALLBACK(popup_cb), (gpointer)R_LOG_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), vw[RECEIVE_W]);
      }
      if (acp.show_tlog != NO_PERMISSION)
      {
         vw[TRANS_W] = gtk_menu_item_new_with_label("Transfer Log");
         g_signal_connect(G_OBJECT(vw[TRANS_W]), "activate",
                          G_CALLBACK(popup_cb), (gpointer)T_LOG_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), vw[TRANS_W]);
      }
      if (acp.show_tdlog != NO_PERMISSION)
      {
         vw[TRANS_DEBUG_W] = gtk_menu_item_new_with_label("Transfer Debug Log");
         g_signal_connect(G_OBJECT(vw[TRANS_DEBUG_W]), "activate",
                          G_CALLBACK(popup_cb), (gpointer)TD_LOG_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), vw[TRANS_DEBUG_W]);
      }
      if ((acp.show_ilog != NO_PERMISSION) ||
          (acp.show_olog != NO_PERMISSION) ||
          (acp.show_dlog != NO_PERMISSION))
      {
#if defined (_INPUT_LOG) || defined (_OUTPUT_LOG) || defined (_DELETE_LOG)
         item = gtk_separator_menu_item_new();
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
#endif
#ifdef _INPUT_LOG
         if (acp.show_ilog != NO_PERMISSION)
         {
            vw[INPUT_W] = gtk_menu_item_new_with_label("Input Log");
            g_signal_connect(G_OBJECT(vw[INPUT_W]), "activate",
                             G_CALLBACK(popup_cb), (gpointer)I_LOG_SEL);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), vw[INPUT_W]);
         }
#endif
#ifdef _OUTPUT_LOG
         if (acp.show_olog != NO_PERMISSION)
         {
            vw[OUTPUT_W] = gtk_menu_item_new_with_label("Output Log");
            g_signal_connect(G_OBJECT(vw[OUTPUT_W]), "activate",
                             G_CALLBACK(popup_cb), (gpointer)O_LOG_SEL);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), vw[OUTPUT_W]);
         }
#endif
#ifdef _DELETE_LOG
         if (acp.show_dlog != NO_PERMISSION)
         {
            vw[DELETE_W] = gtk_menu_item_new_with_label("Delete Log");
            g_signal_connect(G_OBJECT(vw[DELETE_W]), "activate",
                             G_CALLBACK(popup_cb), (gpointer)D_LOG_SEL);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), vw[DELETE_W]);
         }
#endif
      }
      if (acp.show_queue != NO_PERMISSION)
      {
         item = gtk_separator_menu_item_new();
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

         vw[SHOW_QUEUE_W] = gtk_menu_item_new_with_label("Queue");
         g_signal_connect(G_OBJECT(vw[SHOW_QUEUE_W]), "activate",
                          G_CALLBACK(popup_cb), (gpointer)SHOW_QUEUE_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), vw[SHOW_QUEUE_W]);
      }
      if ((acp.info != NO_PERMISSION) || (acp.view_dc != NO_PERMISSION))
      {
         item = gtk_separator_menu_item_new();
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

         if (acp.info != NO_PERMISSION)
         {
            vw[INFO_W] = gtk_menu_item_new_with_label("Info");
            g_signal_connect(G_OBJECT(vw[INFO_W]), "activate",
                             G_CALLBACK(popup_cb), (gpointer)INFO_SEL);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), vw[INFO_W]);
         }
         if (acp.view_dc != NO_PERMISSION)
         {
            vw[VIEW_DC_W] = gtk_menu_item_new_with_label("Configuration");
            g_signal_connect(G_OBJECT(vw[VIEW_DC_W]), "activate",
                             G_CALLBACK(popup_cb), (gpointer)VIEW_DC_SEL);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), vw[VIEW_DC_W]);
         }
      }
      if (acp.view_jobs != NO_PERMISSION)
      {
         item = gtk_separator_menu_item_new();
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

         vw[VIEW_JOB_W] = gtk_menu_item_new_with_label("Job details");
         g_signal_connect(G_OBJECT(vw[VIEW_JOB_W]), "activate",
                          G_CALLBACK(popup_cb), (gpointer)VIEW_JOB_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), vw[VIEW_JOB_W]);
      }

      mw[LOG_W] = gtk_menu_item_new_with_mnemonic("_View");
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(mw[LOG_W]), menu);
      gtk_menu_bar_append(GTK_MENU_BAR(*menu_w), mw[LOG_W]);
   }

   /**********************************************************************/
   /*                          Control Menu                              */
   /**********************************************************************/
   if ((acp.amg_ctrl != NO_PERMISSION) || (acp.fd_ctrl != NO_PERMISSION) ||
       (acp.rr_dc != NO_PERMISSION) || (acp.rr_hc != NO_PERMISSION) ||
       (acp.edit_hc != NO_PERMISSION) || (acp.startup_afd != NO_PERMISSION) ||
       (acp.shutdown_afd != NO_PERMISSION) || (acp.dir_ctrl != NO_PERMISSION))
   {
      menu = gtk_menu_new();
      item = gtk_tearoff_menu_item_new();
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

      if (acp.amg_ctrl != NO_PERMISSION)
      {
         cw[AMG_CTRL_W] = gtk_menu_item_new_with_label("Start/Stop AMG");
         g_signal_connect(G_OBJECT(cw[AMG_CTRL_W]), "activate",
                          G_CALLBACK(control_cb), (gpointer)CONTROL_AMG_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), cw[AMG_CTRL_W]);
      }
      if (acp.fd_ctrl != NO_PERMISSION)
      {
         cw[FD_CTRL_W] = gtk_menu_item_new_with_label("Start/Stop FD");
         g_signal_connect(G_OBJECT(cw[FD_CTRL_W]), "activate",
                          G_CALLBACK(control_cb), (gpointer)CONTROL_FD_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), cw[FD_CTRL_W]);
      }
      if ((acp.rr_dc != NO_PERMISSION) || (acp.rr_hc != NO_PERMISSION))
      {
         item = gtk_separator_menu_item_new();
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

         if (acp.rr_dc != NO_PERMISSION)
         {
            cw[RR_DC_W] = gtk_menu_item_new_with_label("Reread DIR_CONFIG");
            g_signal_connect(G_OBJECT(cw[RR_DC_W]), "activate",
                             G_CALLBACK(control_cb),
                             (gpointer)REREAD_DIR_CONFIG_SEL);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), cw[RR_DC_W]);
         }
         if (acp.rr_hc != NO_PERMISSION)
         {
            cw[RR_HC_W] = gtk_menu_item_new_with_label("Reread HOST_CONFIG");
            g_signal_connect(G_OBJECT(cw[RR_HC_W]), "activate",
                             G_CALLBACK(control_cb),
                             (gpointer)REREAD_HOST_CONFIG_SEL);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), cw[RR_HC_W]);
         }
      }
      if (acp.edit_hc != NO_PERMISSION)
      {
         item = gtk_separator_menu_item_new();
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

         cw[EDIT_HC_W] = gtk_menu_item_new_with_label("Edit HOST_CONFIG");
         g_signal_connect(G_OBJECT(cw[EDIT_HC_W]), "activate",
                          G_CALLBACK(popup_cb), (gpointer)EDIT_HC_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), cw[EDIT_HC_W]);
      }
      if (acp.dir_ctrl != NO_PERMISSION)
      {
         item = gtk_separator_menu_item_new();
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

         cw[DIR_CTRL_W] = gtk_menu_item_new_with_label("Directory Control");
         g_signal_connect(G_OBJECT(cw[DIR_CTRL_W]), "activate",
                          G_CALLBACK(popup_cb), (gpointer)DIR_CTRL_SEL);
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), cw[DIR_CTRL_W]);
      }

      /* Startup/Shutdown of AFD. */
      if ((acp.startup_afd != NO_PERMISSION) ||
          (acp.shutdown_afd != NO_PERMISSION))
      {
         item = gtk_separator_menu_item_new();
         gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

         if (acp.startup_afd != NO_PERMISSION)
         {
            cw[STARTUP_AFD_W] = gtk_menu_item_new_with_label("Startup AFD");
            g_signal_connect(G_OBJECT(cw[STARTUP_AFD_W]), "activate",
                             G_CALLBACK(control_cb), (gpointer)STARTUP_AFD_SEL);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), cw[STARTUP_AFD_W]);
         }
         if (acp.shutdown_afd != NO_PERMISSION)
         {
            cw[SHUTDOWN_AFD_W] = gtk_menu_item_new_with_label("Shutdown AFD");
            g_signal_connect(G_OBJECT(cw[SHUTDOWN_AFD_W]), "activate",
                             G_CALLBACK(control_cb),
                             (gpointer)SHUTDOWN_AFD_SEL);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), cw[SHUTDOWN_AFD_W]);
         }
      }

      mw[CONTROL_W] = gtk_menu_item_new_with_mnemonic("_Control");
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(mw[CONTROL_W]), menu);
      gtk_menu_bar_append(GTK_MENU_BAR(*menu_w), mw[CONTROL_W]);
   }

   /**********************************************************************/
   /*                           Setup Menu                               */
   /**********************************************************************/
   menu = gtk_menu_new();
   item = gtk_tearoff_menu_item_new();
   gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

   pullright_font = gtk_menu_new();
   sw[FONT_W] = gtk_menu_item_new_with_label("Font size");
   gtk_menu_item_set_submenu(GTK_MENU_ITEM(sw[FONT_W]), pullright_font);
   gtk_menu_shell_append(GTK_MENU_SHELL(menu), sw[FONT_W]);
   create_pullright_font(pullright_font);

   pullright_row = gtk_menu_new();
   sw[ROWS_W] = gtk_menu_item_new_with_label("Number of rows");
   gtk_menu_item_set_submenu(GTK_MENU_ITEM(sw[ROWS_W]), pullright_row);
   gtk_menu_shell_append(GTK_MENU_SHELL(menu), sw[ROWS_W]);
   create_pullright_row(pullright_row);

   pullright_line_style = gtk_menu_new();
   sw[STYLE_W] = gtk_menu_item_new_with_label("Line Style");
   gtk_menu_item_set_submenu(GTK_MENU_ITEM(sw[STYLE_W]), pullright_line_style);
   gtk_menu_shell_append(GTK_MENU_SHELL(menu), sw[STYLE_W]);
   create_pullright_line_style(pullright_line_style);

   item = gtk_separator_menu_item_new();
   gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

   sw[SAVE_W] = gtk_menu_item_new_with_label("Save Setup");
   g_signal_connect(G_OBJECT(sw[SAVE_W]), "activate",
                    G_CALLBACK(save_setup_cb), (gpointer)0);
   gtk_menu_shell_append(GTK_MENU_SHELL(menu), sw[SAVE_W]);

   mw[CONFIG_W] = gtk_menu_item_new_with_mnemonic("Setu_p");
   gtk_menu_item_set_submenu(GTK_MENU_ITEM(mw[CONFIG_W]), menu);
   gtk_menu_bar_append(GTK_MENU_BAR(*menu_w), mw[CONFIG_W]);

#ifdef _WITH_HELP_PULLDOWN
   /**********************************************************************/
   /*                            Help Menu                               */
   /**********************************************************************/
#endif

   return;
}


/*+++++++++++++++++++++++++ init_popup_menu() +++++++++++++++++++++++++++*/
static void
init_popup_menu(Widget w)
{
   XmString x_string;
   Widget   popupmenu;
   Arg      args[6];
   Cardinal argcount;

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
         argcount = 0;
         x_string = XmStringCreateLocalized("Handle event");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
         pw[0] = XmCreatePushButton(popupmenu, "Event", args, argcount);
         XtAddCallback(pw[0], XmNactivateCallback,
                       popup_cb, (XtPointer)EVENT_SEL);
         XtManageChild(pw[0]);
         XmStringFree(x_string);
      }
      if (acp.ctrl_queue != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Start/Stop input queue");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
         pw[1] = XmCreatePushButton(popupmenu, "Queue", args, argcount);
         XtAddCallback(pw[1], XmNactivateCallback,
                       popup_cb, (XtPointer)QUEUE_SEL);
         XtManageChild(pw[1]);
         XmStringFree(x_string);
      }
      if (acp.ctrl_transfer != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Start/Stop transfer");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
         pw[2] = XmCreatePushButton(popupmenu, "Transfer", args, argcount);
         XtAddCallback(pw[2], XmNactivateCallback,
                       popup_cb, (XtPointer)TRANS_SEL);
         XtManageChild(pw[2]);
         XmStringFree(x_string);
      }
      if (acp.ctrl_queue_transfer != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Start/Stop host");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
         pw[3] = XmCreatePushButton(popupmenu, "Host", args, argcount);
         XtAddCallback(pw[3], XmNactivateCallback,
                       popup_cb, (XtPointer)QUEUE_TRANS_SEL);
         XtManageChild(pw[3]);
         XmStringFree(x_string);
      }
      if (acp.disable != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Enable/Disable host");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
         pw[4] = XmCreatePushButton(popupmenu, "Disable", args, argcount);
         XtAddCallback(pw[4], XmNactivateCallback,
                       popup_cb, (XtPointer)DISABLE_SEL);
         XtManageChild(pw[4]);
         XmStringFree(x_string);
      }
      if (acp.switch_host != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Switch host");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
         pw[5] = XmCreatePushButton(popupmenu, "Switch", args, argcount);
         XtAddCallback(pw[5], XmNactivateCallback,
                       popup_cb, (XtPointer)SWITCH_SEL);
         XtManageChild(pw[5]);
         XmStringFree(x_string);
      }
      if (acp.retry != NO_PERMISSION)
      {
         argcount = 0;
#ifdef WITH_CTRL_ACCELERATOR
         x_string = XmStringCreateLocalized("Retry (Ctrl+r)");
#else
         x_string = XmStringCreateLocalized("Retry (Alt+r)");
#endif
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
#ifdef WITH_CTRL_ACCELERATOR
         XtSetArg(args[argcount], XmNaccelerator, "Ctrl<Key>R"); argcount++;
#else
         XtSetArg(args[argcount], XmNaccelerator, "Alt<Key>R"); argcount++;
#endif
         XtSetArg(args[argcount], XmNmnemonic, 'R'); argcount++;
         XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
         pw[6] = XmCreatePushButton(popupmenu, "Retry", args, argcount);
         XtAddCallback(pw[6], XmNactivateCallback,
                       popup_cb, (XtPointer)RETRY_SEL);
         XtManageChild(pw[6]);
         XmStringFree(x_string);
      }
      if (acp.debug != NO_PERMISSION)
      {
         Widget pullright_debug;

         pullright_debug = XmCreateSimplePulldownMenu(popupmenu,
                                                      "pullright_debug",
                                                      NULL, 0);
         pw[7] = XtVaCreateManagedWidget("Debug",
                                 xmCascadeButtonWidgetClass, popupmenu,
                                 XmNfontList,                fontlist,
                                 XmNsubMenuId,               pullright_debug,
                                 NULL);
         create_pullright_debug(pullright_debug);
      }
      if (acp.info != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Info");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
#ifdef WITH_CTRL_ACCELERATOR
         XtSetArg(args[argcount], XmNaccelerator, "Ctrl<Key>I"); argcount++;
#else
         XtSetArg(args[argcount], XmNaccelerator, "Alt<Key>I"); argcount++;
#endif
         XtSetArg(args[argcount], XmNmnemonic, 'I'); argcount++;
         XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
         pw[8] = XmCreatePushButton(popupmenu, "Info", args, argcount);
         XtAddCallback(pw[8], XmNactivateCallback,
                       popup_cb, (XtPointer)INFO_SEL);
         XtManageChild(pw[8]);
         XmStringFree(x_string);
      }
      if (acp.view_dc != NO_PERMISSION)
      {
         argcount = 0;
         x_string = XmStringCreateLocalized("Configuration");
         XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
         XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
         pw[9] = XmCreatePushButton(popupmenu, "Configuration",
                                         args, argcount);
         XtAddCallback(pw[9], XmNactivateCallback,
                       popup_cb, (XtPointer)VIEW_DC_SEL);
         XtManageChild(pw[9]);
         XmStringFree(x_string);
      }
   }

   XtAddEventHandler(w,
                     ButtonPressMask | ButtonReleaseMask |
                     Button1MotionMask,
                     False, (XtEventHandler)popup_menu_cb, popupmenu);

   return;
}


/*------------------------ create_pullright_test() ----------------------*/
static void
create_pullright_test(GtkWidget *pullright_test)
{
   if (ping_cmd != NULL)
   {
      /* Create pullright for "Ping". */
      tw[PING_W] = gtk_menu_item_new_with_label(SHOW_PING_TEST);
      g_signal_connect(G_OBJECT(tw[PING_W]), "activate",
                       G_CALLBACK(popup_cb), (gpointer)PING_SEL);
      gtk_menu_shell_append(GTK_MENU_SHELL(pullright_test), tw[PING_W]);
   }

   if (traceroute_cmd != NULL)
   {
      /* Create pullright for "Traceroute". */
      tw[TRACEROUTE_W] = gtk_menu_item_new_with_label(SHOW_TRACEROUTE_TEST);
      g_signal_connect(G_OBJECT(tw[TRACEROUTE_W]), "activate",
                       G_CALLBACK(popup_cb), (gpointer)TRACEROUTE_SEL);
      gtk_menu_shell_append(GTK_MENU_SHELL(pullright_test), tw[TRACEROUTE_W]);
   }

   return;
}


/*------------------------ create_pullright_load() ----------------------*/
static void
create_pullright_load(GtkWidget *pullright_line_load)
{
   /* Create pullright for "Files". */
   lw[FILE_LOAD_W] = gtk_menu_item_new_with_label(SHOW_FILE_LOAD);
   g_signal_connect(G_OBJECT(lw[FILE_LOAD_W]), "activate",
                    G_CALLBACK(popup_cb), (gpointer)VIEW_FILE_LOAD_SEL);
   gtk_menu_shell_append(GTK_MENU_SHELL(pullright_load), lw[FILE_LOAD_W]);

   /* Create pullright for "KBytes". */
   lw[KBYTE_LOAD_W] = gtk_menu_item_new_with_label(SHOW_KBYTE_LOAD);
   g_signal_connect(G_OBJECT(lw[KBYTE_LOAD_W]), "activate",
                    G_CALLBACK(popup_cb), (gpointer)VIEW_KBYTE_LOAD_SEL);
   gtk_menu_shell_append(GTK_MENU_SHELL(pullright_load), lw[KBYTE_LOAD_W]);

   /* Create pullright for "Connections". */
   lw[CONNECTION_LOAD_W] = gtk_menu_item_new_with_label(SHOW_CONNECTION_LOAD);
   g_signal_connect(G_OBJECT(lw[CONNECTION_LOAD_W]), "activate",
                    G_CALLBACK(popup_cb), (gpointer)VIEW_CONNECTION_LOAD_SEL);
   gtk_menu_shell_append(GTK_MENU_SHELL(pullright_load), lw[CONNECTION_LOAD_W]);

   /* Create pullright for "Active-Transfers". */
   lw[TRANSFER_LOAD_W] = gtk_menu_item_new_with_label(SHOW_TRANSFER_LOAD);
   g_signal_connect(G_OBJECT(lw[TRANSFER_LOAD_W]), "activate",
                    G_CALLBACK(popup_cb), (gpointer)VIEW_TRANSFER_LOAD_SEL);
   gtk_menu_shell_append(GTK_MENU_SHELL(pullright_load), lw[TRANSFER_LOAD_W]);

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
   Arg             args[MAXARGS];
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
   }

   return;
}


/*------------------------ create_pullright_row() -----------------------*/
static void
create_pullright_row(Widget pullright_row)
{
   int    i;
   char   *row[NO_OF_ROWS] =
          {
             ROW_0, ROW_1, ROW_2, ROW_3, ROW_4, ROW_5, ROW_6,
             ROW_7, ROW_8, ROW_9, ROW_10, ROW_11, ROW_12, ROW_13,
             ROW_14, ROW_15, ROW_16
          };
   GSList *group;

   group = NULL;
   for (i = 0; i < NO_OF_ROWS; i++)
   {
      rw[i] = gtk_radio_menu_item_new_with_label(group, row[i]);
      group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(rw[i]));
      if ((current_row == -1) && (no_of_rows_set == atoi(row[i])))
      {
         current_row = i;
         gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(rw[i]), TRUE);
      }
      g_signal_connect(G_OBJECT(rw[i]), "activate",
                       G_CALLBACK(change_rows_cb), GINT_TO_POINTER(i));

      gtk_menu_shell_append(GTK_MENU_SHELL(pullright_row), rw[i]);
   }

   return;
}


/*------------------------ create_pullright_style() ---------------------*/
static void
create_pullright_style(Widget pullright_line_style)
{
   XmString x_string;
   Arg      args[MAXARGS];
   Cardinal argcount;

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

   argcount = 0;
   x_string = XmStringCreateLocalized("Process");
   XtSetArg(args[argcount], XmNlabelString, x_string); argcount++;
   XtSetArg(args[argcount], XmNindicatorType, XmN_OF_MANY); argcount++;
   XtSetArg(args[argcount], XmNfontList, fontlist); argcount++;
   lsw[JOBS_STYLE_W] = XmCreateToggleButton(pullright_line_style, "style_1",
                                            args, argcount);
   XtAddCallback(lsw[JOBS_STYLE_W], XmNvalueChangedCallback, change_style_cb,
                 (XtPointer)JOBS_STYLE_W);
   XtManageChild(lsw[JOBS_STYLE_W]);
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


/*------------------------ create_pullright_debug() ---------------------*/
static void
create_pullright_debug(GtkWidget *pullright_debug)
{
   if (acp.debug != NO_PERMISSION)
   {
      /* Create button for "Debug". */
      dprw[DEBUG_STYLE_W] = gtk_menu_item_new_with_mnemonic("_Debug");
      g_signal_connect(G_OBJECT(dprw[DEBUG_STYLE_W]), "activate",
                       G_CALLBACK(popup_cb), (gpointer)DEBUG_SEL);
      gtk_menu_shell_append(GTK_MENU_SHELL(pullright_debug),
                            dprw[DEBUG_STYLE_W]);
   }
   if (acp.trace != NO_PERMISSION)
   {
      /* Create button for "Trace". */
      dprw[TRACE_STYLE_W] = gtk_menu_item_new_with_label("Trace");
      g_signal_connect(G_OBJECT(dprw[TRACE_STYLE_W]), "activate",
                       G_CALLBACK(popup_cb), (gpointer)TRACE_SEL);
      gtk_menu_shell_append(GTK_MENU_SHELL(pullright_debug),
                            dprw[TRACE_STYLE_W]);
   }
   if (acp.full_trace != NO_PERMISSION)
   {
      /* Create button for "Full Trace". */
      dprw[FULL_TRACE_STYLE_W] = gtk_menu_item_new_with_label("Full Trace");
      g_signal_connect(G_OBJECT(dprw[FULL_TRACE_STYLE_W]), "activate",
      G_CALLBACK(popup_cb), (gpointer)FULL_TRACE_SEL);
      gtk_menu_shell_append(GTK_MENU_SHELL(pullright_debug),
                            dprw[FULL_TRACE_STYLE_W]);
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
      acp.ctrl_transfer            = YES;   /* Start/Stop a transfer */
      acp.ctrl_transfer_list       = NULL;
      acp.ctrl_queue               = YES;   /* Start/Stop the input queue */
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
      acp.retry                    = YES;
      acp.retry_list               = NULL;
      acp.show_slog                = YES;   /* View the system log   */
      acp.show_slog_list           = NULL;
      acp.show_rlog                = YES;   /* View the receive log */
      acp.show_rlog_list           = NULL;
      acp.show_tlog                = YES;   /* View the transfer log */
      acp.show_tlog_list           = NULL;
      acp.show_dlog                = YES;   /* View the debug log    */
      acp.show_dlog_list           = NULL;
      acp.show_ilog                = YES;   /* View the input log    */
      acp.show_ilog_list           = NULL;
      acp.show_olog                = YES;   /* View the output log   */
      acp.show_olog_list           = NULL;
      acp.show_elog                = YES;   /* View the delete log   */
      acp.show_elog_list           = NULL;
      acp.show_queue               = YES;   /* View the AFD queue    */
      acp.show_queue_list          = NULL;
      acp.view_jobs                = YES;   /* View jobs             */
      acp.view_jobs_list           = NULL;
      acp.edit_hc                  = YES;   /* Edit Host Configuration */
      acp.edit_hc_list             = NULL;
      acp.view_dc                  = YES;   /* View DIR_CONFIG entries */
      acp.view_dc_list             = NULL;
      acp.dir_ctrl                 = YES;
   }
   else
   {
      /*
       * First of all check if the user may use this program
       * at all.
       */
      if ((ptr = posi(perm_buffer, AFD_CTRL_PERM)) == NULL)
      {
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
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
   }

   return;
}


/*+++++++++++++++++++++++++++ afd_ctrl_exit() +++++++++++++++++++++++++++*/
static void
afd_ctrl_exit(void)
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

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void                                                                
sig_segv(int signo)
{
   afd_ctrl_exit();
   (void)fprintf(stderr, "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
                 __FILE__, __LINE__);
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void                                                                
sig_bus(int signo)
{
   afd_ctrl_exit();
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
