

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define FILE_SIZE   10240
#define NO_OF_FILES 10000

int
main(void)
{
   int  fd,
        i = 0;
   char file_name[20];

   for (;;)
   {
      (void)sprintf(file_name, ".%d", i);
      if ((fd = open(file_name, O_RDWR | O_CREAT | O_TRUNC,
                     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1)
      {
         (void)fprintf(stderr, "Failed to open() %s : %s\n",
                       file_name, strerror(errno));
         exit(1);
      }
      if (lseek(fd, FILE_SIZE - 1, SEEK_SET) == -1)
      {
         (void)fprintf(stderr, "Failed to lseek() in %s : %s\n",
                       file_name, strerror(errno));
         exit(1);
      }
      if (write(fd, "", 1) != 1)
      {
         (void)fprintf(stderr, "Failed to write() to %s : %s\n",
                       file_name, strerror(errno));
         exit(1);
      }
      if (close(fd) == -1)
      {
         (void)fprintf(stderr, "Failed to close() %s : %s\n",
                       file_name, strerror(errno));
      }
      if (rename(file_name, &file_name[1]) == -1)
      {
         (void)fprintf(stderr, "Failed to rename() %s to %s : %s\n",
                       file_name, &file_name[1], strerror(errno));
         exit(1);
      }
      i++;
      if (i == NO_OF_FILES)
      {
         i = 0;
      }
   }

   exit(0);
}
