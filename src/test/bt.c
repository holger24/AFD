
#include <stdio.h>
#include <time.h>


int
main(int argc, char *argv[])
{
   time_t    current_time,
             new_time;
   struct tm *bd_time;

   if (argc != 2)
   {
      (void)fprintf(stderr, "Usage: %s <modifier>\n", argv[0]);
      exit(1);
   }
   current_time = time((time_t *)0);
   bd_time = localtime(&current_time);
   bd_time->tm_min = atoi(argv[1]);

   new_time = mktime(bd_time) + timezone;

   (void)fprintf(stdout, "TIME: %.24s %s %s\n", ctime(&new_time), tzname[0], tzname[1]);

   exit(0);
}
