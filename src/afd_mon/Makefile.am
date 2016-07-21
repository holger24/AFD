## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2001 - 2016 Holger Kiehl
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

MAINTAINERCLEANFILES   = Makefile.in
CLEANFILES             = gmon.out core core.*
noinst_HEADERS         = mondefs.h sumdefs.h
AM_CPPFLAGS            = -I.. -I../init_afd -I../servers/afdd -I../log\
                         -I../fd
bin_PROGRAMS           = afd_mon afd_mon_status mafd msa_view topview\
                         view_hosts mon log_mon mafdcmd
afd_mon_SOURCES        = afd_mon.c\
                         check_mon.c\
                         convert_msa.c\
                         create_msa.c\
                         eval_afd_mon_db.c\
                         init_fifos_mon.c\
                         mon_log.c\
                         start_all.c\
                         start_process.c\
                         stop_process.c
afd_mon_LDADD          = ../common/libafd.a
afd_mon_status_SOURCES = afd_mon_status.c
afd_mon_status_LDADD   = ../common/libafd.a
mafd_SOURCES           = check_mon.c\
                         mafd.c\
                         shutdown_mon.c
mafd_LDADD             = ../common/libafd.a
msa_view_SOURCES       = msa_view.c
msa_view_LDADD         = ../common/libafd.a
topview_SOURCES        = topview.c
topview_LDADD          = ../common/libafd.a
view_hosts_SOURCES     = view_hosts.c
view_hosts_LDADD       = ../common/libafd.a
mafdcmd_SOURCES        = mafdcmd.c
mafdcmd_LDADD          = ../common/libafd.a
mon_SOURCES            = evaluate_message.c\
                         mon.c\
                         mon_log.c\
                         tcpcmd.c
mon_LDADD              = ../common/libafd.a @EXTRA_SOCKET_LIBS@
log_mon_SOURCES        = log_mon.c\
                         mon_log.c\
                         send_log_cmd.c\
                         tcpcmd.c\
                         write_afd_log.c
log_mon_LDADD          = ../common/libafd.a @EXTRA_SOCKET_LIBS@

if WITH_SETUID_PROGS
install-exec-local:
	chmod 4755 $(DESTDIR)$(bindir)/mafd
else
install-exec-local:
endif
