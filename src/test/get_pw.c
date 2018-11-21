
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "afddefs.h"


int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir;
const char *sys_log_name = SYSTEM_LOG_FIFO;

int
main(int argc, char *argv[])
{
   int           ret;
   char          password[MAX_USER_NAME_LENGTH],
                 *p_uh_name,
                 work_dir[MAX_PATH_LENGTH];

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(1);
   }
   p_work_dir = work_dir;
   if (argc == 1)
   {
      (void)fprintf(stderr, "Usage: %s <uh_name>\n", argv[0]);
      exit(1);
   }
   else
   {
      p_uh_name = argv[1];
   }

   ret = get_pw(p_uh_name, password, NO);
   (void)printf("ret=%d uh_name=%s password=`%s'\n", ret, p_uh_name, password);

   return(0);
}
