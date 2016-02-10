## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2015 Holger Kiehl
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

if WITH_ATPD_SUPPORT
   ATPD_DIR  = atpd
else
   ATPD_DIR  =
endif
if WITH_WMOD_SUPPORT
   WMOD_DIR  = wmod
else
   WMOD_DIR  =
endif
if WITH_DE_MAIL_SUPPORT
   DEMCD_DIR = demcd
else
   DEMCD_DIR =
endif
MAINTAINERCLEANFILES     = Makefile.in
SUBDIRS                  = common afdd $(ATPD_DIR) $(DEMCD_DIR) $(WMOD_DIR)
