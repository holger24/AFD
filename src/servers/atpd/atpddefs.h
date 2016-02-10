/*
 *  atpddefs - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2015 Deutscher Wetterdienst (DWD),
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

#ifndef __atpddefs_h
#define __atpddefs_h

#define NOT_SET                  -1
#define HUNK_MAX                 4096
#define DEFAULT_ATP_PORT_NO      "5555"
#define ATPD_CMD_TIMEOUT         900
#define ATPD_LOG_CHECK_INTERVAL  2
#define MAX_ATPD_CONNECTIONS     5
#define MAX_ATPD_CONNECTIONS_DEF "MAX_ATPD_CONNECTIONS"

#define DEFAULT_CHECK_INTERVAL   3 /* Default interval in seconds to */
                                   /* check if certain values have   */
                                   /* changed in the FSA.            */

#define HELP_CMD              "HELP\r\n"
#define QUIT_CMD              "QUIT\r\n"
#define NOP_CMD               "NOP"
#define NOP_CMD_LENGTH        (sizeof(NOP_CMD) - 1)
#define NOP_CMDL              "NOP\r\n"

#define QUIT_SYNTAX           "214 Syntax: QUIT (terminate service)"
#define HELP_SYNTAX           "214 Syntax: HELP [ <sp> <string> ]"
#define NOP_SYNTAX            "214 Syntax: NOP (checks if connection is still up)"

#define ATPD_SHUTDOWN_MESSAGE "500 ATPD shutdown."

/* Function prototypes. */
extern int  get_display_data(char *, int, char *, int, int, int, int);
extern void check_changes(FILE *),
            close_get_display_data(void),
            handle_request(int, int, int, char *),
            init_get_display_data(void);

#endif /* __atpddefs_h */
