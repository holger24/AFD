/*
 *  sdr_str.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2024 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __sdr_str_h
#define __sdr_str_h

static const char *sdrstr[] = /*Short delete reason string. */
                  {
                     "Age-limit(O) ",  /*  0 */
                     "Age-limit(I) ",  /*  1 */
                     "User delete  ",  /*  2 */
                     "Exec delete  ",  /*  3 */
                     "Missing msg  ",  /*  4 */
                     "Duplicate(I) ",  /*  5 NOTE: Do not use WITH_DUP_CHECK! */ 
                     "Duplicate(O) ",  /*  6 NOTE: Do not use WITH_DUP_CHECK! */
                     "Unknown file ",  /*  7 */
                     "Job ID error ",  /*  8 */
                     "Old lock file",  /*  9 */
                     "Queued file  ",  /* 10 */
                     "Delete option",  /* 11 */
                     "Del stale job",  /* 12 */
                     "Update DB del",  /* 13 */
                     "Other proc   ",  /* 14 */
                     "Del pool dir ",  /* 15 */
                     "Exec stored  ",  /* 16 */
                     "Host disabled",  /* 17 */
                     "Convert fail ",  /* 18 */
                     "Rename overwr",  /* 19 */
                     "Mail recp rej",  /* 20 */
                     "Mirror delete",  /* 21 */
                     "Queue error  ",  /* 22 */
                     "Int. link err",  /* 23 */
                     "Del unreadabl",  /* 24 */
                     "Unk. file (G)",  /* 25 */
                     "Old lock (G) ",  /* 26 */
                     "Old lck re(G)",  /* 27 */
                     "Queued (G)   ",  /* 28 */
                     "Old lck in(G)"   /* 29 */
                  };

#endif /* __sdr_str_h */
