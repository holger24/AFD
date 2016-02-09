
#include "afddefs.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int        mmap_hunk_max,
           sys_log_fd = STDERR_FILENO;
const char *sys_log_name = SYSTEM_LOG_FIFO;

int
main(int argc, char *argv[])
{
   int  i;
   char *p_file,
        target_dir[512];

   if (argc < 3)
   {
      (void)fprintf(stderr,
                    "Usage: %s <filename 1> .... <filename n> <destination dir>\n",
                    argv[0]);
      exit(-1);
   }
   (void)strcpy(target_dir, argv[argc - 1]);
   p_file = target_dir + strlen(target_dir) + 1;
   *(p_file - 1) = '/';

   if ((mmap_hunk_max = sysconf(_SC_PAGESIZE)) < 1)
   {
      (void)fprintf(stderr, "sysconf() error\n");
      exit(-1);
   }
   printf("PAGESIZE = %d   mmap_hunk_max = %d\n", mmap_hunk_max, mmap_hunk_max*100);
   mmap_hunk_max *= 100;

   for (i = 1; i < (argc - 1); i++)
   {
      (void)strcpy(p_file, argv[i]);
      if (copy_file(argv[i], target_dir, NULL) < 0)
      {
         (void)fprintf(stderr, "Failed to copy file.\n");
         exit(-1);
      }
   }
   printf("Done!\n");

   exit(0);
}
