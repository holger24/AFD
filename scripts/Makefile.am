## Makefile.am

if WITH_AFD_MON
   AFD_MON_FILES = rmon_ctrl rmon_ctrl_ssh
   EXTRA_DIST    = rafdd_cmd.in rafdd_cmd_ssh.in rmon_ctrl.in rmon_ctrl_ssh.in\
                   afdalarm.in afd.in afd.service afdmon.in afd.sysconfig\
                   afdmon.sysconfig afd_auto_config.in afd_unused_infos.in
else
   AFD_MON_FILES =
   EXTRA_SCRIPTS = rmon_ctrl rmon_ctrl_ssh
   EXTRA_DIST    = rafdd_cmd.in rafdd_cmd_ssh.in afdalarm.in afd.in afd.service\
                   afdmon.in afd.sysconfig afdmon.sysconfig afd_auto_config.in\
                   afd_unused_infos.in
endif

MAINTAINERCLEANFILES = Makefile.in
CLEANFILES           = afd afdalarm afdmon afd_auto_config afd_unused_infos\
                       mirror_fra_cfg rafdd_cmd rafdd_cmd_ssh rmon_ctrl\
                       rmon_ctrl_ssh
bin_SCRIPTS          = afdalarm afd_auto_config rafdd_cmd rafdd_cmd_ssh\
                       afd_unused_infos mirror_fra_cfg $(AFD_MON_FILES)\
			     afd_environment_wrapper

install-exec-local:
	cd $(DESTDIR)$(bindir) && rm -f mon_unused_infos
	cd $(DESTDIR)$(bindir) && ln -s afd_unused_infos mon_unused_infos

uninstall-local:
	cd $(DESTDIR)$(bindir) && rm -f mon_unused_infos
