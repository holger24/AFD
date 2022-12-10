/*
 *  afd_stat.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2022 Holger Kiehl <Holger.Kiehl@dwd.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "afddefs.h"

DESCR__S_M1
/*
 ** NAME
 **   afd_stat - keeps track of the input and output statistics
 **
 ** SYNOPSIS
 **   afd_stat
 **
 ** DESCRIPTION
 **   Logs the number of files, the size of the files, the number of
 **   connections and the number of errors on a per hosts basis. For the
 **   input it logs the number of files and bytes received, queued and
 **   files that are not to be send for each directory monitored by
 **   AFD.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.04.1996 H.Kiehl Created
 **   18.10.1997 H.Kiehl Initialize day, hour and second counter.
 **   23.02.2003 H.Kiehl Added input statistics.
 **   01.01.2006 H.Kiehl Catch leap seconds when we store an old year.
 **   03.01.2009 H.Kiehl Catch another leap second bug.
 **   30.03.2017 H.Kiehl Do not count the values of group elements.
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>         /* strcpy(), strcat(), strerror(), strcmp()  */
#include <limits.h>         /* UINT_MAX                                  */
#include <time.h>           /* time()                                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>       /* struct timeval                            */
#ifdef HAVE_SETPRIORITY
# include <sys/resource.h>  /* setpriority()                             */
#endif
#include <sys/mman.h>       /* msync(), munmap()                         */
#include <unistd.h>         /* STDERR_FILENO                             */
#include <stdlib.h>         /* getenv()                                  */
#include <fcntl.h>
#include <signal.h>         /* signal()                                  */
#include <errno.h>
#include "statdefs.h"
#include "version.h"

/* Global variables */
int                        sys_log_fd = STDERR_FILENO,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                           sys_log_readfd,
#endif
                           fra_fd = -1,
                           fra_id,
                           fsa_fd = -1,
                           fsa_id,
                           lock_fd,
                           locki_fd,
                           no_of_dirs = 0,
                           no_of_hosts = 0,
                           other_file = NO;
#ifdef HAVE_MMAP
off_t                      fra_size,
                           fsa_size;
#endif
size_t                     istat_db_size = 0,
                           stat_db_size = 0;
char                       *p_work_dir,
                           istatistic_file[MAX_PATH_LENGTH],
                           new_istatistic_file[MAX_PATH_LENGTH],
                           statistic_file[MAX_PATH_LENGTH],
                           new_statistic_file[MAX_PATH_LENGTH];
struct afdstat             *stat_db = NULL;
struct afdistat            *istat_db = NULL;
struct filetransfer_status *fsa;
struct fileretrieve_status *fra;
const char                 *sys_log_name = SYSTEM_LOG_FIFO;

/* Local functions prototypes. */
#ifdef HAVE_SETPRIORITY
static void                get_afd_config_value(void);
#endif
static void                sig_exit(int),
                           sig_segv(int),
                           sig_bus(int),
                           stat_exit(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   register int   i,
                  j;
   int            current_year,
                  hour,
                  new_year,
                  status,
                  test_sec_counter,
                  test_hour_counter;
   unsigned int   ui_value;          /* Temporary storage for uns. ints  */
   time_t         next_rescan_time = 0L,
                  now;
   long int       sleep_time;
   double         d_value[MAX_NO_PARALLEL_JOBS]; /* Temporary storage    */
                                                 /* for doubles.         */
   char           work_dir[MAX_PATH_LENGTH],
                  istatistic_file_name[MAX_FILENAME_LENGTH],
                  statistic_file_name[MAX_FILENAME_LENGTH];
   struct timeval timeout;
#ifdef HAVE_STATX
   struct statx   stat_buf;
#else
   struct stat    stat_buf;
#endif
   struct tm      *p_ts;

   CHECK_FOR_VERSION(argc, argv);

   /* Evaluate arguments */
   statistic_file_name[0] = istatistic_file_name[0] = '\0';
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   eval_input_as(argc, argv, work_dir, statistic_file_name,
                 istatistic_file_name);
#ifdef HAVE_SETPRIORITY
   get_afd_config_value();
#endif

   /* Initialize variables */
   now = time(NULL);
   p_ts = gmtime(&now);

   /*
    * NOTE: We must put the hour into a temporary storage since
    *       function system_log() uses the function localtime which seems
    *       to overwrite our p_ts->tm_hour with local time!?
    */
   hour = p_ts->tm_hour;
   current_year = p_ts->tm_year + 1900;
   if ((current_year > 9999) || (current_year < 0))
   {
      (void)fprintf(stderr,
                    "ERROR   : We can only handle a 4 digit centry :-( (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   p_work_dir = work_dir;
   if (statistic_file_name[0] == '\0')
   {
      char str_year[6];

      (void)snprintf(str_year, 6, ".%d", current_year);
      (void)strcpy(statistic_file, work_dir);
#ifdef STAT_IN_FIFODIR
      (void)strcat(statistic_file, FIFO_DIR);
#else
      (void)strcat(statistic_file, LOG_DIR);
#endif
      (void)strcpy(new_statistic_file, statistic_file);
      (void)strcat(new_statistic_file, NEW_STATISTIC_FILE);
      (void)strcat(new_statistic_file, str_year);
      (void)strcpy(new_istatistic_file, statistic_file);
      (void)strcat(new_istatistic_file, NEW_ISTATISTIC_FILE);
      (void)strcat(new_istatistic_file, str_year);
      (void)strcpy(istatistic_file, statistic_file);
      (void)strcat(istatistic_file, ISTATISTIC_FILE);
      (void)strcat(istatistic_file, str_year);
      (void)strcat(statistic_file, STATISTIC_FILE);
      (void)strcat(statistic_file, str_year);
   }
   else
   {
      (void)strcpy(statistic_file, statistic_file_name);
      (void)strcpy(new_statistic_file, statistic_file);
      (void)strcat(new_statistic_file, ".NEW");
      (void)strcpy(istatistic_file, istatistic_file_name);
      (void)strcpy(new_istatistic_file, istatistic_file);
      (void)strcat(new_istatistic_file, ".NEW");
   }

   if (other_file == NO)
   {
      char sys_log_fifo[MAX_PATH_LENGTH];

      (void)strcpy(sys_log_fifo, work_dir);
      (void)strcat(sys_log_fifo, FIFO_DIR);
      (void)strcat(sys_log_fifo, SYSTEM_LOG_FIFO);

      /* If the process AFD has not yet created the */
      /* system log fifo create it now.             */
#ifdef HAVE_STATX
      if ((statx(0, sys_log_fifo, AT_STATX_SYNC_AS_STAT,
                 STATX_MODE, &stat_buf) == -1) ||
          (!S_ISFIFO(stat_buf.stx_mode)))
#else
      if ((stat(sys_log_fifo, &stat_buf) == -1) ||
          (!S_ISFIFO(stat_buf.st_mode)))
#endif
      {
         if (make_fifo(sys_log_fifo) < 0)
         {
            (void)fprintf(stderr,
                          "ERROR   : Could not create fifo %s. (%s %d)\n",
                          sys_log_fifo, __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }

      /* Open system log fifo */
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(sys_log_fifo, &sys_log_readfd, &sys_log_fd) == -1)
#else
      if ((sys_log_fd = open(sys_log_fifo, O_RDWR)) == -1)
#endif
      {
         (void)fprintf(stderr, "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                       sys_log_fifo, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

   /* Attach to FSA (output) and FRA (input) so we can */
   /* accumulate the statistics for both.              */
   if (fsa_attach_passive(NO, AFD_STAT) != SUCCESS)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, "Failed to attach to FSA.");
      exit(INCORRECT);
   }
   if (fra_attach_passive() != SUCCESS)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__, "Failed to attach to FRA.");
      exit(INCORRECT);
   }

   /*
    * Read old AFD statistics database file if it is there. If not creat it!
    */
   read_afd_stat_db(no_of_hosts);
   read_afd_istat_db(no_of_dirs);

   /* Tell user we are starting the AFD_STAT */
   if (other_file == NO)
   {
      system_log(INFO_SIGN, NULL, 0, "Starting %s (%s)",
                 AFD_STAT, PACKAGE_VERSION);
   }

   /* Do some cleanups when we exit */
   if (atexit(stat_exit) != 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not register exit handler : %s", strerror(errno));            
      exit(INCORRECT);
   }

   /* Ignore any SIGHUP signal. */
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "signal() error : %s", strerror(errno));
   }

   next_rescan_time = (now / STAT_RESCAN_TIME) *
                      STAT_RESCAN_TIME + STAT_RESCAN_TIME;

   /* Initialize sec_counter, hour_counter and day_counter. */
   test_sec_counter = (((p_ts->tm_min * 60) + p_ts->tm_sec) /
                       STAT_RESCAN_TIME) + 1;
   for (i = 0; i < no_of_hosts; i++)
   {
      stat_db[i].sec_counter = test_sec_counter;
      stat_db[i].hour_counter = hour;
      stat_db[i].day_counter = p_ts->tm_yday;
   }
   for (i = 0; i < no_of_dirs; i++)
   {
      istat_db[i].sec_counter = test_sec_counter;
      istat_db[i].hour_counter = hour;
      istat_db[i].day_counter = p_ts->tm_yday;
   }

   for (;;)
   {
      if ((sleep_time = (next_rescan_time - time(NULL))) < 0)
      {
         sleep_time = 0L;
      }
      timeout.tv_usec = 0;
      timeout.tv_sec = sleep_time;
      status = select(0, NULL, NULL, NULL, &timeout);

      /* Did we get a timeout? */
      if (status == 0)
      {
         now = time(NULL);
         if (now != next_rescan_time)
         {
            now = (now + (STAT_RESCAN_TIME / 2)) / STAT_RESCAN_TIME *
                  STAT_RESCAN_TIME;
         }
         next_rescan_time = (now / STAT_RESCAN_TIME) *
                            STAT_RESCAN_TIME + STAT_RESCAN_TIME;
         p_ts = gmtime(&now);
         test_sec_counter = ((p_ts->tm_min * 60) + p_ts->tm_sec) /
                            STAT_RESCAN_TIME;
         test_hour_counter = p_ts->tm_hour;
         if (test_sec_counter != stat_db[0].sec_counter)
         {
            if ((((stat_db[0].sec_counter - test_sec_counter) == 1) &&
                 (test_hour_counter == stat_db[0].hour_counter)) ||
                 ((stat_db[0].sec_counter == 0) && (test_sec_counter == 719)))
            {
               (void)sleep(STAT_RESCAN_TIME);
            }
            else
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Hmmm..., second counter wrong [%d -> %d]. Correcting.",
                          stat_db[0].sec_counter, test_sec_counter);
               for (i = 0; i < no_of_hosts; i++)
               {
                  stat_db[i].sec_counter = test_sec_counter;
               }
               for (i = 0; i < no_of_dirs; i++)
               {
                  istat_db[i].sec_counter = test_sec_counter;
               }
            }
         }
         if (test_hour_counter != stat_db[0].hour_counter)
         {
            if ((((test_hour_counter + 1) == stat_db[0].hour_counter) ||
                 ((stat_db[0].hour_counter == 0) && (test_hour_counter == 23))) &&
                (p_ts->tm_min == 59) && (p_ts->tm_sec > 54))
            {
               /* Sometimes it happens that current time is just one  */
               /* second behind and when this is just before the hour */
               /* value changes we think we are an hour behind.       */;
            }
            else
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Hmmm..., hour counter wrong [%d -> %d]. Correcting.",
                          stat_db[0].hour_counter, test_hour_counter);
               for (i = 0; i < no_of_hosts; i++)
               {
                  stat_db[i].hour_counter = test_hour_counter;
                  stat_db[i].day[stat_db[i].hour_counter].nfs = 0;
                  stat_db[i].day[stat_db[i].hour_counter].nbs = 0.0;
                  stat_db[i].day[stat_db[i].hour_counter].ne  = 0;
                  stat_db[i].day[stat_db[i].hour_counter].nc  = 0;
               }
               for (i = 0; i < no_of_dirs; i++)
               {
                  istat_db[i].hour_counter = test_hour_counter;
                  istat_db[i].day[istat_db[i].hour_counter].nfr = 0;
                  istat_db[i].day[istat_db[i].hour_counter].nbr = 0.0;
               }
            }
         }

         /*
          * If the FSA or FRA canges we have to reread everything. This
          * is easier then trying to find out where the change took
          * place and change only that part. This is not very effective.
          * But since changes in the FSA or FRA are very seldom and the
          * size of the status is relative small, it seems to be
          * the best solution at this point.
          */
         if (check_fsa(YES, AFD_STAT) == YES)
         {
            read_afd_stat_db(no_of_hosts);
         }
         if (check_fra(YES) == YES)
         {
            read_afd_istat_db(no_of_dirs);
         }

         /*
          * Now lets update the statistics. There are two methods that
          * can be used to update the data. The first one is to have
          * a pointer telling which is the newest data. In the second
          * method we always move everything in the array one position
          * backwards. The advantage of the first method is that we
          * don't have to move around any data. However it is more
          * difficult to evaluate the data when we want to display
          * the information graphicaly. This can be done better with the
          * second method.
          * Lets try the first method first and see how it works.
          */
         for (i = 0; i < no_of_hosts; i++)
         {
            if (fsa[i].real_hostname[0][0] != GROUP_IDENTIFIER)
            {
               /***********************************/
               /* Handle structure entry for day. */
               /***********************************/

               /* Store number of files send. */
               ui_value = fsa[i].file_counter_done;
               if (ui_value >= stat_db[i].prev_nfs)
               {
                  stat_db[i].hour[stat_db[i].sec_counter].nfs = ui_value - stat_db[i].prev_nfs;
               }
               else
               {
                  /* Check if an overflow has occured */
                  if ((UINT_MAX - stat_db[i].prev_nfs) <= MAX_FILES_PER_SCAN)
                  {
                     stat_db[i].hour[stat_db[i].sec_counter].nfs = ui_value + UINT_MAX - stat_db[i].prev_nfs;
                  }
                  else /* To large. Lets assume it was a reset of the AFD. */
                  {
                     stat_db[i].hour[stat_db[i].sec_counter].nfs = ui_value;
                  }
               }
               stat_db[i].day[stat_db[i].hour_counter].nfs += stat_db[i].hour[stat_db[i].sec_counter].nfs;
               stat_db[i].prev_nfs = ui_value;

               /* Store number of bytes send. */
               stat_db[i].hour[stat_db[i].sec_counter].nbs = 0.0;
               for (j = 0; j < MAX_NO_PARALLEL_JOBS; j++)
               {
                  d_value[j] = (double)fsa[i].job_status[j].bytes_send;
                  if (d_value[j] >= stat_db[i].prev_nbs[j])
                  {
                     stat_db[i].hour[stat_db[i].sec_counter].nbs += d_value[j] - stat_db[i].prev_nbs[j];
                  }
                  else
                  {
                     stat_db[i].hour[stat_db[i].sec_counter].nbs += d_value[j];
                  }
                  stat_db[i].prev_nbs[j] = d_value[j];
               }
               if (stat_db[i].hour[stat_db[i].sec_counter].nbs < 0.0)
               {
                  stat_db[i].hour[stat_db[i].sec_counter].nbs = 0.0;
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Hmm.... Byte counter less then zero?!? [%d]", i);
               }
               stat_db[i].day[stat_db[i].hour_counter].nbs += stat_db[i].hour[stat_db[i].sec_counter].nbs;

               /* Store number of errors. */
               ui_value = fsa[i].total_errors;
               if (ui_value >= stat_db[i].prev_ne)
               {
                  stat_db[i].hour[stat_db[i].sec_counter].ne = ui_value - stat_db[i].prev_ne;
               }
               else
               {
                  stat_db[i].hour[stat_db[i].sec_counter].ne = ui_value;
               }
               stat_db[i].day[stat_db[i].hour_counter].ne += stat_db[i].hour[stat_db[i].sec_counter].ne;
               stat_db[i].prev_ne = ui_value;

               /* Store number of connections. */
               ui_value = fsa[i].connections;
               if (ui_value >= stat_db[i].prev_nc)
               {
                  stat_db[i].hour[stat_db[i].sec_counter].nc = ui_value - stat_db[i].prev_nc;
               }
               else
               {
                  stat_db[i].hour[stat_db[i].sec_counter].nc = ui_value;
               }
               stat_db[i].day[stat_db[i].hour_counter].nc += stat_db[i].hour[stat_db[i].sec_counter].nc;
               stat_db[i].prev_nc = ui_value;
            } /* (fsa[i].real_hostname[0][0] != GROUP_IDENTIFIER) */
            stat_db[i].sec_counter++;
         } /* for (i = 0; i < no_of_hosts; i++) */

         /* Now do the same thing for the input. */
         for (i = 0; i < no_of_dirs; i++)
         {
            /***********************************/
            /* Handle structure entry for day. */
            /***********************************/

            /* Store number of files received. */
            ui_value = fra[i].files_received;
            if (ui_value >= istat_db[i].prev_nfr)
            {
               istat_db[i].hour[istat_db[i].sec_counter].nfr = ui_value - istat_db[i].prev_nfr;
            }
            else
            {
               /* Check if an overflow has occured */
               if ((UINT_MAX - istat_db[i].prev_nfr) <= MAX_FILES_PER_SCAN)
               {
                  istat_db[i].hour[istat_db[i].sec_counter].nfr = ui_value + UINT_MAX - istat_db[i].prev_nfr;
               }
               else /* To large. Lets assume it was a reset of the AFD. */
               {
                  istat_db[i].hour[istat_db[i].sec_counter].nfr = ui_value;
               }
            }
            istat_db[i].day[istat_db[i].hour_counter].nfr += istat_db[i].hour[istat_db[i].sec_counter].nfr;
            istat_db[i].prev_nfr = ui_value;

            /* Store number of bytes received. */
            d_value[0] = (double)fra[i].bytes_received;
            if (d_value[0] >= istat_db[i].prev_nbr)
            {
               istat_db[i].hour[istat_db[i].sec_counter].nbr = d_value[0] - istat_db[i].prev_nbr;
            }
            else
            {
               istat_db[i].hour[istat_db[i].sec_counter].nbr = d_value[0];
            }
            if (istat_db[i].hour[istat_db[i].sec_counter].nbr < 0.0)
            {
               istat_db[i].hour[istat_db[i].sec_counter].nbr = 0.0;
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Hmm.... Byte counter less then zero?!? [%d: %f %f]",
                          i, d_value[0], istat_db[i].prev_nbr);
            }
            istat_db[i].day[istat_db[i].hour_counter].nbr += istat_db[i].hour[istat_db[i].sec_counter].nbr;
            istat_db[i].prev_nbr = d_value[0];
            istat_db[i].sec_counter++;
         } /* for (i = 0; i < no_of_dirs; i++) */

         /* Did we reach another hour? */
         if (stat_db[0].sec_counter >= SECS_PER_HOUR)
         {
            for (i = 0; i < no_of_hosts; i++)
            {
               /* Reset the counter for the day structure */
               stat_db[i].sec_counter = 0;
               stat_db[i].hour_counter++;

               if (stat_db[i].hour_counter >= HOURS_PER_DAY)
               {
                  stat_db[i].hour_counter = 0;
                  stat_db[i].year[stat_db[i].day_counter].nfs = 0;
                  stat_db[i].year[stat_db[i].day_counter].nbs = 0.0;
                  stat_db[i].year[stat_db[i].day_counter].ne  = 0;
                  stat_db[i].year[stat_db[i].day_counter].nc  = 0;
                  for (j = 0; j < HOURS_PER_DAY; j++)
                  {
                     stat_db[i].year[stat_db[i].day_counter].nfs += stat_db[i].day[j].nfs;
                     stat_db[i].year[stat_db[i].day_counter].nbs += stat_db[i].day[j].nbs;
                     stat_db[i].year[stat_db[i].day_counter].ne  += stat_db[i].day[j].ne;
                     stat_db[i].year[stat_db[i].day_counter].nc  += stat_db[i].day[j].nc;
                  }
                  stat_db[i].day_counter++;
               }
               stat_db[i].day[stat_db[i].hour_counter].nfs = 0;
               stat_db[i].day[stat_db[i].hour_counter].nbs = 0.0;
               stat_db[i].day[stat_db[i].hour_counter].ne  = 0;
               stat_db[i].day[stat_db[i].hour_counter].nc  = 0;
            }
            for (i = 0; i < no_of_dirs; i++)
            {
               /* Reset the counter for the day structure */
               istat_db[i].sec_counter = 0;
               istat_db[i].hour_counter++;

               if (istat_db[i].hour_counter >= HOURS_PER_DAY)
               {
                  istat_db[i].hour_counter = 0;
                  istat_db[i].year[istat_db[i].day_counter].nfr = 0;
                  istat_db[i].year[istat_db[i].day_counter].nbr = 0.0;
                  for (j = 0; j < HOURS_PER_DAY; j++)
                  {
                     istat_db[i].year[istat_db[i].day_counter].nfr += istat_db[i].day[j].nfr;
                     istat_db[i].year[istat_db[i].day_counter].nbr += istat_db[i].day[j].nbr;
                  }
                  istat_db[i].day_counter++;
               }
               istat_db[i].day[istat_db[i].hour_counter].nfr = 0;
               istat_db[i].day[istat_db[i].hour_counter].nbr = 0.0;
            }
         } /* if (stat_db[i].sec_counter >= SECS_PER_HOUR) */

         /* Did we reach another year? */
         new_year = p_ts->tm_year + 1900;
         if (current_year != new_year)
         {
            if (current_year > new_year)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Hmmm..., year jumped back from %d to %d.",
                          current_year, new_year);
            }
            else if ((new_year - current_year) > 1)
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "Hmmm..., year jumped forward from %d to %d.",
                               current_year, new_year);
                 }
            if (other_file == NO)
            {
               save_old_input_year(new_year);
               save_old_output_year(new_year);
            }
            current_year = new_year;

            /*
             * Reset all values in current memory mapped file. Watch out
             * for leap seconds and NTP!
             */
            if ((test_hour_counter == 23) && (p_ts->tm_min == 59) &&
                (p_ts->tm_yday >= 363))
            {
               test_sec_counter = 0;
               test_hour_counter = 0;
               j = 0;
            }
            else
            {
               test_sec_counter = ((p_ts->tm_min * 60) + p_ts->tm_sec) /
                                  STAT_RESCAN_TIME;
               j = p_ts->tm_yday;
            }
            for (i = 0; i < no_of_hosts; i++)
            {
               stat_db[i].sec_counter = test_sec_counter;
               stat_db[i].hour_counter = test_hour_counter;
               stat_db[i].day_counter = j;
               (void)memset(&stat_db[i].year, 0, (DAYS_PER_YEAR * sizeof(struct statistics)));
               (void)memset(&stat_db[i].day, 0, (HOURS_PER_DAY * sizeof(struct statistics)));
               (void)memset(&stat_db[i].hour, 0, (SECS_PER_HOUR * sizeof(struct statistics)));
            }
            for (i = 0; i < no_of_dirs; i++)
            {
               istat_db[i].sec_counter = test_sec_counter;
               istat_db[i].hour_counter = test_hour_counter;
               istat_db[i].day_counter = j;
               (void)memset(&istat_db[i].year, 0, (DAYS_PER_YEAR * sizeof(struct istatistics)));
               (void)memset(&istat_db[i].day, 0, (HOURS_PER_DAY * sizeof(struct istatistics)));
               (void)memset(&istat_db[i].hour, 0, (SECS_PER_HOUR * sizeof(struct istatistics)));
            }
         }

         if (stat_db[0].day_counter >= DAYS_PER_YEAR)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Hmmm..., day counter wrong [%d -> 0]. Correcting.",
                       stat_db[0].day_counter);
            for (i = 0; i < no_of_hosts; i++)
            {
               stat_db[i].day_counter = p_ts->tm_yday;
            }
            for (i = 0; i < no_of_dirs; i++)
            {
               istat_db[i].day_counter = p_ts->tm_yday;
            }
         }
      }
           /* An error has occured */
      else if (status < 0)
           {
              system_log(FATAL_SIGN, __FILE__, __LINE__,
                         "select() error : %s", strerror(errno));
              exit(INCORRECT);
           }
           else
           {
              system_log(FATAL_SIGN, __FILE__, __LINE__, "Unknown condition.");
              exit(INCORRECT);
           }
   } /* for (;;) */

   exit(SUCCESS);
}


#ifdef HAVE_SETPRIORITY
/*+++++++++++++++++++++++++ get_afd_config_value() ++++++++++++++++++++++*/
static void
get_afd_config_value(void)
{
   char *buffer,
        config_file[MAX_PATH_LENGTH];

   (void)sprintf(config_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_CONFIG_FILE);
   if ((eaccess(config_file, F_OK) == 0) &&
       (read_file_no_cr(config_file, &buffer, YES, __FILE__, __LINE__) != INCORRECT))
   {
      char value[MAX_INT_LENGTH];

      if (get_definition(buffer, AFD_STAT_PRIORITY_DEF,
                         value, MAX_INT_LENGTH) != NULL)
      {
         if (setpriority(PRIO_PROCESS, 0, atoi(value)) == -1)
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Failed to set priority to %d : %s",
                       atoi(value), strerror(errno));
         }
      }
      free(buffer);
   }

   return;
}
#endif


/*+++++++++++++++++++++++++++++ stat_exit() +++++++++++++++++++++++++++++*/
static void
stat_exit(void)
{
   if (msync(((char *)stat_db - AFD_WORD_OFFSET), stat_db_size, MS_SYNC) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_SIZE_T == 4
                 "msync() error [stat_db_size=%d] : %s",
#else
                 "msync() error [stat_db_size=ll%d] : %s",
#endif
                 (pri_size_t)stat_db_size, strerror(errno));
   }
   if (msync(((char *)istat_db - AFD_WORD_OFFSET), istat_db_size, MS_SYNC) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_SIZE_T == 4
                 "msync() error [istat_db_size=%d] : %s",
#else
                 "msync() error [istat_db_size=%lld] : %s",
#endif
                 (pri_size_t)istat_db_size, strerror(errno));
   }
   if (munmap(((char *)stat_db - AFD_WORD_OFFSET), stat_db_size) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_SIZE_T == 4
                 "munmap() error [stat_db_size=%d] : %s",
#else
                 "munmap() error [stat_db_size=%lld] : %s",
#endif
                 (pri_size_t)stat_db_size, strerror(errno));
   }
   if (munmap(((char *)istat_db - AFD_WORD_OFFSET), istat_db_size) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_SIZE_T == 4
                 "munmap() error [istat_db_size=%d] : %s",
#else
                 "munmap() error [istat_db_size=%lld] : %s",
#endif
                 (pri_size_t)istat_db_size, strerror(errno));
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void                                                                
sig_segv(int signo)
{                  
   system_log(FATAL_SIGN, __FILE__, __LINE__, "Aaarrrggh! Received SIGSEGV.");
   stat_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__, "Uuurrrggh! Received SIGBUS.");
   stat_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   int ret;

   (void)fprintf(stderr,
#if SIZEOF_PID_T == 4
                 "%s terminated by signal %d (%d)\n",
#else
                 "%s terminated by signal %d (%lld)\n",
#endif
                 AFD_STAT, signo, (pri_pid_t)getpid());
   if ((signo == SIGINT) || (signo == SIGTERM))
   {
      ret = SUCCESS;
   }
   else
   {
      ret = INCORRECT;
   }

   exit(ret);
}
