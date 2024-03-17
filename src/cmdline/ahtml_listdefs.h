/*
 *  ahtml_listdefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __ahtml_listdefs_h
#define __ahtml_listdefs_h

#ifndef _STANDALONE_
#include "afddefs.h"
#include "fddefs.h"
#endif
#include "cmdline.h"

#define DEFAULT_HTML_LIST_FILENAME        ".html_list_filename.txt"
#define DEFAULT_HTML_LIST_FILENAME_LENGTH (sizeof(DEFAULT_HTML_LIST_FILENAME) - 1)

/* Function prototypes. */
extern void get_html_content(char *, struct data *);
extern int  eval_html_dir_list(char *, off_t, unsigned char, int, int *,
                               struct data *);

#endif /* __ahtml_listdefs_h */
