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

if WITH_DELETE_LOG
   REMOVE_POOL_DIR           = remove_pool_directory.c
   EXTRA_RM_POOL_DIR         =
else
   REMOVE_POOL_DIR           =
   EXTRA_RM_POOL_DIR         = remove_pool_directory.c
endif
if WITH_DISTRIBUTION_LOG
   DIS_LOG                   = dis_log.c
   EXTRA_DIS_LOG             =
else
   DIS_LOG                   =
   EXTRA_DIS_LOG             = dis_log.c
endif
if WITH_ONETIME_SUPPORT
   CHECK_OT_CONF             = check_onetime_config.c receive_log.c
   EXTRA_CHECK_OT_CONF       =
else
   CHECK_OT_CONF             =
   EXTRA_CHECK_OT_CONF       = check_onetime_config.c
endif
if WITH_INOTIFY_SUPPORT
   CHECK_INOTIFY_FILES       = check_inotify_files.c del_unknown_inotify_files.c
   EXTRA_CHECK_INOTIFY_FILES =
else
   CHECK_INOTIFY_FILES       =
   EXTRA_CHECK_INOTIFY_FILES = check_inotify_files.c del_unknown_inotify_files.c
endif

noinst_HEADERS         = amgdefs.h
AM_CPPFLAGS            = -I.. -I../init_afd -I../fd
bin_PROGRAMS           = amg dir_check $(ONETIME_PROG)
EXTRA_PROGRAMS         = test_in_time test_time show_amg_data
EXTRA_DIST             = $(EXTRA_RM_POOL_DIR) $(EXTRA_DIS_LOG) $(EXTRA_CHECK_OT_CONF)
MAINTAINERCLEANFILES   = Makefile.in
CLEANFILES             = gmon.out core core.* $(EXTRA_PROGRAMS)
amg_SOURCES            = add_file_mask.c\
                         amg.c\
                         amg_zombie_check.c\
                         $(CHECK_OT_CONF)\
                         check_option.c\
                         clear_pool_dir.c\
                         com.c\
                         convert_fsa.c\
                         convert_fra.c\
                         create_fsa.c\
                         create_fra.c\
                         create_sa.c\
                         eval_dir_config.c\
                         eval_dir_options.c\
                         handle_full_dc_names.c\
                         lookup_dc_id.c\
                         lookup_dir_id.c\
                         make_process_amg.c\
                         $(REMOVE_POOL_DIR)\
                         reread_dir_config.c\
                         reread_host_config.c\
                         store_file_mask.c\
                         store_passwd.c
amg_LDADD              = ../common/libafd.a
dir_check_SOURCES      = check_file_dir.c\
                         check_files.c\
                         check_paused_dir.c\
                         check_old_time_jobs.c\
                         clear_msg_buffer.c\
                         convert_jid.c\
                         count_pool_files.c\
                         create_db.c\
                         dir_check.c\
                         $(DIS_LOG)\
                         enter_time_job.c\
                         eval_bul_rep_config.c\
                         get_last_char.c\
                         $(CHECK_INOTIFY_FILES)\
                         handle_options.c\
                         handle_retrieve_list.c\
                         handle_time_jobs.c\
                         init_dir_check.c\
                         init_job_data.c\
                         init_msg_buffer.c\
                         in_time.c\
                         link_files.c\
                         lookup_fra_pos.c\
                         lookup_file_mask_id.c\
                         lookup_job_id.c\
                         next.c\
                         receive_log.c\
                         remove_old_ls_data_files.c\
                         remove_time_dir.c\
                         rename_files.c\
                         save_files.c\
                         search_old_files.c\
                         send_message.c\
                         sort_time_job.c\
                         store_passwd.c
dir_check_LDADD        = ../misc/libmisc.a ../common/libafd.a @EXTRA_SOCKET_LIBS@
test_time_SOURCES      = test_time.c
test_time_LDADD        = ../common/libafd.a
test_in_time_SOURCES   = in_time.c\
                         test_in_time.c
test_in_time_LDADD     = ../common/libafd.a
show_amg_data_SOURCES  = show_amg_data.c
show_amg_data_LDADD    = ../common/libafd.a
