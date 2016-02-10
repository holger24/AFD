/*
 *  p_str.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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


#include "ui_common_defs.h"

#ifndef __p_str_h
#define __p_str_h

static const char *pstr[] = /* Protocol string. */
                  {
#ifdef _WITH_FTP_SUPPORT
                     "FTP",
# ifdef WITH_SSL
                     "FTPS",
# endif
#endif
#ifdef _WITH_HTTP_SUPPORT
                     "HTTP",
# ifdef WITH_SSL
                     "HTTPS",
# endif
#endif
#ifdef _WITH_SMTP_SUPPORT
                     "SMTP",
# ifdef WITH_SSL
                     "SMTPS",
# endif
# ifdef _WITH_DE_MAIL_SUPPORT
                     "DEMAIL",
# endif
#endif
#ifdef _WITH_LOC_SUPPORT
                     "FILE",
#endif
#ifdef _WITH_FD_EXEC_SUPPORT
                     "EXEC",
#endif
#ifdef _WITH_SFTP_SUPPORT
                     "SFTP",
#endif
#ifdef _WITH_SCP_SUPPORT
                     "SCP",
#endif
#ifdef _WITH_WMO_SUPPORT
                     "WMO",
#endif
#ifdef _WITH_MAP_SUPPORT
                     "MAP",
#endif
#ifdef _WITH_DFAX_SUPPORT
                     "DFAX",
#endif
                  };

static const int pflag[] = /* Protocol flag. */
                 {
#ifdef _WITH_FTP_SUPPORT
                    SHOW_FTP,
# ifdef WITH_SSL
                    SHOW_FTPS,
# endif
#endif
#ifdef _WITH_HTTP_SUPPORT
                    SHOW_HTTP,
# ifdef WITH_SSL
                    SHOW_HTTPS,
# endif
#endif
#ifdef _WITH_SMTP_SUPPORT
                    SHOW_SMTP,
# ifdef WITH_SSL
                    SHOW_SMTPS,
# endif
# ifdef _WITH_DE_MAIL_SUPPORT
                    SHOW_DEMAIL,
# endif
#endif
#ifdef _WITH_LOC_SUPPORT
                    SHOW_FILE,
#endif
#ifdef _WITH_FD_EXEC_SUPPORT
                    SHOW_EXEC,
#endif
#ifdef _WITH_SFTP_SUPPORT
                    SHOW_SFTP,
#endif
#ifdef _WITH_SCP_SUPPORT
                    SHOW_SCP,
#endif
#ifdef _WITH_WMO_SUPPORT
                    SHOW_WMO,
#endif
#ifdef _WITH_MAP_SUPPORT
                    SHOW_MAP,
#endif
#ifdef _WITH_DFAX_SUPPORT
                    SHOW_DFAX,
#endif
                  };

#endif /* __p_str_h */
