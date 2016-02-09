## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2001 - 2013 Holger Kiehl
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

MAINTAINERCLEANFILES = Makefile.in
CLEANFILES           = gmon.out core core.*
noinst_HEADERS       = edit_hc.h no_source.h no_source_mask.h source.h\
                       source_mask.h
AM_CPPFLAGS          = @AFD_MOTIF_INCLUDES@ -I../../.. -I../../common\
                       -I../common -I../../../init_afd -I../../../amg\
                       -I../../../log -I../mafd_ctrl
bin_PROGRAMS         = edit_hc
edit_hc_SOURCES      = accept_drop.c\
                       edit_hc.c\
                       edit_hc_callbacks.c\
                       init_host_list.c\
                       remove_host.c\
                       start_drag.c
edit_hc_LDADD        = ../common/libmotifafd.a\
                       ../../../common/libafd.a @AFD_MOTIF_LIBS@

if WITH_SETUID_PROGS
install-exec-local:
	@if [ "`find $(DESTDIR)$(bindir) -user 0 -name edit_hc -print`" = "" ] ; \
	then \
		chmod 4755 $(DESTDIR)$(bindir)/edit_hc ; \
	fi
else
install-exec-local:
endif
