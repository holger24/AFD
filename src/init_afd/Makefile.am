## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2001 - 2018 Holger Kiehl
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

MAINTAINERCLEANFILES    = Makefile.in
CLEANFILES              = gmon.out core core.*
noinst_HEADERS          = afddefs.h bit_array.h dr_str.h ea_str.h permission.h\
                          ports.h
AM_CPPFLAGS             = -I.. -I../amg -I../log -I../statistics
bin_PROGRAMS            = init_afd init_afd_worker afd
init_afd_SOURCES        = init_afd.c\
                          check_afd_heartbeat.c\
                          check_permissions.c\
                          init_fifos_afd.c
init_afd_LDADD          = ../common/libafd.a @EXTRA_SOCKET_LIBS@
init_afd_worker_SOURCES = init_afd_worker.c\
                          count_files.c\
                          ia_trans_log.c
init_afd_worker_LDADD   = ../common/libafd.a @EXTRA_SOCKET_LIBS@
afd_SOURCES             = afd.c\
                          check_afd_heartbeat.c\
                          initialize_db.c\
                          startup_afd.c\
                          shutdown_afd.c
afd_LDADD               = ../common/libafd.a

if WITH_SETUID_PROGS
install-exec-local:
	chmod 4755 $(DESTDIR)$(bindir)/init_afd
	chmod 4755 $(DESTDIR)$(bindir)/afd
else
install-exec-local:
endif

clean-local:
	-rm -rf .deps
