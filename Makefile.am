## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2001 - 2022 Holger Kiehl
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

MAINTAINERCLEANFILES = Makefile.in\
                       aclocal.m4\
                       configure\
                       configure~\
                       ac-tools/config.guess\
                       ac-tools/config.h.in\
                       ac-tools/config.h.in~\
                       ac-tools/config.sub\
                       ac-tools/depcomp\
                       ac-tools/install-sh\
                       ac-tools/missing\
                       ac-tools/mkinstalldirs\
                       ac-tools/compile\
                       $(PACKAGE)-$(VERSION).tar.gz\
                       $(PACKAGE)-$(VERSION).tar.bz2\
                       afd.spec\
                       errors
CLEANFILES           = core\
                       core.*\
                       typescript configmake.h configmake.h-t\
                       po/$(PACKAGE).pot\
                       afd.spec\
                       $(PACKAGE)-$(VERSION).tar.gz\
                       $(PACKAGE)-$(VERSION).tar.bz2
BUILT_SOURCES        = configmake.h
SUBDIRS              = po scripts doc
DISTDOCS             = LICENSE\
                       CREDITS\
                       INSTALL\
                       KNOWN-BUGS\
                       README.configure\
                       THANKS\
                       TODO\
                       changes-1.2.x-1.3.x
EXTRA_DIST           = ac-tools\
                       scripts\
                       afd.spec.in\
                       $(DISTDOCS)
doc_DATA             = $(DISTDOCS)
AM_CPPFLAGS          = @AFD_SSL_INCLUDES@\
                       -Isrc\
                       -Isrc/init_afd\
			     -Isrc/amg\
			     -Isrc/fd\
			     -Isrc/protocols\
			     -Isrc/log\
			     -Isrc/log/alda\
			     -Isrc/archive_watch\
			     -Isrc/misc\
			     -Isrc/afd_mon\
			     -Isrc/statistics\
			     -Isrc/cmdline\
			     -Isrc/servers/common\
			     -Isrc/servers/afdd\
			     -Isrc/servers/atpd\
			     -Isrc/servers/demcd\
			     -Isrc/servers/wmod\
			     -Isrc/UI/common\
			     @AFD_MOTIF_INCLUDES@\
			     @AFD_XAW_INCLUDES@\
			     -Isrc/UI/Motif/common\
			     -Isrc/UI/Motif/mafd_ctrl\
			     -Isrc/UI/Motif/dir_ctrl\
			     -Isrc/UI/Motif/mon_ctrl\
			     -Isrc/UI/Motif/afd_info\
			     -Isrc/UI/Motif/dir_info\
			     -Isrc/UI/Motif/mon_info\
			     -Isrc/UI/Motif/afd_load\
			     -Isrc/UI/Motif/edit_hc\
			     -Isrc/UI/Motif/show_queue\
			     -Isrc/UI/Motif/show_elog\
			     -Isrc/UI/Motif/show_ilog\
			     -Isrc/UI/Motif/show_olog\
			     -Isrc/UI/Motif/show_plog\
			     -Isrc/UI/Motif/show_cmd\
			     -Isrc/UI/Motif/view_dc\
			     -Isrc/UI/Motif/handle_event\
			     -Isrc/UI/Motif/mshow_log\
			     -Isrc/UI/Motif/xsend_file\
			     -Isrc/UI/Motif/xshow_stat\
			     @AFD_GTK_CFLAGS@\
			     -Isrc/UI/Gtk+/common\
			     -Isrc/UI/Gtk+/gafd_ctrl\
			     -Isrc/UI/Gtk+/gshow_log\
			     -Isrc/UI/HTML/hmon_ctrl

##########################################################################
#                                 src                                    #
##########################################################################
noinst_HEADERS       = src/afdsetup.h\
                       src/version.h
CLEANFILES          += src/core\
                       src/core.*
noinst_LIBRARIES     =
bin_PROGRAMS         =
sbin_PROGRAMS        =
EXTRA_PROGRAMS       =
INSTALL_EXEC_HOOKS   =
UNINSTALL_HOOKS      =

include src/common/Makemodule.am
include src/log/Makemodule.am
if WITH_ALDA
include src/log/alda/Makemodule.am
endif
if WITH_AFD_MON
include src/afd_mon/Makemodule.am
endif
if !WITH_AFD_MON_ONLY
include src/misc/Makemodule.am
include src/protocols/Makemodule.am
include src/init_afd/Makemodule.am
include src/amg/Makemodule.am
include src/fd/Makemodule.am
include src/archive_watch/Makemodule.am
include src/statistics/Makemodule.am
include src/tools/Makemodule.am
include src/servers/common/Makemodule.am
include src/servers/afdd/Makemodule.am
include src/servers/afdds/Makemodule.am
if WITH_ATPD_SUPPORT
include src/servers/atpd/Makemodule.am
endif
if WITH_WMOD_SUPPORT
include src/servers/wmod/Makemodule.am
endif
if WITH_DE_MAIL_SUPPORT
include src/servers/demcd/Makemodule.am
endif
include src/cmdline/Makemodule.am
include src/test/Makemodule.am
include src/test/filter/Makemodule.am
endif # !WITH_AFD_MON_ONLY

if WITH_NCURSES
include src/UI/nCurses/nafd_ctrl/Makemodule.am
endif

if HAVE_MOTIF
EXTRA_DIST += src/UI/Motif/AFD
include src/UI/common/Makemodule.am
include src/UI/Motif/common/Makemodule.am
if !WITH_AFD_MON_ONLY
include src/UI/Motif/mafd_ctrl/Makemodule.am
include src/UI/Motif/dir_ctrl/Makemodule.am
include src/UI/Motif/afd_info/Makemodule.am
include src/UI/Motif/dir_info/Makemodule.am
include src/UI/Motif/afd_load/Makemodule.am
include src/UI/Motif/edit_hc/Makemodule.am
include src/UI/Motif/show_cmd/Makemodule.am
include src/UI/Motif/mshow_log/Makemodule.am
if WITH_OUTPUT_LOG
include src/UI/Motif/show_olog/Makemodule.am
endif
if WITH_PRODUCTION_LOG
include src/UI/Motif/show_plog/Makemodule.am
endif
if WITH_INPUT_LOG
include src/UI/Motif/show_ilog/Makemodule.am
endif
if WITH_DELETE_LOG
include src/UI/Motif/show_dlog/Makemodule.am
endif
include src/UI/Motif/show_elog/Makemodule.am
include src/UI/Motif/show_queue/Makemodule.am
include src/UI/Motif/view_dc/Makemodule.am
include src/UI/Motif/xsend_file/Makemodule.am
include src/UI/Motif/xshow_stat/Makemodule.am
include src/UI/Motif/handle_event/Makemodule.am
endif # !WITH_AFD_MON_ONLY
if WITH_AFD_MON
include src/UI/Motif/mon_ctrl/Makemodule.am
include src/UI/Motif/mon_info/Makemodule.am
endif
endif HAVE_MOTIF
if !WITH_AFD_MON_ONLY
if WITH_GTK
include src/UI/Gtk+/common/Makemodule.am
include src/UI/Gtk+/gafd_ctrl/Makemodule.am
include src/UI/Gtk+/gshow_log/Makemodule.am
endif WITH_GTK
endif !WITH_AFD_MON_ONLY
if WITH_HTML_GUI
include src/UI/HTML/hmon_ctrl/Makemodule.am
endif WITH_HTML_GUI

install-exec-hook: $(INSTALL_EXEC_HOOKS)

uninstall-hook: $(UNINSTALL_HOOKS)

maintainer-clean-local:
	-rm -rf src/amg/.deps\
		  src/protocols/.deps\
		  src/UI/common/.deps\
		  src/UI/Motif/show_olog/.deps\
		  src/UI/Motif/mshow_log/.deps\
		  src/UI/Motif/show_dlog/.deps\
		  src/UI/Motif/show_ilog/.deps\
		  src/UI/Motif/afd_load/.deps\
		  src/UI/Motif/show_plog/.deps\
		  src/UI/Motif/mafd_ctrl/.deps\
		  src/UI/Motif/dir_ctrl/.deps\
		  src/UI/Motif/common/.deps\
		  src/UI/Motif/afd_info/.deps\
		  src/UI/Motif/view_dc/.deps\
		  src/UI/Motif/xsend_file/.deps\
		  src/UI/Motif/dir_info/.deps\
		  src/UI/Motif/mon_ctrl/.deps\
		  src/UI/Motif/show_queue/.deps\
		  src/UI/Motif/show_elog/.deps\
		  src/UI/Motif/xshow_stat/.deps\
		  src/UI/Motif/show_cmd/.deps\
		  src/UI/Motif/mon_info/.deps\
		  src/UI/Motif/handle_event/.deps\
		  src/UI/Motif/edit_hc/.deps\
		  src/UI/Gtk+/gshow_log/.deps\
		  src/UI/Gtk+/gafd_ctrl/.deps\
		  src/UI/Gtk+/common/.deps\
		  src/UI/nCurses/nafd_ctrl/.deps\
		  src/UI/HTML/hmon_ctrl/.deps\
		  src/common/.deps\
		  src/cmdline/.deps\
		  src/afd_mon/.deps\
		  src/log/.deps\
		  src/log/alda/.deps\
		  src/statistics/.deps\
		  src/test/.deps\
		  src/test/filter/.deps\
		  src/fd/.deps\
		  src/init_afd/.deps\
		  src/archive_watch/.deps\
		  src/servers/demcd/.deps\
		  src/servers/common/.deps\
		  src/servers/atpd/.deps\
		  src/servers/wmod/.deps\
		  src/servers/afdd/.deps\
		  src/servers/afdds/.deps\
		  src/tools/.deps\
		  src/misc/.deps

configmake.h:	Makefile
	rm -f $@-t $@
	{ echo '/* DO NOT EDIT! GENERATED AUTOMATICALLY! */'; \
	  echo '#define PREFIX "$(prefix)"'; \
	  echo '#define EXEC_PREFIX "$(exec_prefix)"'; \
	  echo '#define BINDIR "$(bindir)"'; \
	  echo '#define SBINDIR "$(sbindir)"'; \
	  echo '#define LIBEXECDIR "$(libexecdir)"'; \
	  echo '#define DATAROOTDIR "$(datarootdir)"'; \
	  echo '#define DATADIR "$(datadir)"'; \
	  echo '#define SYSCONFDIR "$(sysconfdir)"'; \
	  echo '#define SHAREDSTATEDIR "$(sharedstatedir)"'; \
	  echo '#define LOCALSTATEDIR "$(localstatedir)"'; \
	  echo '#define INCLUDEDIR "$(includedir)"'; \
	  echo '#define OLDINCLUDEDIR "$(oldincludedir)"'; \
	  echo '#define DOCDIR "$(docdir)"'; \
	  echo '#define INFODIR "$(infodir)"'; \
	  echo '#define HTMLDIR "$(htmldir)"'; \
	  echo '#define DVIDIR "$(dvidir)"'; \
	  echo '#define PDFDIR "$(pdfdir)"'; \
	  echo '#define PSDIR "$(psdir)"'; \
	  echo '#define LIBDIR "$(libdir)"'; \
	  echo '#define LISPDIR "$(lispdir)"'; \
	  echo '#define LOCALEDIR "$(localedir)"'; \
	  echo '#define MANDIR "$(mandir)"'; \
	  echo '#define MANEXT "$(manext)"'; \
	  echo '#define PKGDATADIR "$(pkgdatadir)"'; \
	  echo '#define PKGINCLUDEDIR "$(pkgincludedir)"'; \
	  echo '#define PKGLIBDIR "$(pkglibdir)"'; \
	  echo '#define PKGLIBEXECDIR "$(pkglibexecdir)"'; \
	} | sed '/""/d' > $@-t
	mv $@-t $@

rpm:	dist
	$(RPM) -ta $(PACKAGE)-$(VERSION).tar.bz2 </dev/null;

dist-hook:
	rm -f $(PACKAGE)-$(VERSION).tar.bz2 $(PACKAGE)-$(VERSION).tar.gz
	cp afd.spec $(distdir)

.PHONY: rpm
