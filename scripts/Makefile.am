## Makemodule.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2025 Holger Kiehl
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
   AFDMON_FILES      = rmon_ctrl rmon_ctrl_ssh
   AFD_SYSTEMD_FILES = afd.target afdmon.target afd@.service afdmon@.service
   EXTRA_DIST        = rafdd_cmd.in rafdd_cmd_ssh.in rmon_ctrl.in\
                       rmon_ctrl_ssh.in afdalarm.in afd.in afd.service\
			     $(AFD_SYSTEMD_FILES) afdmon.in afd.sysconfig\
			     afdmon.sysconfig afd_auto_config.in afd_unused_infos.in
else
   AFDMON_FILES      =
   AFD_SYSTEMD_FILES = afd.target afd@.service
   EXTRA_SCRIPTS     = rmon_ctrl rmon_ctrl_ssh
   EXTRA_DIST        = rafdd_cmd.in rafdd_cmd_ssh.in afdalarm.in afd.in\
                       afd.service afdmon.target afdmon@.service\
			     $(AFD_SYSTEMD_FILES) afdmon.in afd.sysconfig\
			     afdmon.sysconfig afd_auto_config.in afd_unused_infos.in
endif
MAINTAINERCLEANFILES = Makefile.in
CLEANFILES           = afd afdalarm afdmon afd_auto_config afd_unused_infos\
                       mirror_fra_cfg rafdd_cmd rafdd_cmd_ssh rmon_ctrl\
                       rmon_ctrl_ssh
bin_SCRIPTS          = afdalarm afd_auto_config rafdd_cmd rafdd_cmd_ssh\
                       afd_unused_infos mirror_fra_cfg $(AFDMON_FILES)\
			     afd_environment_wrapper
if WITH_SYSTEMD
if UNIT_DIR_WRITEABLE
   systemdsystemunit_DATA = $(AFD_SYSTEMD_FILES)
endif
endif

install-exec-local:
	cd $(DESTDIR)$(bindir) && rm -f mon_unused_infos
	cd $(DESTDIR)$(bindir) && ln -s afd_unused_infos mon_unused_infos

uninstall-local:
	cd $(DESTDIR)$(bindir) && rm -f mon_unused_infos
