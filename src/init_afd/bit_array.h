/*
 *  bitarray.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1999 - 2008 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __bit_array_h
#define __bit_array_h

#define ALL_MINUTES      1152921504606846975LL
#define ALL_HOURS        16777215
#define ALL_DAY_OF_MONTH 2147483647
#define ALL_MONTH        4095
#define ALL_DAY_OF_WEEK  127

#ifdef HAVE_LONG_LONG
static unsigned long long bit_array_long[60] =
                   {
                      1LL,                   /* 0 */
                      2LL,                   /* 1 */
                      4LL,                   /* 2 */
                      8LL,                   /* 3 */
                      16LL,                  /* 4 */
                      32LL,                  /* 5 */
                      64LL,                  /* 6 */
                      128LL,                 /* 7 */
                      256LL,                 /* 8 */
                      512LL,                 /* 9 */
                      1024LL,                /* 10 */
                      2048LL,                /* 11 */
                      4096LL,                /* 12 */
                      8192LL,                /* 13 */
                      16384LL,               /* 14 */
                      32768LL,               /* 15 */
                      65536LL,               /* 16 */
                      131072LL,              /* 17 */
                      262144LL,              /* 18 */
                      524288LL,              /* 19 */
                      1048576LL,             /* 20 */
                      2097152LL,             /* 21 */
                      4194304LL,             /* 22 */
                      8388608LL,             /* 23 */
                      16777216LL,            /* 24 */
                      33554432LL,            /* 25 */
                      67108864LL,            /* 26 */
                      134217728LL,           /* 27 */
                      268435456LL,           /* 28 */
                      536870912LL,           /* 29 */
                      1073741824LL,          /* 30 */
                      2147483648LL,          /* 31 */
                      4294967296LL,          /* 32 */
                      8589934592LL,          /* 33 */
                      17179869184LL,         /* 34 */
                      34359738368LL,         /* 35 */
                      68719476736LL,         /* 36 */
                      137438953472LL,        /* 37 */
                      274877906944LL,        /* 38 */
                      549755813888LL,        /* 39 */
                      1099511627776LL,       /* 40 */
                      2199023255552LL,       /* 41 */
                      4398046511104LL,       /* 42 */
                      8796093022208LL,       /* 43 */
                      17592186044416LL,      /* 44 */
                      35184372088832LL,      /* 45 */
                      70368744177664LL,      /* 46 */
                      140737488355328LL,     /* 47 */
                      281474976710656LL,     /* 48 */
                      562949953421312LL,     /* 49 */
                      1125899906842624LL,    /* 50 */
                      2251799813685248LL,    /* 51 */
                      4503599627370496LL,    /* 52 */
                      9007199254740992LL,    /* 53 */
                      18014398509481984LL,   /* 54 */
                      36028797018963968LL,   /* 55 */
                      72057594037927936LL,   /* 56 */
                      144115188075855872LL,  /* 57 */
                      288230376151711744LL,  /* 58 */
                      576460752303423488LL   /* 59 */
                   };
#endif /* HAVE_LONG_LONG */

static unsigned int bit_array[31] =
                   {
                      1,                     /* 0 */
                      2,                     /* 1 */
                      4,                     /* 2 */
                      8,                     /* 3 */
                      16,                    /* 4 */
                      32,                    /* 5 */
                      64,                    /* 6 */
                      128,                   /* 7 */
                      256,                   /* 8 */
                      512,                   /* 9 */
                      1024,                  /* 10 */
                      2048,                  /* 11 */
                      4096,                  /* 12 */
                      8192,                  /* 13 */
                      16384,                 /* 14 */
                      32768,                 /* 15 */
                      65536,                 /* 16 */
                      131072,                /* 17 */
                      262144,                /* 18 */
                      524288,                /* 19 */
                      1048576,               /* 20 */
                      2097152,               /* 21 */
                      4194304,               /* 22 */
                      8388608,               /* 23 */
                      16777216,              /* 24 */
                      33554432,              /* 25 */
                      67108864,              /* 26 */
                      134217728,             /* 27 */
                      268435456,             /* 28 */
                      536870912,             /* 29 */
                      1073741824             /* 30 */
                   };

#endif /* __bit_array_h */
