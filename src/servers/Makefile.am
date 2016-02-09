## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2005 - 2013 Holger Kiehl
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

MAINTAINERCLEANFILES  = Makefile.in
CLEANFILES            = gmon.out core core.*
noinst_HEADERS        = commondefs.h wmoddefs.h
AM_CPPFLAGS           = -I.. -I../init_afd
bin_PROGRAMS          = wmod
wmod_SOURCES          = check_fsa_pos.c\
                        get_free_connection.c\
                        handle_wmo_request.c\
                        trans_log.c\
                        wmod.c
wmod_LDADD            = ../common/libafd.a
