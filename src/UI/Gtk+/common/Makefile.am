## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2007 - 2013 Holger Kiehl
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
noinst_HEADERS       = gtk_common_defs.h
AM_CPPFLAGS          = @AFD_GTK_CFLAGS@ -I../../.. -I../../../init_afd\
                       -I../../common -I../gafd_ctrl -I../show_olog
lib_LIBRARIES        = libgtkafd.a
libgtkafd_a_SOURCES  = check_info_file.c\
                       check_nummeric.c\
                       generic_print.c\
                       make_xprocess.c\
                       show_info.c\
                       show_message.c\
                       update_time.c\
                       wait_visible.c\
                       xrec.c
