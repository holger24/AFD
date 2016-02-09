#include <stdio.h>
#include <string.h>

extern int filter(char *, char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ tfilter() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int          ret;
   register int loops = 0;

   if ((argc == 5) && (argv[1][0] == '-') && (argv[1][1] == 'l'))
   {
      loops = atoi(argv[2]);
      argv += 2;
   }
   else if ((argc != 3) ||
            ((argc == 3) && ((argv[1][0] == '-') || (argv[2][0] == '-'))))
        {
           (void)fprintf(stderr,
                         "Usage: %s [-l <loops>] <filter> <file-name>\n",
                         argv[0]);
           exit(1);
        }

   if (loops > 0)
   {
      register int i;

      for (i = 0; i < loops; i++)
      {
         (void)sfilter(argv[1], argv[2]);
      }
   }
   if ((ret = sfilter(argv[1], argv[2])) == 0)
   {
      (void)fprintf(stdout, "GOTHAAA!!!!\n");
   }
   else
   {
      (void)fprintf(stdout, "MISS (%d)\n", ret);
   }

   exit(0);
}
