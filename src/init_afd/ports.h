/*
 *  ports.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 - 2021 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __ports_h
#define __ports_h

#if !defined(HAVE_SETEUID) && defined(HAVE_SETREUID)
# define seteuid(a)    setreuid(-1, (a))
# define setegid(a)    setregid(-1, (a))
# define HAVE_SETEUID
#endif

#ifdef HAVE_STRTOLL
# if SIZEOF_TIME_T == 4
#  define str2timet strtol
# else
#  define str2timet strtoll
# endif
# if SIZEOF_OFF_T == 4
#  define str2offt strtol
# else
#  define str2offt strtoll
# endif
# if SIZEOF_INO_T == 4
#  define str2inot strtol
# else
#  define str2inot strtoll
# endif
# if SIZEOF_DEV_T == 4
#  define str2devt strtol
# else
#  define str2devt strtoll
# endif
#else
# define str2timet strtol
# define str2offt strtol
# define str2inot strtol
# define str2devt strtol
#endif
#ifdef HAVE_STRTOULL
# if !defined ULLONG_MAX
#  define ULLONG_MAX ULONG_MAX
# endif
#endif


#endif /* __ports_h */
