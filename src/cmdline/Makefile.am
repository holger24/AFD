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
   WMO_PROG          = awmo
   EXTRA_WMO_PROG    =
else
   WMO_PROG          =
   EXTRA_WMO_PROG    = awmo
endif
if WITH_FTP_SUPPORT
   FTP_PROG          = aftp
   EXTRA_FTP_PROG    =
else
   FTP_PROG          =
   EXTRA_FTP_PROG    = aftp
endif
if WITH_SMTP_SUPPORT
   SMTP_PROG         = asmtp
   EXTRA_SMTP_PROG   =
else
   SMTP_PROG         =
   EXTRA_SMTP_PROG   = asmtp
endif

MAINTAINERCLEANFILES = Makefile.in
CLEANFILES           = gmon.out core core.*
noinst_HEADERS       = aftpdefs.h asmtpdefs.h awmodefs.h cmdline.h
AM_CPPFLAGS          = -I.. -I../init_afd -I../protocols -I../fd
bin_PROGRAMS         = $(WMO_PROG) $(FTP_PROG) $(SMTP_PROG)
EXTRA_PROGRAMS       = $(EXTRA_WMO_PROG) $(EXTRA_FTP_PROG) $(EXTRA_SMTP_PROG)
aftp_SOURCES         = aftp.c\
                       eval_files.c\
                       get_remote_file_names.c\
                       init_aftp.c\
                       trans_log.c
aftp_LDADD           = @EXTRA_SOCKET_LIBS@ ../protocols/libprotocols.a\
                       ../common/libafd.a @AFD_SSL_LIBS@
asmtp_SOURCES        = asmtp.c\
                       eval_files.c\
                       init_asmtp.c\
                       trans_log.c
asmtp_LDADD          = @EXTRA_SOCKET_LIBS@ ../protocols/libprotocols.a\
                       ../common/libafd.a @AFD_SSL_LIBS@
awmo_SOURCES         = awmo.c\
                       eval_files.c\
                       init_awmo.c\
                       trans_log.c
awmo_LDADD           = @EXTRA_SOCKET_LIBS@ ../protocols/libprotocols.a\
                       ../common/libafd.a

install-exec-local:
	cd $(DESTDIR)$(bindir) && rm -f raftp
	cd $(DESTDIR)$(bindir) && ln -s aftp raftp

uninstall-local:
	cd $(DESTDIR)$(bindir) && rm -f raftp
