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

if WITH_XPM
   AFD_SETUP_ICON     = setup_icon.c
else
   AFD_SETUP_ICON     = 
endif

MAINTAINERCLEANFILES  = Makefile.in
noinst_HEADERS        = motif_common_defs.h
AM_CPPFLAGS           = @AFD_MOTIF_INCLUDES@ -I../../.. -I../../../init_afd\
                        -I../../common -I../mafd_ctrl -I../show_olog
lib_LIBRARIES         = libmotifafd.a
libmotifafd_a_SOURCES = check_nummeric.c\
                        disable_drag_drop.c\
                        eval_time.c\
                        generic_print.c\
                        handle_info_file.c\
                        make_xprocess.c\
                        $(AFD_SETUP_ICON)\
                        remove_paste_newline.c\
                        show_info.c\
                        show_message.c\
                        update_time.c\
                        wait_visible.c\
                        xrec.c

clean-local:
	-rm -rf .deps
