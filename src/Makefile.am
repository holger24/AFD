## Makefile.am

if WITH_AFD_MON
   AFD_MON_DIR = afd_mon
else
   AFD_MON_DIR = 
endif

if WITH_AFD_MON_ONLY
   OTHER_DIRS = common log afd_mon UI
else
   OTHER_DIRS = common misc protocols init_afd amg fd log archive_watch\
                statistics tools $(AFD_MON_DIR) servers cmdline test UI
endif

noinst_HEADERS       = afdsetup.h version.h
MAINTAINERCLEANFILES = Makefile.in
CLEANFILES           = core core.*
SUBDIRS              = $(OTHER_DIRS)
