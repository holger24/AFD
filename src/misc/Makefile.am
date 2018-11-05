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

MAINTAINERCLEANFILES         = Makefile.in
noinst_HEADERS               = eumetsat_header_defs.h mmap_emu.h
AM_CPPFLAGS                  = -I.. -I../init_afd -I../amg
lib_LIBRARIES                = libmisc.a
libmisc_a_SOURCES            = afw2wmo.c\
                               assemble.c\
                               bin_file_chopper.c\
                               convert.c\
                               convert_grib2wmo.c\
                               create_eumetsat_header.c\
                               extract.c\
                               fax2gts.c\
                               gts2tiff.c\
                               iso8859_2ascii.c\
                               tiff2gts.c\
                               wmo2ascii.c\
                               wmoheader_from_grib.c
if WITHOUT_MMAP
bin_PROGRAMS                 = mapper
mapper_SOURCES               = mapper.c
mapper_LDADD                 = ../common/libafd.a
else
EXTRA_DIST                   = mapper.c
endif
EXTRA_PROGRAMS               = create_eumetsat_name todos
create_eumetsat_name_SOURCES = create_eumetsat_name.c
todos_SOURCES                = todos.c
CLEANFILES                   = gmon.out core core.* $(EXTRA_PROGRAMS)

clean-local:
	-rm -rf .deps
