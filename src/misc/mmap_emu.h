/*
 *  mmap_emu.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1994 - 2020 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __mmap_emu_h
#define __mmap_emu_h

#define SHM_MODE              (SHM_R | SHM_W)
#ifndef FILE_MODE
# define FILE_MODE            (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#endif
#define MAX_MAPPED_REGIONS    100
#ifdef ULTRIX
#define MAX_ALLOWED_SHM_SIZE  2097152
#else
#define MAX_ALLOWED_SHM_SIZE  3670016
#endif
#define BUFSIZE               1050
#define STEP_SIZE             100
#define REQUEST_FIFO          "/request.fifo"

struct   map
         {
            int      shmid;
            size_t   size;
            size_t   crc_size;
            size_t   step_size;
            char     *initial_crc_ptr;
            char     *actual_crc_ptr;
            char     *shmptr;
            char     filename[MAX_PATH_LENGTH];
         };
#endif
