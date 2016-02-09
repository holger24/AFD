
#include <stdio.h>


int
main(int argc, char argv[])
{
   int i = 1;

   if (*(char *)&i == 1)
   {
      (void)printf("little-endian | low byte first | LSB\n");
   }
   else
   {
      (void)printf("big-endian | high byte first | MSB\n");
   }

   exit(0);
}
