## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2008 - 2016 Holger Kiehl
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
noinst_HEADERS       = nafd_ctrl.h
AM_CPPFLAGS          = -I../../..  -I../../common -I../../../init_afd\
                       -I../../../log
bin_PROGRAMS         = nafd_ctrl
nafd_ctrl_SOURCES    = draw_line.c\
                       nafd_ctrl.c
nafd_ctrl_LDADD      = ../../../common/libafd.a @AFD_NCURSES_LIBS@

if WITH_SETUID_PROGS
install-exec-local:
	chmod 4755 $(DESTDIR)$(bindir)/nafd_ctrl
else
install-exec-local:
endif
