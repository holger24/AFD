/*
 *  mouse_handler.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2020 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mouse_handler - handles all mouse- AND key events of the dir_ctrl
 **                   dialog
 **
 ** SYNOPSIS
 **   void dir_focus(Widget w, XtPointer client_data, XEvent *event)
 **   void dir_input(Widget w, XtPointer client_data, XtPointer call_data)
 **   void popup_dir_menu_cb(Widget w, XtPointer client_data, XEvent *event)
 **   void save_dir_setup_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void dir_popup_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_dir_font_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_dir_row_cb(Widget w, XtPointer client_data, XtPointer call_data)
 **   void change_dir_style_cb(Widget w, XtPointer client_data, XtPointer call_data)
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
 **   31.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>             /* fprintf(), stderr                      */
#include <string.h>            /* strcpy(), strlen()                     */
#include <stdlib.h>            /* atoi(), exit()                         */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>          /* waitpid()                              */
#include <fcntl.h>
#include <unistd.h>            /* fork()                                 */
#include <time.h>              /* time()                                 */
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/DrawingA.h>
#include "mshow_log.h"
#include "dir_ctrl.h"
#include "permission.h"

/* #define SHOW_CALLS */

/* External global variables. */
extern Display                    *display;
extern Widget                     fw[],
                                  rw[],
                                  tw[],
                                  line_window_w,
                                  lsw[],
                                  oow[];
extern Window                     line_window;
extern Pixmap                     label_pixmap,
                                  line_pixmap;
extern XFontStruct                *font_struct;
extern GC                         letter_gc,
                                  normal_letter_gc,
                                  locked_letter_gc,
                                  color_letter_gc,
                                  red_color_letter_gc,
                                  default_bg_gc,
                                  normal_bg_gc,
                                  locked_bg_gc,
                                  label_bg_gc,
                                  tr_bar_gc,
                                  fr_bar_gc,
                                  color_gc,
                                  black_line_gc,
                                  white_line_gc;
extern int                        depth,
                                  line_height,
                                  line_length,
                                  no_of_active_process,
                                  no_of_dirs,
                                  no_of_jobs_selected,
                                  no_selected,
                                  no_selected_static,
                                  no_of_rows,
                                  no_of_rows_set;
extern XT_PTR_TYPE                current_font,
                                  current_row,
                                  current_style;
extern float                      max_bar_length;
extern unsigned long              color_pool[];
extern char                       fake_user[],
                                  font_name[],
                                  title[],
                                  line_style,
                                  other_options,
                                  profile[],
                                  *p_work_dir,
                                  user[],
                                  username[];
extern struct dir_line            *connect_data;
extern struct fileretrieve_status *fra;
extern struct dir_control_perm    dcp;
extern struct apps_list           *apps_list;

/* Global variables. */
int                               fsa_fd = -1,
                                  fsa_id,
                                  no_of_hosts;
#ifdef HAVE_MMAP
off_t                             fsa_size;
#endif
struct filetransfer_status        *fsa;

/* Local global variables. */
static int                        in_window = NO;


/*############################ dir_focus() ##############################*/
void
dir_focus(Widget      w,
          XtPointer   client_data,
          XEvent      *event)
{
   if (event->xany.type == EnterNotify)
   {
      in_window = YES;
   }
   if (event->xany.type == LeaveNotify)
   {
      in_window = NO;
   }

   return;
}


/*############################ dir_input() ##############################*/
void
dir_input(Widget      w,
          XtPointer   client_data,
          XEvent      *event)
{
   int        select_no;
   static int last_motion_pos = -1;

   if (event->xany.type == EnterNotify)
   {
      XmProcessTraversal(line_window_w, XmTRAVERSE_CURRENT);
   }

   /* Handle any motion event. */
   if ((event->xany.type == MotionNotify) && (in_window == YES))
   {
      select_no = (event->xbutton.y / line_height) +
                  ((event->xbutton.x / line_length) * no_of_rows);

      if ((select_no < no_of_dirs) && (last_motion_pos != select_no))
      {
         if (event->xkey.state & ControlMask)
         {
            if (connect_data[select_no].inverse == STATIC)
            {
               connect_data[select_no].inverse = OFF;
               no_selected_static--;
            }
            else
            {
               connect_data[select_no].inverse = STATIC;
               no_selected_static++;
            }

            draw_dir_line_status(select_no, select_no);
            XFlush(display);
         }
         else if (event->xkey.state & ShiftMask)
              {
                 if (connect_data[select_no].inverse == ON)
                 {
                    connect_data[select_no].inverse = OFF;
                    no_selected--;
                 }
                 else if (connect_data[select_no].inverse == STATIC)
                      {
                         connect_data[select_no].inverse = OFF;
                         no_selected_static--;
                      }
                      else
                      {
                         connect_data[select_no].inverse = ON;
                         no_selected++;
                      }

                 draw_dir_line_status(select_no, 1);
                 XFlush(display);
              }
      }
      last_motion_pos = select_no;

      return;
   }

   /* Handle any button press event. */
   if (event->xbutton.button == 1)
   {
      select_no = (event->xbutton.y / line_height) +
                  ((event->xbutton.x / line_length) * no_of_rows);

      /* Make sure that this field does contain a channel. */
      if (select_no < no_of_dirs)
      {
         if (((event->xkey.state & Mod1Mask) ||
             (event->xkey.state & Mod4Mask)) &&
             (event->xany.type == ButtonPress))
         {
            int    gotcha = NO,
                   i;
            Window window_id;

            for (i = 0; i < no_of_active_process; i++)
            {
               if ((apps_list[i].position == select_no) &&
                   (CHECK_STRCMP(apps_list[i].progname, DIR_INFO) == 0))
               {
                  if ((window_id = get_window_id(apps_list[i].pid,
                                                 DIR_CTRL)) != 0L)
                  {
                     gotcha = YES;
                  }
                  break;
               }
            }
            if (gotcha == NO)
            {
               char *args[8],
                    progname[MAX_PATH_LENGTH];

               args[0] = progname;
               args[1] = "-f";
               args[2] = font_name;
               args[3] = "-d";
               args[4] = fra[select_no].dir_alias;
               if (fake_user[0] != '\0')
               {
                  args[5] = "-u";
                  args[6] = fake_user;
                  args[7] = NULL;
               }
               else
               {
                  args[5] = NULL;
               }
               (void)strcpy(progname, DIR_INFO);

               make_xprocess(progname, progname, args, select_no);
            }
            else
            {
               XRaiseWindow(display, window_id);
               XSetInputFocus(display, window_id, RevertToParent,
                              CurrentTime);
            }
         }
         else if (event->xany.type == ButtonPress)
              {
                 if (event->xkey.state & ControlMask)
                 {
                    if (connect_data[select_no].inverse == STATIC)
                    {
                       connect_data[select_no].inverse = OFF;
                       no_selected_static--;
                    }
                    else
                    {
                       connect_data[select_no].inverse = STATIC;
                       no_selected_static++;
                    }

                    draw_dir_line_status(select_no, 1);
                    XFlush(display);
                 }
                 else if (event->xkey.state & ShiftMask)
                      {
                         if (connect_data[select_no].inverse == OFF)
                         {
                            int i;

                            if (select_no > 0)
                            {
                               for (i = select_no - 1; i > 0; i--)
                               {
                                  if (connect_data[i].inverse != OFF)
                                  {
                                     break;
                                  }
                               }
                            }
                            else
                            {
                               i = 0;
                            }
                            if (connect_data[i].inverse != OFF)
                            {
                               int j;

                               for (j = i + 1; j <= select_no; j++)
                               {
                                  connect_data[j].inverse = connect_data[i].inverse;
                                  draw_dir_line_status(j, 1);
                               }
                            }
                            else
                            {
                               connect_data[select_no].inverse = ON;
                               no_selected++;
                               draw_dir_line_status(select_no, 1);
                            }
                         }
                         else
                         {
                            if (connect_data[select_no].inverse == ON)
                            {
                               connect_data[select_no].inverse = OFF;
                               no_selected--;
                            }
                            else
                            {
                               connect_data[select_no].inverse = OFF;
                               no_selected_static--;
                            }
                            draw_dir_line_status(select_no, 1);
                         }
                         XFlush(display);
                      }
                      else
                      {
                         if ((other_options & FORCE_SHIFT_SELECT) == 0)
                         {
                            if (connect_data[select_no].inverse == ON)
                            {
                               connect_data[select_no].inverse = OFF;
                               no_selected--;
                            }
                            else if (connect_data[select_no].inverse == STATIC)
                                 {
                                    connect_data[select_no].inverse = OFF;
                                    no_selected_static--;
                                 }
                                 else
                                 {
                                    connect_data[select_no].inverse = ON;
                                    no_selected++;
                                 }

                            draw_dir_line_status(select_no, 1);
                            XFlush(display);
                         }
                      }

                 last_motion_pos = select_no;
              }
#ifdef _DEBUG
         (void)fprintf(stderr, "input(): no_selected = %d    select_no = %d\n",
                       no_selected, select_no);
         (void)fprintf(stderr, "input(): xbutton.x     = %d\n",
                       event->xbutton.x);
         (void)fprintf(stderr, "input(): xbutton.y     = %d\n",
                       event->xbutton.y);
#endif
      }
   }

   if ((event->type == KeyPress) && (event->xkey.state & ControlMask))
   {
      int            bufsize = 10,
                     count;
      char           buffer[10];
      KeySym         keysym;
      XComposeStatus compose;

      count = XLookupString((XKeyEvent *)event, buffer, bufsize,
                            &keysym, &compose);
      buffer[count] = '\0';
      if ((keysym == XK_plus) || (keysym == XK_minus))
      {
         XT_PTR_TYPE new_font;

         if (keysym == XK_plus)
         {
            for (new_font = current_font + 1; new_font < NO_OF_FONTS; new_font++)
            {
               if (fw[new_font] != NULL)
               {
                  break;
               }
            }
         }
         else
         {
            for (new_font = current_font - 1; new_font >= 0; new_font--)
            {
               if (fw[new_font] != NULL)
               {
                  break;
               }
            }
         }
         if ((new_font >= 0) && (new_font < NO_OF_FONTS) &&
             (current_font != new_font))
         {
            change_dir_font_cb(w, (XtPointer)new_font, NULL);
         }

         return;
      }
   }

   return;
}


/*########################## popup_dir_menu_cb() ########################*/
void
popup_dir_menu_cb(Widget      w,
                  XtPointer   client_data,
                  XEvent      *event)
{
   Widget popup = (Widget)client_data;

   if ((event->xany.type != ButtonPress) ||
       (event->xbutton.button != 3) ||
       (event->xkey.state & ControlMask))
   {
      return;
   }

   /* Position the menu where the event occurred. */
   XmMenuPosition(popup, (XButtonPressedEvent *) (event));
   XtManageChild(popup);

   return;
}


/*######################## save_dir_setup_cb() ##########################*/
void
save_dir_setup_cb(Widget    w,
                  XtPointer client_data,
                  XtPointer call_data)
{
   write_setup(-1, -1, -1, "");

   return;
}


/*########################### dir_popup_cb() ############################*/
void
dir_popup_cb(Widget    w,
             XtPointer client_data,
             XtPointer call_data)
{
   int         i,
#ifdef _DEBUG
               j,
#endif
               k,
               offset = 0,
               send_msg = NO;
   char        host_err_no[1025],
               progname[MAX_PROCNAME_LENGTH + 1],
               **args = NULL,
               **dirs = NULL,
               **hosts = NULL,
               log_typ[30],
               display_error,
       	       err_msg[1025 + 100];
   size_t      new_size = (no_of_dirs + 12) * sizeof(char *);
   time_t      current_time;
   XT_PTR_TYPE sel_typ = (XT_PTR_TYPE)client_data;

   if ((no_selected == 0) && (no_selected_static == 0) &&
       ((sel_typ == DIR_STOP_SEL) || (sel_typ == DIR_DISABLE_SEL) ||
        (sel_typ == DIR_INFO_SEL) || (sel_typ == DIR_RESCAN_SEL) ||
        (sel_typ == DIR_VIEW_DC_SEL) || (sel_typ == DIR_HANDLE_EVENT_SEL)))
   {
      (void)xrec(INFO_DIALOG,
                 "You must first select a directory!\nUse mouse button 1 together with the SHIFT or CTRL key.");
      return;
   }
   RT_ARRAY(dirs, no_of_dirs, 21, char);
   RT_ARRAY(hosts, no_of_dirs, (MAX_HOSTNAME_LENGTH + 1), char);
   if ((args = malloc(new_size)) == NULL)
   {
      (void)xrec(FATAL_DIALOG, "malloc() error : %s [%d] (%s %d)",
                 strerror(errno), errno, __FILE__, __LINE__);
      FREE_RT_ARRAY(dirs)
      FREE_RT_ARRAY(hosts)
      return;
   }
#ifdef SHOW_CALLS
   args[0] = NULL;
#endif

   switch (sel_typ)
   {
      case DIR_HANDLE_EVENT_SEL:
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         if (title[0] != '\0')
         {
            args[5] = "-t";
            args[6] = title;
            offset = 7;
         }
         else
         {
            offset = 5;
         }
         if (fake_user[0] != '\0')
         {
            args[offset] = "-u";
            args[offset + 1] = fake_user;
            offset += 2;
         }
         if (profile[0] != '\0')
         {
            args[offset] = "-p";
            args[offset + 1] = profile;
            offset += 2;
         }
         (void)strcpy(progname, HANDLE_EVENT);
         break;

      case DIR_STOP_SEL:
      case DIR_DISABLE_SEL:
      case DIR_RESCAN_SEL:
         break;

      case DIR_INFO_SEL : /* Information */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = "-d";
         if (fake_user[0] != '\0')
         {
            args[7] = "-u";
            args[8] = fake_user;
            args[9] = NULL;
         }
         else
         {
            args[7] = NULL;
         }
         (void)strcpy(progname, DIR_INFO);
         break;

      case S_LOG_SEL : /* System Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         if (title[0] != '\0')
         {
            args[5] = "-t";
            args[6] = title;
            offset = 7;
         }
         else
         {
            offset = 5;
         }
         if (fake_user[0] != '\0')
         {
            args[offset] = "-u";
            args[offset + 1] = fake_user;
            offset += 2;
         }
         if (profile[0] != '\0')
         {
            args[offset] = "-p";
            args[offset + 1] = profile;
            offset += 2;
         }
         args[offset] = "-l";
         args[offset + 1] = log_typ;
         args[offset + 2] = NULL;
         (void)strcpy(progname, SHOW_LOG);
         (void)strcpy(log_typ, SYSTEM_STR);
         make_xprocess(progname, progname, args, -1);
         free(args);
         FREE_RT_ARRAY(dirs)
         FREE_RT_ARRAY(hosts)
         return;

      case E_LOG_SEL : /* Event Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         if (title[0] != '\0')
         {
            args[5] = "-t";
            args[6] = title;
            offset = 7;
         }
         else
         {
            offset = 5;
         }
         if (fake_user[0] != '\0')
         {
            args[offset] = "-u";
            args[offset + 1] = fake_user;
            offset += 2;
         }
         (void)strcpy(progname, SHOW_ELOG);
         break;

      case R_LOG_SEL : /* Receive Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         if (title[0] != '\0')
         {
            args[5] = "-t";
            args[6] = title;
            offset = 7;
         }
         else
         {
            offset = 5;
         }
         if (fake_user[0] != '\0')
         {
            args[offset] = "-u";
            args[offset + 1] = fake_user;
            offset += 2;
         }
         if (profile[0] != '\0')
         {
            args[offset] = "-p";
            args[offset + 1] = profile;
            offset += 2;
         }
         args[offset] = "-l";
         args[offset + 1] = log_typ;
         offset += 2;
         (void)strcpy(progname, SHOW_LOG);
         (void)strcpy(log_typ, RECEIVE_STR);
         break;

      case T_LOG_SEL : /* Transfer Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         if (title[0] != '\0')
         {
            args[5] = "-t";
            args[6] = title;
            offset = 7;
         }
         else
         {
            offset = 5;
         }
         if (fake_user[0] != '\0')
         {
            args[offset] = "-u";
            args[offset + 1] = fake_user;
            offset += 2;
         }
         if (profile[0] != '\0')
         {
            args[offset] = "-p";
            args[offset + 1] = profile;
            offset += 2;
         }
         args[offset] = "-l";
         args[offset + 1] = log_typ;
         offset += 2;
         (void)strcpy(progname, SHOW_LOG);
         (void)strcpy(log_typ, TRANSFER_STR);
         break;

      case I_LOG_SEL : /* Input Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         if (title[0] != '\0')
         {
            args[5] = "-t";
            args[6] = title;
            offset = 7;
         }
         else
         {
            offset = 5;
         }
         if (fake_user[0] != '\0')
         {
            args[offset] = "-u";
            args[offset + 1] = fake_user;
            offset += 2;
         }
         if (profile[0] != '\0')
         {
            args[offset] = "-p";
            args[offset + 1] = profile;
            offset += 2;
         }
         (void)strcpy(progname, SHOW_ILOG);
         break;

      case P_LOG_SEL : /* Production Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         if (title[0] != '\0')
         {
            args[5] = "-t";
            args[6] = title;
            offset = 7;
         }
         else
         {
            offset = 5;
         }
         if (fake_user[0] != '\0')
         {
            args[offset] = "-u";
            args[offset + 1] = fake_user;
            offset += 2;
         }
         if (profile[0] != '\0')
         {
            args[offset] = "-p";
            args[offset + 1] = profile;
            offset += 2;
         }
         (void)strcpy(progname, SHOW_PLOG);
         break;

      case O_LOG_SEL : /* Output Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         if (title[0] != '\0')
         {
            args[5] = "-t";
            args[6] = title;
            offset = 7;
         }
         else
         {
            offset = 5;
         }
         if (fake_user[0] != '\0')
         {
            args[offset] = "-u";
            args[offset + 1] = fake_user;
            offset += 2;
         }
         if (profile[0] != '\0')
         {
            args[offset] = "-p";
            args[offset + 1] = profile;
            offset += 2;
         }
         (void)strcpy(progname, SHOW_OLOG);
         break;

      case D_LOG_SEL : /* Delete Log */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         if (title[0] != '\0')
         {
            args[5] = "-t";
            args[6] = title;
            offset = 7;
         }
         else
         {
            offset = 5;
         }
         if (fake_user[0] != '\0')
         {
            args[offset] = "-u";
            args[offset + 1] = fake_user;
            offset += 2;
         }
         if (profile[0] != '\0')
         {
            args[offset] = "-p";
            args[offset + 1] = profile;
            offset += 2;
         }
         (void)strcpy(progname, SHOW_DLOG);
         break;

      case SHOW_QUEUE_SEL : /* Queue */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         if (title[0] != '\0')
         {
            args[5] = "-t";
            args[6] = title;
            offset = 7;
         }
         else
         {
            offset = 5;
         }
         if (fake_user[0] != '\0')
         {
            args[offset] = "-u";
            args[offset + 1] = fake_user;
            offset += 2;
         }
         if (profile[0] != '\0')
         {
            args[offset] = "-p";
            args[offset + 1] = profile;
            offset += 2;
         }
         (void)strcpy(progname, SHOW_QUEUE);
         break;

      case VIEW_FILE_LOAD_SEL : /* File Load */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-l";
         args[4] = log_typ;
         args[5] = "-f";
         args[6] = font_name;
         args[7] = NULL;
         (void)strcpy(progname, AFD_LOAD);
         (void)strcpy(log_typ, SHOW_FILE_LOAD);
	 make_xprocess(progname, progname, args, -1);
         free(args);
         FREE_RT_ARRAY(dirs)
         FREE_RT_ARRAY(hosts)
	 return;

      case VIEW_KBYTE_LOAD_SEL : /* KByte Load */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-l";
         args[4] = log_typ;
         args[5] = "-f";
         args[6] = font_name;
         args[7] = NULL;
         (void)strcpy(progname, AFD_LOAD);
         (void)strcpy(log_typ, SHOW_KBYTE_LOAD);
	 make_xprocess(progname, progname, args, -1);
         free(args);
         FREE_RT_ARRAY(dirs)
         FREE_RT_ARRAY(hosts)
	 return;

      case VIEW_CONNECTION_LOAD_SEL : /* Connection Load */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-l";
         args[4] = log_typ;
         args[5] = "-f";
         args[6] = font_name;
         args[7] = NULL;
         (void)strcpy(progname, AFD_LOAD);
         (void)strcpy(log_typ, SHOW_CONNECTION_LOAD);
         make_xprocess(progname, progname, args, -1);
         free(args);
         FREE_RT_ARRAY(dirs)
         FREE_RT_ARRAY(hosts)
         return;

      case VIEW_TRANSFER_LOAD_SEL : /* Active Transfers Load */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-l";
         args[4] = log_typ;
         args[5] = "-f";
         args[6] = font_name;
         args[7] = NULL;
         (void)strcpy(progname, AFD_LOAD);
         (void)strcpy(log_typ, SHOW_TRANSFER_LOAD);
         make_xprocess(progname, progname, args, -1);
         free(args);
         FREE_RT_ARRAY(dirs)
         FREE_RT_ARRAY(hosts)
         return;

      case DIR_VIEW_DC_SEL : /* View DIR_CONFIG entries. */
         args[0] = progname;
         args[1] = WORK_DIR_ID;
         args[2] = p_work_dir;
         args[3] = "-f";
         args[4] = font_name;
         args[5] = "-d";
         if (fake_user[0] != '\0')
         {
            args[7] = "-u";
            args[8] = fake_user;
            offset = 9;
         }
         else
         {
            offset = 7;
         }
         if (profile[0] != '\0')
         {
            args[offset] = "-p";
            args[offset + 1] = profile;
            offset += 2;
         }
         args[offset] = NULL;
         (void)strcpy(progname, VIEW_DC);
         break;

      case EXIT_SEL  : /* Exit */
         XFreeFont(display, font_struct);
         font_struct = NULL;
         XFreeGC(display, letter_gc);
         XFreeGC(display, normal_letter_gc);
         XFreeGC(display, locked_letter_gc);
         XFreeGC(display, color_letter_gc);
         XFreeGC(display, default_bg_gc);
         XFreeGC(display, normal_bg_gc);
         XFreeGC(display, locked_bg_gc);
         XFreeGC(display, label_bg_gc);
         XFreeGC(display, tr_bar_gc);
         XFreeGC(display, fr_bar_gc);
         XFreeGC(display, color_gc);
         XFreeGC(display, black_line_gc);
         XFreeGC(display, white_line_gc);

         /* Free all the memory from the permission stuff. */
         if (dcp.info_list != NULL)
         {
            FREE_RT_ARRAY(dcp.info_list);
         }
         if (dcp.stop_list != NULL)
         {
            FREE_RT_ARRAY(dcp.stop_list);
         }
         if (dcp.disable_list != NULL)
         {
            FREE_RT_ARRAY(dcp.disable_list);
         }
         if (dcp.rescan_list != NULL)
         {
            FREE_RT_ARRAY(dcp.rescan_list);
         }
         if (dcp.show_slog_list != NULL)
         {
            FREE_RT_ARRAY(dcp.show_slog_list);
         }
         if (dcp.show_rlog_list != NULL)
         {
            FREE_RT_ARRAY(dcp.show_rlog_list);
         }
         if (dcp.show_elog_list != NULL)
         {
            FREE_RT_ARRAY(dcp.show_elog_list);
         }
         if (dcp.show_tlog_list != NULL)
         {
            FREE_RT_ARRAY(dcp.show_tlog_list);
         }
         if (dcp.show_ilog_list != NULL)
         {
            FREE_RT_ARRAY(dcp.show_ilog_list);
         }
         if (dcp.show_olog_list != NULL)
         {
            FREE_RT_ARRAY(dcp.show_olog_list);
         }
         if (dcp.show_dlog_list != NULL)
         {
            FREE_RT_ARRAY(dcp.show_dlog_list);
         }
         if (dcp.afd_load_list != NULL)
         {
            FREE_RT_ARRAY(dcp.afd_load_list);
         }
         if (dcp.view_dc_list != NULL)
         {
            FREE_RT_ARRAY(dcp.view_dc_list);
         }
         free(connect_data);
         free(args);
         FREE_RT_ARRAY(dirs);
         FREE_RT_ARRAY(hosts);
         exit(SUCCESS);

      default  :
#if SIZEOF_LONG == 4
         (void)xrec(WARN_DIALOG, "Impossible item selection (%d).", sel_typ);
#else
         (void)xrec(WARN_DIALOG, "Impossible item selection (%ld).", sel_typ);
#endif
         free(args);
         FREE_RT_ARRAY(dirs)
         FREE_RT_ARRAY(hosts);
         return;
   }
#ifdef _DEBUG
   (void)fprintf(stderr, "Selected %d directories (", no_selected);
   for (i = j = 0; i < no_of_dirs; i++)
   {
      if (connect_data[i].inverse > OFF)
      {
         if (j++ < (no_selected - 1))
         {
            (void)fprintf(stderr, "%d, ", i);
         }
         else
         {
            j = i;
         }
      }
   }
   if (no_selected > 0)
   {
      (void)fprintf(stderr, "%d)\n", j);
   }
   else
   {
      (void)fprintf(stderr, "none)\n");
   }
#endif

   if (sel_typ == T_LOG_SEL)
   {
      if ((k = fsa_attach(DIR_CTRL)) < 0)
      {
         if (k == INCORRECT_VERSION)
         {
            (void)xrec(FATAL_DIALOG, "This program is not able to attach to the FSA due to incorrect version! (%s %d)",
                       __FILE__, __LINE__);
         }
         else
         {
            if (k < 0)
            {
               (void)xrec(FATAL_DIALOG, "Failed to attach to FSA! (%s %d)",
                          __FILE__, __LINE__);
            }
            else
            {
               (void)xrec(FATAL_DIALOG, "Failed to attach to FSA : %s (%s %d)",
                          strerror(k), __FILE__, __LINE__);
            }
         }
         free(args);
         FREE_RT_ARRAY(dirs)
         FREE_RT_ARRAY(hosts)
         return;
      }
   }
   else if (((sel_typ == I_LOG_SEL) || (sel_typ == P_LOG_SEL) ||
             (sel_typ == O_LOG_SEL) || (sel_typ == D_LOG_SEL) ||
             (sel_typ == E_LOG_SEL) || (sel_typ == SHOW_QUEUE_SEL) ||
             (sel_typ == DIR_HANDLE_EVENT_SEL)) &&
            ((no_selected > 0) || (no_selected_static > 0)))
        {
           args[offset] = "-d";
           offset++;
        }

   current_time = time(NULL);

   /* Set each directory. */
   k = display_error = 0;
   for (i = 0; i < no_of_dirs; i++)
   {
      if (connect_data[i].inverse > OFF)
      {
         switch (sel_typ)
         {
            case DIR_STOP_SEL : /* Start/Stop Directory. */
               if (fra[i].dir_flag & DIR_STOPPED)
               {
                  fra[i].dir_flag ^= DIR_STOPPED;
                  SET_DIR_STATUS(fra[i].dir_flag,
                                 current_time,
                                 fra[i].start_event_handle,
                                 fra[i].end_event_handle,
                                 fra[i].dir_status);
                  config_log(EC_DIR, ET_MAN, EA_START_DIRECTORY,
                             fra[i].dir_alias, NULL);
               }
               else
               {
                  if (xrec(QUESTION_DIALOG,
                           "Are you sure that you want to stop %s",
                           fra[i].dir_alias) == YES)
                  {
                     fra[i].dir_flag ^= DIR_STOPPED;
                     SET_DIR_STATUS(fra[i].dir_flag,
                                    current_time,
                                    fra[i].start_event_handle,
                                    fra[i].end_event_handle,
                                    fra[i].dir_status);
                     config_log(EC_DIR, ET_MAN, EA_STOP_DIRECTORY,
                                fra[i].dir_alias, NULL);

                     if (fra[i].host_alias[0] != '\0')
                     {
                        int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                        int  readfd;
#endif
                        char fd_delete_fifo[MAX_PATH_LENGTH];


                        (void)sprintf(fd_delete_fifo, "%s%s%s",
                                      p_work_dir, FIFO_DIR, FD_DELETE_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                        if (open_fifo_rw(fd_delete_fifo, &readfd, &fd) == -1)
#else
                        if ((fd = open(fd_delete_fifo, O_RDWR)) == -1)
#endif
                        {
                           (void)xrec(ERROR_DIALOG,
                                      "Failed to open() %s : %s (%s %d)",
                                      FD_DELETE_FIFO, strerror(errno),
                                      __FILE__, __LINE__);
                        }
                        else
                        {
                           char   wbuf[MAX_DIR_ALIAS_LENGTH + 2];
                           size_t length;

                           wbuf[0] = DELETE_RETRIEVES_FROM_DIR;
                           (void)strcpy(&wbuf[1], fra[i].dir_alias);
                           length = 1 + strlen(fra[i].dir_alias) + 1;
                           if (write(fd, wbuf, length) != length)
                           {
                              (void)xrec(ERROR_DIALOG,
                                         "Failed to write() to %s : %s (%s %d)",
                                         FD_DELETE_FIFO, strerror(errno),
                                         __FILE__, __LINE__);
                           }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           if (close(readfd) == -1)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         "Failed to close() FIFO %s (read) : %s",
                                         FD_DELETE_FIFO, strerror(errno));
                           }
#endif
                           if (close(fd) == -1)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         "Failed to close() FIFO %s (write) : %s",
                                         FD_DELETE_FIFO, strerror(errno));
                           }
                        }
                     }
                  }
               }
               break;

            case DIR_DISABLE_SEL : /* Enable/Disable Directory. */
               if (fra[i].dir_flag & DIR_DISABLED)
               {
                  fra[i].dir_flag ^= DIR_DISABLED;
                  SET_DIR_STATUS(fra[i].dir_flag,
                                 current_time,
                                 fra[i].start_event_handle,
                                 fra[i].end_event_handle,
                                 fra[i].dir_status);
                  config_log(EC_DIR, ET_MAN, EA_ENABLE_DIRECTORY,
                             fra[i].dir_alias, NULL);
               }
               else
               {
                  if (xrec(QUESTION_DIALOG,
                           "Are you sure that you want to disable %s\nThis directory will then not be monitored.",
                           fra[i].dir_alias) == YES)
                  {
                     fra[i].dir_flag ^= DIR_DISABLED;
                     SET_DIR_STATUS(fra[i].dir_flag,
                                    current_time,
                                    fra[i].start_event_handle,
                                    fra[i].end_event_handle,
                                    fra[i].dir_status);
                     config_log(EC_DIR, ET_MAN, EA_DISABLE_DIRECTORY,
                                fra[i].dir_alias, NULL);

                     if (fra[i].host_alias[0] != '\0')
                     {
                        int  fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
                        int  readfd;
#endif
                        char fd_delete_fifo[MAX_PATH_LENGTH];


                        (void)sprintf(fd_delete_fifo, "%s%s%s",
                                      p_work_dir, FIFO_DIR, FD_DELETE_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
                        if (open_fifo_rw(fd_delete_fifo, &readfd, &fd) == -1)
#else
                        if ((fd = open(fd_delete_fifo, O_RDWR)) == -1)
#endif
                        {
                           (void)xrec(ERROR_DIALOG,
                                      "Failed to open() %s : %s (%s %d)",
                                      FD_DELETE_FIFO, strerror(errno),
                                      __FILE__, __LINE__);
                        }
                        else
                        {
                           char   wbuf[MAX_DIR_ALIAS_LENGTH + 2];
                           size_t length;

                           wbuf[0] = DELETE_RETRIEVES_FROM_DIR;
                           (void)strcpy(&wbuf[1], fra[i].dir_alias);
                           length = 1 + strlen(fra[i].dir_alias) + 1;
                           if (write(fd, wbuf, length) != length)
                           {
                              (void)xrec(ERROR_DIALOG,
                                         "Failed to write() to %s : %s (%s %d)",
                                         FD_DELETE_FIFO, strerror(errno),
                                         __FILE__, __LINE__);
                           }
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           if (close(readfd) == -1)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         "Failed to close() FIFO %s (read) : %s",
                                         FD_DELETE_FIFO, strerror(errno));
                           }
#endif
                           if (close(fd) == -1)
                           {
                              system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                         "Failed to close() FIFO %s (write) : %s",
                                         FD_DELETE_FIFO, strerror(errno));
                           }
                        }
                     }
                  }
               }
               break;

            case DIR_RESCAN_SEL : /* Rescan Directory. */
               if ((fra[i].no_of_time_entries > 0) &&
                   (fra[i].next_check_time > current_time))
               {
                  event_log(current_time, EC_DIR, ET_MAN, EA_RESCAN_DIRECTORY,
                            "%s", user);
                  fra[i].next_check_time = current_time;
                  if (fra[i].host_alias[0] != '\0')
                  {
                     send_msg = YES;
                  }
               }
               break;

            case DIR_INFO_SEL : /* Show information for this directory. */
               {
                  int    gotcha = NO,
                         ii;
                  Window window_id;

                  for (ii = 0; ii < no_of_active_process; ii++)
                  {
                     if ((apps_list[ii].position == i) &&
                         (CHECK_STRCMP(apps_list[ii].progname, DIR_INFO) == 0))
                     {
                        if ((window_id = get_window_id(apps_list[ii].pid,
                                                       DIR_CTRL)) != 0L)
                        {
                           gotcha = YES;
                        }
                        break;
                     }
                  }
                  if (gotcha == NO)
                  {
                     args[6] = fra[i].dir_alias;
                     make_xprocess(progname, progname, args, i);
                  }
                  else
                  {
                     XRaiseWindow(display, window_id);
                     XSetInputFocus(display, window_id, RevertToParent,
                                    CurrentTime);
                  }
               }
               break;

            case DIR_VIEW_DC_SEL : /* Show DIR_CONFIG data for this directory. */
               {
                  int    gotcha = NO,
                         ii;
                  Window window_id;

                  for (ii = 0; ii < no_of_active_process; ii++)
                  {
                     if ((apps_list[ii].position == i) &&
                         (CHECK_STRCMP(apps_list[ii].progname, VIEW_DC) == 0))
                     {
                        if ((window_id = get_window_id(apps_list[ii].pid,
                                                       DIR_CTRL)) != 0L)
                        {
                           gotcha = YES;
                        }
                        break;
                     }
                  }
                  if (gotcha == NO)
                  {
                     args[6] = fra[i].dir_alias;
                     make_xprocess(progname, progname, args, i);
                  }
                  else
                  {
                     XRaiseWindow(display, window_id);
                     XSetInputFocus(display, window_id, RevertToParent,
                                    CurrentTime);
                  }
               }
               break;

            case DIR_HANDLE_EVENT_SEL : /* Handle event. */
               {
                  int    gotcha = NO,
                         ii;
                  Window window_id;

                  for (ii = 0; ii < no_of_active_process; ii++)
                  {
                     if ((apps_list[ii].position == -1) &&
                         (CHECK_STRCMP(apps_list[ii].progname, HANDLE_EVENT) == 0))
                     {
                        if ((window_id = get_window_id(apps_list[ii].pid,
                                                       DIR_CTRL)) != 0L)
                        {
                           gotcha = YES;
                        }
                        break;
                     }
                  }
                  if (gotcha == NO)
                  {
                     args[offset + k] = fra[i].dir_alias;
                     k++;
                  }
                  else
                  {
                     XRaiseWindow(display, window_id);
                     XSetInputFocus(display, window_id, RevertToParent,
                                    CurrentTime);
                     free(args);
                     FREE_RT_ARRAY(dirs)
                     FREE_RT_ARRAY(hosts)
                     return;
                  }
               }
               break;

            case E_LOG_SEL : /* View Event Log. */
               args[offset + k] = fra[i].dir_alias;
               k++;
               break;

            case O_LOG_SEL : /* View Output Log.     */
            case D_LOG_SEL : /* View Delete Log.     */
            case I_LOG_SEL : /* View Input Log.      */
            case P_LOG_SEL : /* View Production Log. */
            case SHOW_QUEUE_SEL : /* View Queue.     */
               (void)sprintf(dirs[k], "%x", fra[i].dir_id);
               args[offset + k] = dirs[k];
               k++;
               break;

            case R_LOG_SEL : /* View Retrieve Log. */
               (void)strcpy(dirs[k], fra[i].dir_alias);
               args[offset + k] = dirs[k];
               k++;
               break;

            case T_LOG_SEL : /* View Transfer Log. */
               if (fra[i].host_alias[0] != '\0')
               {
                  (void)strcpy(hosts[k], fra[i].host_alias);
                  if (fsa[fra[i].fsa_pos].host_toggle_str[0] != '\0')
                  {
                     (void)strcat(hosts[k], "?");
                  }
                  args[offset + k] = hosts[k];
                  k++;
               }
               break;

            default :
               (void)xrec(WARN_DIALOG,
                          "Impossible selection! NOOO this can't be true! (%s %d)",
                          __FILE__, __LINE__);
               free(args);
               FREE_RT_ARRAY(dirs)
               FREE_RT_ARRAY(hosts)
               return;
         }
      }
   } /* for (i = 0; i < no_of_dirs; i++) */

   if (send_msg == YES)
   {
      int  fd_cmd_fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int  fd_cmd_readfd;
#endif
      char fd_cmd_fifo[MAX_PATH_LENGTH];

      (void)sprintf(fd_cmd_fifo, "%s%s%s",
                    p_work_dir, FIFO_DIR, FD_CMD_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(fd_cmd_fifo, &fd_cmd_readfd, &fd_cmd_fd) == -1)
#else
      if ((fd_cmd_fd = open(fd_cmd_fifo, O_RDWR)) == -1)
#endif
      {
         (void)xrec(WARN_DIALOG, "Failed to open() %s : %s (%s %d)",
                    fd_cmd_fifo, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         if (send_cmd(FORCE_REMOTE_DIR_CHECK, fd_cmd_fd) != SUCCESS)
         {
            (void)xrec(WARN_DIALOG, "write() error : %s (%s %d)",
                       strerror(errno), __FILE__, __LINE__);
         }
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (close(fd_cmd_readfd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "close() error : %s", strerror(errno));
         }
#endif
         if (close(fd_cmd_fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "close() error : %s", strerror(errno));
         }
      }
   }

   if (sel_typ == T_LOG_SEL)
   {
      (void)fsa_detach(NO);
   }

   if ((sel_typ == R_LOG_SEL) || (sel_typ == T_LOG_SEL))
   {
      args[offset + k] = NULL;
      make_xprocess(progname, progname, args, -1);
   }
   else if ((sel_typ == I_LOG_SEL) || (sel_typ == P_LOG_SEL) ||
            (sel_typ == O_LOG_SEL) || (sel_typ == D_LOG_SEL) ||
            (sel_typ == E_LOG_SEL) || (sel_typ == SHOW_QUEUE_SEL) ||
            (sel_typ == DIR_HANDLE_EVENT_SEL))
        {
           args[k + offset] = NULL;
           make_xprocess(progname, progname, args, -1);
        }

#ifdef SHOW_CALLS
   i = 0;
   while (args[i] != NULL)
   {
      (void)printf("%s ", args[i]);
      i++;
   }
   (void)printf("\n");
#endif

   /* Memory for arg list stuff no longer needed. */
   free(args);
   FREE_RT_ARRAY(dirs)
   FREE_RT_ARRAY(hosts)

   if (display_error > 0)
   {
      if (display_error > 1)
      {
         (void)sprintf(err_msg, "Operation for hosts %s not done.",
                       host_err_no);
      }
      else
      {
         (void)sprintf(err_msg, "Operation for host %s not done.",
                       host_err_no);
      }
   }

   for (i = 0; i < no_of_dirs; i++)
   {
      if (connect_data[i].inverse == ON)
      {
         connect_data[i].inverse = OFF;
         draw_dir_line_status(i, -1);
      }
   }

   /* Make sure that all changes are shown. */
   XFlush(display);

   no_selected = 0;

   return;
}


/*######################## change_dir_font_cb() #########################*/
void
change_dir_font_cb(Widget    w,
                   XtPointer client_data,
                   XtPointer call_data)
{
   XT_PTR_TYPE item_no = (XT_PTR_TYPE)client_data;
   XGCValues   gc_values;

   if (current_font != item_no)
   {
      XtVaSetValues(fw[current_font], XmNset, False, NULL);
      current_font = item_no;
   }

   switch (item_no)
   {
      case 0   :
         (void)strcpy(font_name, FONT_0);
         break;

      case 1   :
         (void)strcpy(font_name, FONT_1);
         break;

      case 2   :
         (void)strcpy(font_name, FONT_2);
         break;

      case 3   :
         (void)strcpy(font_name, FONT_3);
         break;

      case 4   :
         (void)strcpy(font_name, FONT_4);
         break;

      case 5   :
         (void)strcpy(font_name, FONT_5);
         break;

      case 6   :
         (void)strcpy(font_name, FONT_6);
         break;

      case 7   :
         (void)strcpy(font_name, FONT_7);
         break;

      case 8   :
         (void)strcpy(font_name, FONT_8);
         break;

      case 9   :
         (void)strcpy(font_name, FONT_9);
         break;

      case 10  :
         (void)strcpy(font_name, FONT_10);
         break;

      case 11  :
         (void)strcpy(font_name, FONT_11);
         break;

      case 12  :
         (void)strcpy(font_name, FONT_12);
         break;

      default  :
#if SIZEOF_LONG == 4
         (void)xrec(WARN_DIALOG, "Impossible font selection (%d).", item_no);
#else
         (void)xrec(WARN_DIALOG, "Impossible font selection (%ld).", item_no);
#endif
         return;
    }

#ifdef _DEBUG
   (void)fprintf(stderr, "You have chosen: %s\n", font_name);
#endif

   /* Calculate the new values for global variables. */
   setup_dir_window(font_name);

   /* Load the font into the old GC. */
   gc_values.font = font_struct->fid;
   XChangeGC(display, letter_gc, GCFont, &gc_values);
   XChangeGC(display, normal_letter_gc, GCFont, &gc_values);
   XChangeGC(display, locked_letter_gc, GCFont, &gc_values);
   XChangeGC(display, color_letter_gc, GCFont, &gc_values);
   XChangeGC(display, red_color_letter_gc, GCFont, &gc_values);
   XFlush(display);

   /* Resize and redraw window if necessary. */
   if (resize_dir_window() == YES)
   {
      redraw_all();
      XFlush(display);
   }

   return;
}


/*######################## change_dir_rows_cb() #########################*/
void
change_dir_rows_cb(Widget    w,
                   XtPointer client_data,
                   XtPointer call_data)
{
   XT_PTR_TYPE item_no = (XT_PTR_TYPE)client_data;

   if (current_row != item_no)
   {
      XtVaSetValues(rw[current_row], XmNset, False, NULL);
      current_row = item_no;
   }

   switch (item_no)
   {
      case 0   :
         no_of_rows_set = atoi(ROW_0);
         break;

      case 1   :
         no_of_rows_set = atoi(ROW_1);
         break;

      case 2   :
         no_of_rows_set = atoi(ROW_2);
         break;

      case 3   :
         no_of_rows_set = atoi(ROW_3);
         break;

      case 4   :
         no_of_rows_set = atoi(ROW_4);
         break;

      case 5   :
         no_of_rows_set = atoi(ROW_5);
         break;

      case 6   :
         no_of_rows_set = atoi(ROW_6);
         break;

      case 7   :
         no_of_rows_set = atoi(ROW_7);
         break;

      case 8   :
         no_of_rows_set = atoi(ROW_8);
         break;

      case 9   :
         no_of_rows_set = atoi(ROW_9);
         break;

      case 10  :
         no_of_rows_set = atoi(ROW_10);
         break;

      case 11  :
         no_of_rows_set = atoi(ROW_11);
         break;

      case 12  :
         no_of_rows_set = atoi(ROW_12);
         break;

      case 13  :
         no_of_rows_set = atoi(ROW_13);
         break;

      case 14  :
         no_of_rows_set = atoi(ROW_14);
         break;

      case 15  :
         no_of_rows_set = atoi(ROW_15);
         break;

      case 16  :
         no_of_rows_set = atoi(ROW_16);
         break;

      case 17  :
         no_of_rows_set = atoi(ROW_17);
         break;

      case 18  :
         no_of_rows_set = atoi(ROW_18);
         break;

      case 19  :
         no_of_rows_set = atoi(ROW_19);
         break;

      case 20  :
         no_of_rows_set = atoi(ROW_20);
         break;

      default  :
#if SIZEOF_LONG == 4
         (void)xrec(WARN_DIALOG, "Impossible row selection (%d).", item_no);
#else
         (void)xrec(WARN_DIALOG, "Impossible row selection (%ld).", item_no);
#endif
         return;
   }

   if (no_of_rows_set == 0)
   {
      no_of_rows_set = 2;
   }

#ifdef _DEBUG
   (void)fprintf(stderr, "%s: You have chosen: %d rows/column\n",
                 __FILE__, no_of_rows_set);
#endif

   if (resize_dir_window() == YES)
   {
      redraw_all();
      XFlush(display);
   }

  return;
}


/*######################## change_dir_style_cb() ########################*/
void
change_dir_style_cb(Widget    w,
                    XtPointer client_data,
                    XtPointer call_data)
{
   XT_PTR_TYPE item_no = (XT_PTR_TYPE)client_data;

   if (current_style != item_no)
   {
      XtVaSetValues(lsw[current_style], XmNset, False, NULL);
      current_style = item_no;
   }

   switch (item_no)
   {
      case 0   :
         line_style = BARS_ONLY;
         break;

      case 1   :
         line_style = CHARACTERS_ONLY;
         break;

      case 2   :
         line_style = CHARACTERS_AND_BARS;
         break;

      default  :
#if SIZEOF_LONG == 4
         (void)xrec(WARN_DIALOG, "Impossible row selection (%d).", item_no);
#else
         (void)xrec(WARN_DIALOG, "Impossible row selection (%ld).", item_no);
#endif
         return;
   }

#ifdef _DEBUG
   switch (line_style)
   {
      case BARS_ONLY             :
         (void)fprintf(stderr, "Changing line style to bars only.\n");
         break;

      case CHARACTERS_ONLY       :
         (void)fprintf(stderr, "Changing line style to characters only.\n");
         break;

      case CHARACTERS_AND_BARS   :
         (void)fprintf(stderr, "Changing line style to bars and characters.\n");
         break;

      default                    : /* IMPOSSIBLE !!! */
         break;
   }
#endif

   setup_dir_window(font_name);

   if (resize_dir_window() == YES)
   {
      redraw_all();
      XFlush(display);
   }

   return;
}


/*######################## change_dir_other_cb() ########################*/
void
change_dir_other_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
   XT_PTR_TYPE item_no = (XT_PTR_TYPE)client_data;

   switch (item_no)
   {
      case FORCE_SHIFT_SELECT_W :
         if (other_options & FORCE_SHIFT_SELECT)
         {
            other_options &= ~FORCE_SHIFT_SELECT;
            XtVaSetValues(oow[FORCE_SHIFT_SELECT_W], XmNset, False, NULL);
         }
         else
         {
            other_options |= FORCE_SHIFT_SELECT;
            XtVaSetValues(oow[FORCE_SHIFT_SELECT_W], XmNset, True, NULL);
         }
         break;

      case AUTO_SAVE_W :
         if (other_options & AUTO_SAVE)
         {
            other_options &= ~AUTO_SAVE;
            XtVaSetValues(oow[AUTO_SAVE_W], XmNset, False, NULL);
         }
         else
         {
            other_options |= AUTO_SAVE;
            XtVaSetValues(oow[AUTO_SAVE_W], XmNset, True, NULL);
         }
         break;

      default  :
#if SIZEOF_LONG == 4
         (void)xrec(WARN_DIALOG, "Impossible other selection (%d).", item_no);
#else
         (void)xrec(WARN_DIALOG, "Impossible other selection (%ld).", item_no);
#endif
         return;
   }

#ifdef _DEBUG
   switch (item_no)
   {
      case FORCE_SHIFT_SELECT_W :
         if (other_options & FORCE_SHIFT_SELECT)
         {
            (void)fprintf(stderr, "Adding force shift select.\n");
         }
         else
         {
            (void)fprintf(stderr, "Removing force shift select.\n");
         }
         break;

      case AUTO_SAVE_W :
         if (other_options & AUTO_SAVE)
         {
            (void)fprintf(stderr, "Adding auto save.\n");
         }
         else
         {
            (void)fprintf(stderr, "Removing auto save.\n");
         }
         break;

      default : /* IMPOSSIBLE !!! */
         break;
   }
#endif

   return;
}
