/*
 *  wmoheader_from_grib.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 - 2012 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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

#include "afddefs.h"

DESCR__S_M3
/*
 ** NAME
 **   wmoheader_from_grib - tries to create a WMO header from the content
 **                         of a GRIB file
 **
 ** SYNOPSIS
 **   void wmoheader_from_grib(char *grib_buffer,
 **                            char *TTAAii_CCCC_YYGGgg,
 **                            char *default_CCCC)
 **
 ** DESCRIPTION
 **   The function wmoheader_from_grib() TRIES to create a WMO header
 **   from the buffer 'grib_buffer' of the following format:
 **           TTAAii_CCCC_YYGGgg
 **   and stores the result in the buffer 'TTAAii_CCCC_YYGGgg'. Since
 **   there can be so many different GRIB types this code will never
 **   be complete and there will always be assumptions being made
 **   about somne of the letters.
 **
 **   NOTE: This code is far from complete and only works for a view
 **         grib types. Please do contact Holger.Kiehl@dwd.de if
 **         this does not work for your grib type, so this can be
 **         implemented.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.08.2002 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>               /* sprintf()                            */
#include <string.h>              /* memcpy()                             */
#include "amgdefs.h"


/*######################## wmoheader_from_grib() ########################*/
void
wmoheader_from_grib(char *grib_buffer,
                    char *TTAAii_CCCC_YYGGgg,
                    char *default_CCCC)
{
   char AA[3],
        CCCC[5],
        TT[3],
        ii[3],
        *ptr;

   ptr = grib_buffer + 4;

   if (((unsigned char)(*(ptr + 10)) < 201) || /* PDS Octet 7 */
       ((unsigned char)(*(ptr + 10)) > 253))   /* PDS Octet 7 */
   {
      /* General International Exchange */
      TT[0] = 'H';
   }
   else
   {
      /*
       * AWIPS
       * NOTE: this could also be Z, but don't know how to
       *       determine this. :-(
       */
      TT[0] = 'Y';
   }
   switch ((unsigned char)(*(ptr + 12))) /* PDS Octet 9 */
   {
      case  1 : /* Pressure */
      case  2 : /* Pressure reduced to MSL */
      case  3 : /* Pressure tendency (?) */
         TT[1] = 'P';
         break;
      case  6 : /* Geopotential */
      case  7 : /* Geopotential height */
         TT[1] = 'H';
         break;
      case 11 : /* Temperature */
      case 15 : /* Maximum temperature */
      case 16 : /* Minimum temperature */
      case 17 : /* Dew point temperature */
         TT[1] = 'T';
         break;
      case 33 : /* u-component of wind */
         TT[1] = 'U';
         break;
      case 34 : /* v-component of wind */
         TT[1] = 'V';
         break;
      case 39 : /* Vertical velocity (pressure) */
      case 40 : /* Vertical velocity (geometric) */
         TT[1] = 'O';
         break;
      case 52 : /* Relative humidity */
         TT[1] = 'R';
         break;
      case 61 : /* Total precipitation */
         TT[1] = 'E';
         break;
      case 63 : /* Convective precipitation */
         TT[1] = 'G';
         break;
      case 71 : /* Total Cloud Cover (?) */
      case 72 : /* Convective cloud cover (?) */
      case 73 : /* Low cloud cover (?) */
      case 74 : /* Medium cloud cover (?) */
      case 75 : /* High cloud cover (?) */
         TT[1] = 'B';
         break;
      case 78 : /* Convective snow */
      case 79 : /* Large scale snow */
      case 99 : /* Snow melt (?) */
         TT[1] = 'S';
         break;
      case 80 : /* Water Temperature (?) */
         TT[1] = 'Z';
         break;
      case 101: /* Direction of wind waves (?) */
         TT[1] = 'M';
         break;
      case 103: /* Mean period of wind waves (?) */
         TT[1] = 'D';
         break;
      case 140: /* Categorical rain (?) */
         TT[1] = 'X';
         break;
      case 154: /* Ozone mixing ratio (?) */
         TT[1] = 'Q';
         break;
      case 187: /* Lightning (?) */
         TT[1] = 'W';
         break;
      default : /* Unknown */
         receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                     _("Unknown Parameter and Unit ID %d [T2 = Z]"),
                     (unsigned char)(*(ptr + 12)));
         TT[1] = 'Z';
         break;
   }
   TT[2] = '\0';

   switch ((unsigned char)(*(ptr + 10))) /* PDS Octet 7 */
   {
      case 201: /* (AWIPS) */
      case 21 :
         AA[0] = 'A';
         break;
      case 218: /* (AWIPS) */
      case 22 :
         AA[0] = 'B';
         break;
      case 219: /* (AWIPS) */
      case 23 :
         AA[0] = 'C';
         break;
      case 220: /* (AWIPS) */
      case 24 :
         AA[0] = 'D';
         break;
      case 221: /* (AWIPS) */
      case 25 :
         AA[0] = 'E';
         break;
      case 222: /* (AWIPS) */
      case 26 :
         AA[0] = 'F';
         break;
      case 223: /* (AWIPS) */
      case 50 :
         AA[0] = 'G';
         break;
      case 202: /* (AWIPS) */
      case 37 :
         AA[0] = 'I';
         break;
      case 203: /* (AWIPS) */
      case 38 :
         AA[0] = 'J';
         break;
      case 204: /* (AWIPS) */
      case 39 :
         AA[0] = 'K';
         break;
      case 205: /* (AWIPS) */
      case 40 :
         AA[0] = 'L';
         break;
      case 206: /* (AWIPS) */
      case 41 :
         AA[0] = 'M';
         break;
      case 207: /* (AWIPS) */
      case 42 :
         AA[0] = 'N';
         break;
      case 208: /* (AWIPS) */
      case 43 :
         AA[0] = 'O';
         break;
      case 210: /* (AWIPS) */
      case 44 :
         AA[0] = 'P';
         break;
      case 214: /* (AWIPS) */
      case 61 :
         AA[0] = 'T';
         break;
      case 215: /* (AWIPS) */
      case 62 :
         AA[0] = 'U';
         break;
      case 216: /* (AWIPS) */
      case 63 :
         AA[0] = 'V';
         break;
      case 217: /* (AWIPS) */
      case 64 :
         AA[0] = 'W';
         break;
      case 255: /* non-defined grid - specified in the GDS */
         if ((unsigned char)(*(ptr + 11)) & 128) /* PDS Octet 8 */
         {
            int           pds_length,
                          gds_length,
                          la1, lo1, la2, lo2,
                          minus;
            unsigned char octet;

            pds_length = 0;
            pds_length |= (unsigned char)*(ptr + 4);
            pds_length <<= 8;
            pds_length |= (unsigned char)*(ptr + 5);
            pds_length <<= 8;
            pds_length |= (unsigned char)*(ptr + 6);

            gds_length = 0;
            gds_length |= (unsigned char)*(ptr + 4 + pds_length);
            gds_length <<= 8;
            gds_length |= (unsigned char)*(ptr + 5 + pds_length);
            gds_length <<= 8;
            gds_length |= (unsigned char)*(ptr + 6 + pds_length);

            /* GDS octet 11 - 13 */
            octet = (unsigned char)*(ptr + pds_length + 14);
            if (octet & 128)
            {
               octet ^= 128;
               minus = YES;
            }
            else
            {
               minus = NO;
            }
            la1 = 0;
            la1 |= octet;
            la1 <<= 8;
            la1 |= (unsigned char)*(ptr + pds_length + 15);
            la1 <<= 8;
            la1 |= (unsigned char)*(ptr + pds_length + 16);
            /* GDS octet 14 - 16 */
            octet = (unsigned char)*(ptr + pds_length + 17);
            if (octet & 128)
            {
               octet ^= 128;
               minus = YES;
            }
            else
            {
               minus = NO;
            }
            lo1 = 0;
            lo1 |= octet;
            lo1 <<= 8;
            lo1 |= (unsigned char)*(ptr + pds_length + 18);
            lo1 <<= 8;
            lo1 |= (unsigned char)*(ptr + pds_length + 19);
            /* GDS octet 18 - 20 */
            octet = (unsigned char)*(ptr + pds_length + 21);
            if (octet & 128)
            {
               octet ^= 128;
               minus = YES;
            }
            else
            {
               minus = NO;
            }
            la2 = 0;
            la2 |= octet;
            la2 <<= 8;
            la2 |= (unsigned char)*(ptr + pds_length + 22);
            la2 <<= 8;
            la2 |= (unsigned char)*(ptr + pds_length + 23);
            if (minus == YES)
            {
               la2 = -la2;
            }
            /* GDS octet 21 - 23 */
            octet = (unsigned char)*(ptr + pds_length + 24);
            if (octet & 128)
            {
               octet ^= 128;
               minus = YES;
            }
            else
            {
               minus = NO;
            }
            lo2 = 0;
            lo2 |= octet;
            lo2 <<= 8;
            lo2 |= (unsigned char)*(ptr + pds_length + 25);
            lo2 <<= 8;
            lo2 |= (unsigned char)*(ptr + pds_length + 26);
            if (minus == YES)
            {
               lo2 = -lo2;
            }

            if ((la1 >= 0) && (lo1 >= -90000) && (la2 <= 90000) && (lo2 <= 0))
            {
               AA[0] = 'A';
            }
            else if ((la1 >= 0) && (lo1 >= -180000) && (la2 <= 90000) && (lo2 <= -90000))
                 {
                    AA[0] = 'B';
                 }
            else if ((la1 >= 0) && (lo1 >= 90000) && (la2 <= 90000) && (lo2 <= 180000))
                 {
                    AA[0] = 'C';
                 }
            else if ((la1 >= 0) && (lo1 >= 0) && (la2 <= 90000) && (lo2 <= 90000))
                 {
                    AA[0] = 'D';
                 }
            else if ((la1 >= -45000) && (lo1 >= -90000) && (la2 <= 45000) && (lo2 <= 0))
                 {
                    AA[0] = 'E';
                 }
            else if ((la1 >= -45000) && (lo1 >= -180000) && (la2 <= 45000) && (lo2 <= -90000))
                 {
                    AA[0] = 'F';
                 }
            else if ((la1 >= -45000) && (lo1 >= 90000) && (la2 <= 45000) && (lo2 <= 180000))
                 {
                    AA[0] = 'G';
                 }
            else if ((la1 >= -45000) && (lo1 >= 0) && (la2 <= 45000) && (lo2 <= 90000))
                 {
                    AA[0] = 'H';
                 }
            else if ((la1 >= -90000) && (lo1 >= -90000) && (la2 <= 0) && (lo2 <= 0))
                 {
                    AA[0] = 'I';
                 }
            else if ((la1 >= -90000) && (lo1 >= -180000) && (la2 <= 0) && (lo2 <= -90000))
                 {
                    AA[0] = 'J';
                 }
            else if ((la1 >= -90000) && (lo1 >= 90000) && (la2 <= 0) && (lo2 <= 180000))
                 {
                    AA[0] = 'K';
                 }
            else if ((la1 >= -90000) && (lo1 >= 0) && (la2 <= 0) && (lo2 <= 90000))
                 {
                    AA[0] = 'L';
                 }
            else if ((la1 >= 0) && (lo1 >= -45000) && (la2 <= 90000) && (lo2 <= 180000))
                 {
                    AA[0] = 'T';
                 }
                 else
                 {
                    receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                                "La1 = %d (%d.%d.%d) Lo1 = %d (%d.%d.%d) La2 = %d  Lo2 = %d  Scanmode = %d [A1 = X]",
                                la1,
                                (unsigned char)*(ptr + pds_length + 14),
                                (unsigned char)*(ptr + pds_length + 15),
                                (unsigned char)*(ptr + pds_length + 16),
                                lo1,
                                (unsigned char)*(ptr + pds_length + 17),
                                (unsigned char)*(ptr + pds_length + 18),
                                (unsigned char)*(ptr + pds_length + 19),
                                la2, lo2,
                                (unsigned char)(*(ptr + pds_length + 31)));
                    AA[0] = 'X';
                 }

#ifdef _WHEN_KNOWN
            switch ((unsigned char)(*(ptr + pds_length + 9))) /* GDS Octet 6 */
            {
               case 0  : /* Latitude/Longitude Grid */
               case 10 : /* Rotated Latitude/Longitude Grid */
               case 20 : /* Stretched Latitude/Longitude Grid */
               case 30 : /* Stretched and Rotated Latitude/Longitude Grid */
            }
            receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                        "pds_length = %d  gds_length = %d",
                        pds_length, gds_length);
#endif /* _WHEN_KNOWN */
         }
         else
         {
            receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                        _("Hmmm, no GDS present %d! [A1 = X]"),
                        (unsigned char)(*(ptr + 11)));
            AA[0] = 'X';
         }
         break;
      case 211:
         if ((TT[0] == 'Y') || (TT[0] == 'Z'))
         {
            AA[0] = 'Q';
         }
         else
         {
            receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                        _("Unknown Grid Identificator %d [A1 = X]"),
                        (unsigned char)(*(ptr + 10)));
            AA[0] = 'X';
         }
         break;
      case 213:
         if ((TT[0] == 'Y') || (TT[0] == 'Z'))
         {
            AA[0] = 'H';
         }
         else
         {
            receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                        _("Unknown Grid Identificator %d [A1 = X]"),
                        (unsigned char)(*(ptr + 10)));
            AA[0] = 'X';
         }
         break;
      default : /* Unknown, lets mark this as experimental. */
         receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                     _("Unknown Grid Identificator %d [A1 = X]"),
                     (unsigned char)(*(ptr + 10)));
         AA[0] = 'X';
         break;
   }
   switch ((unsigned char)(*(ptr + 24))) /* PDS Octet 21 */
   {
      case 0  :
         if ((unsigned char)(*(ptr + 22)) > 0)
         {
            int tr;

            switch ((unsigned char)(*(ptr + 21))) /* PDS Octet 18 */
            {
               case 254: /* Second */
               case 0  : /* Minute */
                  tr = 0;
                  break;
               case 1 : /* Hour */
                  tr = (unsigned char)(*(ptr + 22));
                  break;
               case 2 : /* Day */
                  tr = (unsigned char)(*(ptr + 22)) * 24;
                  break;
               case 10 : /* 3 hours */
                  tr = (unsigned char)(*(ptr + 22)) * 3;
                  break;
               case 11 : /* 6 hours */
                  tr = (unsigned char)(*(ptr + 22)) * 6;
                  break;
               case 12 : /* 12 hours */
                  tr = (unsigned char)(*(ptr + 22)) * 12;
                  break;
               default : /* Impossible */
                  receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                              _("Impossible forecast time unit %d [A2 = A]"),
                              (unsigned char)(*(ptr + 21)));
                  tr = 0;
                  break;
            }
            if (TT[0] == 'H')
            {
               switch (tr)
               {
                  case 0 : /* hour analysis */
                  case 1 : /* (?) */
                  case 2 : /* (?) */
                  case 3 : /* (?) */
                     AA[1] = 'A';
                     break;
                  case 4 : /* (?) */
                  case 5 : /* (?) */
                  case 6 : /* 06 hour fcst */
                  case 7 : /* (?) */
                  case 8 : /* (?) */
                  case 9 : /* (?) */
                     AA[1] = 'B';
                     break;
                  case 10 : /* (?) */
                  case 11 : /* (?) */
                  case 12 : /* 12 hour fcst */
                  case 13 : /* (?) */
                  case 14 : /* (?) */
                  case 15 : /* (?) */
                     AA[1] = 'C';
                     break;
                  case 16 : /* (?) */
                  case 17 : /* (?) */
                  case 18 : /* 18 hour fcst */
                  case 19 : /* (?) */
                  case 20 : /* (?) */
                  case 21 : /* (?) */
                     AA[1] = 'D';
                     break;
                  case 22 : /* (?) */
                  case 23 : /* (?) */
                  case 24 : /* 24 hour fcst */
                  case 25 : /* (?) */
                  case 26 : /* (?) */
                  case 27 : /* (?) */
                     AA[1] = 'E';
                     break;
                  case 28 : /* (?) */
                  case 29 : /* (?) */
                  case 30 : /* 30 hour fcst */
                  case 31 : /* (?) */
                  case 32 : /* (?) */
                  case 33 : /* (?) */
                     AA[1] = 'F';
                     break;
                  case 34 : /* (?) */
                  case 35 : /* (?) */
                  case 36 : /* 36 hour fcst */
                  case 37 : /* (?) */
                  case 38 : /* (?) */
                  case 39 : /* (?) */
                     AA[1] = 'G';
                     break;
                  case 40 : /* (?) */
                  case 41 : /* (?) */
                  case 42 : /* 42 hour fcst */
                  case 43 : /* (?) */
                  case 44 : /* (?) */
                  case 45 : /* (?) */
                     AA[1] = 'H';
                     break;
                  case 46 : /* (?) */
                  case 47 : /* (?) */
                  case 48 : /* 48 hour fcst */
                     AA[1] = 'I';
                     break;
                  case 60 : /* 60 hour fcst */
                     AA[1] = 'J';
                     break;
                  case 72 : /* 72 hour fcst */
                     AA[1] = 'K';
                     break;
                  case 84 : /* 84 hour fcst */
                     AA[1] = 'L';
                     break;
                  case 96 : /* 96 hour fcst */
                     AA[1] = 'M';
                     break;
                  case 108 : /* 108 hour fcst */
                     AA[1] = 'N';
                     break;
                  case 120 : /* 120 hour fcst */
                     AA[1] = 'O';
                     break;
                  case 132 : /* 132 hour fcst */
                     AA[1] = 'P';
                     break;
                  case 144 : /* 144 hour fcst */
                     AA[1] = 'Q';
                     break;
                  case 156 : /* 156 hour fcst */
                     AA[1] = 'R';
                     break;
                  case 168 : /* 168 hour fcst */
                     AA[1] = 'S';
                     break;
                  case 180 : /* 180 hour fcst */
                     AA[1] = 'T';
                     break;
                  case 192 : /* 192 hour fcst */
                     AA[1] = 'U';
                     break;
                  case 204 : /* 204 hour fcst */
                     AA[1] = 'V';
                     break;
                  case 216 : /* 216 hour fcst */
                     AA[1] = 'W';
                     break;
                  case 228 : /* 228 hour fcst */
                     AA[1] = 'X';
                     break;
                  case 240 : /* 240 hour fcst */
                     AA[1] = 'Y';
                     break;
                  default : /* Unknown */
                     receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                                 _("Impossible forecast %d [A2 = Z] [unit = %d]"),
                                 tr, *(ptr + 21));
                     AA[1] = 'Z';
                     break;
               }
            }
            else
            {
               switch (tr)
               {
                  case 0 : /* hour analysis */
                     AA[1] = 'A';
                     break;
                  case 3 : /* 03 hour fcst */
                     AA[1] = 'B';
                     break;
                  case 6 : /* 06 hour fcst */
                     AA[1] = 'C';
                     break;
                  case 9 : /* 9 hour fcst */
                     AA[1] = 'D';
                     break;
                  case 12 : /* 12 hour fcst */
                     AA[1] = 'E';
                     break;
                  case 15 : /* 15 hour fcst */
                     AA[1] = 'F';
                     break;
                  case 18 : /* 18 hour fcst */
                     AA[1] = 'G';
                     break;
                  case 21 : /* 21 hour fcst */
                     AA[1] = 'H';
                     break;
                  case 24 : /* 24 hour fcst */
                     AA[1] = 'I';
                     break;
                  case 27 : /* 27 hour fcst */
                     AA[1] = 'J';
                     break;
                  case 30 : /* 30 hour fcst */
                     AA[1] = 'K';
                     break;
                  case 33 : /* 33 hour fcst */
                     AA[1] = 'L';
                     break;
                  case 36 : /* 36 hour fcst */
                     AA[1] = 'M';
                     break;
                  case 39 : /* 39 hour fcst */
                     AA[1] = 'N';
                     break;
                  case 42 : /* 42 hour fcst */
                     AA[1] = 'O';
                     break;
                  case 45 : /* 45 hour fcst */
                     AA[1] = 'P';
                     break;
                  case 48 : /* 48 hour fcst */
                     AA[1] = 'Q';
                     break;
                  case 54 : /* 54 hour fcst */
                     AA[1] = 'R';
                     break;
                  case 60 : /* 60 hour fcst */
                     AA[1] = 'S';
                     break;
                  case 66 : /* 66 hour fcst */
                     AA[1] = 'T';
                     break;
                  case 72 : /* 72 hour fcst */
                     AA[1] = 'U';
                     break;
                  case 78 : /* 78 hour fcst */
                     AA[1] = 'V';
                     break;
                  default : /* Unknown */
                     receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                                 _("Impossible forecast %d [A2 = Z] [unit = %d]"),
                                 tr, *(ptr + 21));
                     AA[1] = 'Z';
                     break;
               }
            }
         }
         else
         {
            AA[1] = 'A';
         }
         break;
      case 1  :
         AA[1] = 'A';
         break;
      case 2  : /* Time range */
      case 3  : /* Average */
      case 4  : /* Accumulation */
      case 5  : /* Difference */
      case 6  : /* Average */
      case 7  : /* Average */
         /* For these its necessary to point at GRIB PDS. */
         AA[1] = 'Z';
         break;
      default : /* Unknown. */
         receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                     _("Unknown Time Range Indicator %d [A2 = Z]"),
                     (unsigned char)(*(ptr + 24)));
         AA[1] = 'Z';
   }
   AA[2] = '\0';

   /* Level designators ii */
   switch ((unsigned char)(*(ptr + 13))) /* PDS Octet 10 */
   {
      case 1 : /* surface (of the Earth, which includes sea surface) */
         ii[0] = '9'; ii[1] = '8';
         break;
#ifdef _UNKNOWN_
      case 2 : /* cloud base level */
#endif /* _UNKNOWN_ */
      case 3 : /* cloud top level */
         ii[0] = '7'; ii[1] = '4';
         break;
      case 4 : /* 0 deg isotherm level */
         ii[0] = '9'; ii[1] = '4';
         break;
#ifdef _UNKNOWN_
      case 5 : /* adiabatic condensation level */
      case 6 : /* maximum wind speed level */
#endif /* _UNKNOWN_ */
      case 7 : /* tropopause level */
         ii[0] = '9'; ii[1] = '7';
         break;
      case 100 : /* isobaric level */
         {
            int hpa;

            hpa = ((unsigned char)(*(ptr + 14)) * 256) + (unsigned char)(*(ptr + 15));
            if (hpa == 1000)
            {
               ii[0] = '9'; ii[1] = '9';
            }
            else
            {
               hpa = hpa / 10;

               if (hpa < 10)
               {
                  ii[0] = '0'; ii[1] = hpa + '0';
               }
               else
               {
                  ii[0] = (hpa / 10) + '0';
                  ii[1] = (hpa % 10) + '0';
               }
            }
         }
         break;
      case 101 : /* layer between two isobaric levels */
         ii[0] = '8'; ii[1] = '7';
         break;
#ifdef WHEN_WE_KNOW
      case 105 : /* Land/Water Properties at Surface of Earth/Ocean */
         ii[0] = '8'; ii[1] = '8';
         break;
#endif
      case 102 : /* mean sea level */
      case 103 :
         ii[0] = '8'; ii[1] = '9';
         break;
      default : /* Unknown, refer to GRID PDS */
         receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                     _("Unknown Indicator of type of level or layer %d [ii = 01]"),
                     (unsigned char)(*(ptr + 13)));
         ii[0] = '0'; ii[1] = '1';
         break;
   }
   ii[2] = '\0';

   /* Get Originator CCCC. */
   if (default_CCCC == NULL)
   {
      switch ((unsigned char)(*(ptr + 8))) /* PDS Octet 5 */
      {
         case 1  :
         case 2  :
         case 3  : /* Melbourne */
            CCCC[0] = 'A'; CCCC[1] = 'M'; CCCC[2] = 'M'; CCCC[3] = 'C';
            break;
         case 76 :
         case 4  :
         case 5  :
         case 6  : /* Moscow */
            CCCC[0] = 'R'; CCCC[1] = 'U'; CCCC[2] = 'M'; CCCC[3] = 'S';
            break;
         case 7  : /* US Weather Service - National Met. Center */
            /* break; */
         case 8  : /* US National Weather Service Telecommunications Gateway */
            CCCC[0] = 'K'; CCCC[1] = 'W'; CCCC[2] = 'B'; CCCC[3] = 'C';
            break;
         case 10 :
         case 11 : /* Cairo (RSMC/RAFC) */
            CCCC[0] = 'H'; CCCC[1] = 'E'; CCCC[2] = 'C'; CCCC[3] = 'A';
            break;
         case 12 :
         case 13 : /* Dakar (RSMC/RAFC) */
            CCCC[0] = 'G'; CCCC[1] = 'O'; CCCC[2] = 'O'; CCCC[3] = 'O';
            break;
         case 14 :
         case 15 : /* Nairobi (RSMC/RAFC) */
            CCCC[0] = 'H'; CCCC[1] = 'K'; CCCC[2] = 'N'; CCCC[3] = 'C';
            break;
         case 18 :
         case 19 : /* Tunis-Casablanca (RSMC) */
            CCCC[0] = 'D'; CCCC[1] = 'T'; CCCC[2] = 'T'; CCCC[3] = 'A';
            break;
         case 22 : /* Lagos */
            CCCC[0] = 'D'; CCCC[1] = 'N'; CCCC[2] = 'M'; CCCC[3] = 'M';
            break;
         case 24 : /* Pretoria (RSMC) */
            CCCC[0] = 'F'; CCCC[1] = 'A'; CCCC[2] = 'P'; CCCC[3] = 'R';
            break;
         case 28 :
         case 29 : /* New Delhi (RSMC/RAFC) */
            CCCC[0] = 'D'; CCCC[1] = 'E'; CCCC[2] = 'M'; CCCC[3] = 'S';
            break;
         case 30 :
         case 31 : /* Novosibirsk (RSMC) */
            CCCC[0] = 'U'; CCCC[1] = 'N'; CCCC[2] = 'N'; CCCC[3] = 'N';
            break;
         case 32 : /* Tashkent (RSMC) */
            CCCC[0] = 'U'; CCCC[1] = 'T'; CCCC[2] = 'T'; CCCC[3] = 'T';
            break;
         case 33 : /* Jeddah (RSMC) */
            CCCC[0] = 'O'; CCCC[1] = 'E'; CCCC[2] = 'J'; CCCC[3] = 'D';
            break;
         case 34 :
         case 35 : /* Japanese Meteorological Agency - Tokyo */
            CCCC[0] = 'R'; CCCC[1] = 'J'; CCCC[2] = 'T'; CCCC[3] = 'D';
            break;
         case 38 :
         case 39 : /* Beijing (RSMC) */
            CCCC[0] = 'B'; CCCC[1] = 'A'; CCCC[2] = 'B'; CCCC[3] = 'J';
            break;
         case 40 : /* Seoul */
            CCCC[0] = 'R'; CCCC[1] = 'K'; CCCC[2] = 'S'; CCCC[3] = 'L';
            break;
         case 41 :
         case 42 : /* Buenos Aires (RSMC/RAFC) */
            CCCC[0] = 'S'; CCCC[1] = 'A'; CCCC[2] = 'B'; CCCC[3] = 'M';
            break;
         case 43 :
         case 44 : /* Brasilia (RSMC/RAFC) */
               CCCC[0] = 'S'; CCCC[1] = 'B'; CCCC[2] = 'B'; CCCC[3] = 'R';
            break;
         case 45 : /* Santiago */
               CCCC[0] = 'S'; CCCC[1] = 'C'; CCCC[2] = 'T'; CCCC[3] = 'B';
            break;
         case 51 :
         case 52 : /* Miami */
            CCCC[0] = 'K'; CCCC[1] = 'N'; CCCC[2] = 'H'; CCCC[3] = 'C';
            break;
         case 53 :
         case 54 : /* Canadian Meteorological Service - Montreal */
            CCCC[0] = 'C'; CCCC[1] = 'Y'; CCCC[2] = 'U'; CCCC[3] = 'L';
            break;
         case 55 : /* San Francisco */
            CCCC[0] = 'K'; CCCC[1] = 'S'; CCCC[2] = 'F'; CCCC[3] = 'S';
            break;
         case 58 : /* US Navy  - Fleet Numerical Oceanography Center */
            CCCC[0] = 'K'; CCCC[1] = 'N'; CCCC[2] = 'W'; CCCC[3] = 'C';
            break;
         case 59 : /* NOAA Forcast System Laboratory Boulder */
            CCCC[0] = 'K'; CCCC[1] = 'W'; CCCC[2] = 'N'; CCCC[3] = 'P';
            break;
         case 60 : /* Honolulu */
            CCCC[0] = 'P'; CCCC[1] = 'H'; CCCC[2] = 'Z'; CCCC[3] = 'H';
            break;
         case 65 :
         case 66 : /* Darwin (RSMC) */
            CCCC[0] = 'Y'; CCCC[1] = 'D'; CCCC[2] = 'D'; CCCC[3] = 'N';
            break;
         case 67 : /* Melbourne (RSMC) */
            CCCC[0] = 'Y'; CCCC[1] = 'M'; CCCC[2] = 'E'; CCCC[3] = 'N';
            break;
         case 69 :
         case 70 : /* Wellington (RSMC/RAFC) */
            CCCC[0] = 'N'; CCCC[1] = 'Z'; CCCC[2] = 'K'; CCCC[3] = 'L';
            break;
         case 74 :
         case 75 : /* U.K. Met Office - Bracknell */
            CCCC[0] = 'E'; CCCC[1] = 'G'; CCCC[2] = 'R'; CCCC[3] = 'R';
            break;
         case 78 :
         case 79 : /* Offenbach (RSMC) */
            CCCC[0] = 'E'; CCCC[1] = 'D'; CCCC[2] = 'Z'; CCCC[3] = 'W';
            break;
         case 80 :
         case 81 : /* Rome (RSMC) */
            CCCC[0] = 'L'; CCCC[1] = 'I'; CCCC[2] = 'I'; CCCC[3] = 'B';
            break;
         case 82 :
         case 83 : /* Norrkoeping */
            CCCC[0] = 'E'; CCCC[1] = 'S'; CCCC[2] = 'W'; CCCC[3] = 'I';
            break;
         case 216:
         case 84 :
         case 85 : /* Toulouse (RSMC) */
            CCCC[0] = 'L'; CCCC[1] = 'F'; CCCC[2] = 'P'; CCCC[3] = 'W';
            break;
         case 86 : /* Helsinki */
            CCCC[0] = 'E'; CCCC[1] = 'F'; CCCC[2] = 'K'; CCCC[3] = 'L';
            break;
         case 87 : /* Belgrade */
            CCCC[0] = 'L'; CCCC[1] = 'Y'; CCCC[2] = 'B'; CCCC[3] = 'M';
            break;
         case 88 : /* Oslo */
            CCCC[0] = 'E'; CCCC[1] = 'N'; CCCC[2] = 'M'; CCCC[3] = 'I';
            break;
         case 89 : /* Prague */
            CCCC[0] = 'O'; CCCC[1] = 'K'; CCCC[2] = 'P'; CCCC[3] = 'R';
            break;
         case 90 : /* Episkopi */
            CCCC[0] = 'L'; CCCC[1] = 'C'; CCCC[2] = 'R'; CCCC[3] = 'O';
            break;
         case 91 : /* Ankara */
            CCCC[0] = 'L'; CCCC[1] = 'T'; CCCC[2] = 'A'; CCCC[3] = 'A';
            break;
         case 92 : /* Frankfurt/Main (RAFC) */
            CCCC[0] = 'E'; CCCC[1] = 'D'; CCCC[2] = 'Z'; CCCC[3] = 'F';
            break;
         case 93 : /* London (WAFC) */
            CCCC[0] = 'E'; CCCC[1] = 'G'; CCCC[2] = 'R'; CCCC[3] = 'B';
            break;
         case 94 : /* Copenhagen */
            CCCC[0] = 'E'; CCCC[1] = 'K'; CCCC[2] = 'M'; CCCC[3] = 'I';
            break;
         case 95 : /* Rota */
            CCCC[0] = 'L'; CCCC[1] = 'E'; CCCC[2] = 'R'; CCCC[3] = 'T';
            break;
         case 96 : /* Athens */
            CCCC[0] = 'L'; CCCC[1] = 'G'; CCCC[2] = 'A'; CCCC[3] = 'T';
            break;
         case 254:
         case 97 : /* European Space Agency (ESA) */
            CCCC[0] = 'E'; CCCC[1] = 'U'; CCCC[2] = 'M'; CCCC[3] = 'S';
            break;
         case 98 : /* European Center for Medium-Range Weather Forecasts - Reading */
            CCCC[0] = 'E'; CCCC[1] = 'C'; CCCC[2] = 'M'; CCCC[3] = 'F';
            break;
         case 99 : /* De Bilt */
            CCCC[0] = 'E'; CCCC[1] = 'H'; CCCC[2] = 'D'; CCCC[3] = 'B';
            break;
         case 110: /* Hong Kong */
            CCCC[0] = 'V'; CCCC[1] = 'H'; CCCC[2] = 'H'; CCCC[3] = 'H';
            break;
         case 212: /* Lisboa */
            CCCC[0] = 'L'; CCCC[1] = 'P'; CCCC[2] = 'M'; CCCC[3] = 'G';
            break;
         case 214: /* Madrid */
            CCCC[0] = 'L'; CCCC[1] = 'E'; CCCC[2] = 'M'; CCCC[3] = 'M';
            break;
         case 215: /* Zurich */
            CCCC[0] = 'L'; CCCC[1] = 'S'; CCCC[2] = 'S'; CCCC[3] = 'W';
            break;
         default : /* Not found. */
            receive_log(DEBUG_SIGN, __FILE__, __LINE__, 0L,
                        _("Unknown center identifier %d [CCCC = XXXX]"),
                        (unsigned char)(*(ptr + 8)));
            CCCC[0] = 'X'; CCCC[1] = 'X'; CCCC[2] = 'X'; CCCC[3] = 'X';
            break;
      }
   }
   else
   {
      (void)memcpy(CCCC, default_CCCC, 4);
   }
   CCCC[4] = '\0';

   (void)snprintf(TTAAii_CCCC_YYGGgg, 19, "%s%s%s_%s_%02d%02d%02d",
                  TT, AA, ii, CCCC,
                  *(ptr + 18),  /* PDS 15 */
                  *(ptr + 19),  /* PDS 16 */
                  *(ptr + 20)); /* PDS 17 */
   return;
}
