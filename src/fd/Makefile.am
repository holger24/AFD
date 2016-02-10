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

if WITH_MISC_LIB
   MISC_LIB = ../misc/libmisc.a
else
   MISC_LIB =
endif
if WITH_MAP_SUPPORT
   AFD_MAP_PROG          = sf_map
   EXTRA_MAP_PROG        =
else
   AFD_MAP_PROG          =
   EXTRA_MAP_PROG        = sf_map
endif
if WITH_DFAX_SUPPORT
   AFD_DFAX_PROG         = sf_dfax
   EXTRA_DFAX_PROG       =
else
   AFD_DFAX_PROG         =
   EXTRA_DFAX_PROG       = sf_dfax
endif
if WITH_LOC_SUPPORT
   LOC_PROG              = sf_loc
   EXTRA_LOC_PROG        =
else
   LOC_PROG              =
   EXTRA_LOC_PROG        = sf_loc
endif
if WITH_FD_EXEC_SUPPORT
   EXEC_PROG             = gf_exec sf_exec
   EXTRA_EXEC_PROG       =
else
   EXEC_PROG             =
   EXTRA_EXEC_PROG       = gf_exec sf_exec
endif
if WITH_FTP_SUPPORT
   FTP_PROG              = gf_ftp gf_ftp_trace sf_ftp sf_ftp_trace
   EXTRA_FTP_PROG        =
else
   FTP_PROG              =
   EXTRA_FTP_PROG        = gf_ftp gf_ftp_trace sf_ftp sf_ftp_trace
endif
if WITH_SCP_SUPPORT
   SCP_PROG              = sf_scp sf_scp_trace
   EXTRA_SCP_PROG        =
else
   SCP_PROG              =
   EXTRA_SCP_PROG        = sf_scp sf_scp_trace
endif
if WITH_WMO_SUPPORT
   WMO_PROG              = sf_wmo sf_wmo_trace
   EXTRA_WMO_PROG        =
else
   WMO_PROG              =
   EXTRA_WMO_PROG        = sf_wmo sf_wmo_trace
endif
if WITH_SMTP_SUPPORT
   SMTP_PROG             = sf_smtp sf_smtp_trace
   EXTRA_SMTP_PROG       =
else
   SMTP_PROG             =
   EXTRA_SMTP_PROG       = sf_smtp sf_smtp_trace
endif
if WITH_HTTP_SUPPORT
   HTTP_PROG             = gf_http gf_http_trace sf_http sf_http_trace
   EXTRA_HTTP_PROG       =
else
   HTTP_PROG             =
   EXTRA_HTTP_PROG       = gf_http gf_http_trace sf_http sf_http_trace
endif
if WITH_SFTP_SUPPORT
   SFTP_PROG             = gf_sftp gf_sftp_trace sf_sftp sf_sftp_trace
   EXTRA_SFTP_PROG       =
else
   SFTP_PROG             =
   EXTRA_SFTP_PROG       = gf_sftp gf_sftp_trace sf_sftp sf_sftp_trace
endif
if WITH_TRANS_EXEC
   TRANS_EXEC_PTRS       = trans_exec.c
   EXTRA_TRANS_EXEC_PTRS =
else
   TRANS_EXEC_PTRS       =
   EXTRA_TRANS_EXEC_PTRS = trans_exec.c
endif
if WITH_DE_MAIL_SUPPORT
   DEMCD_LOG_FD          = demcd_log_fd.c
   EXTRA_DEMCD_LOG_FD    =
   DEMCD_LOG_PTRS        = demcd_log_ptrs.c
   EXTRA_DEMCD_LOG_PTRS  =
else
   DEMCD_LOG_FD          =
   EXTRA_DEMCD_LOG_FD    = demcd_log_fd.c
   DEMCD_LOG_PTRS        =
   EXTRA_DEMCD_LOG_PTRS  = demcd_log_ptrs.c
endif

noinst_HEADERS        = fddefs.h
AM_CPPFLAGS           = -I.. -I../init_afd -I../amg -I../log -I../misc\
                        -I../protocols
bin_PROGRAMS          = fd $(EXEC_PROG) $(FTP_PROG) $(SMTP_PROG) $(SCP_PROG)\
                        $(SFTP_PROG) $(WMO_PROG) $(AFD_MAP_PROG) $(LOC_PROG)\
                        $(HTTP_PROG) $(AFD_DFAX_PROG)
EXTRA_PROGRAMS        = $(EXTRA_MAP_PROG) $(EXTRA_FTP_PROG) $(EXTRA_SFTP_PROG)\
                        $(EXTRA_WMO_PROG) $(EXTRA_HTTP_PROG) $(EXTRA_SCP_PROG)\
			$(EXTRA_EXEC_PROG) $(EXTRA_LOC_PROG) $(EXTRA_SMTP_PROG)\
			$(EXTRA_DFAX_PROG)
EXTRA_DIST            = $(EXTRA_TRANS_EXEC_PTRS) $(EXTRA_DEMCD_LOG_FD)\
                        $(EXTRA_DEMCD_LOG_PTRS)
MAINTAINERCLEANFILES  = Makefile.in
CLEANFILES            = gmon.out core core.* $(EXTRA_PROGRAMS)
fd_SOURCES            = check_fra_fd.c\
                        check_fsa_entries.c\
                        check_msg_time.c\
                        check_queue_space.c\
                        fd.c\
                        fd_check_fsa.c\
                        get_job_data.c\
                        get_new_positions.c\
                        handle_delete_fifo.c\
                        init_fifos_fd.c\
                        init_fra_data.c\
                        init_msg_buffer.c\
                        init_msg_ptrs.c\
                        lookup_job_id.c\
                        manage_trl_process.c\
                        read_current_msg_list.c\
                        recount_jobs_queued.c\
                        recreate_msg.c\
                        remove_connection.c\
                        remove_msg.c
fd_LDADD              = ../common/libafd.a
fd_DEPENDENCIES       = $(fd_LDADD) fddefs.h
sf_ftp_SOURCES        = append.c\
                        archive_file.c\
                        check_burst_sf.c\
                        check_exec_type.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        handle_proxy.c\
                        init_sf.c\
                        init_sf_burst2.c\
                        limit_transfer_rate.c\
                        noop_wrapper_ftp.c\
                        receive_log.c\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_ftp.c\
                        trans_db_log.c\
                        $(TRANS_EXEC_PTRS)\
                        trans_log.c\
                        update_tfc.c
sf_ftp_LDADD          = $(MISC_LIB) @EXTRA_SOCKET_LIBS@\
                        ../protocols/libprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
sf_ftp_trace_SOURCES  = $(sf_ftp_SOURCES) trace_log.c
sf_ftp_trace_LDADD    = $(MISC_LIB) @EXTRA_SOCKET_LIBS@\
                        ../protocols/libtraceprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
sf_http_SOURCES       = append.c\
                        archive_file.c\
                        check_burst_sf.c\
                        check_exec_type.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        init_sf.c\
                        init_sf_burst2.c\
                        limit_transfer_rate.c\
                        noop_wrapper_dummy.c\
                        receive_log.c\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_http.c\
                        trans_db_log.c\
                        $(TRANS_EXEC_PTRS)\
                        trans_log.c\
                        update_tfc.c
sf_http_LDADD         = $(MISC_LIB) @EXTRA_SOCKET_LIBS@\
                        ../protocols/libprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
sf_http_trace_SOURCES = $(sf_http_SOURCES) trace_log.c
sf_http_trace_CFLAGS  = -DWITH_TRACE
sf_http_trace_LDADD   = $(MISC_LIB) @EXTRA_SOCKET_LIBS@\
                        ../protocols/libtraceprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
sf_sftp_SOURCES       = append.c\
                        archive_file.c\
                        check_burst_sf.c\
                        check_exec_type.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        init_sf.c\
                        init_sf_burst2.c\
                        limit_transfer_rate.c\
                        noop_wrapper_sftp.c\
                        receive_log.c\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_sftp.c\
                        trans_db_log.c\
                        $(TRANS_EXEC_PTRS)\
                        trans_log.c\
                        update_tfc.c
sf_sftp_LDADD         = $(MISC_LIB) @EXTRA_SOCKET_LIBS@\
                        ../protocols/libprotocols.a\
                        ../common/libafd.a @AFD_UTIL_LIB@
sf_sftp_trace_SOURCES = $(sf_sftp_SOURCES) trace_log.c
sf_sftp_trace_LDADD   = $(MISC_LIB) @EXTRA_SOCKET_LIBS@\
                        ../protocols/libtraceprotocols.a\
                        ../common/libafd.a @AFD_UTIL_LIB@
sf_smtp_SOURCES       = append.c\
                        archive_file.c\
                        check_burst_sf.c\
                        check_exec_type.c\
			$(DEMCD_LOG_FD)\
			$(DEMCD_LOG_PTRS)\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        init_sf.c\
                        init_sf_burst2.c\
                        limit_transfer_rate.c\
                        noop_wrapper_smtp.c\
                        receive_log.c\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_smtp.c\
                        trans_db_log.c\
                        $(TRANS_EXEC_PTRS)\
                        trans_log.c\
                        update_tfc.c
sf_smtp_LDADD         = @EXTRA_SOCKET_LIBS@ ../protocols/libprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
sf_smtp_trace_SOURCES = $(sf_smtp_SOURCES) trace_log.c
sf_smtp_trace_LDADD   = @EXTRA_SOCKET_LIBS@ ../protocols/libtraceprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
sf_scp_SOURCES        = append.c\
                        archive_file.c\
                        check_burst_sf.c\
                        check_exec_type.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        init_sf.c\
                        init_sf_burst2.c\
                        limit_transfer_rate.c\
                        noop_wrapper_dummy.c\
                        receive_log.c\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_scp.c\
                        trans_db_log.c\
                        $(TRANS_EXEC_PTRS)\
                        trans_log.c\
                        update_tfc.c
sf_scp_LDADD          = @EXTRA_SOCKET_LIBS@ ../protocols/libprotocols.a\
                        ../common/libafd.a @AFD_UTIL_LIB@
sf_scp_trace_SOURCES  = $(sf_scp_SOURCES) trace_log.c
sf_scp_trace_LDADD    = @EXTRA_SOCKET_LIBS@ ../protocols/libtraceprotocols.a\
                        ../common/libafd.a @AFD_UTIL_LIB@
sf_wmo_SOURCES        = append.c\
                        archive_file.c\
                        check_burst_sf.c\
                        check_exec_type.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        init_sf.c\
                        init_sf_burst2.c\
                        limit_transfer_rate.c\
                        noop_wrapper_dummy.c\
                        receive_log.c\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_wmo.c\
                        trans_db_log.c\
                        $(TRANS_EXEC_PTRS)\
                        trans_log.c\
                        update_tfc.c
sf_wmo_LDADD          = @EXTRA_SOCKET_LIBS@ ../protocols/libprotocols.a\
                        ../common/libafd.a
sf_wmo_trace_SOURCES  = $(sf_wmo_SOURCES) trace_log.c
sf_wmo_trace_LDADD    = @EXTRA_SOCKET_LIBS@ ../protocols/libtraceprotocols.a\
                        ../common/libafd.a
sf_map_SOURCES        = append.c\
                        archive_file.c\
                        check_exec_type.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        init_sf.c\
                        noop_wrapper_dummy.c\
                        receive_log.c\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_map.c\
                        trans_db_log.c\
                        trans_log.c\
                        update_tfc.c
sf_map_LDADD          = @EXTRA_SOCKET_LIBS@ ../common/libafd.a @AFD_MAP_LIBS@
sf_dfax_SOURCES       = append.c\
                        archive_file.c\
                        check_exec_type.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        init_sf.c\
                        noop_wrapper_dummy.c\
                        receive_log.c\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_dfax.c\
                        trans_db_log.c\
                        trans_log.c\
                        update_tfc.c
sf_dfax_LDADD         = @EXTRA_SOCKET_LIBS@ ../common/libafd.a @AFD_DFAX_LIBS@
sf_loc_SOURCES        = append.c\
                        archive_file.c\
                        handle_ls_data.c\
                        check_burst_sf.c\
                        check_exec_type.c\
                        compare_dir_local.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        init_sf.c\
                        init_sf_burst2.c\
                        noop_wrapper_dummy.c\
                        receive_log.c\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_loc.c\
                        trans_db_log.c\
                        $(TRANS_EXEC_PTRS)\
                        trans_log.c\
                        update_tfc.c
sf_loc_LDADD          = @EXTRA_SOCKET_LIBS@ ../common/libafd.a
sf_exec_SOURCES       = append.c\
                        archive_file.c\
                        handle_ls_data.c\
                        check_burst_sf.c\
                        check_exec_type.c\
                        compare_dir_local.c\
                        eval_input_sf.c\
                        eval_message.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_file_names.c\
                        get_group_list.c\
                        gsf_check_fsa.c\
                        init_sf.c\
                        init_sf_burst2.c\
                        noop_wrapper_dummy.c\
                        receive_log.c\
                        reset_fsa.c\
                        send_proc_fin.c\
                        sf_exec.c\
                        trans_db_log.c\
                        $(TRANS_EXEC_PTRS)\
                        trans_log.c\
                        update_tfc.c
sf_exec_LDADD         = ../common/libafd.a
gf_exec_SOURCES       = eval_input_gf.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_group_list.c\
                        gf_exec.c\
                        gsf_check_fra.c\
                        gsf_check_fsa.c\
                        init_gf.c\
                        reset_fsa.c\
                        reset_values.c\
                        send_proc_fin.c\
                        trans_db_log.c\
                        trans_log.c
gf_exec_LDADD         = ../common/libafd.a
gf_ftp_SOURCES        = check_burst_gf.c\
                        handle_ls_data.c\
                        delete_remote_file.c\
                        delete_wrapper_ftp.c\
                        eval_input_gf.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_group_list.c\
                        get_remote_file_names_ftp.c\
                        get_remote_file_names_ftp_list.c\
                        get_remote_file_names_ftp_mlst.c\
                        gf_ftp.c\
                        gsf_check_fra.c\
                        gsf_check_fsa.c\
                        handle_proxy.c\
                        init_gf.c\
                        init_gf_burst2.c\
                        limit_transfer_rate.c\
                        noop_wrapper_ftp.c\
                        read_file_mask.c\
                        reset_fsa.c\
                        reset_values.c\
                        send_proc_fin.c\
                        trans_db_log.c\
                        trans_log.c
gf_ftp_LDADD          = @EXTRA_SOCKET_LIBS@ ../protocols/libprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
gf_ftp_trace_SOURCES  = $(gf_ftp_SOURCES) trace_log.c
gf_ftp_trace_LDADD    = @EXTRA_SOCKET_LIBS@ ../protocols/libtraceprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
gf_sftp_SOURCES       = check_burst_gf.c\
                        handle_ls_data.c\
                        delete_remote_file.c\
                        delete_wrapper_sftp.c\
                        eval_input_gf.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_group_list.c\
                        get_remote_file_names_sftp.c\
                        gf_sftp.c\
                        gsf_check_fra.c\
                        gsf_check_fsa.c\
                        init_gf.c\
                        init_gf_burst2.c\
                        limit_transfer_rate.c\
                        noop_wrapper_sftp.c\
                        read_file_mask.c\
                        reset_fsa.c\
                        reset_values.c\
                        send_proc_fin.c\
                        trans_db_log.c\
                        trans_log.c
gf_sftp_LDADD         = @EXTRA_SOCKET_LIBS@ ../protocols/libprotocols.a\
                        ../common/libafd.a @AFD_UTIL_LIB@
gf_sftp_trace_SOURCES = $(gf_sftp_SOURCES) trace_log.c
gf_sftp_trace_LDADD   = @EXTRA_SOCKET_LIBS@ ../protocols/libtraceprotocols.a\
                        ../common/libafd.a @AFD_UTIL_LIB@
gf_http_SOURCES       = check_burst_gf.c\
                        handle_ls_data.c\
                        delete_remote_file.c\
                        delete_wrapper_http.c\
                        eval_input_gf.c\
                        eval_recipient.c\
                        fsa_attach_pos.c\
                        get_group_list.c\
                        get_remote_file_names_http.c\
                        gf_http.c\
                        gsf_check_fra.c\
                        gsf_check_fsa.c\
                        init_gf.c\
                        init_gf_burst2.c\
                        limit_transfer_rate.c\
                        noop_wrapper_dummy.c\
                        read_file_mask.c\
                        reset_fsa.c\
                        reset_values.c\
                        send_proc_fin.c\
                        trans_db_log.c\
                        trans_log.c
gf_http_LDADD         = @EXTRA_SOCKET_LIBS@ ../protocols/libprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
gf_http_trace_SOURCES = $(gf_http_SOURCES) trace_log.c
gf_http_trace_LDADD   = @EXTRA_SOCKET_LIBS@ ../protocols/libtraceprotocols.a\
                        ../common/libafd.a @AFD_SSL_LIBS@
