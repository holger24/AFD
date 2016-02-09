#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "afddefs.h"

int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ arg() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   char **buffer;
   int  no_of_elements = 0;

   if (get_arg_array(&argc, argv, "-w", &buffer, &no_of_elements) == INCORRECT)
/*
   char buffer[MAX_PATH_LENGTH];

   if (get_arg(&argc, argv, "-w", buffer, 1) == INCORRECT)
*/
   {
      (void)fprintf(stderr, "Argument not in list, or some other error!\n");
      exit(INCORRECT);
   }
   else
   {
      register int i;

/*
      (void)fprintf(stdout, "GOTCHA! %s\n\n", buffer);
*/
      (void)fprintf(stdout, "GOTCHA! %d elements\n", no_of_elements);
      for (i = 0; i < no_of_elements; i++)
      {
         (void)fprintf(stdout, "%s ", buffer[i]);
      }
      (void)fprintf(stdout, "\n\n");
      (void)fprintf(stdout, "Argument list: <%s", argv[0]);
      for (i = 1; i < argc; i++)
      {
         (void)fprintf(stdout, " %s", argv[i]);
      }
      (void)fprintf(stdout, ">\n");
   }

   exit(SUCCESS);
}
