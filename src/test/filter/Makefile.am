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

AM_CPPFLAGS          = -I../.. -I../../init_afd
EXTRA_PROGRAMS       = tfilter tfilter1 tfilter2 tfilter3
MAINTAINERCLEANFILES = Makefile.in
CLEANFILES           = gmon.out core core.* $(EXTRA_PROGRAMS)
tfilter_SOURCES      = tfilter.c
tfilter_LDADD        = ../../common/libafd.a
tfilter1_SOURCES     = tfilter.c\
                       filter1.c
tfilter2_SOURCES     = tfilter.c\
                       filter2.c
tfilter3_SOURCES     = tfilter.c\
                       filter3.c
EXTRA_DIST           = match.c match2.c sfilter.c pmatch.data

clean-local:
	-rm -rf .deps
