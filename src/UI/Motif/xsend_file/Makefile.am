## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2005 - 2018 Holger Kiehl
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
noinst_HEADERS       = xsend_file.h
AM_CPPFLAGS          = @AFD_MOTIF_INCLUDES@ -I../../.. -I../../common\
                       -I../common -I../../../init_afd -I../../../protocols\
                       -I../../../log -I../mafd_ctrl
bin_PROGRAMS         = xsend_file
xsend_file_SOURCES   = callbacks.c\
                       create_url_file.c\
                       rr_file_name_file.c\
                       send_file.c\
                       xsend_file.c
xsend_file_LDADD     = ../common/libmotifafd.a\
                       ../../../common/libafd.a @AFD_MOTIF_LIBS@

clean-local:
	-rm -rf .deps
