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
noinst_HEADERS       = afdddefs.h
AM_CPPFLAGS          = -I../.. -I../../log -I../../init_afd -I../common
bin_PROGRAMS         = afdd
afdd_SOURCES         = afdd.c\
                       check_changes.c\
                       check_logs.c\
                       display_file.c\
                       get_display_data.c\
                       handle_request.c\
                       show_dir_list.c\
                       show_host_list.c\
                       show_host_stat.c\
                       show_job_list.c\
                       show_summary_stat.c
afdd_LDADD           = ../../common/libafd.a ../common/libserverafd.a @EXTRA_SOCKET_LIBS@

clean-local:
	-rm -rf .deps
