/*
 *  asmtpdefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __asmtpdefs_h
#define __asmtpdefs_h

#ifndef _STANDALONE_
# include "afddefs.h"
# include "fddefs.h"
#endif
#include "cmdline.h"

#define DEFAULT_AFD_USER     "anonymous"
#define DEFAULT_AFD_PASSWORD "afd@someplace"

/* Function prototypes. */
extern int init_asmtp(int, char **, struct data *);

#endif /* __asmtpdefs_h */
