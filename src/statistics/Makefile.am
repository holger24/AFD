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
noinst_HEADERS          = statdefs.h
AM_CPPFLAGS             = -I.. -I../init_afd
bin_PROGRAMS            = afd_stat show_stat show_istat show_bench_stat
afd_stat_SOURCES        = afd_stat.c\
                          eval_input_as.c\
                          locate_dir.c\
                          locate_host.c\
                          read_afd_istat_db.c\
                          read_afd_stat_db.c\
                          save_old_input_year.c\
                          save_old_output_year.c
afd_stat_LDADD          = ../common/libafd.a
show_stat_SOURCES       = show_stat.c\
                          eval_input_ss.c\
			  get_real_hostname.c\
                          locate_host.c
show_stat_LDADD         = ../common/libafd.a
show_istat_SOURCES      = show_istat.c\
                          eval_input_ss.c\
			  get_dir_name.c\
                          locate_dir.c
show_istat_LDADD        = ../common/libafd.a
show_bench_stat_SOURCES = show_bench_stat.c
show_bench_stat_LDADD   = ../common/libafd.a

clean-local:
	-rm -rf .deps
