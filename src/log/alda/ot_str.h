/*
 *  ot_str.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2010 - 2015 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __ot_str_h
#define __ot_str_h

#if defined(_WITH_DE_MAIL_SUPPORT) && !defined(_CONFIRMATION_LOG)
# define MAX_OUTPUT_TYPES 13
#else
# define MAX_OUTPUT_TYPES 9
#endif
static const char *otstr[] = /* Output type string. */
                  {
                     "Normal (delivered)",                   /*  0 */
                     "Age limit (del)",                      /*  1 */
                     "Duplicate (stored)",                   /*  2 */
                     "Duplicate (del)",                      /*  3 */
                     "Other process (del)",                  /*  4 */
                     "Address rejected (del)",               /*  5 */
                     "Host disabled (del)",                  /*  6 */
                     "Duplicate",                            /*  7 */
                     "Unknown",                              /*  8 */
#if defined(_WITH_DE_MAIL_SUPPORT) && !defined(_CONFIRMATION_LOG)
                     "Normal (received)",                    /*  9 */
                     "Conf of dispatch",                     /* 10 */
                     "Conf of receipt",                      /* 11 */
                     "Conf of retrieve",                     /* 12 */
                     "Conf time up"                          /* 13 */
#else
                     "Normal (received)"                     /*  9 */
#endif
                  };

#endif /* __ot_str_h */
