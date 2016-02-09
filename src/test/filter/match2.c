


int
match(char *filter, char *file)
{
   for (;;)
   {
      switch (*filter)
      {
         case '*' :
            return(match(filter + 1, file) ||
                   ((*file != '\0') && match(filter, file + 1)));

         case '?' :
            if (*file == '\0')
            {
               return(0);
            }
            filter++;
            file++;
            break;

         case '\0' :
            if (*file == *filter)
            {
               return(1);
            }
            else
            {
               return(0);
            }

         default :
            if (*filter != *file)
            {
               return(0);
            }
            filter++;
            file++;
            break;
      }
   }
}
