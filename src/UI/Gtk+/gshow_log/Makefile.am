## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2008 - 2018 Holger Kiehl
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
noinst_HEADERS       = gshow_log.h
AM_CPPFLAGS          = @AFD_GTK_CFLAGS@ -I../../.. -I../../common\
                       -I../common -I../../../init_afd -I../../../log\
                       -I../gafd_ctrl -I../../../afd_mon
bin_PROGRAMS         = gshow_log                         
gshow_log_SOURCES    = check_log.c\
                       init_text.c\
                       log_callbacks.c\
                       log_filter.c\
                       gshow_log.c
gshow_log_LDADD      = ../common/libgtkafd.a\
                       ../../../common/libafd.a @AFD_GTK_LIBS@

install-exec-local:
	cd $(DESTDIR)$(bindir) && rm -f show_log && ln -s gshow_log show_log

uninstall-local:
	cd $(DESTDIR)$(bindir) && rm -f show_log

clean-local:
	-rm -rf .deps
