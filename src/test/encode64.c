#include "afddefs.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

int        line_length = 0,
           sys_log_fd = STDERR_FILENO;
const char *sys_log_name = SYSTEM_LOG_FIFO;

extern int encode_base64(unsigned char *, int, unsigned char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int         fd,
               i,
               length,
               loops,
               rest;
   char        buffer[333333],
               convert_buffer[666666];
   struct stat stat_buf;

   if (argc != 2)
   {
      (void)fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
      exit(INCORRECT);
   }

   if ((fd = open(argv[1], O_RDONLY)) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "Could not open %s : %s (%s %d)\n",
                argv[1], strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (fstat(fd, &stat_buf) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Could not fstat() %s : %s (%s %d)\n",
                argv[1], strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      exit(INCORRECT);
   }

   loops = stat_buf.st_size / 3333;
   rest = stat_buf.st_size % 3333;
   for (i = 0; i < loops; i++)
   {
      if (read(fd, buffer, 3333) != 3333)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "Failed to read %s : %s (%s %d)\n",
                   argv[1], strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         exit(INCORRECT);
      }
      length = encode_base64(buffer, 3333, convert_buffer);
      convert_buffer[length] = '\0';
      (void)printf("%s", convert_buffer);
   }
   if (rest > 0)
   {
      if (read(fd, buffer, rest) != rest)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "Failed to read %s : %s (%s %d)\n",
                   argv[1], strerror(errno), __FILE__, __LINE__);
         (void)close(fd);
         exit(INCORRECT);
      }
      length = encode_base64(buffer, rest, convert_buffer);
      convert_buffer[length] = '\0';
      (void)printf("%s", convert_buffer);
   }
   (void)printf("\n");

   exit(SUCCESS);
}
