/*
 *  nafd_ctrl.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   nafd_ctrl - controls and monitors the AFD
 **
 ** SYNOPSIS
 **   nafd_ctrl [--version]
 **             [-w <AFD working directory>]
 **             [-p <user profile>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.08.2008 H.Kiehl Created
 **
 */
DESCR__E_M1

/* #define WITH_MEMCHECK */

#include <stdio.h>            /* fprintf(), stderr                       */
#include <string.h>           /* strcpy(), strcmp()                      */
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
#include <ncurses.h>
#include "nafd_ctrl.h"
#include <errno.h>
#include "version.h"
#include "permission.h"

/* Global variables. */
int                        event_log_fd = STDERR_FILENO,
                           fsa_fd = -1,
                           fsa_id,
                           hostname_display_length,
                           *line_length = NULL,
                           max_line_length,
                           no_selected,
                           no_selected_static,
                           no_of_active_process = 0,
                           no_of_columns,
                           no_of_rows,
                           no_of_rows_set,
                           no_of_hosts,
                           no_of_jobs_selected,
                           sys_log_fd = STDERR_FILENO,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           sys_log_readfd,
#endif
                           window_width,
                           window_height,
                           x_offset_led,
                           x_offset_debug_led,
                           x_offset_proc,
                           x_offset_bars,
                           x_offset_characters,
                           y_offset_led;
long                       danger_no_of_jobs,
                           link_max;
#ifdef HAVE_MMAP
off_t                      fsa_size,
                           afd_active_size;
#endif
time_t                     afd_active_time;
unsigned short             step_size;
unsigned long              redraw_time_host,
                           redraw_time_status;
unsigned int               text_offset;
char                       work_dir[MAX_PATH_LENGTH],
                           *p_work_dir,
                           *pid_list,
                           afd_active_file[MAX_PATH_LENGTH],
                           *db_update_reply_fifo = NULL,
                           line_style,
                           fake_user[MAX_FULL_USER_ID_LENGTH],
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
struct line                *connect_data;
struct job_data            *jd = NULL;
struct afd_status          *p_afd_status,
                           prev_afd_status;
struct filetransfer_status *fsa;
struct afd_control_perm    acp;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void                nafd_ctrl_exit(void),
                           eval_permissions(char *),
                           init_nafd_ctrl(int *, char **),
                           sig_bus(int),
                           sig_exit(int),
                           sig_segv(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int ch;

#ifdef WITH_MEMCHECK
   mtrace();
#endif
   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values. */
   init_nafd_ctrl(&argc, argv);

   /* Set some signal handlers. */
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR))
   {
      (void)fprintf(stderr,
                    "Failed to set signal handlers for nafd_ctrl : %s\n",
                    strerror(errno));
   }

   /* Exit handler so we can close applications that the user started. */
   if (atexit(nafd_ctrl_exit) != 0)
   {
      (void)fprintf(stderr,
                    "Failed to set exit handler for nafd_ctrl : %s\n",
                    strerror(errno));
   }

   initscr();

   /* Setup colors. */
   start_color();
   init_pair(1, COLOR_WHITE, COLOR_BLUE);

   noecho();
   nodelay(stdscr, TRUE);
   for (;;)
   {
      if ((ch = getch()) == 'q')
      {
         break;
      }
      else
      {
         napms(500); /* Wait 0.5 seconds. */
      }
   }

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++ init_nafd_ctrl() ++++++++++++++++++++++++++*/
static void
init_nafd_ctrl(int *argc, char *argv[])
{
   int          fd,
                i,
                j,
                *no_of_invisible_members = 0,
                prev_plus_minus,
                user_offset;
   time_t       start_time;
   char         afd_file_dir[MAX_PATH_LENGTH],
                *buffer,
                config_file[MAX_PATH_LENGTH],
                hostname[MAX_AFD_NAME_LENGTH],
                **hosts = NULL,
                **invisible_members = NULL,
                *perm_buffer;
   struct tms   tmsdummy;
   struct stat  stat_buf;

   /* See if user wants some help. */
   if ((get_arg(argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      (void)fprintf(stdout,
                    "Usage: %s [-w <work_dir>] [-p <profile>] [-u[ <user>]]\n",
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

   get_user(user, fake_user, user_offset);

   /*
    * Attach to the FSA and get the number of host
    * and the fsa_id of the FSA.
    */
   if (fsa_attach("nafd_ctrl") != SUCCESS)
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
   RT_ARRAY(hosts, no_of_hosts, (MAX_HOSTNAME_LENGTH + 4 + 1), char);
   read_setup(AFD_CTRL, profile, &hostname_display_length,
              &filename_display_length, NULL, hosts, MAX_HOSTNAME_LENGTH,
              &no_of_invisible_members, &invisible_members);
   prev_plus_minus = PM_OPEN_STATE;

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
   }

   if (invisible_members != NULL)
   {
      FREE_RT_ARRAY(invisible_members);
   }
   FREE_RT_ARRAY(hosts);

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


/*+++++++++++++++++++++++++++ nafd_ctrl_exit() ++++++++++++++++++++++++++*/
static void
nafd_ctrl_exit(void)
{
   int i;

   endwin();

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
   nafd_ctrl_exit();
   (void)fprintf(stderr, "Aaarrrggh! Received SIGSEGV. (%s %d)\n",
                 __FILE__, __LINE__);
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void                                                                
sig_bus(int signo)
{
   nafd_ctrl_exit();
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
