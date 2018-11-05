## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2015 - 2018 Holger Kiehl
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
noinst_HEADERS          = demcddefs.h
AM_CPPFLAGS             = -I../.. -I../../init_afd -I../common
bin_PROGRAMS            = demcd
EXTRA_PROGRAMS          = demcd_queue_spy
CLEANFILES              = gmon.out core core.* $(EXTRA_PROGRAMS)
demcd_SOURCES           = check_demcd_queue_space.c\
		          check_line.c\
                          demcd.c\
		          log_confirmation.c
demcd_LDADD             = ../../common/libafd.a
demcd_queue_spy_SOURCES = demcd_queue_spy.c
demcd_queue_spy_LDADD   = ../../common/libafd.a

clean-local:
	-rm -rf .deps
