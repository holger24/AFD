
#include "afddefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir;
const char *sys_log_name = SYSTEM_LOG_FIFO;

int
main(int argc, char *argv[])
{
   int  *counter,
        counter_fd;
   char new_name[MAX_PATH_LENGTH],
        work_dir[MAX_PATH_LENGTH];

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(1);
   }
   p_work_dir = work_dir;
   if (argc != 4)
   {
      (void)fprintf(stderr,
                    "Usage: %s <original name> <filter> <rename to rule>\n",
                    argv[0]);
      exit(1);
   }
   if ((counter_fd = open_counter_file(COUNTER_FILE, &counter)) == -1)
   {
      printf("Failed to open counter file.\n");
      exit(1);
   }
   change_name(argv[1], argv[2], argv[3], new_name, MAX_PATH_LENGTH, &counter_fd, &counter, 0);
   printf("new name = %s\n", new_name);

   exit(0);
}
