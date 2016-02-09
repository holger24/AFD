/*
 *  ea_str.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007 - 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __ea_str_h
#define __ea_str_h

static const char *eastr[] = /* Event action string. */
                  {
                     "",                                /*  0 */
                     "Reread DIR_CONFIG",               /*  1 */
                     "Reread HOST_CONFIG",              /*  2 */
                     "Reread rename.rule",              /*  3 */
                     "AFD_CONFIG changed",              /*  4 */
                     "Enable retrieve",                 /*  5 */
                     "Disable retrieve",                /*  6 */
                     "Enable archive",                  /*  7 */
                     "Disable archive",                 /*  8 */
                     "Enable create target dir",        /*  9 */
                     "Disable create target dir",       /* 10 */
                     "Enable dir warn time",            /* 11 */
                     "Disable dir warn time",           /* 12 */
                     "Stopping AMG",                    /* 13 */
                     "Starting AMG",                    /* 14 */
                     "Stopping FD",                     /* 15 */
                     "Starting FD",                     /* 16 */
                     "AFD shutdown",                    /* 17 */
                     "AFD startup",                     /* 18 */
                     "Production error",                /* 19 */
                     "Error start",                     /* 20 */
                     "Error end",                       /* 21 */
                     "Enable directory",                /* 22 */
                     "Disable directory",               /* 23 */
                     "Rescan directory",                /* 24 */
                     "Exec error action start",         /* 25 */
                     "Exec error action stop",          /* 26 */
                     "Offline",                         /* 27 */
                     "Acknowledge",                     /* 28 */
                     "Enable host",                     /* 29 */
                     "Disable host",                    /* 30 */
                     "Start transfer",                  /* 31 */
                     "Stop transfer",                   /* 32 */
                     "Start queue",                     /* 33 */
                     "Stop queue",                      /* 34 */
                     "Start error queue",               /* 35 */
                     "Stop error queue",                /* 36 */
                     "Switching",                       /* 37 */
                     "Retry transfer",                  /* 38 */
                     "Enable debug",                    /* 39 */
                     "Enable trace",                    /* 40 */
                     "Enable full trace",               /* 41 */
                     "Disable debug",                   /* 42 */
                     "Disable trace",                   /* 43 */
                     "Disable full trace",              /* 44 */
                     "Unset Acknowledge/Offline",       /* 45 */
                     "Warn time set",                   /* 46 */
                     "Warn time unset",                 /* 47 */
                     "Enable host warn time",           /* 48 */
                     "Disable host warn time",          /* 49 */
                     "Enable delete data",              /* 50 */
                     "Disable delete data",             /* 51 */
                     "Exec warn action start",          /* 52 */
                     "Exec warn action stop",           /* 53 */
                     "Exec success action start",       /* 54 */
                     "Exec success action stop",        /* 55 */
                     "Start directory",                 /* 56 */
                     "Stop directory",                  /* 57 */
                     "Changed information",             /* 58 */
                     "Enable create source dir",        /* 59 */
                     "Disable create source dir",       /* 60 */
                     "Info time set",                   /* 61 */
                     "Info time unset",                 /* 62 */
                     "Exec info action start",          /* 63 */
                     "Exec info action stop",           /* 64 */
                     "Enable simulate send",            /* 65 */
                     "Disable simulate send",           /* 66 */
                     "Enable simulate host",            /* 67 */
                     "Disable simulate host"            /* 68 */
                  };

/* NOTE: If the maximum length changes, don't forget to change */
/*       MAX_EVENT_ACTION_LENGTH in afddefs.h !!!              */

#endif /* __ea_str_h */
