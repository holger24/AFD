

#include "afddefs.h"
#include "amgdefs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int         sys_log_fd = STDERR_FILENO;
char        *p_work_dir;
const char  *sys_log_name = SYSTEM_LOG_FIFO;

static char formats[3][5] =
            {
               "GRIB",
               "BUFR",
               "BLOK"
            };

static int  get_number_of_fields(char *);
static char *search_start(char *, int, int *, int *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int dummy1;
   off_t dummy2;
   char work_dir[MAX_PATH_LENGTH];

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

/*
   while (argc > 1)
   {
*/
      get_number_of_fields(argv[1]);
/*
      argv++; argc--;
   }

#ifdef _PRODUCTION_LOG
   bin_file_chopper(argv[1], &dummy1, &dummy2, NO, NULL, NULL, NULL);
#else
   bin_file_chopper(argv[1], &dummy1, &dummy2, NO);
#endif
*/

   exit(0);
}


/*+++++++++++++++++++++++ get_number_of_fields() ++++++++++++++++++++++++*/
static int
get_number_of_fields(char *bin_file)
{
  int          counter[3],
               fd,
               message_length,
               total_length,
               total_message_length = 0,
               type;
  char         *buffer,
               *ptr;
#ifdef HAVE_STATX
  struct statx stat_buf;
#else
  struct stat  stat_buf;
#endif

  counter[0] = counter[1] = counter[2] = 0;

#ifdef HAVE_STATX
  if (statx(0, bin_file, AT_STATX_SYNC_AS_STAT, STATX_SIZE, &stat_buf) != 0)
#else
  if (stat(bin_file, &stat_buf) != 0)
#endif
   {
      if (errno == ENOENT)
      {
         /*
          * If the file is not there, why should we bother?
          */
         (void)fprintf(stderr, "Failed to access %s : %s (%s %d)\n",
                       bin_file, strerror(errno), __FILE__, __LINE__);
         return(SUCCESS);
      }
      else
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "Failed to access %s : %s (%s %d)\n",
                   bin_file, strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
   }

   /*
    * If the size of the file is less then 10 forget it. There must be
    * something really wrong.
    */
#ifdef HAVE_STATX
   if (stat_buf.stx_size < 10)
#else
   if (stat_buf.st_size < 10)
#endif
   {
      return(INCORRECT);
   }
   if ((fd = open(bin_file, O_RDONLY)) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "Failed to open() %s : %s (%s %d)\n",
                bin_file, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

#ifdef HAVE_STATX
   if ((buffer = malloc(stat_buf.stx_size)) == NULL)
#else
   if ((buffer = malloc(stat_buf.st_size)) == NULL)
#endif
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "malloc() error (size = %d) : %s (%s %d)\n",
#ifdef HAVE_STATX
                stat_buf.stx_size,
#else
                stat_buf.st_size,
#endif
                strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      return(INCORRECT);
   }

   /*
    * Read the hole file in one go. We can do this here since the
    * files in question are not larger then appr. 500KB.
    */
#ifdef HAVE_STATX
   if (read(fd, buffer, stat_buf.stx_size) != stat_buf.stx_size)
#else
   if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
#endif
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "read() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      free(buffer);
      return(INCORRECT);
   }

   /* Close the file since we do not need it anymore. */
   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
#ifdef HAVE_STATX
   total_length = stat_buf.stx_size;
#else
   total_length = stat_buf.st_size;
#endif

   ptr = buffer;

   while ((ptr = search_start(ptr, total_length, &type, &total_length)) != NULL)
   {
      message_length = 0;
      message_length |= (unsigned char)*ptr;
      message_length <<= 8;
      message_length |= (unsigned char)*(ptr + 1);
      message_length <<= 8;
      message_length |= (unsigned char)*(ptr + 2);
      if (message_length % 2)
      {
         (void)fprintf(stderr, "message_length = %d\n", message_length);
      }
      total_message_length += message_length;
      counter[type]++;
   }
   (void)fprintf(stderr, "Found:  %d GRIB   %d BUFR   %d BLOK  Total Length = %d\n",
                 counter[0], counter[1], counter[2], total_message_length);
   free(buffer);
   
   return(SUCCESS);
}



/*+++++++++++++++++++++++++++ search_start() ++++++++++++++++++++++++++++*/
static char *
search_start(char *search_text,
             int  search_length,
             int  *i,
             int  *total_length)
{
   int    hit[3] = { 0, 0, 0 },
          count[3] = { 0, 0, 0 },
          counter = 0,
          tmp_length = *total_length;

   while (counter != search_length)
   {
      for (*i = 0; *i < 3; (*i)++)
      {
         if (*search_text == formats[*i][count[*i]])
         {
            if (++hit[*i] == 4)
            {
               (*total_length)--;
               return(++search_text);
            }
            (count[*i])++;
         }
         else
         {
            count[*i] = hit[*i] = 0;
         }
      }

      search_text++; counter++;
      (*total_length)--;
   }
   *total_length = tmp_length;

   return(NULL); /* Found nothing. */
}
