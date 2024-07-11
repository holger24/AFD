/*
 *  callbacks.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   callbacks - all callback functions for module send_file
 **
 ** SYNOPSIS
 **   callbacks
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   24.01.2005 H.Kiehl Created
 **   11.02.2014 A.Maul  Properly set and unset active, passive mode.
 **
 */
DESCR__E_M3

#include <stdio.h>                  /* popen(), pclose()                 */
#include <string.h>
#include <stdlib.h>                 /* exit()                            */
#include <ctype.h>                  /* isdigit()                         */
#include <sys/types.h>
#include <signal.h>                 /* kill()                            */
#include <unistd.h>                 /* read(), close()                   */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/LabelG.h>
#include <errno.h>
#include "mafd_ctrl.h"
#include "xsend_file.h"
#include "ftpdefs.h"
#include "smtpdefs.h"
#include "ssh_commondefs.h"
#ifdef _WITH_WMO_SUPPORT
# include "wmodefs.h"
#endif

/* External global variables. */
extern char             cmd[];
extern pid_t            cmd_pid;
extern int              button_flag,
                        cmd_fd;
extern Display          *display;
extern Widget           active_passive_w,
                        address_w,
                        address_label_w,
                        ap_radio_box_w,
                        ca_button_w,
                        cmd_output,
                        create_attach_w,
                        dir_subject_label_w,
                        dir_subject_w,
                        hs_label_w,
                        hs_w,
                        lock_box_w,
                        mode_box_w,
                        option_menu_w,
                        password_label_w,
                        password_w,
                        port_label_w,
                        port_w,
                        prefix_w,
                        proxy_label_w,
                        proxy_w,
                        recipientbox_w,
                        server_w,
                        server_label_w,
                        statusbox_w,
                        timeout_label_w,
                        timeout_w,
                        user_name_label_w,
                        user_name_w;
extern XtInputId        cmd_input_id;
extern XmTextPosition   wpr_position;
extern XmFontList       fontlist;
extern struct send_data *db;


/*############################ close_button() ###########################*/
void
close_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   exit(SUCCESS);
}


/*####################### active_passive_radio() ########################*/
void
active_passive_radio(Widget w, XtPointer client_data, XtPointer call_data)
{
   if ((XT_PTR_TYPE)client_data == SET_ACTIVE)
   {
      db->mode_flag |= ACTIVE_MODE;
      db->mode_flag &= ~PASSIVE_MODE;
   }
   else
   {
      db->mode_flag &= ~ACTIVE_MODE;
      db->mode_flag |= PASSIVE_MODE;
   }

   return;
}


/*############################ lock_radio() #############################*/
void
lock_radio(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (db->lock != (XT_PTR_TYPE)client_data)
   {
      if ((XT_PTR_TYPE)client_data == SET_LOCK_PREFIX)
      {
         XtSetSensitive(prefix_w, True);
      }
      else if (db->lock == SET_LOCK_PREFIX)
           {
              XtSetSensitive(prefix_w, False);
           }
   }
   db->lock = (XT_PTR_TYPE)client_data;

   return;
}


/*####################### create_attach_toggle() ########################*/
void
create_attach_toggle(Widget w, XtPointer client_data, XtPointer call_data)
{
   if ((XT_PTR_TYPE)client_data == CREATE_DIR_TOGGLE)
   {
      if (db->create_target_dir == NO)
      {
         db->create_target_dir = YES;
      }
      else
      {
         db->create_target_dir = NO;
      }
   }
   else
   {
      if (db->attach_file_flag == NO)
      {
         db->attach_file_flag = YES;
      }
      else
      {
         db->attach_file_flag = NO;
      }
   }

   return;
}


/*######################### extended_toggle() ###########################*/
void
extended_toggle(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (db->mode_flag & EXTENDED_MODE)
   {
      db->mode_flag &= ~EXTENDED_MODE;
   }
   else
   {
      db->mode_flag |= EXTENDED_MODE;
   }

   return;
}


/*########################### debug_toggle() ############################*/
void
debug_toggle(Widget w, XtPointer client_data, XtPointer call_data)
{
   if (db->debug == NO)
   {
      db->debug = YES;
   }
   else
   {
      db->debug = NO;
   }

   return;
}


/*############################ mode_radio() #############################*/
void
mode_radio(Widget w, XtPointer client_data, XtPointer call_data)
{
   db->transfer_mode = (XT_PTR_TYPE)client_data;

   return;
}


/*############################ send_button() ############################*/
void
send_button(Widget w, XtPointer client_data, XtPointer call_data)
{
   XmString xstr;

   if (button_flag == SEND_BUTTON)
   {
      reset_message(statusbox_w);

      /*
       * First lets check if all required parameters are available.
       */
      switch (db->protocol)
      {
         case FTP :
         case SFTP :
         case SMTP :
#ifdef _WITH_SCP_SUPPORT
         case SCP :
#endif
            if (db->user[0] == '\0')
            {
               show_message(statusbox_w, "No user name given!");
               XmProcessTraversal(user_name_w, XmTRAVERSE_CURRENT);
               return;
            }
            if (db->hostname[0] == '\0')
            {
               show_message(statusbox_w, "No hostname given!");
               XmProcessTraversal(hs_w, XmTRAVERSE_CURRENT);
               return;
            }
            break;

         case LOC :
            break;

#ifdef _WITH_WMO_SUPPORT
         case WMO :
            if (db->hostname[0] == '\0')
            {
               show_message(statusbox_w, "No hostname given!");
               XmProcessTraversal(hs_w, XmTRAVERSE_CURRENT);
               return;
            }
            if (db->port == -1)
            {
               show_message(statusbox_w, "No port given!");
               XmProcessTraversal(port_w, XmTRAVERSE_CURRENT);
               return;
            }
            break;
#endif

         default :
            show_message(statusbox_w, "No protocol selected, or unknown.");
            break;
      }

      if (wpr_position != 0)
      {
         XmTextSetInsertionPosition(cmd_output, 0);
         XmTextSetString(cmd_output, NULL);
         XFlush(display);
         wpr_position = 0;
      }
      create_url_file();
      send_file();

      /* Set Stop button. */
      xstr = XmStringCreateLtoR("Stop", XmFONTLIST_DEFAULT_TAG);
      XtVaSetValues(w, XmNlabelString, xstr, NULL);
      XmStringFree(xstr);
      button_flag = STOP_BUTTON;
   }
   else
   {
      if (cmd_pid > 0)
      {
         if (kill(cmd_pid, SIGINT) == -1)
         {
#if SIZEOF_PID_T == 4
            (void)fprintf(stderr, "Failed to kill() process %d : %s (%s %d)\n",
#else
            (void)fprintf(stderr, "Failed to kill() process %lld : %s (%s %d)\n",
#endif
                          (pri_pid_t)cmd_pid, strerror(errno),
                          __FILE__, __LINE__);
         }
#ifdef _IF_IT_DOES_NOT_WORK
         if (wait(NULL) == -1)
         {
            (void)fprintf(stderr, "wait() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
#endif
      }
#ifdef _IF_IT_DOES_NOT_WORK
      if (cmd_input_id != 0L)
      {
         XtRemoveInput(cmd_input_id);
         cmd_input_id = 0L;
         if (close(cmd_fd) == -1)
         {
            (void)fprintf(stderr, "close() error : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
         }
      }
#endif

      /* Set button back to Send. */
      xstr = XmStringCreateLtoR("Send", XmFONTLIST_DEFAULT_TAG);
      XtVaSetValues(w, XmNlabelString, xstr, NULL);
      XmStringFree(xstr);
      button_flag = SEND_BUTTON;
   }

   return;
}


/*######################### protocol_toggled() ##########################*/
void
protocol_toggled(Widget w, XtPointer client_data, XtPointer call_data)
{
   XmString xstr;

   if (db->protocol == (XT_PTR_TYPE)client_data)
   {
      return;
   }
   switch ((XT_PTR_TYPE)client_data)
   {
      case FTP :
         if (address_label_w != NULL)
         {
            XtDestroyWidget(address_label_w);
            address_label_w = NULL;
         }
         if (address_w != NULL)
         {
            XtRemoveCallback(address_w, XmNmodifyVerifyCallback, send_save_input,
                             (XtPointer)USER_NO_ENTER);
            XtRemoveCallback(address_w, XmNactivateCallback, send_save_input,
                             (XtPointer)USER_ENTER);
            XtDestroyWidget(address_w);
            address_w = NULL;

            /* Change callback when we press Create Dir/Attach file button. */
            XtRemoveCallback(ca_button_w, XmNvalueChangedCallback,
                             (XtCallbackProc)create_attach_toggle,
                             (XtPointer)ATTACH_FILE_TOGGLE);
            XtAddCallback(ca_button_w, XmNvalueChangedCallback,
                          (XtCallbackProc)create_attach_toggle,
                          (XtPointer)CREATE_DIR_TOGGLE);

            /* Change some labels. */
            xstr = XmStringCreateLtoR("Directory :", XmFONTLIST_DEFAULT_TAG);
            XtVaSetValues(dir_subject_label_w, XmNlabelString, xstr, NULL);
            XmStringFree(xstr);
            xstr =  XmStringCreateLocalized("Create Dir  ");
            XtVaSetValues(ca_button_w, XmNlabelString, xstr, NULL);
            XmStringFree(xstr);

            if (my_strcmp(db->hostname, SMTP_HOST_NAME) == 0)
            {
               db->hostname[0] = '\0';
            }
         }

         if (user_name_label_w == NULL)
         {
            CREATE_USER_FIELD();
            XmTextSetString(user_name_w, db->user);
         }
         else
         {
            XtSetSensitive(user_name_label_w, True);
            XtSetSensitive(user_name_w, True);
         }

         if (password_label_w == NULL)
         {
            /* Password. */
            CREATE_PASSWORD_FIELD();
            if ((db->password != NULL) && (db->password[0] != '\0'))
            {
               size_t length;
               char   *password_str;

               length = strlen(db->password);
               if ((password_str = malloc(length + 1)) == NULL)
               {
                  (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                                strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
               else
               {
                  (void)memset(password_str, '*', length);
                  password_str[length] = '\0';
                  XmTextSetString(password_w, password_str);
                  free(password_str);
               }
            }
         }
         else
         {
            XtSetSensitive(password_label_w, True);
            XtSetSensitive(password_w, True);
         }

         XmTextSetString(hs_w, db->hostname);
         xstr = XmStringCreateLtoR("Hostname :", XmFONTLIST_DEFAULT_TAG);
         XtVaSetValues(hs_label_w, XmNlabelString, xstr, NULL);
         XmStringFree(xstr);
         XtSetSensitive(hs_label_w, True);
         XtSetSensitive(hs_w, True);

         if ((db->port == 0) || (db->port == DEFAULT_SMTP_PORT) ||
             (db->port == DEFAULT_SSH_PORT))
         {
            char str_line[MAX_PORT_DIGITS + 1];

            db->port = DEFAULT_FTP_PORT;
            (void)sprintf(str_line, "%d", db->port);
            XmTextSetString(port_w, str_line);
         }
         XtSetSensitive(port_label_w, True);
         XtSetSensitive(port_w, True);

         XmTextSetString(dir_subject_w, db->target_dir);
         XtSetSensitive(dir_subject_label_w, True);
         XtSetSensitive(dir_subject_w, True);

         XtSetSensitive(create_attach_w, True);

         XtSetSensitive(timeout_label_w, True);
         XtSetSensitive(timeout_w, True);

         XtSetSensitive(active_passive_w, True);
         XtSetSensitive(ap_radio_box_w, True);
         XtSetSensitive(mode_box_w, True);
         XtSetSensitive(lock_box_w, True);
         XtSetSensitive(prefix_w, True);

         XmTextSetString(proxy_w, db->proxy_name);
         XtSetSensitive(proxy_label_w, True);
         XtSetSensitive(proxy_w, True);

         break;

      case SFTP :
         if (address_label_w != NULL)
         {
            XtDestroyWidget(address_label_w);
            address_label_w = NULL;
         }
         if (address_w != NULL)
         {
            XtRemoveCallback(address_w, XmNmodifyVerifyCallback, send_save_input,
                             (XtPointer)USER_NO_ENTER);
            XtRemoveCallback(address_w, XmNactivateCallback, send_save_input,
                             (XtPointer)USER_ENTER);
            XtDestroyWidget(address_w);
            address_w = NULL;

            /* Change callback when we press Create Dir/Attach file button. */
            XtRemoveCallback(ca_button_w, XmNvalueChangedCallback,
                             (XtCallbackProc)create_attach_toggle,
                             (XtPointer)ATTACH_FILE_TOGGLE);
            XtAddCallback(ca_button_w, XmNvalueChangedCallback,
                          (XtCallbackProc)create_attach_toggle,
                          (XtPointer)CREATE_DIR_TOGGLE);

            /* Change some labels. */
            xstr = XmStringCreateLtoR("Directory :", XmFONTLIST_DEFAULT_TAG);
            XtVaSetValues(dir_subject_label_w, XmNlabelString, xstr, NULL);
            XmStringFree(xstr);
            xstr =  XmStringCreateLocalized("Create Dir  ");
            XtVaSetValues(ca_button_w, XmNlabelString, xstr, NULL);
            XmStringFree(xstr);

            if (my_strcmp(db->hostname, SMTP_HOST_NAME) == 0)
            {
               db->hostname[0] = '\0';
            }
         }

         if (user_name_label_w == NULL)
         {
            CREATE_USER_FIELD();
            XmTextSetString(user_name_w, db->user);
         }
         else
         {
            XtSetSensitive(user_name_label_w, True);
            XtSetSensitive(user_name_w, True);
         }

         if (password_label_w == NULL)
         {
            /* Password. */
            CREATE_PASSWORD_FIELD();
            if ((db->password != NULL) && (db->password[0] != '\0'))
            {
               size_t length;
               char   *password_str;

               length = strlen(db->password);
               if ((password_str = malloc(length + 1)) == NULL)
               {
                  (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                                strerror(errno), __FILE__, __LINE__);
                  exit(INCORRECT);
               }
               else
               {
                  (void)memset(password_str, '*', length);
                  password_str[length] = '\0';
                  XmTextSetString(password_w, password_str);
                  free(password_str);
               }
            }
         }
         else
         {
            XtSetSensitive(password_label_w, True);
            XtSetSensitive(password_w, True);
         }

         XmTextSetString(hs_w, db->hostname);
         xstr = XmStringCreateLtoR("Hostname :", XmFONTLIST_DEFAULT_TAG);
         XtVaSetValues(hs_label_w, XmNlabelString, xstr, NULL);
         XmStringFree(xstr);
         XtSetSensitive(hs_label_w, True);
         XtSetSensitive(hs_w, True);

         if ((db->port == 0) || (db->port == DEFAULT_SMTP_PORT) ||
             (db->port == DEFAULT_FTP_PORT))
         {
            char str_line[MAX_PORT_DIGITS + 1];

            db->port = DEFAULT_SSH_PORT;
            (void)sprintf(str_line, "%d", db->port);
            XmTextSetString(port_w, str_line);
         }
         XtSetSensitive(port_label_w, True);
         XtSetSensitive(port_w, True);

         XmTextSetString(dir_subject_w, db->target_dir);
         XtSetSensitive(dir_subject_label_w, True);
         XtSetSensitive(dir_subject_w, True);

         XtSetSensitive(create_attach_w, True);

         XtSetSensitive(timeout_label_w, True);
         XtSetSensitive(timeout_w, True);

         XtSetSensitive(active_passive_w, False);
         XtSetSensitive(ap_radio_box_w, False);
         XtSetSensitive(mode_box_w, True);
         XtSetSensitive(lock_box_w, True);
         XtSetSensitive(prefix_w, True);

         XtSetSensitive(proxy_label_w, False);
         XtSetSensitive(proxy_w, False);

         break;


      case SMTP :
         if (user_name_label_w != NULL)
         {
            XtDestroyWidget(user_name_label_w);
            user_name_label_w = NULL;
         }
         if (user_name_w != NULL)
         {
            XtRemoveCallback(user_name_w, XmNlosingFocusCallback, send_save_input,
                             (XtPointer)USER_NO_ENTER);
            XtRemoveCallback(user_name_w, XmNactivateCallback, send_save_input,
                             (XtPointer)USER_ENTER);
            XtDestroyWidget(user_name_w);
            user_name_w = NULL;

            xstr = XmStringCreateLtoR("Subject   :", XmFONTLIST_DEFAULT_TAG);
            XtVaSetValues(dir_subject_label_w, XmNlabelString, xstr, NULL);
            XmStringFree(xstr);
            xstr = XmStringCreateLocalized("Attach file ");
            XtVaSetValues(ca_button_w, XmNlabelString, xstr, NULL);
            XmStringFree(xstr);
         }
         if (password_label_w != NULL)
         {
            XtDestroyWidget(password_label_w);
            password_label_w = NULL;
         }
         if (password_w != NULL)
         {
            XtRemoveCallback(password_w, XmNmodifyVerifyCallback, send_save_input,
                             (XtPointer)PASSWORD_NO_ENTER);
            XtRemoveCallback(password_w, XmNactivateCallback, send_save_input,
                             (XtPointer)PASSWORD_ENTER);
            XtDestroyWidget(password_w);
            password_w = NULL;
         }

         if (address_label_w == NULL)
         {
            address_label_w = XtVaCreateManagedWidget("Address :",
                                xmLabelGadgetClass,  recipientbox_w,
                                XmNfontList,         fontlist,
                                XmNtopAttachment,    XmATTACH_FORM,
                                XmNbottomAttachment, XmATTACH_FORM,
                                XmNleftAttachment,   XmATTACH_WIDGET,
                                XmNleftWidget,       option_menu_w,
                                XmNalignment,        XmALIGNMENT_END,
                                NULL);
            address_w = XtVaCreateManagedWidget("",
                                xmTextWidgetClass,   recipientbox_w,
                                XmNfontList,         fontlist,
                                XmNmarginHeight,     1,
                                XmNmarginWidth,      1,
                                XmNshadowThickness,  1,
                                XmNrows,             1,
                                XmNcolumns,          25,
                                XmNmaxLength,        MAX_USER_NAME_LENGTH,
                                XmNtopAttachment,    XmATTACH_FORM,
                                XmNtopOffset,        6,
                                XmNleftAttachment,   XmATTACH_WIDGET,
                                XmNleftWidget,       address_label_w,
                                NULL);
            XtAddCallback(address_w, XmNlosingFocusCallback, send_save_input,
                          (XtPointer)USER_NO_ENTER);
            XtAddCallback(address_w, XmNactivateCallback, send_save_input,
                          (XtPointer)USER_ENTER);
            XmTextSetString(address_w, db->user);

            /* Change callback when we press Create Dir/Attach file button. */
            XtRemoveCallback(ca_button_w, XmNvalueChangedCallback,
                             (XtCallbackProc)create_attach_toggle,
                             (XtPointer)CREATE_DIR_TOGGLE);
            XtAddCallback(ca_button_w, XmNvalueChangedCallback,
                          (XtCallbackProc)create_attach_toggle,
                          (XtPointer)ATTACH_FILE_TOGGLE);
         }
         else
         {
            XtSetSensitive(address_label_w, True);
            XtSetSensitive(address_w, True);
         }


         if ((db->hostname[0] == '\0') || (db->smtp_server[0] == '\0'))
         {
            (void)strcpy(db->hostname, SMTP_HOST_NAME);
            (void)strcpy(db->smtp_server, SMTP_HOST_NAME);
         }
         XmTextSetString(hs_w, db->hostname);
         xstr = XmStringCreateLtoR("Mailserver", XmFONTLIST_DEFAULT_TAG);
         XtVaSetValues(hs_label_w, XmNlabelString, xstr, NULL);
         XmStringFree(xstr);
         XtSetSensitive(hs_label_w, True);
         XtSetSensitive(hs_w, True);

         if ((db->port == 0) || (db->port == DEFAULT_FTP_PORT) ||
             (db->port == DEFAULT_SSH_PORT))
         {
            char str_line[MAX_PORT_DIGITS + 1];

            db->port = DEFAULT_SMTP_PORT;
            (void)sprintf(str_line, "%d", db->port);
            XmTextSetString(port_w, str_line);
         }
         XtSetSensitive(port_label_w, True);
         XtSetSensitive(port_w, True);

         XmTextSetString(dir_subject_w, db->target_dir);
         XtSetSensitive(dir_subject_label_w, True);
         XtSetSensitive(dir_subject_w, True);

         XtSetSensitive(create_attach_w, True);

         XtSetSensitive(timeout_label_w, True);
         XtSetSensitive(timeout_w, True);

         XtSetSensitive(active_passive_w, False);
         XtSetSensitive(ap_radio_box_w, False);
         XtSetSensitive(mode_box_w, False);
         XtSetSensitive(lock_box_w, False);
         XtSetSensitive(prefix_w, False);

         XtSetSensitive(proxy_label_w, False);
         XtSetSensitive(proxy_w, False);
         break;

      case LOC :
         XtSetSensitive(user_name_label_w, False);
         XtSetSensitive(user_name_w, False);
         XtSetSensitive(password_label_w, False);
         XtSetSensitive(password_w, False);
         XtSetSensitive(hs_label_w, False);
         XtSetSensitive(hs_w, False);
         XtSetSensitive(proxy_label_w, False);
         XtSetSensitive(proxy_w, False);
         XtSetSensitive(dir_subject_label_w, True);
         XtSetSensitive(dir_subject_w, True);
         XtSetSensitive(create_attach_w, True);
         XtSetSensitive(port_label_w, False);
         XtSetSensitive(port_w, False);
         XtSetSensitive(timeout_label_w, False);
         XtSetSensitive(timeout_w, False);
         XtSetSensitive(active_passive_w, False);
         XtSetSensitive(ap_radio_box_w, False);
         XtSetSensitive(mode_box_w, False);
         XtSetSensitive(lock_box_w, True);
         XtSetSensitive(prefix_w, True);
         break;

#ifdef _WITH_SCP_SUPPORT
      case SCP :
         XtSetSensitive(user_name_label_w, True);
         XtSetSensitive(user_name_w, True);
         XtSetSensitive(password_label_w, True);
         XtSetSensitive(password_w, True);
         XtSetSensitive(hs_label_w, True);
         XtSetSensitive(hs_w, True);
         XtSetSensitive(proxy_label_w, False);
         XtSetSensitive(proxy_w, False);
         XtSetSensitive(dir_subject_label_w, True);
         XtSetSensitive(dir_subject_w, True);
         XtSetSensitive(create_attach_w, False);
         XtSetSensitive(port_label_w, True);
         XtSetSensitive(port_w, True);
         XtSetSensitive(timeout_label_w, True);
         XtSetSensitive(timeout_w, True);
         XtSetSensitive(active_passive_w, False);
         XtSetSensitive(ap_radio_box_w, False);
         XtSetSensitive(mode_box_w, False);
         XtSetSensitive(lock_box_w, True);
         XtSetSensitive(prefix_w, True);
         if (db->port == 0)
         {
            char str_line[MAX_PORT_DIGITS + 1];

            db->port = DEFAULT_SSH_PORT;
            (void)sprintf(str_line, "%d", db->port);
            XmTextSetString(port_w, str_line);
         }
         break;
#endif /* _WITH_SCP_SUPPORT */

#ifdef _WITH_WMO_SUPPORT
      case WMO :
         if (address_label_w != NULL)
         {
            XtSetSensitive(address_label_w, False);
         }
         if (address_w != NULL)
         {
            XtSetSensitive(address_w, False);

            if (my_strcmp(db->hostname, SMTP_HOST_NAME) == 0)
            {
               db->hostname[0] = '\0';
            }
         }
         if (user_name_label_w != NULL)
         {
            XtSetSensitive(user_name_label_w, False);
         }
         if (user_name_w != NULL)
         {
            XtSetSensitive(user_name_w, False);
         }
         if (password_label_w != NULL)
         {
            XtSetSensitive(password_label_w, False);
         }
         if (password_w != NULL)
         {
            XtSetSensitive(password_w, False);
         }

         XmTextSetString(hs_w, db->hostname);
         xstr = XmStringCreateLtoR("Hostname :", XmFONTLIST_DEFAULT_TAG);
         XtVaSetValues(hs_label_w, XmNlabelString, xstr, NULL);
         XmStringFree(xstr);
         XtSetSensitive(hs_label_w, True);
         XtSetSensitive(hs_w, True);

         if ((db->port == DEFAULT_FTP_PORT) ||
             (db->port == DEFAULT_SMTP_PORT) ||
             (db->port == DEFAULT_SSH_PORT))
         {
            char str_line[MAX_PORT_DIGITS + 1];

            db->port = 0;
            str_line[0] = '0';
            str_line[1] = '\0';
            XmTextSetString(port_w, str_line);
         }
         XtSetSensitive(port_label_w, True);
         XtSetSensitive(port_w, True);

         XtSetSensitive(dir_subject_label_w, False);
         XtSetSensitive(dir_subject_w, False);

         XtSetSensitive(create_attach_w, False);

         XtSetSensitive(timeout_label_w, True);
         XtSetSensitive(timeout_w, True);

         XtSetSensitive(active_passive_w, False);
         XtSetSensitive(ap_radio_box_w, False);
         XtSetSensitive(mode_box_w, False);
         XtSetSensitive(lock_box_w, False);
         XtSetSensitive(prefix_w, False);

         XtSetSensitive(proxy_label_w, False);
         XtSetSensitive(proxy_w, False);
         break;
#endif /* _WITH_WMO_SUPPORT */

      default : /* Should never get here! */
         (void)fprintf(stderr, "Junk programmer!\n");
         exit(INCORRECT);
   }
   db->protocol = (XT_PTR_TYPE)client_data;

   return;
}


/*########################## send_save_input() ##########################*/
void
send_save_input(Widget w, XtPointer client_data, XtPointer call_data)
{
   XT_PTR_TYPE type = (XT_PTR_TYPE)client_data;
   char        *value = XmTextGetString(w);

   switch (type)
   {
      case HOSTNAME_NO_ENTER :
         if (db->protocol == SMTP)
         {
            (void)strcpy(db->smtp_server, value);
         }
         else
         {
            (void)strcpy(db->hostname, value);
         }
         break;

      case HOSTNAME_ENTER :
         if (db->protocol == SMTP)
         {
            (void)strcpy(db->smtp_server, value);
         }
         else
         {
            (void)strcpy(db->hostname, value);
         }
         reset_message(statusbox_w);
         XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         break;

      case PROXY_NO_ENTER :
         (void)strcpy(db->proxy_name, value);
         break;

      case PROXY_ENTER :
         (void)strcpy(db->proxy_name, value);
         reset_message(statusbox_w);
         XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         break;

      case USER_NO_ENTER :
         (void)strcpy(db->user, value);
         break;

      case USER_ENTER :
         (void)strcpy(db->user, value);
         reset_message(statusbox_w);
         XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         break;

      case TARGET_DIR_NO_ENTER :
         if (db->protocol == SMTP)
         {
            (void)strcpy(db->subject, value);
         }
         else
         {
            (void)strcpy(db->target_dir, value);
         }
         break;

      case TARGET_DIR_ENTER :
         (void)strcpy(db->target_dir, value);
         reset_message(statusbox_w);
         XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         break;

      case PORT_NO_ENTER :
      case PORT_ENTER :
         if (value[0] != '\0')
         {
            char *ptr = value;

            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }
            while (*ptr != '\0')
            {
               if (!isdigit((int)(*ptr)))
               {
                  show_message(statusbox_w, "Invalid port number!");
                  XtFree(value);
                  return;
               }
               ptr++;
            }
            if (*ptr == '\0')
            {
               db->port = atoi(value);
            }
         }
         if (type == PORT_ENTER)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         reset_message(statusbox_w);
         break;

      case TIMEOUT_NO_ENTER :
      case TIMEOUT_ENTER :
         if (value[0] != '\0')
         {
            char *ptr = value;

            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }
            while (*ptr != '\0')
            {
               if (!isdigit((int)(*ptr)))
               {
                  show_message(statusbox_w, "Invalid timeout!");
                  XtFree(value);
                  return;
               }
               ptr++;
            }
            if (*ptr == '\0')
            {
               db->timeout = atoi(value);
            }
         }
         if (type == TIMEOUT_ENTER)
         {
            XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         }
         reset_message(statusbox_w);
         break;

      case PREFIX_NO_ENTER :
         (void)strcpy(db->prefix, value);
         break;

      case PREFIX_ENTER :
         (void)strcpy(db->prefix, value);
         reset_message(statusbox_w);
         XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
         break;

      default :
         (void)fprintf(stderr, "ERROR   : Impossible! (%s %d)\n",
                       __FILE__, __LINE__);
         exit(INCORRECT);
   }
   XtFree(value);

   return;
}


/*########################### enter_passwd() ############################*/
/*                            --------------                             */
/* Description: Displays the password as asterisk '*' to make it         */
/*              invisible.                                               */
/* Source     : Motif Programming Manual Volume 6A                       */
/*              by Dan Heller & Paula M. Ferguson                        */
/*              Page 502                                                 */
/*#######################################################################*/
void
enter_passwd(Widget w, XtPointer client_data, XtPointer call_data)
{
   int                        length;
   XT_PTR_TYPE                type = (XT_PTR_TYPE)client_data;
   char                       *new;
   XmTextVerifyCallbackStruct *cbs = (XmTextVerifyCallbackStruct *)call_data;

   if (cbs->reason == XmCR_ACTIVATE)
   {
      return;
   }

   /* Backspace. */
   if (cbs->startPos < cbs->currInsert)
   {
      cbs->endPos = strlen(db->password);
      db->password[cbs->startPos] = '\0';
      return;
   }

#ifdef DO_NOT_ALLOW_PASTING
   /* Pasting no allowed. */
   if (cbs->text->length > 1)
   {
      cbs->doit = False;
      return;
   }
#endif

   new = XtMalloc(cbs->text->length + 1);
   if (db->password)
   {
      (void)strcpy(new, db->password);
      XtFree(db->password);
   }
   else
   {
      new[0] = '\0';
   }
   db->password = new;
   (void)strncat(db->password, cbs->text->ptr, cbs->text->length);
   db->password[cbs->endPos + cbs->text->length] = '\0';
   for (length = 0; length < cbs->text->length; length++)
   {
      cbs->text->ptr[length] = '*';
   }

   if (type == PASSWORD_ENTER)
   {
      XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
   }

   return;
}
