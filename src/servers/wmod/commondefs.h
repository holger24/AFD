/*
 *  commondefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __commondefs_h
#define __commondefs_h

struct proclist
       {
          pid_t pid;
          int   job_pos;
       };

/* Function prototypes */
extern int  get_free_connection(int, pid_t);
extern void check_fsa_pos(void),
            trans_log(char *, char *, int, int, char *, ...);

#endif /* __commondefs_h */
