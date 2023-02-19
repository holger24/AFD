/*
 *  version.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2023 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __version_h
#define __version_h

#define VERSION_ID     "--version"
#define AFD_MAINTAINER "Holger.Kiehl@dwd.de"

#define COPYRIGHT_0 "\n\n   Copyright (C) 1995 - 2023 Deutscher Wetterdienst, Holger Kiehl.\n\n"
#define COPYRIGHT_1 "   This program is free software;  you can redistribute  it and/or\n"
#define COPYRIGHT_2 "   modify it under the terms set out in the  LICENSE  file,  which\n"
#define COPYRIGHT_3 "   is included in the AFD source distribution.\n\n"
#define COPYRIGHT_4 "   This program is distributed in the hope that it will be useful,\n"
#define COPYRIGHT_5 "   but WITHOUT ANY WARRANTY;  without even the implied warranty of\n"
#define COPYRIGHT_6 "   MERCHANTABILITY  or  FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
#define COPYRIGHT_7 "   LICENSE  file for more details.\n\n"

/* Print current version and compilation time. */
#define PRINT_VERSION(prog)                                                  \
        {                                                                    \
           (void)fprintf(stderr,                                             \
                         "%s -- Version: %s <%s> %6.6s %s [AFD]%s%s%s%s%s%s%s%s",\
                         (prog), PACKAGE_VERSION, AFD_MAINTAINER, __DATE__,  \
                         __TIME__, COPYRIGHT_0, COPYRIGHT_1, COPYRIGHT_2,    \
                         COPYRIGHT_3, COPYRIGHT_4, COPYRIGHT_5, COPYRIGHT_6, \
                         COPYRIGHT_7);             \
        }

#define CHECK_FOR_VERSION(argc, argv)              \
        {                                          \
           if (argc == 2)                          \
           {                                       \
              if (my_strcmp(argv[1], VERSION_ID) == 0)\
              {                                    \
                 PRINT_VERSION(argv[0]);           \
                 exit(SUCCESS);                    \
              }                                    \
           }                                       \
        }

#endif /* __version_h */
