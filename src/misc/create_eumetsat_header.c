/*
 *  create_eumetsat_header.c - Part of AFD, an automatic file distribution
 *                             program.
 *  Copyright (c) 1999 - 2014 Deutscher Wetterdienst (DWD),
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
 **   create_eumetsat_header - generates an EUMETSAT header
 **
 ** SYNOPSIS
 **   char *create_eumetsat_header(unsigned char *source_cpu_id,
 **                                unsigned char dest_env_id,
 **                                unsigned int  data_length,
 **                                time_t        file_time,
 **                                size_t        *header_size)
 **
 ** DESCRIPTION
 **   This function creates the header and subheader for EUMETSAT
 **   according to "MSG Ground Segment Design Specification Volume
 **   F). Both headers will be written in a buffer which this
 **   function allocates and return them to the caller.
 **
 ** RETURN VALUES
 **   Returns NULL if it fails to allocate memory for the header.
 **   Otherwise it returns a pointer to the memory holding the header
 **   and the length of the header in the variable 'header_size'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.02.1999 H.Kiehl  Created
 **   25.02.1999 H.Lepper Time conversion for CDS time.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#endif
#include "eumetsat_header_defs.h"


/*####################### create_eumetsat_header() ######################*/
char *
create_eumetsat_header(unsigned char *source_cpu_id, /* 4 unsigned chars */
                       unsigned char dest_env_id,
                       unsigned int  data_length,
                       time_t        file_time,
                       size_t        *header_size)
{
   int            byte_order = 1,
                  sub_header_length;
   unsigned int   milliseconds_of_day,
                  offset;
   unsigned short day;
   unsigned char  *p_header,
                  source_cpu_id_str[16];
   struct tm      *p_tm;

   sub_header_length = 1 +     /* Sub Header Version Number */
                       1 +     /* Service Type */
                       1 +     /* Service Sub Type */
                       2 + 4 + /* File Time */
                       2 +     /* Spacecraft ID */
                       ((2 + MAX_FIELD_NAME_LENGTH + 15) * 5) + 2;
                               /* Textual description */

   *header_size = 1 +          /* Header Version Number */
                  1 +          /* File Type */
                  1 +          /* Subheader Type */
                  1 +          /* Source Facility ID */
                  1 +          /* Source Facility Environment */
                  1 +          /* Source SU Instance ID */
                  4 +          /* Source SU ID */
                  4 +          /* Source CPU ID */
                  1 +          /* Destination Facility ID */
                  1 +          /* Destination Facility Environment */
                  4 +          /* Data Field Length */
                  ((2 + MAX_FIELD_NAME_LENGTH + 15) * 11) + 2 +
                               /* Textual description */
                  sub_header_length;

   data_length = data_length + sub_header_length - 1;

   if ((p_header = malloc(*header_size)) == NULL)
   {
      return(NULL);
   }

   *p_header = HEADER_VERSION_NO;
   *(p_header + 1) = 2;    /* file_type          : Mission Data    */
   *(p_header + 2) = 1;    /* sub_header_type    : GP_FI_SH1       */
   *(p_header + 3) = 131;  /* source_facility_id : RTH             */
   *(p_header + 4) = 0;    /* source_env_id      : NoEnvironment   */
   *(p_header + 5) = 0;    /* source_instance_id : 0               */
   *(p_header + 6) = 0;    /* source_su_id       : 0               */
   *(p_header + 7) = 0;
   *(p_header + 8) = 0;
   *(p_header + 9) = 0;
   *(p_header + 10) = source_cpu_id[0];
   *(p_header + 11) = source_cpu_id[1];
   *(p_header + 12) = source_cpu_id[2];
   *(p_header + 13) = source_cpu_id[3];
   *(p_header + 14) = 3;   /* dest_facility_id   : DADF            */
   *(p_header + 15) = dest_env_id;

   /* MSB first! */
   if (*(char *)&byte_order == 1)
   {
      /* Swap bytes */
      *(p_header + 16) = ((char *)&data_length)[3];
      *(p_header + 17) = ((char *)&data_length)[2];
      *(p_header + 18) = ((char *)&data_length)[1];
      *(p_header + 19) = ((char *)&data_length)[0];
   }
   else
   {
      *(p_header + 16) = ((char *)&data_length)[0];
      *(p_header + 17) = ((char *)&data_length)[1];
      *(p_header + 18) = ((char *)&data_length)[2];
      *(p_header + 19) = ((char *)&data_length)[3];
   }

   offset = 20;
   offset += snprintf((char *)(p_header + offset), *header_size - offset,
                      "\r\n%-*s%-15d",
                      MAX_FIELD_NAME_LENGTH, HEADER_VERSION_NO_NAME,
                      HEADER_VERSION_NO);
   offset += snprintf((char *)(p_header + offset), *header_size - offset,
                      "\r\n%-*s%-15d",
                      MAX_FIELD_NAME_LENGTH, FILE_TYPE, 2);
   offset += snprintf((char *)(p_header + offset), *header_size - offset,
                      "\r\n%-*s%-15d",
                      MAX_FIELD_NAME_LENGTH, SUB_HEADER_TYPE, 1);
   offset += snprintf((char *)(p_header + offset), *header_size - offset,
                      "\r\n%-*s%-15d",
                      MAX_FIELD_NAME_LENGTH, SOURCE_FACILITY_ID, 131);
   offset += snprintf((char *)(p_header + offset), *header_size - offset,
                      "\r\n%-*s%-15d",
                      MAX_FIELD_NAME_LENGTH, SOURCE_ENV_ID, 0);
   offset += snprintf((char *)(p_header + offset), *header_size - offset,
                      "\r\n%-*s%-15d",
                      MAX_FIELD_NAME_LENGTH, SOURCE_INSTANCE_ID, 0);
   offset += snprintf((char *)(p_header + offset), *header_size - offset,
                      "\r\n%-*s%-15d",
                      MAX_FIELD_NAME_LENGTH, SOURCE_SU_ID, 0);
   (void)snprintf((char *)source_cpu_id_str, 16, "%u.%u.%u.%u",
                  (unsigned int)source_cpu_id[0],
                  (unsigned int)source_cpu_id[1],
                  (unsigned int)source_cpu_id[2],
                  (unsigned int)source_cpu_id[3]);
   offset += snprintf((char *)(p_header + offset), *header_size - offset,
                      "\r\n%-*s%-15s",
                      MAX_FIELD_NAME_LENGTH, SOURCE_CPU_ID, source_cpu_id_str);
   offset += snprintf((char *)(p_header + offset), *header_size - offset,
                      "\r\n%-*s%-15d",
                      MAX_FIELD_NAME_LENGTH, DEST_FACILITY_ID, 3);
   offset += snprintf((char *)(p_header + offset), *header_size - offset,
                      "\r\n%-*s%-15d",
                      MAX_FIELD_NAME_LENGTH, DEST_ENV_ID,
                      (int)dest_env_id);
   offset += snprintf((char *)(p_header + offset), *header_size - offset,
                      "\r\n%-*s%-15d\r\n",
                      MAX_FIELD_NAME_LENGTH, DATA_FIELD_LENGTH,
                      (int)data_length);

   *(p_header + offset) = 0;       /* sub_header_version_no : 0               */
   *(p_header + offset + 1) = 162; /* service_type          : GTSDataDelivery */
   *(p_header + offset + 2) = 1;   /* service_sub_type      : 1               */

   p_tm = gmtime(&file_time);
   day = (file_time / 86400) + ((70 - 58) * 365) + 3;
   milliseconds_of_day = ((((p_tm->tm_hour * 60) + p_tm->tm_min) * 60) +
                          p_tm->tm_sec) * 1000;
   if (*(char *)&byte_order == 1)
   {
      /* Swap bytes */
      *(p_header + offset + 3) = ((char *)&day)[1];
      *(p_header + offset + 4) = ((char *)&day)[0];

      *(p_header + offset + 5) = ((char *)&milliseconds_of_day)[3];
      *(p_header + offset + 6) = ((char *)&milliseconds_of_day)[2];
      *(p_header + offset + 7) = ((char *)&milliseconds_of_day)[1];
      *(p_header + offset + 8) = ((char *)&milliseconds_of_day)[0];
   }
   else
   {
      *(p_header + offset + 3) = ((char *)&day)[0];
      *(p_header + offset + 4) = ((char *)&day)[1];

      *(p_header + offset + 5) = ((char *)&milliseconds_of_day)[0];
      *(p_header + offset + 6) = ((char *)&milliseconds_of_day)[1];
      *(p_header + offset + 7) = ((char *)&milliseconds_of_day)[2];
      *(p_header + offset + 8) = ((char *)&milliseconds_of_day)[3];
   }

   *(p_header + offset + 9) = 0;   /* spacecraft_id         : NoSpacecraft    */
   *(p_header + offset + 10) = 0;

   offset += 11;
   offset += snprintf((char *)(p_header + offset), *header_size - offset,
                      "\r\n%-*s%-15d",
                      MAX_FIELD_NAME_LENGTH, SUB_HEADER_VERSION_NO, 0);
   offset += snprintf((char *)(p_header + offset), *header_size - offset,
                      "\r\n%-*s%-15d",
                      MAX_FIELD_NAME_LENGTH, SERVICE_TYPE, 162);
   offset += snprintf((char *)(p_header + offset), *header_size - offset,
                      "\r\n%-*s%-15d",
                      MAX_FIELD_NAME_LENGTH, SERVICE_SUB_TYPE, 1);
   offset += snprintf((char *)(p_header + offset), *header_size - offset,
                      "\r\n%-*s%.6d:%.8d",
                      MAX_FIELD_NAME_LENGTH, FILE_TIME, day,
                      milliseconds_of_day);
   (void)snprintf((char *)(p_header + offset), *header_size - offset,
                  "\r\n%-*s%-15d\r\n",
                  MAX_FIELD_NAME_LENGTH, SPACECRAFT_ID, 0);

   return((char *)p_header);
}
