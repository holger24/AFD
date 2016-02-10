## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2001 - 2015 Holger Kiehl
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
noinst_HEADERS       = mon_ctrl.h
AM_CPPFLAGS          = @AFD_MOTIF_INCLUDES@ -I../../.. -I../../common\
                       -I../common -I../mshow_log -I../../../afd_mon\
                       -I../../../fd -I../../../init_afd
bin_PROGRAMS         = mon_ctrl
mon_ctrl_SOURCES     = calc_mon_but_coord.c\
                       check_afd_status.c\
                       check_mon_status.c\
                       draw_mon_line.c\
                       mconfig_log.c\
                       mon_ctrl.c\
                       mon_error_history.c\
                       mon_expose_handler.c\
                       mouse_handler.c\
                       redraw_all.c\
                       resize_mon_window.c\
                       select_afd_dialog.c\
                       setup_mon_window.c\
                       mon_window_size.c
mon_ctrl_LDADD       = @AFD_MATH_LIB@ ../common/libmotifafd.a\
                       ../../common/libuiafd.a ../../../common/libafd.a @AFD_MOTIF_LIBS@

if WITH_SETUID_PROGS
install-exec-local:
	@if [ "`find $(DESTDIR)$(bindir) -user 0 -name mon_ctrl -print`" = "" ] ; \
	then \
		chmod 4755 $(DESTDIR)$(bindir)/mon_ctrl ; \
	fi
else
install-exec-local:
endif
