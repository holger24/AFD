#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "afddefs.h"
#include "fddefs.h"
#include "scp1defs.h"

int    timeout_flag;
long   transfer_timeout = 30L;
char   msg_str[MAX_RET_MSG_LENGTH],
       tr_hostname[MAX_HOSTNAME_LENGTH + 1];
struct job db;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ ascp1 $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int         i,
               fd,
               loops,
               rest;
   char        block[1024];
   struct stat stat_buf;

   if (argc != 2)
   {
      (void)fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
      exit(-1);
   }
   if (scp_connect("localhost", 22, NULL, NULL, ".") == -1)
   {
      (void)fprintf(stderr, "scp_connect() failed\n");
      exit(-1);
   }
   if ((fd = open(argv[1], O_RDONLY)) == -1)
   {
      (void)fprintf(stderr, "Failed to open() %s : %s\n",
                    argv[1], strerror(errno));
      exit(-1);
   }
   if (fstat(fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr, "Failed to fstat() %s : %s\n",
                    argv[1], strerror(errno));
      exit(-1);
   }
   if (scp_open_file(argv[1], stat_buf.st_size, stat_buf.st_mode) == -1)
   {
      (void)fprintf(stderr, "scp_open_file() %s failed\n", argv[1]);
      exit(-1);
   }
   loops = stat_buf.st_size / 1024;
   rest = stat_buf.st_size % 1024;
   fprintf(stdout, "\n");
   for (i = 0; i < loops; i++)
   {
      if (read(fd, block, 1024) != 1024)
      {
         (void)fprintf(stderr, "Failed to read() from %s : %s\n",
                       argv[1], strerror(errno));
         exit(-1);
      }
      if (scp_write(block, 1024) == -1)
      {
         (void)fprintf(stderr, "scp_write() %s failed\n", argv[1]);
         exit(-1);
      }
      fprintf(stdout, "*");
      fflush(stdout);
   }
   if (rest > 0)
   {
      if (read(fd, block, rest) != rest)
      {
         (void)fprintf(stderr, "Failed to read() from %s : %s\n",
                       argv[1], strerror(errno));
         exit(-1);
      }
      if (scp_write(block, rest) == -1)
      {
         (void)fprintf(stderr, "scp_write() %s failed\n", argv[1]);
         exit(-1);
      }
   }
   fprintf(stdout, "*\n");
   fflush(stdout);
   (void)close(fd);
   if (scp_close_file() == -1)
   {
      (void)fprintf(stderr, "scp_close_file() %s failed\n", argv[1]);
      exit(-1);
   }
   scp_quit();

   return(0);
}
