## Makefile.am

if WITH_NCURSES
   NCURSES_DIR = nCurses
else
   NCURSES_DIR =
endif

if HAVE_MOTIF
   MOTIF_DIR = Motif
else
   MOTIF_DIR =
endif

if WITH_GTK
   GTK_DIR = Gtk+
else
   GTK_DIR =
endif

if HAVE_MOTIF
   COMMON_DIR = common
else
if WITH_GTK
   COMMON_DIR = common
else
   COMMON_DIR = 
endif
endif

MAINTAINERCLEANFILES = Makefile.in
SUBDIRS              = $(NCURSES_DIR) $(COMMON_DIR) $(MOTIF_DIR) $(GTK_DIR)
