


int
filter(char *p_filter, char *p_file)
{
   register int inverse = 0,
                sc = 0;
   char         *tmp_p_filter[10] = { NULL, NULL, NULL, NULL, NULL,
                                    NULL, NULL, NULL, NULL, NULL },
                *tmp_p_file[10];

   if (*p_filter == '!')
   {
      p_filter++;
      inverse = 1;
   }

   for (;;)
   {
      switch (*p_filter)
      {
         case '*' :
            if (tmp_p_filter[sc] == NULL)
            {
               tmp_p_filter[sc] = p_filter;
               tmp_p_file[sc] = p_file;
               do
               {
                  p_filter++;
               } while (*p_filter == '*');
               sc++;
            }
            else
            {
               if (*p_file != '\0')
               {
                  p_filter = tmp_p_filter[sc];
                  p_file = tmp_p_file[sc];
                  p_file++;
                  tmp_p_filter[sc] = NULL;
               }
               else
               {
                  return((inverse == 0) ? 0 : 1);
               }
            }
            break;

         case '?' :
            if (*p_file == '\0')
            {
               if ((sc == 0) || (tmp_p_filter[sc - 1] == NULL))
               {
                  return((inverse == 0) ? 0 : 1);
               }
               else
               {
                  sc--;
                  p_filter = tmp_p_filter[sc];
                  p_file = tmp_p_file[sc];
                  break;
               }
            }
            p_filter++;
            p_file++;
            break;

         case '\0' :
            if (*p_file == *p_filter)
            {
               return((inverse == 0) ? 1 : 0);
            }
            else
            {
               if ((sc == 0) || (tmp_p_filter[sc - 1] == NULL))
               {
                  return((inverse == 0) ? 0 : 1);
               }
               else
               {
                  sc--;
                  p_filter = tmp_p_filter[sc];
                  p_file = tmp_p_file[sc];
                  break;
               }
            }

         default :
            if (*p_filter != *p_file)
            {
               if ((sc == 0) || (tmp_p_filter[sc - 1] == NULL))
               {
                  return((inverse == 0) ? 0 : 1);
               }
               else
               {
                  sc--;
                  p_filter = tmp_p_filter[sc];
                  p_file = tmp_p_file[sc];
                  break;
               }
            }
            p_filter++;
            p_file++;
            break;
      }
   }
}
