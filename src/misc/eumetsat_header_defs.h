/*
 *  eumetsat_header_defs.h - Part of AFD, an automatic file distribution
 *                           program.
 *  Copyright (c) 1999 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __eumetsat_header_defs_h
#define __eumetsat_header_defs_h

#define HEADER_VERSION_NO          0

#define MAX_FIELD_NAME_LENGTH      20

/* Names for the description fields for header. */
#define HEADER_VERSION_NO_NAME     "HeaderVersionNo"
#define FILE_TYPE                  "FileType"
#define SUB_HEADER_TYPE            "SubHeaderType"
#define SOURCE_FACILITY_ID         "SourceFacilityId"
#define SOURCE_ENV_ID              "SourceEnvId"
#define SOURCE_INSTANCE_ID         "SourceInstanceId"
#define SOURCE_SU_ID               "SourceSUId"
#define SOURCE_CPU_ID              "SourceCPUId"
#define DEST_FACILITY_ID           "DestFacilityId"
#define DEST_ENV_ID                "DestEnvId"
#define DATA_FIELD_LENGTH          "DataFieldLength"

/* Names for the description fields for subheader. */
#define SUB_HEADER_VERSION_NO      "SubheaderVersionNo"
#define SERVICE_TYPE               "ServiceType"
#define SERVICE_SUB_TYPE           "ServiceSubtype"
#define FILE_TIME                  "FileTime"
#define SPACECRAFT_ID              "SpacecraftId"

/* Function prototype */
extern char *create_eumetsat_header(unsigned char *, unsigned char,
                                    unsigned int, time_t, size_t *);

#endif /* __eumetsat_header_defs_h */
