/*
 *  dr_str.h - Part of AFD, an automatic file distribution program.
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

#ifndef __dr_str_h
#define __dr_str_h

static const char *drstr[] = /* Delete reason string. */
                  {
                     "Age limit [option] (output)",                  /*  0 */
                     "Age limit [option] (input)",                   /*  1 */
                     "Deleted on user request",                      /*  2 */
                     "Exec [option] failed delete",                  /*  3 */
                     "Failed to read messages file",                 /*  4 */
                     "Duplicate check delete (input)",               /*  5 NOTE: Do not use WITH_DUP_CHECK! */
                     "Duplicate check delete (output)" ,             /*  6 NOTE: Do not use WITH_DUP_CHECK! */
                     "Delete unknown file [dir options]",            /*  7 */
                     "Failed to locate job ID",                      /*  8 */
                     "Delete old locked file [dir options]",         /*  9 */
                     "Delete queued file [dir options]",             /* 10 */
                     "Delete [option]",                              /* 11 */
                     "Delete stale error jobs (AFD_CONFIG)",         /* 12 */
                     "Stale job after updating database",            /* 13 */
                     "File transmitted by other process",            /* 14 */
                     "Deleting unknown pool/source dir",             /* 15 */
                     "Exec [option] failed stored",                  /* 16 */
                     "Host disabled",                                /* 17 */
                     "Conversion failed",                            /* 18 */
                     "Rename overwrite",                             /* 19 */
                     "Mail recipient rejected",                      /* 20 */
                     "Mirror delete",                                /* 21 */
                     "Mkdir queue error",                            /* 22 */
                     "Internal link failed",                         /* 23 */
                     "Delete unreadable file",                       /* 24 */
                     "Delete unknown file (AFD_CONFIG)",             /* 25 */
                     "Delete old locked file (AFD_CONFIG)",          /* 26 */
                     "Delete old locked file remote (AFD_CONFIG)",   /* 27 */
                     "Delete queued file (AFD_CONFIG)",              /* 28 */
                     "Delete old locked file incoming (AFD_CONFIG)"  /* 29 */
                  };

/* NOTE: If the maximum length changes, don't forget to change */
/*       MAX_DELETE_REASON_LENGTH in afddefs.h !!! Also modify */
/*       sdr_str.h when adding or removing a line here.        */

#endif /* __dr_str_h */
