#include <stdio.h>
#include <unistd.h>
#include "afddefs.h"

#define SLEEP_TIME 120L

int        sys_log_fd = STDOUT_FILENO;
char       *p_work_dir;
const char *sys_log_name = SYSTEM_LOG_FIFO;


int
main(int argc, char *argv[])
{
   if (argc != 2)
   {
      (void)fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
      return(1);
   }
   if (lock_file(argv[1], OFF) == LOCK_IS_SET)
   {
      (void)fprintf(stdout, "File %s is locked!\n", argv[1]);
   }
   else
   {
      (void)fprintf(stdout, "Locked file %s and sleep for %ld seconds...\n",
                    argv[1], SLEEP_TIME);
      (void)sleep(SLEEP_TIME);
   }
   return(0);
}
