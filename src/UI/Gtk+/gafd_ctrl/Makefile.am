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
noinst_HEADERS       = gafd_ctrl.h
AM_CPPFLAGS          = @AFD_GTK_CFLAGS@ -I../../.. -I../../common\
                       -I../common -I../../../init_afd -I../../../log\
                       -I../gshow_log
bin_PROGRAMS         = gafd_ctrl
gafd_ctrl_SOURCES    = calc_but_coord.c\
                       check_host_status.c\
                       check_status.c\
                       check_tv_status.c\
                       create_tv_window.c\
                       draw_line.c\
                       draw_tv_line.c\
                       error_history.c\
                       event_reason.c\
                       expose_handler.c\
                       find_host_dialog.c\
                       gafd_ctrl.c\
		       get_line_pos.c\
                       init_jd_structure.c\
                       locate_xy_column.c\
                       locate_xy_short.c\
                       mouse_handler.c\
                       redraw_all.c\
                       resize_tv_window.c\
                       resize_window.c\
                       setup_tv_window.c\
                       setup_window.c\
                       tv_window_size.c\
                       window_size.c
gafd_ctrl_LDADD      = @AFD_MATH_LIB@ ../common/libgtkafd.a\
                       ../../common/libuiafd.a ../../../common/libafd.a\
                       @AFD_GTK_LIBS@

if WITH_SETUID_PROGS
install-exec-local:
	@if [ "`find $(DESTDIR)$(bindir) -user 0 -name gafd_ctrl -print`" = "" ] ; \
	then \
		chmod 4755 $(DESTDIR)$(bindir)/gafd_ctrl ; \
	fi
	cd $(DESTDIR)$(bindir) && rm -f afd_ctrl && ln -s gafd_ctrl afd_ctrl
else
install-exec-local:
	cd $(DESTDIR)$(bindir) && rm -f afd_ctrl && ln -s gafd_ctrl afd_ctrl
endif

uninstall-local:
	cd $(DESTDIR)$(bindir) && rm -f afd_ctrl
