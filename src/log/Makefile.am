## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2001 - 2015 Holger Kiehl
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
   AFD_INPUT_LOG          = input_log
   EXTRA_INPUT_LOG        =
else
   AFD_INPUT_LOG          =
   EXTRA_INPUT_LOG        = input_log
endif
if WITH_DISTRIBUTION_LOG
   AFD_DISTRIBUTION_LOG   = distribution_log
   EXTRA_DISTRIBUTION_LOG =
else
   AFD_DISTRIBUTION_LOG   =
   EXTRA_DISTRIBUTION_LOG = distribution_log
endif
if WITH_OUTPUT_LOG
   AFD_OUTPUT_LOG         = output_log
   EXTRA_OUTPUT_LOG       =
else
   AFD_OUTPUT_LOG         =
   EXTRA_OUTPUT_LOG       = output_log
endif
if WITH_CONFIRMATION_LOG
   AFD_CONFIRMATION_LOG   = confirmation_log
   EXTRA_CONFIRMATION_LOG =
else
   AFD_CONFIRMATION_LOG   =
   EXTRA_CONFIRMATION_LOG = confirmation_log
endif
if WITH_DELETE_LOG
   AFD_DELETE_LOG         = delete_log
   EXTRA_DELETE_LOG       =
else
   AFD_DELETE_LOG         =
   EXTRA_DELETE_LOG       = delete_log
endif
if WITH_PRODUCTION_LOG
   AFD_PRODUCTION_LOG     = production_log
   EXTRA_PRODUCTION_LOG   =
else
   AFD_PRODUCTION_LOG     =
   EXTRA_PRODUCTION_LOG   = production_log
endif
if WITH_AFD_MON
   AFD_MON_PROGS          =  mon_sys_log monitor_log
   EXTRA_MON_PROGS        =
   AM_CPPFLAGS            = -I.. -I../init_afd -I../afd_mon
else
   AFD_MON_PROGS          = 
   EXTRA_MON_PROGS        = mon_sys_log monitor_log
   AM_CPPFLAGS            = -I.. -I../init_afd
endif
if WITH_MAINTAINER_LOG
   AFD_MAINTAINER_LOG     = maintainer_log
   EXTRA_MAINTAINER_LOG   =
else
   AFD_MAINTAINER_LOG     =
   EXTRA_MAINTAINER_LOG   = maintainer_log
endif

EXTRA_PROGRAMS           = $(EXTRA_INPUT_LOG) $(EXTRA_DISTRIBUTION_LOG)\
                           $(EXTRA_OUTPUT_LOG) $(EXTRA_CONFIRMATION_LOG)\
                           $(EXTRA_DELETE_LOG) $(EXTRA_PRODUCTION_LOG)\
                           $(EXTRA_MON_PROGS) $(EXTRA_MAINTAINER_LOG)
MAINTAINERCLEANFILES     = Makefile.in
SUBDIRS                  = alda
CLEANFILES               = gmon.out core core.* $(EXTRA_PROGRAMS)
noinst_HEADERS           = logdefs.h
if WITH_AFD_MON_ONLY
   bin_PROGRAMS          = mon_sys_log monitor_log
else
   bin_PROGRAMS          = system_log event_log receive_log transfer_log\
                           trans_db_log $(AFD_MON_PROGS) $(AFD_INPUT_LOG)\
                           $(AFD_DISTRIBUTION_LOG) $(AFD_OUTPUT_LOG)\
                           $(AFD_CONFIRMATION_LOG) $(AFD_DELETE_LOG)\
                           $(AFD_PRODUCTION_LOG) $(AFD_MAINTAINER_LOG)
endif
system_log_SOURCES       = fprint_dup_msg.c\
                           logger.c\
                           system_log.c
system_log_LDADD         = ../common/libafd.a
maintainer_log_SOURCES   = fprint_dup_msg.c\
                           logger.c\
                           maintainer_log.c
maintainer_log_LDADD     = ../common/libafd.a
event_log_SOURCES        = event_log.c\
                           fprint_dup_msg.c\
                           event_logger.c
event_log_LDADD          = ../common/libafd.a
receive_log_SOURCES      = fprint_dup_msg.c\
                           open_log_file.c\
                           receive_log.c
receive_log_LDADD        = ../common/libafd.a
transfer_log_SOURCES     = fprint_dup_msg.c\
                           open_log_file.c\
                           transfer_log.c
transfer_log_LDADD       = ../common/libafd.a
trans_db_log_SOURCES     = fprint_dup_msg.c\
                           logger.c\
                           trans_db_log.c
trans_db_log_LDADD       = ../common/libafd.a
mon_sys_log_SOURCES      = fprint_dup_msg.c\
                           logger.c\
                           mon_sys_log.c
mon_sys_log_LDADD        = ../common/libafd.a
monitor_log_SOURCES      = fprint_dup_msg.c\
                           monitor_log.c\
                           open_log_file.c
monitor_log_LDADD        = ../common/libafd.a
input_log_SOURCES        = input_log.c\
                           open_log_file.c
input_log_LDADD          = ../common/libafd.a
distribution_log_SOURCES = distribution_log.c\
                           open_log_file.c
distribution_log_LDADD   = ../common/libafd.a
output_log_SOURCES       = output_log.c\
                           open_log_file.c
output_log_LDADD         = ../common/libafd.a
confirmation_log_SOURCES = confirmation_log.c\
                           open_log_file.c
confirmation_log_LDADD   = ../common/libafd.a
delete_log_SOURCES       = delete_log.c\
                           open_log_file.c
delete_log_LDADD         = ../common/libafd.a
production_log_SOURCES   = production_log.c\
                           open_log_file.c
production_log_LDADD     = ../common/libafd.a
