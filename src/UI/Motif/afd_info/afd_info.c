/*
 *  afd_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2017 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   afd_info - displays information on a single host
 **
 ** SYNOPSIS
 **   afd_info [--version] [-w <work dir>] [-f <font name>] -h host-name
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.11.1996 H.Kiehl Created
 **   06.10.1997 H.Kiehl Take real hostname when displaying the IP number.
 **   01.06.1998 H.Kiehl Show real host names and protocols.
 **   06.08.2004 H.Kiehl Write window ID to a common file.
 **   15.08.2004 H.Kiehl Added HTTP and SSL support.
 **   23.08.2010 H.Kiehl Make information editable.
 **   04.11.2012 H.Kiehl Make active passive sign work again.
 **
 */
DESCR__E_M1

#include <stdio.h>               /* fopen(), NULL                        */
#include <string.h>              /* strcpy(), strcat(), strcmp()         */
#include <time.h>                /* strftime(), localtime()              */
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>              /* getcwd(), gethostname()              */
#include <stdlib.h>              /* getenv(), atexit()                   */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/ToggleBG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/LabelG.h>
#include <Xm/Separator.h>
#include <Xm/ScrollBar.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#ifdef WITH_EDITRES
# include <X11/Xmu/Editres.h>
#endif
#include <errno.h>
#include "active_passive.h"
#include "permission.h"
#include "afd_info.h"
#include "version.h"

/* Global variables. */
Display                    *display;
XtAppContext               app;
XtIntervalId               interval_id_host;
Widget                     appshell,
                           protocol_label,
                           text_wl[NO_OF_FSA_ROWS],
                           text_wr[NO_OF_FSA_ROWS],
                           label_l_widget[NO_OF_FSA_ROWS],
                           label_r_widget[NO_OF_FSA_ROWS],
                           info_w,
                           pll_widget,  /* Pixmap label left.  */
                           plr_widget;  /* Pixmap label right. */
Pixmap                     active_pixmap = XmUNSPECIFIED_PIXMAP,
                           passive_pixmap = XmUNSPECIFIED_PIXMAP;
Colormap                   default_cmap;
int                        editable = NO,
                           event_log_fd = STDERR_FILENO,
                           fsa_id,
                           fsa_fd = -1,
                           host_position,
                           no_of_hosts,
                           sys_log_fd = STDOUT_FILENO;
unsigned long              color_pool[COLOR_POOL_SIZE];
#ifdef HAVE_MMAP
off_t                      fsa_size;
#endif
char                       host_name[MAX_HOSTNAME_LENGTH + 1],
                           font_name[40],
                           host_alias_1[40],
                           host_alias_2[40],
                           *info_data = NULL,
                           protocol_label_str[80],
                           *p_work_dir,
                           label_l[NO_OF_FSA_ROWS][40] =
                           {
                              "",
                              "Real host name 1:",
                              "Files transfered:",
                              "Last connection :",
                              "Total errors    :"
                           },
                           label_r[NO_OF_FSA_ROWS][40] =
                           {
                              "",
                              "Real host name 2     :",
                              "Bytes transfered     :",
                              "No. of connections   :",
                              "Retry interval (min) :"
                           },
                           user[MAX_FULL_USER_ID_LENGTH];
struct filetransfer_status *fsa;
struct prev_values         prev;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void                afd_info_exit(void),
                           eval_permissions(char *),
                           init_afd_info(int *, char **),
                           usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int             i,
                   length;
   char            window_title[100],
                   work_dir[MAX_PATH_LENGTH],
                   str_line[MAX_INFO_STRING_LENGTH],
                   tmp_str_line[MAX_INFO_STRING_LENGTH];
   static String   fallback_res[] =
                   {
                      "*mwmDecorations : 42",
                      "*mwmFunctions : 12",
                      ".afd_info.form*background : NavajoWhite2",
                      ".afd_info.form.fsa_box.?.?.?.text_wl.background : NavajoWhite1",
                      ".afd_info.form.fsa_box.?.?.?.text_wr.background : NavajoWhite1",
                      ".afd_info.form.host_infoSW.host_info.background : NavajoWhite1",
                      ".afd_info.form.buttonbox*background : PaleVioletRed2",
                      ".afd_info.form.buttonbox*foreground : Black",
                      ".afd_info.form.buttonbox*highlightColor : Black",
                      NULL
                   };
   Widget          form,
                   fsa_box,
                   fsa_box1,
                   fsa_box2,
                   fsa_text,
                   button,
                   buttonbox,
                   rowcol1,
                   rowcol2,
                   h_separator1,
                   h_separator2,
                   v_separator;
   Pixel           default_background;
   XmFontListEntry entry;
   XmFontList      fontlist;
   Arg             args[MAXARGS];
   Cardinal        argcount;
   uid_t           euid, /* Effective user ID. */
                   ruid; /* Real user ID. */

   CHECK_FOR_VERSION(argc, argv);

   /* Initialise global values. */
   p_work_dir = work_dir;
   init_afd_info(&argc, argv);

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

   (void)strcpy(window_title, host_name);
   (void)strcat(window_title, " Info");
   argcount = 0;
   XtSetArg(args[argcount], XmNtitle, window_title); argcount++;
   appshell = XtAppInitialize(&app, "AFD", NULL, 0,
                              &argc, argv, fallback_res, args, argcount);
   disable_drag_drop(appshell);
   if (euid != ruid)
   {
      if (seteuid(euid) == -1)
      {
         (void)fprintf(stderr, "Failed to seteuid() to %d : %s (%s %d)\n",
                       euid, strerror(errno), __FILE__, __LINE__);
      }
   }

   display = XtDisplay(appshell);

#ifdef HAVE_XPM
   /* Setup AFD logo as icon. */
   setup_icon(display, appshell);
#endif

   /* Create managing widget. */
   form = XmCreateForm(appshell, "form", NULL, 0);

   entry = XmFontListEntryLoad(display, font_name, XmFONT_IS_FONT, "TAG1");
   fontlist = XmFontListAppendEntry(NULL, entry);
   XmFontListEntryFree(&entry);

   /* Prepare pixmaps. */
   XtVaGetValues(form,
                 XmNbackground, &default_background,
                 XmNcolormap, &default_cmap,
                 NULL);
   init_color(display);
   ximage.width = ACTIVE_PASSIVE_WIDTH;
   ximage.height = ACTIVE_PASSIVE_HEIGHT;
   ximage.data = active_passive_bits;
   ximage.xoffset = 0;
   ximage.format = XYBitmap;
   ximage.byte_order = MSBFirst;
   ximage.bitmap_pad = 8;
   ximage.bitmap_bit_order = LSBFirst;
   ximage.bitmap_unit = 8;
   ximage.depth = 1;
   ximage.bytes_per_line = 2;
   ximage.obdata = NULL;
   if (XmInstallImage(&ximage, "active") == True)
   {
      active_pixmap = XmGetPixmap(XtScreen(appshell),
                                  "active",
                                  color_pool[NORMAL_STATUS], /* Foreground. */
                                  default_background);/* Background. */
   }
   if (XmInstallImage(&ximage, "passive") == True)
   {
      passive_pixmap = XmGetPixmap(XtScreen(appshell),
                                   "passive",
                                   color_pool[BUTTON_BACKGROUND], /* Foreground. */
                                   default_background);/* Background. */
   }

   /* Create host label for host name. */
   if ((fsa[host_position].host_toggle_str[0] != '\0') &&
       (active_pixmap != XmUNSPECIFIED_PIXMAP) &&
       (passive_pixmap != XmUNSPECIFIED_PIXMAP))
   {
      (void)sprintf(label_l[0], "%*c%-*s :",
                    3, ' ', (FSA_INFO_TEXT_WIDTH_L - 3), host_alias_1);
      (void)sprintf(label_r[0], "%*c%-*s :",
                    3, ' ', (FSA_INFO_TEXT_WIDTH_R - 1), host_alias_2);
   }
   else
   {
      (void)sprintf(label_l[0], "%-*s :",
                    FSA_INFO_TEXT_WIDTH_L, host_alias_1);
      (void)sprintf(label_r[0], "%-*s :",
                    FSA_INFO_TEXT_WIDTH_R + 2, host_alias_2);
   }

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM);
   argcount++;
   fsa_box = XmCreateForm(form, "fsa_box", args, argcount);
   XtManageChild(fsa_box);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM);
   argcount++;
   fsa_box1 = XmCreateForm(fsa_box, "fsa_box1", args, argcount);
   XtManageChild(fsa_box1);

   rowcol1 = XtVaCreateWidget("rowcol1", xmRowColumnWidgetClass,
                              fsa_box1, NULL);
   for (i = 0; i < NO_OF_FSA_ROWS; i++)
   {
      fsa_text = XtVaCreateWidget("fsa_text", xmFormWidgetClass,
                                  rowcol1,
                                  XmNfractionBase, 41,
                                  NULL);
      label_l_widget[i] = XtVaCreateManagedWidget(label_l[i],
                              xmLabelGadgetClass,  fsa_text,
                              XmNfontList,         fontlist,
                              XmNtopAttachment,    XmATTACH_POSITION,
                              XmNtopPosition,      1,
                              XmNbottomAttachment, XmATTACH_POSITION,
                              XmNbottomPosition,   40,
                              XmNleftAttachment,   XmATTACH_POSITION,
                              XmNleftPosition,     1,
                              XmNalignment,        XmALIGNMENT_END,
                              NULL);
      if ((i == 0) && (fsa[host_position].host_toggle_str[0] != '\0') &&
          (active_pixmap != XmUNSPECIFIED_PIXMAP) &&
          (passive_pixmap != XmUNSPECIFIED_PIXMAP))
      {
         if (fsa[host_position].host_toggle == HOST_ONE)
         {
            pll_widget = XtVaCreateManagedWidget("pixmap_label_l",
                                    xmLabelGadgetClass,  fsa_text,
                                    XmNtopAttachment,    XmATTACH_POSITION,
                                    XmNtopPosition,      1,
                                    XmNbottomAttachment, XmATTACH_POSITION,
                                    XmNbottomPosition,   40,
                                    XmNleftAttachment,   XmATTACH_POSITION,
                                    XmNleftPosition,     1,
                                    XmNlabelType,        XmPIXMAP,
                                    XmNlabelPixmap,      active_pixmap,
                                    NULL);
         }
         else
         {
            pll_widget = XtVaCreateManagedWidget("pixmap_label_l",
                                    xmLabelGadgetClass,  fsa_text,
                                    XmNtopAttachment,    XmATTACH_POSITION,
                                    XmNtopPosition,      1,
                                    XmNbottomAttachment, XmATTACH_POSITION,
                                    XmNbottomPosition,   40,
                                    XmNleftAttachment,   XmATTACH_POSITION,
                                    XmNleftPosition,     1,
                                    XmNlabelType,        XmPIXMAP,
                                    XmNlabelPixmap,      passive_pixmap,
                                    NULL);
         }
      }
      text_wl[i] = XtVaCreateManagedWidget("text_wl",
                                           xmTextWidgetClass,        fsa_text,
                                           XmNfontList,              fontlist,
                                           XmNcolumns,               AFD_INFO_STR_LENGTH,
                                           XmNtraversalOn,           False,
                                           XmNeditable,              False,
                                           XmNcursorPositionVisible, False,
                                           XmNmarginHeight,          1,
                                           XmNmarginWidth,           1,
                                           XmNshadowThickness,       1,
                                           XmNhighlightThickness,    0,
                                           XmNrightAttachment,       XmATTACH_FORM,
                                           XmNleftAttachment,        XmATTACH_POSITION,
                                           XmNleftPosition,          20,
                                           NULL);
      XtManageChild(fsa_text);
   }
   XtManageChild(rowcol1);

   /* Fill up the text widget with some values. */
   if ((fsa[host_position].real_hostname[0][0] != 1) &&
       ((fsa[host_position].protocol & FTP_FLAG) ||
        (fsa[host_position].protocol & SFTP_FLAG) ||
#ifdef _WITH_SCP_SUPPORT
        (fsa[host_position].protocol & SCP_FLAG) ||
#endif
#ifdef _WITH_WMO_SUPPORT
        (fsa[host_position].protocol & WMO_FLAG) ||
#endif
#ifdef _WITH_MAP_SUPPORT
        (fsa[host_position].protocol & MAP_FLAG) ||
#endif
        (fsa[host_position].protocol & HTTP_FLAG) ||
#ifdef _WITH_DE_MAIL_SUPPORT
        (fsa[host_position].protocol & DE_MAIL_FLAG) ||
#endif
        (fsa[host_position].protocol & SMTP_FLAG)))
   {
      get_ip_no(fsa[host_position].real_hostname[0], tmp_str_line);
   }
   else
   {
      *tmp_str_line = '\0';
   }
   (void)sprintf(str_line, "%*s", AFD_INFO_STR_LENGTH, tmp_str_line);
   XmTextSetString(text_wl[0], str_line);
   (void)sprintf(str_line, "%*s", AFD_INFO_STR_LENGTH, prev.real_hostname[0]);
   XmTextSetString(text_wl[1], str_line);
   (void)sprintf(str_line, "%*u", AFD_INFO_STR_LENGTH, prev.files_send);
   XmTextSetString(text_wl[2], str_line);
   (void)strftime(tmp_str_line, MAX_INFO_STRING_LENGTH, "%d.%m.%Y  %H:%M:%S",
                  localtime(&prev.last_connection));
   (void)sprintf(str_line, "%*s", AFD_INFO_STR_LENGTH, tmp_str_line);
   XmTextSetString(text_wl[3], str_line);
   (void)sprintf(str_line, "%*u", AFD_INFO_STR_LENGTH, prev.total_errors);
   XmTextSetString(text_wl[4], str_line);

   /* Create the first horizontal separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,         XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,             fsa_box);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator1 = XmCreateSeparator(form, "h_separator1", args, argcount);
   XtManageChild(h_separator1);

   /* Create the vertical separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,      XmVERTICAL);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,       fsa_box1);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,    XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   v_separator = XmCreateSeparator(fsa_box, "v_separator", args, argcount);
   XtManageChild(v_separator);

   argcount = 0;
   XtSetArg(args[argcount], XmNtopAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,  XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNleftWidget,      v_separator);
   argcount++;
   fsa_box2 = XmCreateForm(fsa_box, "fsa_box2", args, argcount);
   XtManageChild(fsa_box2);

   rowcol2 = XtVaCreateWidget("rowcol2", xmRowColumnWidgetClass,
                              fsa_box2, NULL);
   for (i = 0; i < NO_OF_FSA_ROWS; i++)
   {
      fsa_text = XtVaCreateWidget("fsa_text", xmFormWidgetClass,
                                  rowcol2,
                                  XmNfractionBase, 41,
                                  NULL);
      label_r_widget[i] = XtVaCreateManagedWidget(label_r[i], xmLabelGadgetClass, fsa_text,
                              XmNfontList,         fontlist,
                              XmNtopAttachment,    XmATTACH_POSITION,
                              XmNtopPosition,      1,
                              XmNbottomAttachment, XmATTACH_POSITION,
                              XmNbottomPosition,   40,
                              XmNleftAttachment,   XmATTACH_POSITION,
                              XmNleftPosition,     1,
                              XmNalignment,        XmALIGNMENT_END,
                              NULL);
      if ((i == 0) && (fsa[host_position].host_toggle_str[0] != '\0') &&
          (active_pixmap != XmUNSPECIFIED_PIXMAP) &&
          (passive_pixmap != XmUNSPECIFIED_PIXMAP) &&
          (fsa[host_position].toggle_pos != 0))
      {
         if (fsa[host_position].host_toggle == HOST_ONE)
         {
            plr_widget = XtVaCreateManagedWidget("pixmap_label_r", xmLabelGadgetClass, fsa_text,
                                    XmNtopAttachment,    XmATTACH_POSITION,
                                    XmNtopPosition,      1,
                                    XmNbottomAttachment, XmATTACH_POSITION,
                                    XmNbottomPosition,   40,
                                    XmNleftAttachment,   XmATTACH_POSITION,
                                    XmNleftPosition,     1,
                                    XmNlabelType,        XmPIXMAP,
                                    XmNlabelPixmap,      passive_pixmap,
                                    NULL);
         }
         else
         {
            plr_widget = XtVaCreateManagedWidget("pixmap_label_r", xmLabelGadgetClass, fsa_text,
                                    XmNtopAttachment,    XmATTACH_POSITION,
                                    XmNtopPosition,      1,
                                    XmNbottomAttachment, XmATTACH_POSITION,
                                    XmNbottomPosition,   40,
                                    XmNleftAttachment,   XmATTACH_POSITION,
                                    XmNleftPosition,     1,
                                    XmNlabelType,        XmPIXMAP,
                                    XmNlabelPixmap,      active_pixmap,
                                    NULL);
         }
      }
      text_wr[i] = XtVaCreateManagedWidget("text_wr", xmTextWidgetClass, fsa_text,
                                           XmNfontList,              fontlist,
                                           XmNcolumns,               AFD_INFO_STR_LENGTH,
                                           XmNtraversalOn,           False,
                                           XmNeditable,              False,
                                           XmNcursorPositionVisible, False,
                                           XmNmarginHeight,          1,
                                           XmNmarginWidth,           1,
                                           XmNshadowThickness,       1,
                                           XmNhighlightThickness,    0,
                                           XmNrightAttachment,       XmATTACH_FORM,
                                           XmNleftAttachment,        XmATTACH_POSITION,
                                           XmNleftPosition,          22,
                                           NULL);
      XtManageChild(fsa_text);
   }
   XtManageChild(rowcol2);

   /* Fill up the text widget with some values. */
   if (prev.toggle_pos != 0)
   {
      if ((fsa[host_position].real_hostname[0][0] != 1) &&
          ((fsa[host_position].protocol & FTP_FLAG) ||
           (fsa[host_position].protocol & SFTP_FLAG) ||
#ifdef _WITH_SCP_SUPPORT
           (fsa[host_position].protocol & SCP_FLAG) ||
#endif
#ifdef _WITH_WMO_SUPPORT
           (fsa[host_position].protocol & WMO_FLAG) ||
#endif
#ifdef _WITH_MAP_SUPPORT
           (fsa[host_position].protocol & MAP_FLAG) ||
#endif
           (fsa[host_position].protocol & HTTP_FLAG) ||
#ifdef _WITH_DE_MAIL_SUPPORT
           (fsa[host_position].protocol & DE_MAIL_FLAG) ||
#endif
           (fsa[host_position].protocol & SMTP_FLAG)))
      {
         get_ip_no(fsa[host_position].real_hostname[1], tmp_str_line);
      }
      else
      {
         *tmp_str_line = '\0';
      }
      (void)sprintf(str_line, "%*s", AFD_INFO_STR_LENGTH, tmp_str_line);
      XmTextSetString(text_wr[0], str_line);
   }
   (void)sprintf(str_line, "%*s", AFD_INFO_STR_LENGTH, prev.real_hostname[1]);
   XmTextSetString(text_wr[1], str_line);
#if SIZEOF_OFF_T == 4
   (void)sprintf(str_line, "%*lu", AFD_INFO_STR_LENGTH, prev.bytes_send);
#else
   (void)sprintf(str_line, "%*llu", AFD_INFO_STR_LENGTH, prev.bytes_send);
#endif
   XmTextSetString(text_wr[2], str_line);
   (void)sprintf(str_line, "%*u", AFD_INFO_STR_LENGTH, prev.no_of_connections);
   XmTextSetString(text_wr[3], str_line);
   (void)sprintf(str_line, "%*d", AFD_INFO_STR_LENGTH, prev.retry_interval / 60);
   XmTextSetString(text_wr[4], str_line);

   length = sprintf(protocol_label_str, "Protocols : ");
   if (fsa[host_position].protocol & FTP_FLAG)
   {
      if (fsa[host_position].protocol_options & FTP_PASSIVE_MODE)
      {
         if (fsa[host_position].protocol_options & FTP_EXTENDED_MODE)
         {
            length += sprintf(&protocol_label_str[length], "FTP (ext passive) ");
         }
         else
         {
            length += sprintf(&protocol_label_str[length], "FTP (passive) ");
         }
      }
      else
      {
         if (fsa[host_position].protocol_options & FTP_EXTENDED_MODE)
         {
            length += sprintf(&protocol_label_str[length], "FTP (ext active) ");
         }
         else
         {
            length += sprintf(&protocol_label_str[length], "FTP (active) ");
         }
      }
   }
   if (fsa[host_position].protocol & SFTP_FLAG)
   {
      length += sprintf(&protocol_label_str[length], "SFTP ");
   }
   if (fsa[host_position].protocol & LOC_FLAG)
   {
      length += sprintf(&protocol_label_str[length], "LOC ");
   }
   if (fsa[host_position].protocol & EXEC_FLAG)
   {
      length += sprintf(&protocol_label_str[length], "EXEC ");
   }
   if (fsa[host_position].protocol & SMTP_FLAG)
   {
      length += sprintf(&protocol_label_str[length], "SMTP ");
   }
#ifdef _WITH_DE_MAIL_SUPPORT
   if (fsa[host_position].protocol & DE_MAIL_FLAG)
   {
      length += sprintf(&protocol_label_str[length], "DEMAIL ");
   }
#endif
   if (fsa[host_position].protocol & HTTP_FLAG)
   {
      length += sprintf(&protocol_label_str[length], "HTTP ");
   }
#ifdef _WITH_SCP_SUPPORT
   if (fsa[host_position].protocol & SCP_FLAG)
   {
      length += sprintf(&protocol_label_str[length], "SCP ");
   }
#endif
#ifdef _WITH_WMO_SUPPORT
   if (fsa[host_position].protocol & WMO_FLAG)
   {
      length += sprintf(&protocol_label_str[length], "WMO ");
   }
#endif
#ifdef _WITH_MAP_SUPPORT
   if (fsa[host_position].protocol & MAP_FLAG)
   {
      length += sprintf(&protocol_label_str[length], "MAP ");
   }
#endif
#ifdef _WITH_DFAX_SUPPORT
   if (fsa[host_position].protocol & DFAX_FLAG)
   {
      length += sprintf(&protocol_label_str[length], "DFAX ");
   }
#endif
#ifdef WITH_SSL
   if (fsa[host_position].protocol & SSL_FLAG)
   {
      length += sprintf(&protocol_label_str[length], "SSL ");
   }
#endif
   protocol_label = XtVaCreateManagedWidget(protocol_label_str,
                                            xmLabelGadgetClass, form,
                                            XmNfontList,        fontlist,
                                            XmNtopAttachment,   XmATTACH_WIDGET,
                                            XmNtopWidget,       h_separator1,
                                            XmNleftAttachment,  XmATTACH_FORM,
                                            XmNrightAttachment, XmATTACH_FORM,
                                            NULL);

   /* Create the second first horizontal separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,         XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,             protocol_label);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator1 = XmCreateSeparator(form, "h_separator1", args, argcount);
   XtManageChild(h_separator1);

   argcount = 0;
   XtSetArg(args[argcount], XmNleftAttachment,   XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,  XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNfractionBase,     21);
   argcount++;
   buttonbox = XmCreateForm(form, "buttonbox", args, argcount);

   /* Create the second horizontal separator. */
   argcount = 0;
   XtSetArg(args[argcount], XmNorientation,           XmHORIZONTAL);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,      XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,          buttonbox);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,       XmATTACH_FORM);
   argcount++;
   h_separator2 = XmCreateSeparator(form, "h_separator2", args, argcount);
   XtManageChild(h_separator2);

   if (editable == YES)
   {
      button = XtVaCreateManagedWidget("Save",
                                       xmPushButtonWidgetClass, buttonbox,
                                       XmNfontList,         fontlist,
                                       XmNtopAttachment,    XmATTACH_POSITION,
                                       XmNtopPosition,      2,
                                       XmNbottomAttachment, XmATTACH_POSITION,
                                       XmNbottomPosition,   19,
                                       XmNleftAttachment,   XmATTACH_POSITION,
                                       XmNleftPosition,     1,
                                       XmNrightAttachment,  XmATTACH_POSITION,
                                       XmNrightPosition,    9,
                                       NULL);
      XtAddCallback(button, XmNactivateCallback,
                    (XtCallbackProc)save_button, (XtPointer)0);
      button = XtVaCreateManagedWidget("Close",
                                       xmPushButtonWidgetClass, buttonbox,
                                       XmNfontList,         fontlist,
                                       XmNtopAttachment,    XmATTACH_POSITION,
                                       XmNtopPosition,      2,
                                       XmNbottomAttachment, XmATTACH_POSITION,
                                       XmNbottomPosition,   19,
                                       XmNleftAttachment,   XmATTACH_POSITION,
                                       XmNleftPosition,     10,
                                       XmNrightAttachment,  XmATTACH_POSITION,
                                       XmNrightPosition,    20,
                                       NULL);
   }
   else
   {
      button = XtVaCreateManagedWidget("Close",
                                       xmPushButtonWidgetClass, buttonbox,
                                       XmNfontList,         fontlist,
                                       XmNtopAttachment,    XmATTACH_POSITION,
                                       XmNtopPosition,      2,
                                       XmNbottomAttachment, XmATTACH_POSITION,
                                       XmNbottomPosition,   19,
                                       XmNleftAttachment,   XmATTACH_POSITION,
                                       XmNleftPosition,     1,
                                       XmNrightAttachment,  XmATTACH_POSITION,
                                       XmNrightPosition,    20,
                                       NULL);
   }
   XtAddCallback(button, XmNactivateCallback,
                 (XtCallbackProc)close_button, (XtPointer)0);
   XtManageChild(buttonbox);

   /* Create log_text as a ScrolledText window. */
   argcount = 0;
   XtSetArg(args[argcount], XmNfontList,               fontlist);
   argcount++;
   XtSetArg(args[argcount], XmNrows,                   10);
   argcount++;
   XtSetArg(args[argcount], XmNcolumns,                80);
   argcount++;
   if (editable == YES)
   {
      XtSetArg(args[argcount], XmNeditable,               True);
      argcount++;
      XtSetArg(args[argcount], XmNcursorPositionVisible,  True);
      argcount++;
      XtSetArg(args[argcount], XmNautoShowCursorPosition, True);
   }
   else
   {
      XtSetArg(args[argcount], XmNeditable,               False);
      argcount++;
      XtSetArg(args[argcount], XmNcursorPositionVisible,  False);
      argcount++;
      XtSetArg(args[argcount], XmNautoShowCursorPosition, False);
   }
   argcount++;
   XtSetArg(args[argcount], XmNeditMode,               XmMULTI_LINE_EDIT);
   argcount++;
   XtSetArg(args[argcount], XmNwordWrap,               False);
   argcount++;
   XtSetArg(args[argcount], XmNscrollHorizontal,       False);
   argcount++;
   XtSetArg(args[argcount], XmNtopAttachment,          XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNtopWidget,              h_separator1);
   argcount++;
   XtSetArg(args[argcount], XmNtopOffset,              3);
   argcount++;
   XtSetArg(args[argcount], XmNleftAttachment,         XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNleftOffset,             3);
   argcount++;
   XtSetArg(args[argcount], XmNrightAttachment,        XmATTACH_FORM);
   argcount++;
   XtSetArg(args[argcount], XmNrightOffset,            3);
   argcount++;
   XtSetArg(args[argcount], XmNbottomAttachment,       XmATTACH_WIDGET);
   argcount++;
   XtSetArg(args[argcount], XmNbottomWidget,           h_separator2);
   argcount++;
   XtSetArg(args[argcount], XmNbottomOffset,           3);
   argcount++;
   info_w = XmCreateScrolledText(form, "host_info", args, argcount);
   XtManageChild(info_w);
   XtManageChild(form);

   /* Free font list. */
   XmFontListFree(fontlist);

#ifdef WITH_EDITRES
   XtAddEventHandler(appshell, (EventMask)0, True,
                     _XEditResCheckMessages, NULL);
#endif

   if (prev.errors_offline == HOST_ERROR_OFFLINE_STATIC)
   {
      XtSetSensitive(label_l_widget[4], False);
      XtSetSensitive(text_wl[4], False);
   }

   /* Realize all widgets. */
   XtRealizeWidget(appshell);
   wait_visible(appshell);

   /* Read and display the information file. */
   (void)check_info_file(host_name, HOST_INFO_FILE, YES);
   XmTextSetString(info_w, NULL);  /* Clears old entry. */
   XmTextSetString(info_w, info_data);

   /* Call update_info() after UPDATE_INTERVAL ms. */
   interval_id_host = XtAppAddTimeOut(app, UPDATE_INTERVAL,
                                      (XtTimerCallbackProc)update_info,
                                      form);

   /* We want the keyboard focus on the Done button. */
   XmProcessTraversal(button, XmTRAVERSE_CURRENT);

   /* Write window ID, so afd_ctrl can set focus if it is called again. */
   write_window_id(XtWindow(appshell), getpid(), AFD_INFO);

   /* Start the main event-handling loop. */
   XtAppMainLoop(app);

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ init_afd_info() ++++++++++++++++++++++++++*/
static void
init_afd_info(int *argc, char *argv[])
{
   int  user_offset;
   char fake_user[MAX_FULL_USER_ID_LENGTH],
        *perm_buffer,
        profile[MAX_PROFILE_NAME_LENGTH + 1],
        *ptr;

   if ((get_arg(argc, argv, "-?", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "-help", NULL, 0) == SUCCESS) ||
       (get_arg(argc, argv, "--help", NULL, 0) == SUCCESS))
   {
      usage(argv[0]);
      exit(SUCCESS);
   }
   if (get_arg(argc, argv, "-f", font_name, 40) == INCORRECT)
   {
      (void)strcpy(font_name, DEFAULT_FONT);
   }
   if (get_arg(argc, argv, "-h", host_name,
               MAX_HOSTNAME_LENGTH + 1) == INCORRECT)
   {
      usage(argv[0]);
      exit(INCORRECT);
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
   if (get_afd_path(argc, argv, p_work_dir) < 0)
   {
      (void)fprintf(stderr,
                    "Failed to get working directory of AFD. (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Do not start if binary dataset matches the one stort on disk. */
   if (check_typesize_data(NULL, NULL) > 0)
   {
      (void)fprintf(stderr,
                    "The compiled binary does not match stored database.\n");
      (void)fprintf(stderr,
                    "Initialize database with the command : afd -i\n");
      exit(INCORRECT);
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

      case NONE :
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
         exit(INCORRECT);

      case SUCCESS : /* Lets evaluate the permissions and see what */
                     /* the user may do.                           */
         eval_permissions(perm_buffer);
         free(perm_buffer);
         break;

      case INCORRECT : /* Hmm. Something did go wrong. Since we want to */
                       /* be able to disable permission checking let    */
                       /* the user have all permissions.                */
         editable = NO;
         break;

      default :
         (void)fprintf(stderr, "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   get_user(user, fake_user, user_offset);

   /* Attach to the FSA. */
   if ((user_offset = fsa_attach_passive(NO, AFD_INFO)) != SUCCESS)
   {
      if (user_offset == INCORRECT_VERSION)
      {
         (void)fprintf(stderr,
                       "This program is not able to attach to the FSA due to incorrect version. (%s %d)\n",
                       __FILE__, __LINE__);
      }
      else
      {
         if (user_offset < 0)
         {
            (void)fprintf(stderr, "Failed to attach to FSA. (%s %d)\n",
                          __FILE__, __LINE__);
         }
         else
         {
            (void)fprintf(stderr, "Failed to attach to FSA : %s (%s %d)\n",
                          strerror(errno), __FILE__, __LINE__);
         }
      }
      exit(INCORRECT);
   }

   if ((host_position = get_host_position(fsa, host_name, no_of_hosts)) < 0)
   {
      (void)fprintf(stderr, "Host %s is not in FSA.\n", host_name);
      exit(INCORRECT);
   }

   if (fsa[host_position].toggle_pos == 0)
   {
      /* There is NO secondary host. */
      (void)strcpy(host_alias_1, host_name);
      (void)strcpy(host_alias_2, NO_SECODARY_HOST);
   }
   else
   {
      /* There IS a secondary host. */
      (void)strcpy(host_alias_1, host_name);
      ptr = host_alias_1 + strlen(host_alias_1);
      *ptr = fsa[host_position].host_toggle_str[1];
      *(ptr + 1) = '\0';
      (void)strcpy(host_alias_2, host_name);
      ptr = host_alias_2 + strlen(host_alias_2);
      *ptr = fsa[host_position].host_toggle_str[2];
      *(ptr + 1) = '\0';
   }

   /* Initialize values in FSA structure. */
   if (fsa[host_position].real_hostname[0][0] == 1)
   {
      prev.real_hostname[0][0] = '\0';
      prev.real_hostname[1][0] = '\0';
   }
   else
   {
      (void)strcpy(prev.real_hostname[0], fsa[host_position].real_hostname[0]);
      (void)strcpy(prev.real_hostname[1], fsa[host_position].real_hostname[1]);
   }
   prev.retry_interval = fsa[host_position].retry_interval;
   prev.files_send = fsa[host_position].file_counter_done;
   prev.bytes_send = fsa[host_position].bytes_send;
   prev.total_errors = fsa[host_position].total_errors;
   prev.no_of_connections = fsa[host_position].connections;
   prev.last_connection = fsa[host_position].last_connection;
   prev.host_toggle = fsa[host_position].host_toggle;
   prev.toggle_pos = fsa[host_position].toggle_pos;
   prev.protocol = fsa[host_position].protocol;
   prev.errors_offline = fsa[host_position].host_status & HOST_ERROR_OFFLINE_STATIC;

   if (atexit(afd_info_exit) != 0)
   {
      (void)xrec(WARN_DIALOG, "Failed to set exit handler for %s : %s",
                 AFD_INFO, strerror(errno));
   }
   check_window_ids(AFD_INFO);

   return;
}


/*------------------------------ usage() --------------------------------*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "Usage : %s [options] -h host-name\n", progname);
   (void)fprintf(stderr, "            --version\n");
   (void)fprintf(stderr, "            -f <font name>\n");
   (void)fprintf(stderr, "            -p <user profile>\n");
   (void)fprintf(stderr, "            -u[ <fake user>]\n");
   (void)fprintf(stderr, "            -w <work directory>\n");
   return;
}


/*-------------------------- eval_permissions() -------------------------*/
static void
eval_permissions(char *perm_buffer)
{
   /*
    * If we find 'all' right at the beginning, no further evaluation
    * is needed, since the user has all permissions.
    */
   if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
       (perm_buffer[2] == 'l') &&
       ((perm_buffer[3] == '\0') || (perm_buffer[3] == ',') ||
        (perm_buffer[3] == ' ') || (perm_buffer[3] == '\t')))
   {
      editable = YES;
   }
   else
   {
      /* May the user change the information? */
      if (posi(perm_buffer, EDIT_AFD_INFO_PERM) == NULL)
      {
         /* The user may NOT change the information. */
         editable = NO;
      }
      else
      {
         /* The user may change the information. */
         editable = YES;
      }
   }

   return;
}


/*--------------------------- afd_info_exit() ---------------------------*/
static void                                                                
afd_info_exit(void)
{
   remove_window_id(getpid(), AFD_INFO);
   return;
}
