dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.59])
AC_INIT([afd],[1.4.8-0.10.beta],[Holger.Kiehl@dwd.de])
AC_CONFIG_SRCDIR(src/init_afd/afd.c)
AC_CONFIG_AUX_DIR(ac-tools)
dnl# AC_PREFIX_DEFAULT("/usr/local/afd")
dnl# [no-dependencies dist-bzip2]
AM_INIT_AUTOMAKE([dist-bzip2 subdir-objects])
AC_CONFIG_HEADERS([config.h:ac-tools/config.h.in])
AC_USE_SYSTEM_EXTENSIONS
AM_SILENT_RULES([yes])

REQUIRES=""
BUILD_REQUIRES=""
BUILD_REQUIRES_REDHAT=""

afd_full_version="$VERSION"
AFD_VERSION=`echo $VERSION | cut -d '-' -f 1`
AC_SUBST(AFD_VERSION)
AFD_RELEASE=`echo $VERSION | cut -d '-' -f 2`
AC_SUBST(AFD_RELEASE)

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[long long i;]])],[AC_DEFINE(HAVE_LONG_LONG, 1, Define if C compiler supports long long type.)],[])

compile_alda=false
dnl# --------------------------------------------------------------------------
dnl# Checking Configure Command Line Options
dnl# --------------------------------------------------------------------------
AC_ARG_ENABLE(input_log,
   [  --disable-input_log             disable input_log support],
   [AM_CONDITIONAL(WITH_INPUT_LOG, false)
    ac_cv_input_log=""],
   [AC_DEFINE([_INPUT_LOG], [], [With input log support])
    ac_cv_input_log="INPUT "
    compile_alda=true
    AM_CONDITIONAL(WITH_INPUT_LOG, true)])

AC_ARG_ENABLE(distribution_log,
   [  --disable-distribution_log      disable distribution_log support],
   [AM_CONDITIONAL(WITH_DISTRIBUTION_LOG, false)
    ac_cv_distribution_log=""],
   [AC_DEFINE([_DISTRIBUTION_LOG], [], [With distribution log support])
    ac_cv_distribution_log="DISTRIBUTION "
    AM_CONDITIONAL(WITH_DISTRIBUTION_LOG, true)])

AC_ARG_ENABLE(output_log,
   [  --disable-output_log            disable output_log support],
   [AM_CONDITIONAL(WITH_OUTPUT_LOG, false)
    ac_cv_output_log=""],
   [AC_DEFINE([_OUTPUT_LOG], [], [With output log support])
    ac_cv_output_log="OUTPUT "
    compile_alda=true
    AM_CONDITIONAL(WITH_OUTPUT_LOG, true)])

AC_ARG_ENABLE(confirmation_log,
   [  --enable-confirmation_log       enable confirmation_log support],
   [AC_DEFINE([_CONFIRMATION_LOG], [], [With confirmation log support])
    ac_cv_confirmation_log="CONFIRMATION "
    AM_CONDITIONAL(WITH_CONFIRMATION_LOG, true)],
   [AM_CONDITIONAL(WITH_CONFIRMATION_LOG, false)
    ac_cv_confirmation_log=""])

AC_ARG_ENABLE(delete_log,
   [  --disable-delete_log            disable delete_log support],
   [AM_CONDITIONAL(WITH_DELETE_LOG, false)
    ac_cv_delete_log=""],
   [AC_DEFINE([_DELETE_LOG], [], [With delete log support])
    ac_cv_delete_log="DELETE "
    compile_alda=true
    AM_CONDITIONAL(WITH_DELETE_LOG, true)])

AC_ARG_ENABLE(production_log,
   [  --disable-production_log        disable production_log support],
   [AM_CONDITIONAL(WITH_PRODUCTION_LOG, false)
    ac_cv_production_log=""],
   [AC_DEFINE([_PRODUCTION_LOG], [], [With production log support])
    ac_cv_production_log="PRODUCTION "
    compile_alda=true
    AM_CONDITIONAL(WITH_PRODUCTION_LOG, true)])

AC_ARG_ENABLE(maintainer_log,
   [  --enable-maintainer_log         enable maintainer_log support],
   [AC_DEFINE([_MAINTAINER_LOG], [], [With maintainer log support])
    ac_cv_maintainer_log="MAINTAINER "
    AM_CONDITIONAL(WITH_MAINTAINER_LOG, true)],
   [AM_CONDITIONAL(WITH_MAINTAINER_LOG, false)
    ac_cv_maintainer_log=""])

AC_ARG_ENABLE(transfer_rate_log,
   [  --enable-transfer_rate_log      enable transfer_rate_log support],
   [AC_DEFINE([_TRANSFER_RATE_LOG], [], [With transfer_rate_log support])
    ac_cv_transfer_rate_log="TRANSFER_RATE "
    AM_CONDITIONAL(WITH_TRANSFER_RATE_LOG, true)],
   [AM_CONDITIONAL(WITH_TRANSFER_RATE_LOG, false)
    ac_cv_transfer_rate_log=""])

AC_ARG_ENABLE(alda_cache_positioning,
   [  --disable-alda_cache_positioning disable ALDA cache positioning support],
   [AC_DEFINE([WITH_LOG_CACHE], [], [With ALDA cache positioning support])
    ac_cv_alda_cache="ALDA-Cache "
    AM_CONDITIONAL(WITH_ALDA_CACHE_POSITIONING, true)],
   [AM_CONDITIONAL(WITH_ALDA_CACHE_POSITIONING, false)
    ac_cv_alda_cache=""])

AM_CONDITIONAL(WITH_ALDA, $compile_alda)

AC_ARG_ENABLE(ncurses_ui,
   [  --disable-ncurses_ui            disable nCurses support],
   [ac_cv_ncurses_ui="no"],
   [ac_cv_ncurses_ui="yes "])

AC_ARG_ENABLE(sse42,
   [  --disable-sse42                 disable SSE4.2 optimizations],
   [ac_cv_allow_sse42="no"],
   [ac_cv_allow_sse42="yes"])

AC_ARG_ENABLE(inotify,
   [  --disable-inotify               disable inotify support],
   [ac_cv_allow_inotify="no"],
   [ac_cv_allow_inotify="yes"])

AC_ARG_ENABLE(new_motif_name,
   [  --disable-new_motif_name        when motif is required sets package name to openmotif],
   [ac_cv_new_motif_name="openmotif"],
   [ac_cv_new_motif_name="motif"])

AC_ARG_ENABLE(error_queue,
   [  --disable-error_queue           disable error_queue support],
   [AM_CONDITIONAL(ERROR_QUEUE_SUPPORT, false)
    ac_cv_error_queue="no"],
   [AC_DEFINE([WITH_ERROR_QUEUE], [], [With error queue support])
    ac_cv_error_queue="yes"
    AM_CONDITIONAL(ERROR_QUEUE_SUPPORT, true)])

AC_ARG_ENABLE(fifo_rw_support,
   [  --disable-fifo_rw_support       disable opening fifo for RW],
   [AM_CONDITIONAL(WITH_FIFO_RW_SUPPORT, false)
    AC_DEFINE([WITHOUT_FIFO_RW_SUPPORT], [], [Without fifo RW support])
    ac_cv_fifo_rw_support="no"],
   [ac_cv_fifo_rw_support="yes"
    AM_CONDITIONAL(WITH_FIFO_RW_SUPPORT, true)])

AC_ARG_ENABLE(wmo_support,
   [  --disable-wmo_support           disable WMO socket support],
   [AM_CONDITIONAL(WITH_WMO_SUPPORT, false)
    ac_cv_wmo_support=""],
   [AC_DEFINE([_WITH_WMO_SUPPORT], [], [With WMO socket support])
    ac_cv_wmo_support="WMO "
    AM_CONDITIONAL(WITH_WMO_SUPPORT, true)])

AC_ARG_ENABLE(loc_support,
   [  --disable-loc_support           disable support for copying files locally],
   [AM_CONDITIONAL(WITH_LOC_SUPPORT, false)
    ac_cv_loc_support=""],
   [AC_DEFINE([_WITH_LOC_SUPPORT], [], [With support for copying files locally])
    ac_cv_loc_support="LOC "
    AM_CONDITIONAL(WITH_LOC_SUPPORT, true)])

AC_ARG_ENABLE(fd_exec_support,
   [  --disable-fd_exec_support       disable support for transmitting files via external program],
   [AM_CONDITIONAL(WITH_FD_EXEC_SUPPORT, false)
    ac_cv_fd_exec_support=""],
   [AC_DEFINE([_WITH_FD_EXEC_SUPPORT], [], [With support for transmitting files via external program])
    ac_cv_fd_exec_support="EXEC "
    AM_CONDITIONAL(WITH_FD_EXEC_SUPPORT, true)])

AC_ARG_ENABLE(ftp_support,
   [  --disable-ftp_support           disable FTP support],
   [AM_CONDITIONAL(WITH_FTP_SUPPORT, false)
    ac_cv_ftp_support=""],
   [AC_DEFINE([_WITH_FTP_SUPPORT], [], [With FTP support])
    ac_cv_ftp_support="FTP "
    AM_CONDITIONAL(WITH_FTP_SUPPORT, true)])

AC_ARG_ENABLE(scp_support,
   [  --disable-scp_support           disable SCP support],
   [AM_CONDITIONAL(WITH_SCP_SUPPORT, false)
    ac_cv_scp_support=""],
   [AC_DEFINE([_WITH_SCP_SUPPORT], [], [With SCP support])
    ac_cv_scp_support="SCP "
    AM_CONDITIONAL(WITH_SCP_SUPPORT, true)])

AC_ARG_ENABLE(smtp_support,
   [  --disable-smtp_support          disable SMTP support],
   [AM_CONDITIONAL(WITH_SMTP_SUPPORT, false)
    ac_cv_smtp_support=""],
   [AC_DEFINE([_WITH_SMTP_SUPPORT], [], [With SMTP support])
    ac_cv_smtp_support="SMTP "
    AM_CONDITIONAL(WITH_SMTP_SUPPORT, true)])

AC_ARG_ENABLE(http_support,
   [  --disable-http_support          disable HTTP support],
   [AM_CONDITIONAL(WITH_HTTP_SUPPORT, false)
    ac_cv_http_support=""],
   [AC_DEFINE([_WITH_HTTP_SUPPORT], [], [With HTTP support])
    ac_cv_http_support="HTTP "
    AM_CONDITIONAL(WITH_HTTP_SUPPORT, true)])

AC_ARG_ENABLE(sftp_support,
   [  --disable-sftp_support          disable SFTP support],
   [AM_CONDITIONAL(WITH_SFTP_SUPPORT, false)
    ac_cv_sftp_support=""],
   [AC_DEFINE([_WITH_SFTP_SUPPORT], [], [With SFTP support])
    ac_cv_sftp_support="SFTP "
    AM_CONDITIONAL(WITH_SFTP_SUPPORT, true)])

AC_ARG_ENABLE(html_gui,
   [  --enable-html_gui               enable HTML gui],
   [AM_CONDITIONAL(WITH_HTML_GUI, true)
    ac_cv_html_gui="yes"],
   [ac_cv_html_gui="no"
    AM_CONDITIONAL(WITH_HTML_GUI, false)])

AC_ARG_ENABLE(ip_database,
   [  --enable-ip_database            enable internal IP database],
   [AM_CONDITIONAL(WITH_IP_DATABASE, true)
    AC_DEFINE([WITH_IP_DB], [], [With internal IP database])
    ac_cv_ip_database="yes"],
   [ac_cv_ip_database="no"
    AM_CONDITIONAL(WITH_IP_DATABASE, false)])

AC_ARG_ENABLE(onetime_support,
   [  --enable-onetime_support        enable support for onetime jobs],
   [AM_CONDITIONAL(WITH_ONETIME_SUPPORT, true)
    AC_DEFINE([WITH_ONETIME], [], [With onetime support])
    ac_cv_onetime_support="yes"],
   [ac_cv_onetime_support="no"
    AM_CONDITIONAL(WITH_ONETIME_SUPPORT, false)])

AC_ARG_ENABLE(dupcheck_support,
   [  --disable-dupcheck_support      disable duplicate check support],
   [AM_CONDITIONAL(WITHOUT_DUPCHECK, true)
    ac_cv_dupcheck_support="no"],
   [AC_DEFINE([WITH_DUP_CHECK], [], [With duplicate check support])
    ac_cv_dupcheck_support="yes"
    AM_CONDITIONAL(WITHOUT_DUPCHECK, false)])

AC_ARG_ENABLE(trans_exec,
   [  --disable-trans_exec            disable trans_exec support],
   [AM_CONDITIONAL(WITH_TRANS_EXEC, false)
    ac_cv_trans_exec="no"],
   [AC_DEFINE([_WITH_TRANS_EXEC], [], [With trans_exec support])
    ac_cv_trans_exec="yes "
    AM_CONDITIONAL(WITH_TRANS_EXEC, true)])

AC_ARG_ENABLE(largefile,
   [  --disable-largefile             disable large file support],
   [ac_cv_largefile="$enableval"],
   [ac_cv_largefile="yes"])

AC_ARG_ENABLE(verify_fsa,
   [  --disable-verify_fsa            disable verify_fsa support],
   [if test x$enableval = xyes; then
      AC_DEFINE([_VERIFY_FSA], [], [With verify FSA support])
    fi],
   [AC_DEFINE([_VERIFY_FSA], [], [With verify FSA support])])

AC_ARG_ENABLE(ignore_duplicate_jobs,
   [  --disable-ignore_duplicate_jobs disable ignoring duplicate job entries],
   [if test x$enableval = xyes; then
      AC_DEFINE([IGNORE_DUPLICATE_JOB_IDS], [], [With ignoring duplicate jobs])
    fi],
   [AC_DEFINE([IGNORE_DUPLICATE_JOB_IDS], [], [With ignoring duplicate jobs])])

AC_ARG_ENABLE(with_ctrl_accelerator,
   [  --enable-with_ctrl_accelerator  use ctrl key as accelerator],
   [if test x$enableval = xyes; then
      AC_DEFINE([WITH_CTRL_ACCELERATOR], [], [Use ctrl key as accelerator])
   fi])

AC_ARG_ENABLE(with_archive_copy_info,
   [  --enable-with_archive_copy_info  inform when files are copied when archived],
   [if test x$enableval = xyes; then
      AC_DEFINE([WITH_ARCHIVE_COPY_INFO], [], [Inform when files are copied when archived])
   fi])

AC_ARG_ENABLE(error_offline_for_new_host,
   [  --enable-error_offline_for_new_host  new host have error offline set],
   [if test x$enableval = xyes; then
      AC_DEFINE([ERROR_OFFLINE_FOR_NEW_HOST], [], [New host have error offline set])
   fi])

AC_ARG_ENABLE(with_afdbench_settings,
   [  --enable-with_afdbench_settings enables settings for running afdbench],
   [AC_DEFINE([AFDBENCH_CONFIG], [], [Enables settings for running afdbench])
    ac_cv_with_afdbench_settings="yes"],
   [ac_cv_with_afdbench_settings="no"])

AC_ARG_ENABLE(auto_create_config,
   [  --disable-auto_create_config    disable automatic creation of configuration],
   [ac_cv_auto_create_config="no"],
   [AC_DEFINE([WITH_AUTO_CONFIG], [], [With automatic creation of configuration])
    ac_cv_auto_create_config="yes"])

AC_ARG_ENABLE(afd_ctrl_proc_check,
   [  --disable-afd_ctrl_proc_check   disable afd_ctrl dialog checking for main AFD process],
   [ac_cv_afd_ctrl_proc_check="no"],
   [AC_DEFINE([AFD_CTRL_PROC_CHECK], [], [With afd_ctrl dialog checking for main AFD process])
    ac_cv_afd_ctrl_proc_check="yes"])

AC_ARG_ENABLE(report_empty_dir_scan,
   [  --enable-report_empty_dir_scan  enable reporting empty dir scans],
   [AC_DEFINE([REPORT_EMPTY_DIR_SCANS], [], [Report empty directory scans])
    ac_cv_report_empty_dir_scan="yes"],
   [ac_cv_report_empty_dir_scan="no"])

AC_ARG_ENABLE(expand_path_in_message,
   [  --enable-expand_path_in_message enable expanding path in message],
   [AC_DEFINE([EXPAND_PATH_IN_MESSAGE], [], [Expand path in message])
    ac_cv_expand_path_in_message="yes"],
   [ac_cv_expand_path_in_message="no"])

AC_ARG_ENABLE(statistics_in_fifodir,
   [  --enable-statistics_in_fifodir  statistics are stored in fifodir],
   [AC_DEFINE([STAT_IN_FIFODIR], [], [Statistics in fifodir])
    ac_cv_statistics_in_fifodir="yes"],
   [ac_cv_statistics_in_fifodir="no"])

AC_ARG_ENABLE(setuid_progs,
   [  --enable-setuid_progs           enable setuid flag when installing],
   [ac_cv_setuid_progs="$enableval"
    AC_DEFINE([WITH_SETUID_PROGS], [], [Set some programms setuid.])
    AM_CONDITIONAL(WITH_SETUID, true)],
   [ac_cv_setuid_progs="no"
    AM_CONDITIONAL(WITH_SETUID, false)])

AC_ARG_ENABLE(atpd_support,
   [  --enable-atpd_support           enable ATPD support],
   [AM_CONDITIONAL(WITH_ATPD_SUPPORT, true)
    AC_DEFINE([_WITH_ATPD_SUPPORT], [], [With ATPD support])
    ac_cv_atpd_support="ATPD "],
   [ac_cv_atpd_support=""
    AM_CONDITIONAL(WITH_ATPD_SUPPORT, false)])

AC_ARG_ENABLE(wmod_support,
   [  --enable-wmod_support           enable WMOD support],
   [AM_CONDITIONAL(WITH_WMOD_SUPPORT, true)
    AC_DEFINE([_WITH_WMOD_SUPPORT], [], [With WMOD support])
    ac_cv_wmod_support="WMOD "],
   [ac_cv_wmod_support=""
    AM_CONDITIONAL(WITH_WMOD_SUPPORT, false)])

AC_ARG_ENABLE(de_mail_support,
   [  --enable-de_mail_support        enable DE-mail support],
   [AM_CONDITIONAL(WITH_DE_MAIL_SUPPORT, true)
    AC_DEFINE([_WITH_DE_MAIL_SUPPORT], [], [With DE-mail support])
    ac_cv_demail_support="DE-mail "],
   [ac_cv_demail_support=""
    AM_CONDITIONAL(WITH_DE_MAIL_SUPPORT, false)])

AC_ARG_ENABLE(with_unlink_delay,
   [  --enable-with_unlink_delay      enable delay and retry when unlink fails],
   [if test x$enableval = xyes; then
      AC_DEFINE([WITH_UNLINK_DELAY], [], [Enable delay and retry when unlink fails])
   fi])

AC_ARG_ENABLE(multi_dir_scans,
   [  --enable-multi_dir_scans        enable multiple directory scans],
   [if test x$enableval = xyes; then
      AC_DEFINE([WITH_MULTI_DIR_SCANS], [], [Enable multiple directory scans])
   fi])

AC_ARG_ENABLE(multi_fs_support,
   [  --enable-multi_fs_support       enable multiple local filesystem support],
   [AM_CONDITIONAL(WITH_MULTI_FS_SUPPORT, true)
    AC_DEFINE([MULTI_FS_SUPPORT], [], [Enable multiple local filesystem support])
    ac_cv_multi_fs_support="yes"],
   [ac_cv_multi_fs_support="no"
    AM_CONDITIONAL(WITH_MULTI_FS_SUPPORT, false)])

AC_ARG_ENABLE(ftp_reuse_data_port,
   [  --enable-ftp_reuse_data_port    enable reusing FTP data port],
   [if test x$enableval = xyes; then
      AC_DEFINE([FTP_REUSE_DATA_PORT], [], [With reusing FTP data port])
   fi])

AC_ARG_ENABLE(ascii_only_subject,
   [  --enable-ascii_only_subject     enable ASCII only subjet in mail],
   [if test x$enableval = xyes; then
      AC_DEFINE([WITH_ASCII_ONLY_SUBJECT], [], [With ASCII only subjects])
   fi])

AC_ARG_ENABLE(ms_error_workaround,
   [  --enable-ms_error_workaround    enable workaround for MS FTP bug],
   [if test x$enableval = xyes; then
      AC_DEFINE([WITH_MS_ERROR_WORKAROUND], [], [With MS FTP error workaround])
   fi])

AC_ARG_ENABLE(group_can_write,
   [  --enable-group_can_write        allow group write access],
   [if test x$enableval = xyes; then
      AC_DEFINE([GROUP_CAN_WRITE], [], [Allow group write access])
   fi
   ac_cv_group_can_write="$enableval"],
   [ac_cv_group_can_write="no"])

AC_ARG_ENABLE(lockdebug,
   [  --enable-lockdebug              enable debug messages of locking],
   [if test x$enableval = xyes; then
      AC_DEFINE([LOCK_DEBUG], [], [With debug messages of locking])
   fi])

AC_ARG_ENABLE(show_exec_times,
   [  --enable-show_exec_times        enable show exec times when rereading DIR_CONFIG],
   [if test x$enableval = xyes; then
      AC_DEFINE([SHOW_EXEC_TIMES], [], [With show exec times])
   fi])

AC_ARG_ENABLE(sendfile_support,
   [  --enable-sendfile_support       enable sendfile support],
   [if test x$enableval = xyes; then
      AC_DEFINE([WITH_SENDFILE], [], [With sendfile support])
   fi])

AC_ARG_ENABLE(splice_support,
   [  --enable-splice_support       enable splice support],
   [if test x$enableval = xyes; then
      AC_DEFINE([WITH_SPLICE_SUPPORT], [], [With splice support])
   fi])

AC_ARG_ENABLE(readyfile,
   [  --enable-readyfile              enable readyfile support],
   [if test x$enableval = xyes; then
      AC_DEFINE([WITH_READY_FILES], [], [With FSS-type ready files])
   fi])

AC_ARG_ENABLE(map_support,
   [  --enable-map_support            enable MAP support],
   [if test x$enableval = xyes; then
      AC_DEFINE([_WITH_MAP_SUPPORT], [], [With MAP support])
      AM_CONDITIONAL(WITH_MAP_SUPPORT, true)
   else
      AM_CONDITIONAL(WITH_MAP_SUPPORT, false)
   fi],
   [AM_CONDITIONAL(WITH_MAP_SUPPORT, false)])

AC_ARG_WITH(map-libraries,
   [  --with-map-libraries=DIR        location where MAP libraries are],
   afd_map_lib_dir="$withval")
if test "x$afd_map_lib_dir" = "x"
then
   AFD_MAP_LIBS=""
else
   AFD_MAP_LIBS="-L$afd_map_lib_dir -lmfse2 -lz -lfaxe -lgeo"
fi
AC_SUBST(AFD_MAP_LIBS)

AC_ARG_ENABLE(dfax_support,
   [  --enable-dfax_support           enable DFAX support],
   [if test x$enableval = xyes; then
      AC_DEFINE([_WITH_DFAX_SUPPORT], [], [With DFAX support])
      AM_CONDITIONAL(WITH_DFAX_SUPPORT, true)
      ac_cv_dfax_support="DFAX "
   else
      AM_CONDITIONAL(WITH_DFAX_SUPPORT, false)
      ac_cv_dfax_support=""
   fi],
   [AM_CONDITIONAL(WITH_DFAX_SUPPORT, false)
    ac_cv_dfax_support=""])

AC_ARG_WITH(dfax-libraries,
   [  --with-dfax-libraries=DIR       location where DFAX libraries are],
   afd_dfax_lib_dir="$withval")
if test "x$afd_dfax_lib_dir" = "x"
then
   AFD_DFAX_LIBS="-lDivaS"
else
   AFD_DFAX_LIBS="-L$afd_dfax_lib_dir -lDivaS"
fi
AC_SUBST(AFD_DFAX_LIBS)

AC_ARG_ENABLE(passwd_in_msg,
   [  --enable-passwd_in_msg          enable password in message],
   [if test x$enableval = xyes; then
      AC_DEFINE([WITH_PASSWD_IN_MSG], [], [With password in message])
   fi])

AC_ARG_ENABLE(eumetsat,
   [  --enable-eumetsat               enable eumetsat header support],
   [if test x$enableval = xyes; then
      AC_DEFINE([WITH_EUMETSAT_HEADERS], [], [With eumetsat header support])
      AM_CONDITIONAL(WITH_MISC_LIB, true)
   else
      AM_CONDITIONAL(WITH_MISC_LIB, false)
   fi],
   AM_CONDITIONAL(WITH_MISC_LIB, true))

AC_ARG_ENABLE([ssl],
   [  --disable-ssl                    disable TLS/SSL support],
   [if test x$enableval = xyes
    then
       if test x$enableval != xyes
       then
          ssl_all_roots=$enableval
       else
          ssl_all_roots="/usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/local /usr /opt"
       fi
       with_ssl="yes"
    else
       with_ssl="no"
    fi],
   [if test x$enableval != xyes
    then
       ssl_all_roots=$enableval
    else
       ssl_all_roots="/usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/local /usr /opt"
    fi
    with_ssl="yes"])

AC_ARG_WITH(ssl-includes,
   [  --with-ssl-includes=DIR         location where TLS/SSL includes are],
   ssl_include_dir="$withval")
AC_ARG_WITH(ssl-libraries,
   [  --with-ssl-libraries=DIR        location where TLS/SSL libraries are],
   ssl_lib_dir="$withval")

AC_ARG_ENABLE(editres,
   [  --enable-editres                enable editres support],
   [if test x$enableval = xyes; then
      AC_DEFINE([WITH_EDITRES], [], [With editres support])
   fi])

AC_ARG_ENABLE(efence,
   [  --enable-efence                 enable debugging with Electric Fence],
   [ac_cv_efence="$enableval"],
   [ac_cv_efence="no"])

AC_ARG_ENABLE(systemd,
   [  --enable-systemd                enable systemd support],
   [ac_cv_systemd="$enableval"],
   [ac_cv_systemd="no"])

dnl **** Check if we should build with GUI and with which widget set.
AC_ARG_WITH(gui,
   [  --with-gui=GUI          Where GUI is either motif (default), gtk or none],
   if test "x$with_gui" = xmotif || test "x$with_gui" = xgtk ; then
      afd_gui="$with_gui"
   else
      afd_gui=
   fi ,
   afd_gui="motif")
dnl **** Check if we want to compile in the XOR key.
AC_ARG_WITH(xor-key,
            AS_HELP_STRING([--with-xor-key=KEY],
                           [Where KEY can be any ascii string]),
            AC_DEFINE_UNQUOTED([XOR_KEY], ["$with_xor_key"], [With compiled in XOR key]),
            with_xor_key="")


dnl **** check if we want to compile afd_mon only
compile_afd_mon_only=false
AC_ARG_ENABLE(compile_afd_mon_only,
   [  --enable-compile_afd_mon_only   enable compiling of afd_mon only],
   [case "${enableval}" in
      yes) compile_afd_mon_only=true
           AC_DEFINE([WITH_AFD_MON], [], [With afd_mon support]) ;;
      no)  compile_afd_mon_only=false;;
      *) AC_MSG_ERROR(bad value ${enableval} for --enable-compile_afd_mon_only) ;;
   esac],[compile_afd_mon_only=false])
AM_CONDITIONAL(WITH_AFD_MON_ONLY, $compile_afd_mon_only)

dnl **** check if we want to use afd_mon
afd_mon=false
AC_ARG_ENABLE(afd_mon,
   [  --enable-afd_mon                enable afd_mon support],
   [case "${enableval}" in
      yes) afd_mon=true
           AC_DEFINE([WITH_AFD_MON], [], [With afd_mon support]) ;;
      no)  afd_mon=false;;
      *) AC_MSG_ERROR(bad value ${enableval} for --enable-afd_mon) ;;
   esac
   ac_cv_afd_mon="$enableval"],[afd_mon=false;ac_cv_afd_mon="no"])
AM_CONDITIONAL(WITH_AFD_MON, $afd_mon)
if test "$afd_mon" = "false"
then
   if test "$compile_afd_mon_only" = "true"
   then
      WITH_AFD_MON_SPEC=1
   else
      WITH_AFD_MON_SPEC=0
   fi
else
   WITH_AFD_MON_SPEC=1
fi
AC_SUBST(WITH_AFD_MON_SPEC)

dnl **** check if user wants to compile with debugging support
AC_ARG_ENABLE(compiler_debug,
   [  --enable-compiler-debug         turn on debugging],
   [case "${enableval}" in
     yes) compiler_debug=true ;;
     no)  compiler_debug=false;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-compiler-debug) ;;
   esac],[compiler_debug=false])

dnl **** check if user wants to compile with compiler warnings
AC_ARG_ENABLE(compiler_warn,
   [  --enable-compiler-warn          turn on compiler warnings],
   [case "${enableval}" in
     yes) compiler_warn=true ;;
     no)  compiler_warn=false;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-compiler-warn) ;;
   esac],[compiler_warn=false])

dnl **** check if user wants to compile with optimization support
AC_ARG_ENABLE(compiler_optimizations,
   [  --enable-compiler-optimizations Enable optimization of the binaries],
   [case "$enableval" in
     yes) compiler_optimizations=true ;;
     no)  compiler_optimizations=false;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-compiler-optimizations) ;;
   esac],[compiler_optimizations=false])

dnl **** check if user wants to compile with security support
AC_ARG_ENABLE(compiler_security,
   [  --enable-compiler-security      Enable compiler security options],
   [case "$enableval" in
     yes) compiler_security=true ;;
     no)  compiler_security=false;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-compiler-security) ;;
   esac],[compiler_security=false])

dnl **** check if user wants to compile with address sanitizer
AC_ARG_ENABLE(compiler_address_sanitizer,
   [  --enable-compiler-address-sanitizer Enable compiler address sanitizer],
   [case "$enableval" in
     yes) compiler_address_sanitizer=true ;;
     no)  compiler_address_sanitizer=false;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-compiler-address-sanitizer) ;;
   esac],[compiler_address_sanitizer=false])

dnl **** check if user wants to compile with analyzer
AC_ARG_ENABLE(compiler_analyzer,
   [  --enable-compiler-analyzer          Enable compiler analyzer],
   [case "$enableval" in
     yes) compiler_analyzer=true ;;
     no)  compiler_analyzer=false;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-compiler-analyzer) ;;
   esac],[compiler_analyzer=false])

dnl **** check if user wants to compile with profiling information
AC_ARG_ENABLE(compiler_profiling,
   [  --enable-compiler-profiling     Enable compiler profiling support],
   [case "$enableval" in
     yes) compiler_profiling=true ;;
     no)  compiler_profiling=false;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-compiler-profiling) ;;
   esac],[compiler_profiling=false])

dnl **** check if user wants to use fortify option of glibc/gcc
AC_ARG_ENABLE(use_fortify,
   [  --enable-use_fortify            Enable glibc/gcc fortify option],
   [case "$enableval" in
     yes) use_fortify=true ;;
     no)  use_fortify=false;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-use_fortify) ;;
   esac],[use_fortify=false])

dnl **** check if user wants to compile with 64-bit support
AC_ARG_ENABLE(compile_64_bit,
   [  --enable-compile-64-bit         Enable compiling 64-bit binaries],
   [case "$enableval" in
     yes) compile_64_bit=true ;;
     no)  compile_64_bit=false;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-compile-64-bit) ;;
   esac],[compile_64_bit=false])

dnl **** Enable/disable distro-style RPMs (taken from heartbeat)
AC_ARG_ENABLE([distro-rpm],                                                    
   [  --enable-distro-rpm             Enable distro-style RPMs [default=no]],
   [], [enable_distro_rpm=no])
case $enable_distro_rpm in
   yes) DISTRO_STYLE_RPMS=1;;
   *)   DISTRO_STYLE_RPMS=0;;
esac                         
AC_SUBST(DISTRO_STYLE_RPMS)

dnl
dnl *** some numeric configurable values
dnl
AC_ARG_WITH(max-no-of-parallel-jobs,
            AS_HELP_STRING([--with-max-no-of-parallel-jobs=N],
                           [max number of parallel transfers per host (default=5)]),
            [],
            if test "$ac_cv_with_afdbench_settings" = "yes"
            then
               with_max_no_of_parallel_jobs=10
            else
               with_max_no_of_parallel_jobs=5
            fi)
AC_DEFINE_UNQUOTED([MAX_NO_PARALLEL_JOBS], [$with_max_no_of_parallel_jobs], [
  The value of MAX_NO_PARALLEL_JOBS determines the number of parallel
  transfers per host. The default is 5, which should be enough in most
  cases. However, on some long distance lines or lines with high latencies
  it might be necessary to increase this value.])
AC_ARG_WITH(default-no-of-parallel-jobs,
            AS_HELP_STRING([--with-default-no-of-parallel-jobs=N],
                           [default number of parallel transfers per host (default=2)]),
            , with_default_no_of_parallel_jobs=2)
AC_DEFINE_UNQUOTED([DEFAULT_NO_PARALLEL_JOBS], [$with_default_no_of_parallel_jobs], [
  The value of DEFAULT_NO_PARALLEL_JOBS determines the default number of
  parallel transfers per host. The default is 2.])
AC_ARG_WITH(default-archive-unit,
            AS_HELP_STRING([--with-default-archive-unit=N],
                           [default archive unit in seconds (default=86400)]),
            [],
            with_default_archive_unit=86400)
AC_DEFINE_UNQUOTED([DEFAULT_ARCHIVE_UNIT], [$with_default_archive_unit], [
  The value of DEFAULT_ARCHIVE_UNIT determines the default archive unit
  in seconds. Best is to leave this at it default for one day (86400 seconds).])

AC_ARG_WITH(max-hostname-length,
            AS_HELP_STRING([--with-max-hostname-length=N],
                           [maximum host alias name length (default=8)]),
            , with_max_hostname_length=8)
AC_DEFINE_UNQUOTED([MAX_HOSTNAME_LENGTH], [$with_max_hostname_length], [
  The value of MAX_HOSTNAME_LENGTH determines how long the host alias
  name length in the afd_ctrl dialog may be.])
AC_ARG_WITH(max-dir-alias-length,
            AS_HELP_STRING([--with-max-dir-alias-length=N],
                           [maximum directory alias name length (default=10)]),
            , with_max_dir_alias_length=10)
AC_DEFINE_UNQUOTED([MAX_DIR_ALIAS_LENGTH], [$with_max_dir_alias_length], [
  The value of MAX_DIR_ALIAS_LENGTH determines how long the directory alias
  name length in the dir_ctrl dialog may be.])
AC_ARG_WITH(max-afdname-length,
            AS_HELP_STRING([--with-max-afdname-length=N],
                           [maximum AFD alias name length (default=12)]),
            , with_max_afdname_length=12)
AC_DEFINE_UNQUOTED([MAX_AFDNAME_LENGTH], [$with_max_afdname_length], [
  The value of MAX_AFDNAME_LENGTH determines how long the AFD alias
  name length in the mon_ctrl dialog may be.])
AC_ARG_WITH(max-real-hostname-length,
            AS_HELP_STRING([--with-max-real-hostname-length=N],
                           [maximum real hostname length (default=40)]),
            , with_max_real_hostname_length=40)
AC_DEFINE_UNQUOTED([MAX_REAL_HOSTNAME_LENGTH], [$with_max_real_hostname_length], [
  The value of MAX_REAL_HOSTNAME_LENGTH determines how long the real
  hostname or its IP number may be.])
AC_ARG_WITH(max-recipient-length,
            AS_HELP_STRING([--with-max-recipient-length=N],
                           [maximum recipient length (default=256)]),
            , with_max_recipient_length=256)
AC_DEFINE_UNQUOTED([MAX_RECIPIENT_LENGTH], [$with_max_recipient_length], [
  The value of MAX_RECIPIENT_LENGTH sets the maximum length of a recipient.])



dnl **** check where init/rc directory is, taken from heartbeat
AC_MSG_CHECKING(which init (rc) directory to use)
INITDIR=""
for initdir in /etc/init.d /etc/rc.d/init.d /sbin/init.d        \
         /usr/local/etc/rc.d /etc/rc.d
do
   if test -d $initdir
   then
      INITDIR=$initdir
      AC_MSG_RESULT($INITDIR);
      break
   fi
done
AC_ARG_WITH(initdir,
   [  --with-initdir=DIR      directroy for init (rc) scripts [${INITDIR}]],
   [ if test x"$withval" = xprefix; then INITDIR=${prefix}; else
         INITDIR="$withval"; fi ])
AC_SUBST(INITDIR)
AC_MSG_CHECKING(which sysconfig directory to use)
SYSCONFIGDIR=""
for sysconfigdir in /etc/sysconfig ${prefix}/etc
do
   if test -d $sysconfigdir
   then
      SYSCONFIGDIR=$sysconfigdir
      AC_MSG_RESULT($SYSCONFIGDIR);
      break
   fi
done
AC_ARG_WITH(sysconfigdir,
   [  --with-sysconfigdir=DIR directory for afd users initialized by init script [${SYSCONFIGDIR}]],
   [ if test x"$withval" = xprefix; then SYSCONFIGDIR=${prefix}; else
         SYSCONFIGDIR="$withval"; fi ])
AC_SUBST(SYSCONFIGDIR)
cleaned_configure_args=""
for j in ${ac_configure_args}
do
   case $j in
      *--libdir=*) ;;
      *)           cleaned_configure_args="$cleaned_configure_args $j" ;;
   esac
done
AC_SUBST(cleaned_configure_args)

dnl **** check if datadir, docdir and mandir are set, needed for spec file
dnl **** This was taken from the heartbeat package.
prefix_orig="$prefix"
prefix=`eval echo "$prefix"`
case $prefix in
  NONE) prefix=/usr;;
esac                       
var() {
  case $1 in
    *'${'*)     res=`eval echo "$1"`;;
    *)          res="$1";;
  esac
  case "$res" in
    ""|NONE)    echo "$2";;
    *)          echo "$res";;
  esac
}
exec_prefix=`var "$exec_prefix" "$prefix"`
bindir=`var "$bindir" "$exec_prefix/bin"`
sbindir=`var "$sbindir" "$exec_prefix/sbin"`
datarootdir=`var "$datarootdir" "$prefix/share"`
datadir=`var "$datadir" "$prefix/share"`
sysconfdir=`var "$sysconfdir" "$prefix/etc"`
mandir=`var "$mandir" "$exec_prefix/man"`
docdir=${datadir}/doc/afd


dnl **** check under which user to install
BINARY_OWNER="root"
AC_ARG_WITH(rpm-binary-owner,
            [  --with-rpm-binary-owner=BINARY_OWNER
                                  BINARY_OWNER who owns the RPM binaries. [default=root]  ],
            [ BINARY_OWNER="$withval" ],
            [ BINARY_OWNER="root" ],
           )
AC_SUBST(BINARY_OWNER)
AC_ARG_WITH(rpm-binary-group,
            [  --with-rpm-binary-group=BINARY_GROUP
                                  BINARY_GROUP who owns the RPM binaries. [default=BINARY_OWNER]  ],
            [ BINARY_GROUP="$withval" ],
            [ BINARY_GROUP=$BINARY_OWNER ],
           )
AC_SUBST(BINARY_GROUP)

dnl Check for host type
AC_CANONICAL_HOST

dnl Checks for programs.
AC_PROG_INSTALL
AC_PROG_CC
AC_PROG_RANLIB

DISTRO=""
DISTRO_VERSION=0
case "${host_os}"
in
   aix*)     if test "${compile_64_bit}" = "true"
             then
                if test "x${GCC}" = "xyes"
                then
                   OS_FLAG="-m64"
                else
                   OS_FLAG="-q64"
                fi
             else
                OS_FLAG=""
             fi
             AM_CONDITIONAL(IS_LINUX, false)
             ;;
   bsdi*)    OS_FLAG=""
             AM_CONDITIONAL(IS_LINUX, false)
             ;;
   cygwin*)  OS_FLAG="-D_CYGWIN"
             AM_CONDITIONAL(IS_LINUX, false)
             ;;
   freebsd*) OS_FLAG="-DFREEBSD"
             AM_CONDITIONAL(IS_LINUX, false)
             ;;
   hpux*)    if test "${compile_64_bit}" = "true"
             then
                if test "x${GCC}" = "xyes"
                then
                   OS_FLAG="-mlp64 -D_HPUX"
                else
                   OS_FLAG="+DD64 -D_HPUX"
                fi
             else
                OS_FLAG="-D_HPUX"
             fi
             if test "${compiler_debug}" = "false"
             then
                CFLAGS=`echo " $CFLAGS " | sed 's/ -g / /g'`
             fi
             AM_CONDITIONAL(IS_LINUX, false)
             ;;
   irix*)    if test "x$afd_map_lib_dir" = "x"
             then
                if test "${compile_64_bit}" = "true"
                then
                   if test "x${GCC}" = "xyes"
                   then
                      OS_FLAG="-m64 -DMMAP_KILLER -DIRIX"
                   else
                      OS_FLAG="-64 -DMMAP_KILLER -DIRIX"
                   fi
                else
                   OS_FLAG="-DMMAP_KILLER -DIRIX"
                fi
             else
                dnl **** MAP libs are only available in 32 Bit
                OS_FLAG="-32 -DMMAP_KILLER -DIRIX"
             fi
             AM_CONDITIONAL(IS_LINUX, false)
             ;;
   linux*)   if test "${compile_64_bit}" = "true"
             then
                OS_FLAG="-m64 -DLINUX"
             else
                OS_FLAG="-DLINUX"
             fi
             AM_CONDITIONAL(IS_LINUX, true)

             dnl **** Try determine distribution.
             if test -f /etc/os-release
             then
                DISTRO=$(grep "^ID=" /etc/os-release | cut -d= -f2 | tr -d \")
                if test "$DISTRO" = "centos"
                then
                   DISTRO="redhat"
                else
                   if test "$DISTRO" = "scientific"
                   then
                      DISTRO="redhat"
                   fi
                fi
                DISTRO_VERSION=$(grep "^VERSION_ID=" /etc/os-release | cut -d= -f2 | tr -dc '0-9.' | cut -d'.' -f1)
             fi
             if test "$DISTRO" = ""
             then
                if test -f /etc/fedora-release
                then
                   DISTRO="fedora"
                   DISTRO_VERSION=`head -1 /etc/fedora-release | tr -dc '0-9.' | cut -d'.' -f1`
                else
                   if test -f /etc/redhat-release
                   then
                      DISTRO="redhat"
                      DISTRO_VERSION=`head -1 /etc/redhat-release | tr -dc '0-9.' | cut -d'.' -f1`
                   else
                      if test -f /etc/SuSE-release
                      then
                         head -1 /etc/SuSE-release | grep -i enterprise > /dev/null
                         if test $? -eq 0
                         then
                            DISTRO="sles"
                            DISTRO_VERSION=`grep VERSION /etc/SuSE-release | cut -d " " -f 3`
                         else
                            head -1 /etc/SuSE-release | grep -i open > /dev/null
                            if test $? -eq 0
                            then
                               DISTRO="opensuse"
                               DISTRO_VERSION=`grep VERSION /etc/SuSE-release | cut -d " " -f 3 | cut -d "." -f 1`
                            else
                               DISTRO="suse"
                            fi
                         fi
                      else
                         if test -f /etc/slackware-release
                         then
                            DISTRO="slackware"
                         else
                            if test -f /etc/debian_release
                            then
                               DISTRO="debian"
                            else
                               if test -f /etc/gentoo-release
                               then
                                  DISTRO="gentoo"
                               else
                                  if test -f /etc/yellowdog-release
                                  then
                                     DISTRO="yellowdog"
                                  else
                                     DISTRO="unknown"
                                  fi
                               fi
                            fi
                         fi
                      fi
                   fi
                fi
             fi
             ;;
   netbsd*)  OS_FLAG=""
             AM_CONDITIONAL(IS_LINUX, false)
             ;;
   sco*)     OS_FLAG="-D_SCO"
             AM_CONDITIONAL(IS_LINUX, false)
             ;;
   solaris*) OS_FLAG="-D_SUN"
             AM_CONDITIONAL(IS_LINUX, false)
             ;;
   sunos*)   OS_FLAG="-D_SUN"
             AM_CONDITIONAL(IS_LINUX, false)
             ;;
   *)        OS_FLAG=""
             AM_CONDITIONAL(IS_LINUX, false)
             ;;
esac

if test "${host}" = "hppa1.1-stratus-sysv4"
then
   OS_FLAG="-DFTX"
   BROKEN_X_EXTRA_LIBS="-lsocket -lnsl -lgen"
else
   BROKEN_X_EXTRA_LIBS="";
fi

if test "${compiler_optimizations}" = "true"
then
   if test "x${GCC}" = "xyes"
   then
     AX_GCC_ARCHFLAG([no], [OPTIMIZE_FLAG="-O2 ${ax_cv_gcc_archflag}"], [OPTIMIZE_FLAG="-O2"])
   else
     case "${host}" in
        alpha*-dec-osf4.*)       OPTIMIZE_FLAG="-O2" ;;
        hppa*-hp-hpux*)          OPTIMIZE_FLAG="-O" ;;
        mips-sgi-irix6.[[4-9]]*) OPTIMIZE_FLAG="-O2 -OPT:Olimit=0"
                                 CXXFLAGS="$CXXFLAGS -LANG:std"
                                 CXXCPP="$CXXCPP -LANG:std" ;;
        mips-sgi-irix*)          OPTIMIZE_FLAG="-O2 -Olimit 3500" ;;
        rs6000-ibm-aix*)         OPTIMIZE_FLAG="-O2" ;;
        *)                       OPTIMIZE_FLAG="-O" ;;
     esac
   fi
else
   OPTIMIZE_FLAG=""
   if test "x${GCC}" != "xyes"
   then
      case "${host}" in
        mips-sgi-irix6.[[4-9]]*) CXXFLAGS="$CXXFLAGS -LANG:std"
                                 CXXCPP="$CXXCPP -LANG:std" ;;
      esac
   fi
fi

if test "${compiler_security}" = "true"
then
   if test "x${GCC}" = "xyes"
   then
      AX_CHECK_COMPILE_FLAG("-fstack-protector", [SECURITY_FLAG="${SECURITY_FLAG} -fstack-protector"], [SECURITY_FLAG="${SECURITY_FLAG}"])
      AX_CHECK_COMPILE_FLAG("-pie", [SECURITY_FLAG="${SECURITY_FLAG} -pie"], [SECURITY_FLAG="${SECURITY_FLAG}"])
      AX_CHECK_COMPILE_FLAG("-fPIC", [SECURITY_FLAG="${SECURITY_FLAG} -fPIC"], [SECURITY_FLAG="${SECURITY_FLAG}"])
   fi
fi

if test "${compiler_address_sanitizer}" = "true"
then
   if test "x${GCC}" = "xyes"
   then
      AX_CHECK_COMPILE_FLAG("-fsanitize=address", [SANITIZER_FLAG="${SANITIZER_FLAG} -fsanitize=address"], [SANITIZER_FLAG="${SANITIZER_FLAG}"])
   fi
fi

if test "${compiler_analyzer}" = "true"
then
   if test "x${GCC}" = "xyes"
   then
      AX_CHECK_COMPILE_FLAG("-fanalyzer", [ANALYZER_FLAG="${ANALYZER_FLAG} -fanalyzer"], [ANALYZER_FLAG="${ANALYZER_FLAG}"])
   fi
fi

if test "${compiler_debug}" = "true"
then
   if test "x${GCC}" = "xyes"
   then
     DEBUG_FLAG="-ggdb3"
   else
     case "${host}" in
        alpha*-dec-osf4.*)       DEBUG_FLAG="-g3" ;;
        hppa*-hp-hpux*)          DEBUG_FLAG="-g" ;;
        mips-sgi-irix6.[[4-9]]*) DEBUG_FLAG="-g3" ;;
        mips-sgi-irix*)          DEBUG_FLAG="-g" ;;
        rs6000-ibm-aix*)         DEBUG_FLAG="-g" ;;
        *)                       DEBUG_FLAG="-g" ;;
     esac
   fi
else
   DEBUG_FLAG=""
   if test "x${GCC}" = "xyes"
   then
      if test "${use_fortify}" = "true"
      then
         AX_CHECK_COMPILE_FLAG("-D_FORTIFY_SOURCE=2", [SECURITY_FLAG="-D_FORTIFY_SOURCE=2"], [SECURITY_FLAG=""])
      fi
   fi
fi

if test "x${GCC}" = "xyes"
then
   AX_CHECK_COMPILE_FLAG("-fPIC", [SECURITY_FLAG="${SECURITY_FLAG} -fPIC"], [SECURITY_FLAG="${SECURITY_FLAG}"])
fi

if test "${compiler_optimizations}" = "true" || test "${compiler_debug}" = "true"
then
   CFLAGS="$DEBUG_FLAG $OPTIMIZE_FLAG $OS_FLAG"
else
   CFLAGS="$CFLAGS $OS_FLAG"
fi
if test "${compiler_security}" = "true"
then
   CFLAGS="$CFLAGS $SECURITY_FLAG"
fi
if test "${compiler_address_sanitizer}" = "true"
then
   CFLAGS="$CFLAGS $SANITIZER_FLAG"
fi
if test "${compiler_analyzer}" = "true"
then
   CFLAGS="$CFLAGS $ANALYZER_FLAG"
fi
if test "${compiler_profiling}" = "true"
then
   CFLAGS="$CFLAGS -pg"
fi

dnl Check if we should do an automatic test if systemd exist
if test "${ac_cv_systemd}" = "yes"
then
   if test -d /run/systemd/system
   then
      use_systemd="yes"
   else
      use_systemd="no"
   fi
else
   use_systemd="no"
fi
AC_MSG_CHECKING([whether we build with systemd support])
if test "${use_systemd}" = "yes"
then
   PKG_CHECK_MODULES([LIBSYSTEMD], [libsystemd],
                     [AC_DEFINE([WITH_SYSTEMD], [], [Building with systemd support])
                      AC_MSG_RESULT(Building with systemd support)],
                     [AC_MSG_RESULT(Building without systemd support)
                     use_systemd="no"])
   if test "${use_systemd}" = "yes"
   then
      if test "$REQUIRES" = ""
      then
         REQUIRES="systemd"
      else
         REQUIRES="`echo $REQUIRES`, systemd"
      fi
      if test "$BUILD_REQUIRES" = ""
      then
         BUILD_REQUIRES="systemd-devel"
      else
         BUILD_REQUIRES="`echo $BUILD_REQUIRES`, systemd-devel"
      fi
      WITH_SYSTMD=1
      AM_CONDITIONAL(WITH_SYSTEMD, true)
   else
      case "$DISTRO" in
         debian)
            echo "Unable to find systemd development package. Install libsystemd-dev package."
            ;;
         fedora)
            echo "Unable to find systemd development package. Install systemd-devel package."
            ;;
         redhat)
            echo "Unable to find systemd development package. Install systemd-devel package."
            ;;
         ubuntu)
            echo "Unable to find systemd development package. Install libsystemd-dev package."
            ;;
         *)
            echo "Unable to find systemd development package."
            ;;
      esac
      exit 1
   fi
else
   AM_CONDITIONAL(WITH_SYSTEMD, false)
   AC_MSG_RESULT(no)
   WITH_SYSTMD=0
fi
AC_SUBST(WITH_SYSTMD)


dnl Test if we can compile in support for CPU CRC calculation
if test "$ac_cv_allow_sse42" = "yes"
then
   if test "x${GCC}" = "xyes"
   then
      AX_CHECK_COMPILE_FLAG("-msse4.2",
                            [CRCFLAGS="-msse4.2"],
                            [CRCFLAGS=""])
      if test "x$CRCFLAGS" != "x"
      then
         SAVE_CFLAGS="$CFLAGS"
         CFLAGS="$CFLAGS $CRCFLAGS"

         AC_MSG_CHECKING([whether the compiler has the __get_cpuid intrinsic])
         AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <cpuid.h>]], [[unsigned int eax, ebx, ecx, edx, i;
                         __get_cpuid(1, &eax, &ebx, &ecx, &edx);
                         i = ecx & bit_SSE4_2; return 0;]])],[HAVE_GET_CPUID=yes],[HAVE_GET_CPUID=no])
         if test "x$HAVE_GET_CPUID" = "xyes"
         then
            AC_MSG_RESULT(yes)

            AC_MSG_CHECKING([whether the compiler has the CRC32 intrinsic])
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdint.h>]], [[unsigned int crc;
                            char *mem;
                            crc = __builtin_ia32_crc32si(crc, *(uint32_t *)mem);
                            crc = __builtin_ia32_crc32qi(crc, *mem++);
                            crc = __builtin_ia32_crc32hi(crc, *(uint16_t *)mem);
                            return 0;]])],[HAVE_CPU_CRC32=yes],[HAVE_CPU_CRC32=no])
            if test "x$HAVE_CPU_CRC32" = "xyes"
            then
               AC_MSG_RESULT(yes)
               AC_DEFINE(HAVE_HW_CRC32, 1, [Adding support for SSE4.2 (CRC32) instructions])
            else
               AC_MSG_RESULT(no)
               CFLAGS="$SAVE_CFLAGS"
            fi
         else
            AC_MSG_RESULT(no)
            CFLAGS="$SAVE_CFLAGS"
         fi
      fi
   fi
fi

if test "${compiler_warn}" = "true"
then
   VL_PROG_CC_WARNINGS
   if test "x${GCC}" = "xyes"
   then
      CFLAGS="$CFLAGS -Wno-format-y2k -Wno-restrict"
   fi
fi

dnl Some programs with suid
if test "$ac_cv_setuid_progs" = yes
then
   AC_DEFINE([WITH_SETUID], [], [Some programs with setuid])
fi

dnl Large file support
if test "$ac_cv_largefile" = yes
then
   AC_SYS_LARGEFILE
fi

AC_CHECK_LIB(m, sin, [AFD_MATH_LIB="-lm"], [AFD_MATH_LIB=""],)
AC_SUBST(AFD_MATH_LIB)
ETR_SOCKET_NSL

if test "x${with_ssl}" = "xyes"
then
   AC_MSG_CHECKING(for TLS/SSL)
   if test "x$ssl_include_dir" = "x" || test "x$ssl_lib_dir" = "x"
   then
      ssl_dir=""

      dnl
      dnl Updated RedHat 8 + 9 need the extra pkg-config check!
      dnl
      AC_PATH_PROG(PKGCONFIG, pkg-config)dnl
      if test -n "$PKGCONFIG"
      then
         $PKGCONFIG openssl
         ret=$?
      else
         ret=1
      fi
      if test $ret -eq 0
      then
         AFD_SSL_INCLUDES="`$PKGCONFIG --cflags-only-I openssl`"
         AFD_SSL_LIBS="`$PKGCONFIG --libs openssl`"
      else
         for dir in $ssl_all_roots
         do
            if test -f "$dir/include/openssl/ssl.h" || test -f "$dir/include/ssl.h"
            then
               ssl_dir="$dir"
               break
            fi
         done
         if test "x$ssl_include_dir" = "x"
         then
            AFD_SSL_INCLUDES="-I$ssl_dir/include"
         else
            AFD_SSL_INCLUDES="-I$ssl_include_dir"
         fi
         if test "x$ssl_lib_dir" = "x"
         then
            AFD_SSL_LIBS="-L$ssl_dir -lssl -lcrypto"
         else
            AFD_SSL_LIBS="-L$ssl_lib_dir -lssl -lcrypto"
         fi
      fi
   else
      AFD_SSL_INCLUDES="-I$ssl_include_dir"
      AFD_SSL_LIBS="-L$ssl_lib_dir -lssl -lcrypto"
   fi
   if test "x$AFD_SSL_INCLUDES" != "x" || test $ret -eq 0
   then
      AC_MSG_RESULT(yes)
      AC_SUBST(AFD_SSL_INCLUDES)
      AC_SUBST(AFD_SSL_LIBS)
      AC_DEFINE([WITH_SSL], [], [with TLS/SSL support])

      dnl Version check taken from openssh package and modified it.
      savedLIBS="$LIBS"
      savedCPPFLAGS="$CPPFLAGS"
      LIBS="$LIBS $AFD_SSL_LIBS"
      CPPFLAGS="$CPPFLAGS $AFD_SSL_INCLUDES"
      AC_MSG_CHECKING([correct OpenSSL version])
      AC_RUN_IFELSE([AC_LANG_SOURCE([[
                    #include <stdio.h>
                    #include <string.h>
                    #include <openssl/opensslv.h>
                    #include <openssl/crypto.h>
                    #include <openssl/ssl.h>
                    #define DATA "conftest.ssllibver"
                    int main(void)
                    {
                       FILE *fd;

                       if ((fd = fopen(DATA,"w")) != NULL)
                       {
                          if (fprintf(fd ,"%x (%s)\n", SSLeay(), SSLeay_version(SSLEAY_VERSION)) >= 0)
                          {
                             SSL_CTX *ssl_ctx;

                             SSLeay_add_ssl_algorithms();
                             ssl_ctx=(SSL_CTX *)SSL_CTX_new(SSLv23_method());
                             SSL_CTX_set_options(ssl_ctx, SSL_OP_ALL);
                             SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);
                             exit(0);
                          }
                       }
                       exit(1);
                    }]])],[ssl_library_ver=`cat conftest.ssllibver`
                  AC_MSG_RESULT(yes $ssl_library_ver)],[AC_MSG_RESULT(incorrect not found)
                  AC_MSG_ERROR(incorect OpenSSL library or not found.)],[])
      LIBS="$savedLIBS"
      CPPFLAGS="$savedCPPFLAGS"
      if test "$REQUIRES" = ""
      then
         REQUIRES="openssl"
      else
         REQUIRES="`echo $REQUIRES`, openssl"
      fi
      if test "$BUILD_REQUIRES" = ""
      then
         BUILD_REQUIRES="openssl-devel"
      else
         BUILD_REQUIRES="`echo $BUILD_REQUIRES`, openssl-devel"
      fi
   else
      AC_MSG_RESULT(no)
   fi
fi

no_x=""
AC_PATH_XTRA
X_EXTRA_LIBS="$X_EXTRA_LIBS $BROKEN_X_EXTRA_LIBS"
AC_MSG_CHECKING(whether Motif support is requested)
if test "x$afd_gui" = "xmotif"
then
   compile_with_motif=true
   AC_MSG_RESULT(yes)
   if test "$no_x" = "yes"
   then
      echo
      echo "X11 libraries or header files could not be found.  Please make"
      echo "sure the X11 development package is installed on your system."
      echo "If it is installed, try setting the include and library"
      echo "paths with the --x-include and --x-libraries options of configure."
      echo "Else you can compile AFD without any GUI by setting"
      echo "--with-gui=none"
      echo
      echo "Aborting."
      echo
      exit 1
   fi
   AC_FIND_LIBXP
   AC_FIND_MOTIF
   if test "$with_motif" = "no"
   then
      echo "Unable to find Motif."
      exit 1
   fi
   AC_CHECK_MOTIF_VERSION
   AC_CHECK_LESSTIF_VERSION
   AC_MSG_CHECKING(whether Motif requires XPM)
   case ${motif_library_ver}
   in
      2.*)
         AC_MSG_RESULT(yes)
         ;;
      *)
         AC_MSG_RESULT(no)
         ;;
   esac
   AC_FIND_XPM
   if test x$with_xpm = xno
   then
      echo "Unable to locate XPM, trying to continue."
      AM_CONDITIONAL(WITH_XPM, false)
      AFD_MOTIF_INCLUDES="$include_motif $X_CFLAGS"
      AFD_MOTIF_LIBS="$link_motif $X_LIBS -lXt $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"
   else
      AM_CONDITIONAL(WITH_XPM, true)
      AFD_MOTIF_INCLUDES="$include_motif $X_CFLAGS $XPM_CFLAGS"
      AFD_MOTIF_LIBS="$link_motif $X_LIBS -lXt $X_PRE_LIBS -lX11 $X_EXTRA_LIBS $XPM_LIBS"
   fi
   AC_MSG_CHECKING(whether Motif requires -lXp)
   case ${motif_library_ver}
   in
      2.1)
         AC_MSG_RESULT(yes)
         AFD_MOTIF_LIBS="$AFD_MOTIF_LIBS $AFD_XP_LIB"
         ;;
      *)
         AC_MSG_RESULT(no)
         ;;
   esac
   case "${host_os}"
   in
      aix*) 
         AFD_XAW_LIBS="-lXaw -lXmu"
         AFD_XAW_INCLUDES=""
         ;;
      linux*)
         AFD_XAW_LIBS="-lXaw -lXmu"
         AFD_XAW_INCLUDES=""
         ;;
      hpux*)
         AFD_XAW_LIBS="-L/usr/contrib/X11R6/lib -lXaw -lXmu"
         AFD_XAW_INCLUDES="-I/usr/contrib/X11R6/include"
         ;;
      sysv4*)
         AFD_XAW_LIBS="-lXaw -lXmu -lXext"
         AFD_XAW_INCLUDES=""
         ;;
      *)
         AFD_XAW_LIBS="-lXaw -lXmu"
         AFD_XAW_INCLUDES=""
         ;;
   esac
   GUIS_STRING="Motif"
   if test "$DISTRO" = "redhat" && test $DISTRO_VERSION -gt 5
   then
      if test "$REQUIRES" = ""
      then
         REQUIRES="xorg-x11-fonts-misc"
      else
         REQUIRES="`echo $REQUIRES`, xorg-x11-fonts-misc"
      fi
   else
      if test "$DISTRO" = "fedora" && test $DISTRO_VERSION -gt 5
      then
         if test "$REQUIRES" = ""
         then
            REQUIRES="xorg-x11-fonts-misc"
         else
            REQUIRES="`echo $REQUIRES`, xorg-x11-fonts-misc"
         fi
      fi
   fi
   if test "$ac_cv_new_motif_name" = "motif"
   then
      if test "$DISTRO" = "redhat" && test $DISTRO_VERSION -lt 7
      then
         ac_cv_new_motif_name="openmotif"
      else
         if test "$DISTRO" = "fedora" && test $DISTRO_VERSION -lt 18
         then
            ac_cv_new_motif_name="openmotif"
         else
            if test "$DISTRO" = "sles" && test $DISTRO_VERSION -lt 12
            then
               ac_cv_new_motif_name="openmotif"
            fi
         fi
      fi
   fi

   if test "$REQUIRES" = ""
   then
      REQUIRES="$ac_cv_new_motif_name"
   else
      REQUIRES="`echo $REQUIRES`, $ac_cv_new_motif_name"
   fi
   if test "$BUILD_REQUIRES" = ""
   then
      BUILD_REQUIRES="${ac_cv_new_motif_name}-devel"
   else
      BUILD_REQUIRES="`echo $BUILD_REQUIRES`, ${ac_cv_new_motif_name}-devel"
   fi
   BUILD_REQUIRES_REDHAT="libXpm-devel, libXaw-devel"
   AC_SUBST(AFD_MOTIF_INCLUDES)
   AC_SUBST(AFD_MOTIF_LIBS)
   AC_SUBST(AFD_XAW_INCLUDES)
   AC_SUBST(AFD_XAW_LIBS)
else
   AM_CONDITIONAL(WITH_XPM, false)
   compile_with_motif=false
   AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(HAVE_MOTIF, $compile_with_motif)

if test "$BUILD_REQUIRES_REDHAT" = ""
then
   BUILD_REQUIRES_REDHAT="`echo $BUILD_REQUIRES`"
else
   BUILD_REQUIRES_REDHAT="`echo $BUILD_REQUIRES`, `echo $BUILD_REQUIRES_REDHAT`"
fi

AC_MSG_CHECKING(whether Gtk+ support is requested)
if test "x$afd_gui" = "xgtk"
then
   compile_with_gtk=true
   AC_MSG_RESULT(yes)
   if test "$no_x" = "yes"
   then
      echo
      echo "X11 libraries or header files could not be found.  Please make"
      echo "sure the X11 development package is installed on your system."
      echo "If it is installed, try setting the include and library"
      echo "paths with the --x-include and --x-libraries options of configure."
      echo "Else you can compile AFD without any GUI by setting"
      echo "--with-gui=none"
      echo
      echo "Aborting."
      echo
      exit 1
   fi
   AC_SUBST(AFD_GTK_CFLAGS)
   AC_SUBST(AFD_GTK_LIBS)
   if pkg-config --atleast-version=2.2 gtk+-2.0
   then
      AFD_GTK_CFLAGS=`pkg-config --cflags gtk+-2.0`
      AFD_GTK_LIBS=`pkg-config --libs gtk+-2.0`
   else
      AC_MSG_ERROR(gtk+-2.0 not found)
   fi
   GUIS_STRING="$GUIS_STRING Gtk+"
else
   AM_CONDITIONAL(WITH_XPM, true)
   compile_with_gtk=false
   AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(WITH_GTK, $compile_with_gtk)

if test "x$GUIS_STRING" = "x"
then
   GUIS_STRING="none"
fi

dnl Check if we have ncurses.
if test "$ac_cv_ncurses_ui" = "xyes"
then
MP_WITH_NCURSES
   if test "x$NCURSES_LIB" != "x"
   then
      AFD_NCURSES_LIB=$NCURSES_LIB
      AC_SUBST(AFD_NCURSES_LIB)
      AM_CONDITIONAL(WITH_NCURSES, true)
   else
      AM_CONDITIONAL(WITH_NCURSES, false)
   fi
else
   AM_CONDITIONAL(WITH_NCURSES, false)
fi

dnl Check if we want to build with Electric Fence
if test "$ac_cv_efence" = yes
then
   AC_CHECK_LIB(efence, malloc)
   AC_DEFINE([WITH_EFENCE], [], [With Electric fence support])
fi

dnl Check if we want to install some binaries with setuid flag
if test "$ac_cv_setuid_progs" = yes
then
   AM_CONDITIONAL(WITH_SETUID_PROGS, true)
else
   AM_CONDITIONAL(WITH_SETUID_PROGS, false)
fi

dnl Taken from heartbeat
AC_PATH_PROGS(RPM, rpmbuild)
if test x"${RPM}" = x""
then
    AC_PATH_PROGS(RPM, rpm)
fi

AC_PATH_PROGS(PING_CMD, ping)
AC_PATH_PROGS(TRACEROUTE_CMD, traceroute)
AC_PATH_PROGS(GZIP_CMD, gzip)

dnl We don't want it to complain about --datarootdir in afd.spec.in
AC_DEFUN([AC_DATAROOTDIR_CHECKED])

dnl The following are needed for generating the spec file.
AC_SUBST(ac_configure_args)
AC_SUBST(REQUIRES)
AC_SUBST(BUILD_REQUIRES)
AC_SUBST(BUILD_REQUIRES_REDHAT)


AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h limits.h sys/file.h sys/time.h unistd.h errno.h \
                 sys/socket.h netdb.h arpa/inet.h arpa/telnet.h stdint.h)

dnl
dnl FreeBSD and Darwin requires sys/param.h and in.h to compile test
dnl netinet headers. This check was taken from the hearteat package.
dnl
AC_CHECK_HEADERS(sys/param.h)
AC_CHECK_HEADERS(netinet/in.h)
if test "$ac_cv_header_sys_param_h" -a "$ac_cv_header_netinet_in_h" = no
then
   AC_CHECK_HEADERS(netinet/ip.h)
   AC_CHECK_HEADERS(netinet/in_systm.h)
else
   AC_CHECK_HEADERS(netinet/in_systm.h,[],[],
                    [#include <sys/param.h>
                     #include <netinet/in.h>])
   if test "$ac_cv_header_in_systm_h" = no
   then
      AC_CHECK_HEADERS(netinet/ip.h,[],[],
                       [#include <sys/param.h>
                        #include <netinet/in.h>])
   else
      AC_CHECK_HEADERS(netinet/ip.h,[],[],
                       [#include <sys/param.h>
                        #include <netinet/in_systm.h>
                        #include <netinet/in.h>])
   fi
fi

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_CHECK_SIZEOF(off_t)
AC_CHECK_SIZEOF(pid_t)
AC_CHECK_SIZEOF(time_t)
AC_CHECK_SIZEOF(nlink_t)
AC_CHECK_SIZEOF(size_t)
AC_CHECK_SIZEOF(ssize_t)
AC_CHECK_SIZEOF(suseconds_t)
AC_CHECK_SIZEOF(ino_t)
AC_CHECK_SIZEOF(dev_t)
AC_CHECK_SIZEOF(uid_t)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF([long long])
AC_CHECK_SIZEOF(char)

dnl On irix some arguments which are usually socklen_t are int.
case "${host_os}"
in
   irix*)    AC_DEFINE([my_socklen_t], [int], [Set correct argument of getsockname and other functions])
             ;;
   hpux*)    if test "${compile_64_bit}" = "true"
             then
                AC_DEFINE([my_socklen_t], [int], [Set correct argument of getsockname and other functions])
             else
                TYPE_SOCKLEN_T
             fi
             ;;
   *)        TYPE_SOCKLEN_T
             ;;
esac

dnl Checks for library functions.
AC_FUNC_MEMCMP
AC_FUNC_STRFTIME
AC_FUNC_UTIME_NULL
AC_FUNC_VPRINTF

AC_CONFIG_LIBOBJ_DIR([src/init_afd/ports])
HW_FUNC_VSNPRINTF
HW_FUNC_SNPRINTF

dnl There is no "standard" pty allocation method. Every system is different.
dnl The following was taken from the xemacs package and modified for AFD.
AC_CHECK_FUNCS(getpt _getpty)
AC_CHECK_FUNC([openpty], [have_openpty=yes],
              [AC_CHECK_LIB([util], [openpty], [have_openpty=yes need_libutil=yes])])
if test "$have_openpty" = "yes"
then
   AC_DEFINE([HAVE_OPENPTY], [], [With openpty support])
   AC_CHECK_HEADERS(pty.h libutil.h util.h)
   if test "$need_libutil" = "yes"
   then
      AFD_UTIL_LIB="-lutil"
   else
      AFD_UTIL_LIB=""
   fi
else
   AFD_UTIL_LIB=""
fi
AC_SUBST(AFD_UTIL_LIB)
if test "x$ac_cv_allow_inotify" = "xyes"
then
   AC_CHECK_FUNC([inotify_init],
                 [AM_CONDITIONAL(WITH_INOTIFY_SUPPORT, true)
                  AC_DEFINE([WITH_INOTIFY], [], [With inotify support])],
                 [AM_CONDITIONAL(WITH_INOTIFY_SUPPORT, false)])
else
   AM_CONDITIONAL(WITH_INOTIFY_SUPPORT, false)
fi
AC_CHECK_FUNC([eaccess],
              [AM_CONDITIONAL(HAVE_EACCESS, true)
               AC_DEFINE([HAVE_EACCESS], [], [With eaccess support])],
              [AM_CONDITIONAL(HAVE_EACCESS, false)])
AC_CHECK_FUNC([wait4],
              [AM_CONDITIONAL(HAVE_WAIT4, true)
               AC_DEFINE([HAVE_WAIT4], [], [With wait4 support])],
              [AM_CONDITIONAL(HAVE_WAIT4, false)])
AC_CHECK_FUNC([mmap],
              [AM_CONDITIONAL(WITHOUT_MMAP, false)
               AC_DEFINE([HAVE_MMAP], [], [With mmap support])],
              [AM_CONDITIONAL(WITHOUT_MMAP, true)])
AC_CHECK_FUNCS(fdatasync gai_strerror getline getaddrinfo gethostname memcpy\
               mkdir mkfifo mktime rmdir select setpriority seteuid setreuid\
               socket strcasestr strerror strtod strtol strtoll strtoul\
               strtoull unsetenv)

AM_GNU_GETTEXT([external])
if test "x$LIBINTL" != "x"
then
   LIBS="$LIBS $LIBINTL"
fi

AC_CONFIG_FILES([Makefile
po/Makefile.in
doc/Makefile
doc/html-en/Makefile
doc/images/Makefile
doc/man/Makefile
doc/txt/Makefile
doc/etc/Makefile
doc/etc/AFD_CONFIG.sample
doc/etc/MON_CONFIG.sample
doc/etc/afd.users.sample
scripts/Makefile
scripts/afd
scripts/afdmon
scripts/afd_auto_config
scripts/afdalarm
scripts/afd_unused_infos
scripts/mirror_fra_cfg
scripts/rafdd_cmd
scripts/rafdd_cmd_ssh
scripts/rmon_ctrl
scripts/rmon_ctrl_ssh
afd.spec])
AC_OUTPUT


echo ""
echo "                          Configuration"
echo ""
echo "    Source code location             : ${srcdir}"
echo "    Install path bin + sbin          : ${prefix}/bin ${prefix}/sbin"
echo "    Install path doc                 : ${docdir}"
echo "    Install path man                 : ${mandir}"
echo "    AFD Version                      : ${afd_full_version}"
echo "    Compiler                         : ${CC}"
echo "    Compiler flags                   : ${CFLAGS}"
echo "    GUI                              : ${GUIS_STRING}"
if test "x${ac_cv_with_afdbench_settings}" = "xyes"
then
   echo "    Settings for afdbench enabled    : yes"
fi
if test "x${with_ssl}" = "xyes"
then
   echo "    With TLS/SSL support             : yes"
else
   echo "    With TLS/SSL support             : no"
fi
echo "    Use systemd                      : ${use_systemd}"
echo "    Logs                             : ${ac_cv_input_log}${ac_cv_distribution_log}${ac_cv_output_log}${ac_cv_confirmation_log}${ac_cv_delete_log}${ac_cv_production_log}${ac_cv_maintainer_log}${ac_cv_alda_cache}${ac_cv_transfer_rate_log}"
echo "    Error queue support              : ${ac_cv_error_queue}"
echo "    With AFD monitor                 : ${ac_cv_afd_mon}"
echo "    Multiple file system support     : ${ac_cv_multi_fs_support}"
echo "    With duplicate check support     : ${ac_cv_dupcheck_support}"
echo "    Protocols supported              : ${ac_cv_ftp_support}${ac_cv_sftp_support}${ac_cv_smtp_support}${ac_cv_http_support}${ac_cv_wmo_support}${ac_cv_loc_support}${ac_cv_fd_exec_support}${ac_cv_scp_support}${ac_cv_demail_support}${ac_cv_dfax_support}"
echo "    With server support              : afdd ${ac_cv_atpd_support}${ac_cv_wmod_support}"
echo "    With largefile support           : ${ac_cv_largefile}"
echo "    With inotify support             : ${ac_cv_allow_inotify}"
echo "    With internal IP database        : ${ac_cv_ip_database}"
echo "    Max number of parallel transfers : ${with_max_no_of_parallel_jobs}"
echo "    Def number of parallel transfers : ${with_default_no_of_parallel_jobs}"
echo "    Maximum host alias name length   : ${with_max_hostname_length}"
echo "    Maximum real host name length    : ${with_max_real_hostname_length}"
echo "    Maximum AFD alias name length    : ${with_max_afdname_length}"
echo "    Max directory alias name length  : ${with_max_dir_alias_length}"
echo "    Max recipient length             : ${with_max_recipient_length}"
echo "    Some binaries with setuid flag   : ${ac_cv_setuid_progs}"
echo "    Group can write                  : ${ac_cv_group_can_write}"
echo "    Automatically create configs     : ${ac_cv_auto_create_config}"
echo "    Enable Distro-style RPMs         : ${enable_distro_rpm}"
if test "$DISTRO" != ""
then
   echo "    Distribution                     : $DISTRO $DISTRO_VERSION"
fi
echo ""
echo "------------- type make to compile -------------"
