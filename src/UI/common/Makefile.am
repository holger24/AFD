## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2007 - 2014 Holger Kiehl
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
noinst_HEADERS       = ui_common_defs.h afd_logo.h
AM_CPPFLAGS          = -I../.. -I../../init_afd
lib_LIBRARIES        = libuiafd.a
libuiafd_a_SOURCES   = check_host_permissions.c\
                       config_log.c\
                       eval_alda_data.c\
                       get_current_jid_list.c\
                       get_ip_no.c\
                       get_printer_cmd.c\
                       handle_setup_file.c\
                       init_color.c\
                       insert_passwd.c\
                       locate_xy.c\
                       lookup_color.c\
                       print_file_size.c\
                       sfilter.c\
                       store_host_names.c\
                       view_data.c\
                       view_data_no_filter.c\
                       window_id.c
