## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2008 - 2014 Holger Kiehl
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
   AFD_CHECK_INPUT_LINE          = check_input_line.c
   EXTRA_CHECK_INPUT_LINE        =
else
   AFD_INPUT_LOG                 =
   EXTRA_INPUT_LOG               = check_input_line.c
endif
if WITH_DISTRIBUTION_LOG
   AFD_CHECK_DISTRIBUTION_LINE   = check_distribution_line.c
   EXTRA_CHECK_DISTRIBUTION_LINE =
else
   AFD_CHECK_DISTRIBUTION_LINE   =
   EXTRA_CHECK_DISTRIBUTION_LINE = check_distribution_line.c
endif
if WITH_OUTPUT_LOG
   AFD_CHECK_OUTPUT_LINE         = check_output_line.c get_recipient.c
   EXTRA_CHECK_OUTPUT_LINE       =
else
   AFD_CHECK_OUTPUT_LINE         =
   EXTRA_CHECK_OUTPUT_LINE       = check_output_line.c get_recipient.c
endif
if WITH_DELETE_LOG
   AFD_CHECK_DELETE_LINE         = check_delete_line.c
   EXTRA_CHECK_DELETE_LINE       =
else
   AFD_CHECK_DELETE_LINE         =
   EXTRA_CHECK_DELETE_LINE       = check_delete_line.c
endif
if WITH_PRODUCTION_LOG
   AFD_CHECK_PRODUCTION_LINE     = check_production_line.c
   EXTRA_CHECK_PRODUCTION_LINE   =
else
   AFD_CHECK_PRODUCTION_LINE     =
   EXTRA_CHECK_PRODUCTION_LINE   = check_production_line.c
endif
if WITH_ALDA_CACHE_POSITIONING
   AFD_SEEK_ALDA_CACHE_POSITION  = seek_cache_position.c
   EXTRA_SEEK_ALDA_CACHE_POSITION=
else
   AFD_SEEK_ALDA_CACHE_POSITION  =
   EXTRA_SEEK_ALDA_CACHE_POSITION= seek_cache_position.c
endif
if WITH_AFD_MON
   HANDLE_AXL                    = handle_adl.c handle_ahl.c handle_atd.c
   EXTRA_HANDLE_AXL              =
else
   HANDLE_AXL                    =
   EXTRA_HANDLE_AXL              = handle_adl.c handle_ahl.c handle_atd.c
endif
EXTRA_DIST                       = $(EXTRA_CHECK_INPUT_LINE)\
                                   $(EXTRA_CHECK_DISTRIBUTION_LINE)\
                                   $(EXTRA_CHECK_OUTPUT_LINE)\
                                   $(EXTRA_CHECK_DELETE_LINE)\
                                   $(EXTRA_HANDLE_AXL)\
                                   $(EXTRA_CHECK_PRODUCTION_LINE)\
                                   $(EXTRA_SEEK_ALDA_CACHE_POSITION)
noinst_HEADERS                   = aldadefs.h ot_str.h
if WITH_AFD_MON
   AM_CPPFLAGS                   = -I.. -I../.. -I../../init_afd -I../../afd_mon
else
   AM_CPPFLAGS                   = -I.. -I../.. -I../../init_afd
endif
bin_PROGRAMS                     = alda aldad
EXTRA_PROGRAMS                   = check_alda_cache
MAINTAINERCLEANFILES             = Makefile.in $(EXTRA_PROGRAMS)
CLEANFILES                       = gmon.out core core.*
alda_SOURCES                     = alda.c\
                                   $(AFD_CHECK_DELETE_LINE)\
                                   check_did.c\
                                   $(AFD_CHECK_DISTRIBUTION_LINE)\
                                   check_dna.c\
                                   check_host_alias.c\
                                   $(AFD_CHECK_INPUT_LINE)\
                                   $(AFD_CHECK_OUTPUT_LINE)\
                                   $(AFD_CHECK_PRODUCTION_LINE)\
                                   eval_input_alda.c\
                                   get_full_source.c\
                                   get_real_hostname.c\
                                   $(HANDLE_AXL)\
                                   handle_jid.c\
                                   print_alda_data.c\
                                   $(AFD_SEEK_ALDA_CACHE_POSITION)\
                                   show_file_content.c
alda_LDADD                       = ../../common/libafd.a
aldad_SOURCES                    = aldad.c
aldad_LDADD                      = ../../common/libafd.a
check_alda_cache_SOURCES         = check_alda_cache.c
check_alda_cache_LDADD           = ../../common/libafd.a
