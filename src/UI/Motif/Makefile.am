## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2008 - 2016 Holger Kiehl
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

if WITH_INPUT_LOG
   AFD_SHOW_ILOG = show_ilog
else
   AFD_SHOW_ILOG = 
endif
if WITH_PRODUCTION_LOG
   AFD_SHOW_PLOG = show_plog
else
   AFD_SHOW_PLOG = 
endif
if WITH_OUTPUT_LOG
   AFD_SHOW_OLOG = show_olog
else
   AFD_SHOW_OLOG = 
endif
if WITH_DELETE_LOG
   AFD_SHOW_DLOG = show_dlog
else
   AFD_SHOW_DLOG = 
endif

if WITH_AFD_MON
   AFD_MON_DIR = mon_ctrl mon_info
else
   AFD_MON_DIR = 
endif

if WITH_AFD_MON_ONLY
   OTHER_DIRS = common mon_ctrl mon_info mshow_log
else
   OTHER_DIRS = common mafd_ctrl dir_ctrl afd_info dir_info afd_load\
                edit_hc show_cmd mshow_log $(AFD_SHOW_OLOG) $(AFD_SHOW_PLOG)\
		$(AFD_SHOW_ILOG) $(AFD_SHOW_DLOG) show_elog show_queue view_dc\
		xsend_file xshow_stat handle_event $(AFD_MON_DIR)
endif

MAINTAINERCLEANFILES = Makefile.in
SUBDIRS              = $(OTHER_DIRS)
EXTRA_DIST           = AFD
