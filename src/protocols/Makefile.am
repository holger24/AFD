## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2001 - 2014 Holger Kiehl
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
##

if WITH_WMO_SUPPORT
   WMOCMD                   = wmocmd.c
   EXTRA_WMOCMD             =
else
   WMOCMD                   =
   EXTRA_WMOCMD             = wmocmd.c
endif
if WITH_FTP_SUPPORT
   FTPCMD                   = ftpcmd.c ftpparse.c
   EXTRA_FTPCMD             =
else
   FTPCMD                   =
   EXTRA_FTPCMD             = ftpcmd.c ftpparse.c
endif
if WITH_SCP_SUPPORT
   SCPCMD                   = scpcmd.c
   EXTRA_SCPCMD             =
else
   SCPCMD                   =
   EXTRA_SCPCMD             = scpcmd.c
endif
if WITH_SFTP_SUPPORT
   SFTPCMD                  = sftpcmd.c
   EXTRA_SFTPCMD            =
else
   SFTPCMD                  =
   EXTRA_SFTPCMD            = sftpcmd.c
endif
if WITH_SMTP_SUPPORT
   SMTPCMD                  = smtpcmd.c encode_base64.c
   EXTRA_SMTPCMD            =
else
   SMTPCMD                  =
   EXTRA_SMTPCMD            = smtpcmd.c encode_base64.c
endif
if WITH_HTTP_SUPPORT
   HTTPCMD                  = httpcmd.c
   EXTRA_HTTPCMD            =
else
   HTTPCMD                  =
   EXTRA_HTTPCMD            = httpcmd.c
endif

EXTRA_DIST                  = $(EXTRA_WMOCMD) $(EXTRA_FTPCMD) $(EXTRA_SCPCMD)\
                              $(EXTRA_SFTPCMD) $(EXTRA_SMTPCMD) $(EXTRA_HTTPCMD)
MAINTAINERCLEANFILES        = Makefile.in
noinst_HEADERS              = commondefs.h ftpdefs.h httpdefs.h pop3defs.h\
                              scpdefs.h sftpdefs.h smtpdefs.h ssh_commondefs.h\
                              wmodefs.h ftpparse.h
AM_CPPFLAGS                 = -I.. -I../init_afd -I../fd @AFD_SSL_INCLUDES@
lib_LIBRARIES               = libprotocols.a libtraceprotocols.a
libprotocols_a_SOURCES      = common.c\
                              connect_with_timeout.c\
                              $(FTPCMD)\
                              $(HTTPCMD)\
                              pop3cmd.c\
                              $(SCPCMD)\
                              $(SFTPCMD)\
                              $(SMTPCMD)\
                              ssh_common.c\
                              $(WMOCMD)
libtraceprotocols_a_SOURCES = $(libprotocols_a_SOURCES)
libtraceprotocols_a_CFLAGS  = -DWITH_TRACE
