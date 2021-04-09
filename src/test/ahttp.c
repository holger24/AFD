#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "afddefs.h"
#include "fddefs.h"
#include "httpdefs.h"

int        sys_log_fd = STDERR_FILENO,
           timeout_flag;
long       transfer_timeout = 30L;
char       msg_str[MAX_RET_MSG_LENGTH],
           *p_work_dir,
           tr_hostname[MAX_HOSTNAME_LENGTH + 1];
struct job db;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ ahttp $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int   bytes_buffered,
         fd,
         hunk_size;
   off_t bytes_read = 0;
   char  block[1024],
#ifdef _WITH_EXTRA_CHECK
         etag[MAX_EXTRA_LS_DATA_LENGTH + 1],
#endif
         hostname[45],
         path[MAX_PATH_LENGTH];

   if ((argc != 2) && (argc != 3) && (argc != 4))
   {
      (void)fprintf(stderr, "Usage: %s <filename> [<host> [<path>]]\n", argv[0]);
      exit(-1);
   }
   if (argc == 4)
   {
      (void)strncpy(hostname, argv[2], 44);
      (void)strncpy(path, argv[3], MAX_PATH_LENGTH - 1);
   }
   else if (argc == 3)
        {
           (void)strncpy(hostname, argv[2], 44);
           (void)strcpy(path, "/");
        }
        else
        {
           (void)strcpy(hostname, "localhost");
           (void)strcpy(path, "/");
        }
   if (http_connect(hostname, DEFAULT_HTTP_PORT) == -1)
   {
      (void)fprintf(stderr, "http_connect() failed\n");
      exit(-1);
   }
#ifdef _WITH_EXTRA_CHECK
   etag[0] = '\0';
   if (http_get(hostname, path, argv[1], etag, &bytes_buffered) == INCORRECT)
#else
   if (http_get(hostname, path, argv[1], &bytes_buffered) == INCORRECT)
#endif
   {
      (void)fprintf(stderr, "http_get() failed\n");
      exit(-1);
   }
   if ((fd = open(argv[1], (O_WRONLY | O_CREAT | O_TRUNC),
                  (S_IRUSR | S_IWUSR))) == -1)
   {
      (void)fprintf(stderr, "Failed to open() %s : %s\n",
                    argv[1], strerror(errno));
      exit(-1);
   }
   if (bytes_buffered > 0)
   {
      bytes_read += bytes_buffered;
      if (write(fd, msg_str, bytes_buffered) != bytes_buffered)
      {
         (void)fprintf(stderr, "write() error : %s\n", strerror(errno));
         exit(-1);
      }
   }
   while ((hunk_size = http_read(block, 1024)) > 0)
   {
      if (write(fd, block, hunk_size) != hunk_size)
      {
         (void)fprintf(stderr, "write() error : %s\n", strerror(errno));
         exit(-1);
      }
      bytes_read += hunk_size;
   }
   if (close(fd) == -1)
   {
      (void)fprintf(stderr, "close() error : %s\n", strerror(errno));
   }
   (void)fprintf(stdout, "Got file %s with %d Bytes.\n", argv[1], bytes_read);

   return(0);
}
