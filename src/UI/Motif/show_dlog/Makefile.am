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

MAINTAINERCLEANFILES = Makefile.in
CLEANFILES           = gmon.out core core.*
noinst_HEADERS       = show_dlog.h sdr_str.h
AM_CPPFLAGS          = @AFD_MOTIF_INCLUDES@ -I../../.. -I../../common\
                       -I../common -I../../../init_afd -I../../../log\
                       -I../../../fd -I../mafd_ctrl
bin_PROGRAMS         = show_dlog
show_dlog_SOURCES    = calculate_summary.c\
                       callbacks.c\
                       format_info.c\
                       get_data.c\
                       get_info.c\
                       print_data.c\
                       select_delete_reason.c\
                       show_dlog.c
show_dlog_LDADD      = ../common/libmotifafd.a ../../common/libuiafd.a\
                       ../../../common/libafd.a @AFD_MOTIF_LIBS@

clean-local:
	-rm -rf .deps
