## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2007 - 2015 Holger Kiehl
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

if WITH_HTML_GUI
   HTML_DIR = HTML
else
   HTML_DIR =
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
SUBDIRS              = $(NCURSES_DIR) $(COMMON_DIR) $(MOTIF_DIR) $(GTK_DIR) $(HTML_DIR)
