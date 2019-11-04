## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2001 - 2019 Holger Kiehl
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

if WITH_AFD_MON
   AFD_MON_PROGS = msa_attach.c msa_detach.c attach_afd_mon_status.c\
                   detach_afd_mon_status.c handle_adl.c handle_ahl.c\
		   handle_atd.c
   EXTRA_AFD_MON =
else
   AFD_MON_PROGS =
   EXTRA_AFD_MON = msa_attach.c msa_detach.c attach_afd_mon_status.c\
                   detach_afd_mon_status.c handle_adl.c handle_ahl.c\
		   handle_atd.c
endif
if WITHOUT_MMAP
   AFD_MAPPER = mmap_emu.c msync_emu.c munmap_emu.c
   EXTRA_MAPPER =
else
   AFD_MAPPER =
   EXTRA_MAPPER = mmap_emu.c msync_emu.c munmap_emu.c
endif
if HAVE_EACCESS
   AFD_EACCESS =
   EXTRA_EACCESS = eaccess.c
else
   AFD_EACCESS = eaccess.c
   EXTRA_EACCESS =
endif
if WITHOUT_DUPCHECK
   DUPCHECK =
   EXTRA_DUPCHECK = isdup.c eval_dupcheck_options.c
else
   DUPCHECK = isdup.c eval_dupcheck_options.c
   EXTRA_DUPCHECK =
endif
if WITH_SETUID
   SET_AFD_EUID = set_afd_euid.c
   EXTRA_SET_AFD_EUID =
else
   SET_AFD_EUID =
   EXTRA_SET_AFD_EUID = set_afd_euid.c
endif
if WITH_PRODUCTION_LOG
   PRODUCTION_LOG = production_log.c get_sum_cpu_usage.c
   EXTRA_PRODUCTION_LOG =
else
   PRODUCTION_LOG =
   EXTRA_PRODUCTION_LOG = production_log.c get_sum_cpu_usage.c
endif
if WITH_OUTPUT_LOG
   OUTPUT_LOG_PTRS       = output_log_fd.c output_log_ptrs.c
   EXTRA_OUTPUT_PTRS     =
else
   OUTPUT_LOG_PTRS       =
   EXTRA_OUTPUT_PTRS     = output_log_fd.c output_log_ptrs.c
endif
if WITH_DELETE_LOG
   DELETE_LOG_PTRS = delete_log_ptrs.c
   EXTRA_DELETE_LOG_PTRS =
else
   DELETE_LOG_PTRS =
   EXTRA_DELETE_LOG_PTRS = delete_log_ptrs.c
endif
if WITH_FIFO_RW_SUPPORT
   OPEN_FIFO_RW =
   EXTRA_OPEN_FIFO_RW = open_fifo_rw.c
else
   OPEN_FIFO_RW = open_fifo_rw.c
   EXTRA_OPEN_FIFO_RW =
endif
if ERROR_QUEUE_SUPPORT
   HANDLE_ERROR_QUEUE = convert_error_queue.c handle_error_queue.c
   EXTRA_HANDLE_ERROR_QUEUE =
else
   HANDLE_ERROR_QUEUE =
   EXTRA_HANDLE_ERROR_QUEUE = convert_error_queue.c handle_error_queue.c
endif
if WITH_IP_DATABASE
   HANDLE_IP = handle_ip.c
   EXTRA_HANDLE_IP =
else
   HANDLE_IP =
   EXTRA_HANDLE_IP = handle_ip.c
endif
if WITH_MAINTAINER_LOG
   MAINTAINER_LOG = maintainer_log.c
   EXTRA_MAINTAINER_LOG =
else
   MAINTAINER_LOG =
   EXTRA_MAINTAINER_LOG = maintainer_log.c
endif
if WITH_MULTI_FS_SUPPORT
   HANDLE_EXTRA_WORK_DIRS = handle_extra_workdirs.c
   EXTRA_HANDLE_EXTRA_WORK_DIRS =
else
   HANDLE_EXTRA_WORK_DIRS =
   EXTRA_HANDLE_EXTRA_WORK_DIRS = handle_extra_workdirs.c
endif
EXTRA_DIST = $(EXTRA_MAPPER) $(EXTRA_AFD_MON) $(EXTRA_EACCESS)\
             $(EXTRA_DUPCHECK) $(EXTRA_SET_AFD_EUID) $(EXTRA_PRODUCTION_LOG)\
             $(EXTRA_DELETE_LOG_PTRS) $(EXTRA_OPEN_FIFO_RW)\
	     $(EXTRA_OUTPUT_PTRS) $(EXTRA_HANDLE_ERROR_QUEUE)\
	     $(EXTRA_HANDLE_IP) $(EXTRA_MAINTAINER_LOG)\
	     $(EXTRA_HANDLE_EXTRA_WORK_DIRS) pmatch_old.c

MAINTAINERCLEANFILES = Makefile.in
AM_CPPFLAGS          = -I.. -I../init_afd -I../amg -I../fd -I../misc\
                       -I../afd_mon -I../log/alda

lib_LIBRARIES        = libafd.a
libafd_a_SOURCES     = attach_afd_status.c\
                       attach_buf.c\
                       bitstuff.c\
                       calc_next_time.c\
                       change_alias_order.c\
                       change_name.c\
                       check_lock.c\
                       coe_open.c\
                       convert_ls_data.c\
                       create_message.c\
                       create_remote_dir.c\
                       check_additional_lock_filters.c\
                       check_create_path.c\
                       check_dir.c\
                       check_fake_user.c\
                       check_fra.c\
                       check_fsa.c\
                       check_msa.c\
                       clr_fl.c\
                       copy_file.c\
                       create_name.c\
                       daemon_init.c\
                       datestr2unixtime.c\
                       $(DELETE_LOG_PTRS)\
                       detach_afd_status.c\
                       $(AFD_EACCESS)\
                       error_action.c\
                       eval_host_config.c\
                       eval_timeout.c\
                       eval_time_str.c\
                       event_log.c\
                       exec_cmd.c\
                       expand_path.c\
                       extract_cus.c\
                       fra_attach.c\
                       fra_detach.c\
                       fsa_attach.c\
                       fsa_detach.c\
                       fra_disable_all_flag.c\
                       get_additional_locked_files.c\
                       get_afd_name.c\
                       get_afd_path.c\
                       get_afd_status_struct_size.c\
                       get_arg.c\
                       get_checksum.c\
                       get_checksum_crc32c.c\
                       get_definition.c\
                       get_dir_alias.c\
                       get_dir_number.c\
                       get_dir_options.c\
                       get_error_str.c\
                       get_file_mask_list.c\
                       get_log_type_data.c\
                       get_log_number.c\
                       get_max_log_values.c\
                       get_mon_path.c\
                       get_permissions.c\
                       get_position.c\
                       get_pw.c\
                       get_rename_rules.c\
                       get_rule.c\
                       get_update_config_str.c\
                       get_user.c\
                       handle_counter.c\
                       $(HANDLE_ERROR_QUEUE)\
                       $(HANDLE_EXTRA_WORK_DIRS)\
                       $(HANDLE_IP)\
                       handle_jid.c\
                       handle_jis.c\
                       handle_xor_crypt.c\
                       inform_fd_about_fsa_change.c\
                       $(DUPCHECK)\
                       is_msgname.c\
			     jid_attach.c\
			     jid_detach.c\
                       lock_file.c\
                       lock_proc.c\
                       lock_region.c\
                       lock_region_w.c\
                       $(MAINTAINER_LOG)\
                       make_fifo.c\
                       map_file.c\
                       mode_t2str.c\
                       $(AFD_MAPPER)\
                       mmap_resize.c\
                       $(AFD_MON_PROGS)\
                       move_file.c\
                       my_power.c\
                       my_strcmp.c\
                       my_strncpy.c\
                       my_usleep.c\
                       $(OPEN_FIFO_RW)\
                       $(OUTPUT_LOG_PTRS)\
                       posi.c\
                       pmatch.c\
                       $(PRODUCTION_LOG)\
                       read_file.c\
                       read_file_no_cr.c\
                       read_job_ids.c\
                       readn.c\
                       rec.c\
                       rec_rmdir.c\
                       remove_dir.c\
                       remove_files.c\
                       remove_nnn_files.c\
                       remove_job_files.c\
                       reshuffel_log_files.c\
                       rlock_region.c\
                       handle_alias_name.c\
                       send_cmd.c\
                       $(SET_AFD_EUID)\
                       set_fl.c\
                       show_fifo_data.c\
                       store_real_hostname.c\
                       str2mode_t.c\
                       system_data.c\
                       system_log.c\
                       t_hostname.c\
                       typesize_data.c\
                       unlock_region.c\
                       unmap_data.c\
                       update_db_log.c\
                       url.c\
                       write_host_config.c\
                       writen.c\
                       ../init_afd/afddefs.h\
                       ../afdsetup.h\
                       ../../config.h

clean-local:
	-rm -rf .deps

