## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2001 - 2017 Holger Kiehl
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
                      
SUBDIRS              = filter
AM_CPPFLAGS          = -I.. -I../init_afd -I../protocols -I../fd -I../log\
                       -I../afd_mon
EXTRA_PROGRAMS       = als arg ahttp ascp bt change_name copy_file create_files\
                       encode64 html_listing testlock url_check
MAINTAINERCLEANFILES = Makefile.in
CLEANFILES           = gmon.out core core.* $(EXTRA_PROGRAMS)
als_SOURCES          = als.c
als_LDADD            = ../common/libafd.a
encode64_SOURCES     = encode64.c\
                       encode_base64.c
encode64_LDADD       = ../common/libafd.a
arg_SOURCES          = arg.c
arg_LDADD            = ../common/libafd.a
bt_SOURCES           = bt.c
change_name_SOURCES  = change_name.c
change_name_LDADD    = ../common/libafd.a
copy_file_SOURCES    = copy_file.c
create_files_SOURCES = create_files.c
testlock_SOURCES     = testlock.c
testlock_LDADD       = ../common/libafd.a
ahttp_SOURCES        = ahttp.c\
                       trans_log.c
ahttp_LDADD          = ../protocols/libprotocols.a ../common/libafd.a
ascp_SOURCES         = ascp.c\
                       trans_log.c
ascp_LDADD           = ../protocols/libprotocols.a ../common/libafd.a
url_check_SOURCES    = url_check.c
url_check_LDADD      = ../common/libafd.a
html_listing_SOURCES = html_listing.c
html_listing_LDADD   = ../common/libafd.a
