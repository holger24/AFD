## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2008 - 2013 Holger Kiehl
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

docdir               = $(datadir)/doc/${PACKAGE_TARNAME}/etc
MAINTAINERCLEANFILES = Makefile.in
CLEANFILES           = AFD_CONFIG.sample MON_CONFIG.sample afd.users.sample
DISTDOCS             = AFD_CONFIG.sample AFD_MON_CONFIG.sample\
                       MON_CONFIG.sample DIR_CONFIG.sample afd.users.sample\
                       INFO-LOOP.sample afd.name.sample host.info.sample\
                       rename.rule.sample
EXTRA_DIST           = $(DISTDOCS) AFD_CONFIG.sample.in MON_CONFIG.sample.in\
                       afd.users.sample.in
doc_DATA             = $(DISTDOCS)
