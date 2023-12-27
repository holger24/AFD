/*
 * ftpparse.c, ftpparse.h: library for parsing FTP LIST responses
 * 20001223
 *
 * D. J. Bernstein, djb@cr.yp.to
 * http://cr.yp.to/ftpparse.html
 *
 * Commercial use is fine, if you let me know what programs you're using
 * this in.
 */

struct ftpparse
       {
          char   *name; /* not necessarily 0-terminated */
          char   *id; /* not necessarily 0-terminated */
          int    namelen;
          int    flagtrycwd; /* 0 if cwd is definitely pointless, 1 otherwise */
          int    flagtryretr; /* 0 if retr is definitely pointless, 1 otherwise */
          int    sizetype;
          int    mtimetype;
          int    idtype;
          int    idlen;
       };

#define FTPPARSE_SIZE_UNKNOWN 0
#define FTPPARSE_SIZE_BINARY 1 /* size is the number of octets in TYPE I */
#define FTPPARSE_SIZE_ASCII 2 /* size is the number of octets in TYPE A */

#define FTPPARSE_MTIME_UNKNOWN 0
#define FTPPARSE_MTIME_LOCAL 1 /* time is correct */
#define FTPPARSE_MTIME_REMOTEMINUTE 2 /* time zone and secs are unknown */
#define FTPPARSE_MTIME_REMOTEDAY 3 /* time zone and time of day are unknown */
/*
 * When a time zone is unknown, it is assumed to be GMT. You may want
 * to use localtime() for LOCAL times, along with an indication that the
 * time is correct in the local time zone, and gmtime() for REMOTE* times.
*/

#define FTPPARSE_ID_UNKNOWN 0
#define FTPPARSE_ID_FULL 1 /* unique identifier for files on this FTP server */


/* Function prototype */
extern int ftpparse(struct ftpparse *, off_t *, int *, time_t *, int *,
                    char *, int);
