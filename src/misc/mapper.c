/*
 *  mapper.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2022 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mapper - emulates mmap(), msync() and munmap()
 **
 ** SYNOPSIS
 **   mapper
 **
 ** DESCRIPTION
 **   This programm emulates the mmap(), msync() and munmap()
 **   functions. If a process requests a file to be mapped, it
 **   copies the contents into a shared memory region and returns
 **   the shared memory ID to the calling process. If the pre-
 **   compiler option WATCH was specified, another process is
 **   activated which checks every SLEEP_TIME second if the
 **   shared memory region has been changed. If this is the case
 **   then region is saved in the relevant file.
 **   The msync() and munmap() functions work as the original
 **   functions.
 **   The inter process communication between the mapper and the
 **   calling process (mmap_emu(), msync_emu() and munmap_emu())
 **   is done via fifos.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   29.09.1994 H.Kiehl Created
 **   18.03.1997 H.Kiehl Modified so it can be used with the AFD.
 **
 */
DESCR__E_M1

#ifndef HAVE_MMAP
#include <stdio.h>         /* sprintf(), fopen(), getc(), fclose()           */
#include <string.h>        /* strlen(), strcpy()                             */
#include <stdlib.h>        /* exit(), atoi()                                 */
#include <unistd.h>        /* unlink(), open(), read(), write(), close(),    */
                           /* execlp(), fsync()                              */
#include <sys/types.h>
#include <sys/stat.h>      /* mkfifo()                                       */
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>      /* select()                                       */
#include <signal.h>        /* signal(), kill()                               */
#include <fcntl.h>
#include <errno.h>
#include "mmap_emu.h"

/* Global variables */
char        request_fifo[MAX_PATH_LENGTH];
int         count = 0,
#ifdef WITHOUT_FIFO_RW_SUPPORT
            sys_log_readfd,
#endif
            sys_log_fd = STDERR_FILENO;
struct map  region[MAX_MAPPED_REGIONS];
const char  *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes */
static void read_crc(char *, char *, size_t, size_t),
            saver(int),
            save_region(int, char *, int, char *, int);
static int  find_filename(char *);
#endif /* !HAVE_MMAP */


/*################################ main() ###################################*/
int
main(int argc, char *argv[])
{
#ifndef HAVE_MMAP
   int            i,
                  n = 0,
                  status,
                  fd,
                  read_fd1,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  write_fd1,
#endif
                  write_fd,
                  shmid,
                  posi,
                  type,
                  size;
   size_t         shmid_length;
   char           *ptr,
                  *shmptr,
                  strtype[2],
                  strsize[15],
                  strshmid[16],
                  filename[MAX_PATH_LENGTH],
                  fifoname[MAX_PATH_LENGTH],
                  work_dir[MAX_PATH_LENGTH],
                  sys_log_fifo[MAX_PATH_LENGTH],
                  buf[BUFSIZE];
   fd_set         rset,
                  initialset;
#ifdef HAVE_STATX
   struct statx   stat_buf;
#else
   struct stat    stat_buf;
#endif
   struct timeval timeout;

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   (void)strcpy(request_fifo, work_dir);
   (void)strcat(request_fifo, FIFO_DIR);
   (void)strcpy(sys_log_fifo, request_fifo);
   (void)strcat(sys_log_fifo, SYSTEM_LOG_FIFO);
   (void)strcpy(fifoname, request_fifo);
   (void)strcat(request_fifo, REQUEST_FIFO);

   /* Open/create fifo to write system log data */
#ifdef HAVE_STATX
   if ((statx(0, sys_log_fifo, AT_STATX_SYNC_AS_STAT,
              STAX_MODE, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.stx_mode)))
#else
   if ((stat(sys_log_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
#endif
   {
      if (make_fifo(sys_log_fifo) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN, "Failed to create fifo %s. (%s %d)\n",
                   sys_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(sys_log_fifo, &sys_log_readfd, &sys_log_fd) == -1)
#else
   if ((sys_log_fd = open(sys_log_fifo, O_RDWR)) == -1)
#endif
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Could not open fifo %s : %s (%s %d)\n",
                sys_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialise well known fifo to receive requests from clients */
   (void)unlink(request_fifo);
   if (mkfifo(request_fifo, FILE_MODE) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Failed to mkfifo() %s : %s (%s %d)\n",
                request_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(request_fifo, &read_fd1, &write_fd1) == -1)
#else
   if ((read_fd1 = open(request_fifo, O_RDWR)) == -1)
#endif
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Failed to open() %s : %s (%s %d)\n",
                request_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((signal(SIGINT, saver) == SIG_ERR) ||
       (signal(SIGQUIT, saver) == SIG_ERR) ||
       (signal(SIGTERM, saver) == SIG_ERR) ||
       (signal(SIGABRT, saver) == SIG_ERR) ||
       (signal(SIGSEGV, saver) == SIG_ERR))
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Could not set signal handler : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   FD_ZERO(&initialset);
   FD_SET(read_fd1, &initialset);

   for (;;)
   {
      rset = initialset;
      timeout.tv_usec = 0;
      timeout.tv_sec = 15;

      status = select(read_fd1 + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         for (i = 0; i < count; i++)
         {
            read_crc(region[i].shmptr,
                     region[i].actual_crc_ptr,
                     region[i].crc_size,
                     region[i].step_size);
            if (memcmp(region[i].initial_crc_ptr,
                       region[i].actual_crc_ptr, region[i].crc_size) != 0)
            {
               (void)memcpy(region[i].initial_crc_ptr, region[i].actual_crc_ptr, region[i].crc_size);

               /* Copy memory into file */
               save_region(region[i].shmid,
                           region[i].filename,
                           region[i].size,
                           region[i].shmptr,
                           NO);
            }
         }
      }
      else if (FD_ISSET(read_fd1, &rset))
           {
              /* read from well known fifo */
              n = read(read_fd1, buf, BUFSIZE);

              while (n > 0)
              {
                 /* first read message type */
                 filename[0] = '\0';
                 ptr = buf;
                 strtype[0] = *ptr;
                 strtype[1] = '\0';
                 type = atoi(strtype);
                 ptr++; ptr++;
                 n -= 2;

                 switch (type)
                 {
                    case 1:  /* mmap() emmulator */

                       /* store message from client in separate variables */
                       i = 0;
                       while ((*ptr != '\t') && (i < (MAX_PATH_LENGTH - 1)))
                       {
                          filename[i++] = *(ptr++);
                       }
                       if (i == (MAX_PATH_LENGTH - 1))
                       {
                          (void)rec(sys_log_fd, FATAL_SIGN, "MAPPER    : Could not extract filename. (%s %d)\n",
                                    __FILE__, __LINE__);
                          exit(INCORRECT);
                       }
                       filename[i++] = '\0';
                       ptr++;
                       n -= i;
                       i = 0;
                       while ((*ptr != '\t') && (i < 14))
                       {
                          strsize[i++] = *(ptr++);
                       }
                       if (i == 14)
                       {
                          (void)rec(sys_log_fd, FATAL_SIGN, "MAPPER    : Could not extract size. (%s %d)\n",
                                    __FILE__, __LINE__);
                          exit(INCORRECT);
                       }
                       strsize[i++] = '\0';
                       ptr++;
                       n -= i;
                       size = atoi((char *)strsize);
                       i = 0;
                       while ((*ptr != '\n') && (i < (MAX_FILENAME_LENGTH - 1)))
                       {
                          fifoname[i++] = *(ptr++);
                       }
                       if (i == (MAX_FILENAME_LENGTH - 1))
                       {
                          (void)rec(sys_log_fd, FATAL_SIGN, "MAPPER    : Could not extract fifoname. (%s %d)\n",
                                    __FILE__, __LINE__);
                          exit(INCORRECT);
                       }
                       fifoname[i++] = '\0';
                       ptr++;
                       n -= i;
                       (void)strcpy((char *) buf, ptr);
#ifdef _MMAP_EMU_DEBUG
                       (void)fprintf(stderr, "MAPPER    : mapping\t%s\t%d\t%s\n", filename, size, fifoname);
#endif

                       /* Open fifo to answer client */
                       if ((write_fd = open(fifoname, O_WRONLY)) < 0)
                       {
                          (void)rec(sys_log_fd, FATAL_SIGN, "Failed to open() %s : %s (%s %d)\n",
                                    fifoname, strerror(errno), __FILE__, __LINE__);
                          exit(INCORRECT);
                       }

                       if ((posi = find_filename((char *)filename)) < 0)
                       {
                          /* Check if size is not to large */
                          if ((size + MAX_PATH_LENGTH) > MAX_ALLOWED_SHM_SIZE)
                          {
                             (void)fprintf(stderr, "MAPPER    : Filesize (%d) to large, changed to %d\n", size, MAX_ALLOWED_SHM_SIZE);
                             size = MAX_ALLOWED_SHM_SIZE - MAX_PATH_LENGTH;
                          }
                          (void)snprintf(strsize, 15, "%d", size);

                          /* Prepare shared memory region */
                          if ((shmid = shmget(IPC_PRIVATE, (size + MAX_PATH_LENGTH), SHM_MODE)) < 0)
                          {
                             (void)rec(sys_log_fd, FATAL_SIGN, "shmget() error : %s (%s %d)\n",
                                       strerror(errno), __FILE__, __LINE__);
                             exit(INCORRECT);
                          }
                          if ((shmptr = shmat(shmid, 0, 0)) < (char *) 0)
                          {
                             (void)rec(sys_log_fd, FATAL_SIGN, "shmat() error : %s (%s %d)\n",
                                       strerror(errno), __FILE__, __LINE__);
                             exit(INCORRECT);
                          }

                          /* Write filename into the attached area */
                          (void)snprintf(shmptr, size + MAX_PATH_LENGTH,
                                         "%s\n", filename);

                          /* Add region to table */
                          if (count == MAX_MAPPED_REGIONS)
                          {
                             (void)shmdt(shmptr);
                             (void)rec(sys_log_fd, FATAL_SIGN, "MAPPER    : Have reached maximum number of allowed mapped regions (%d). (%s %d)\n",
                                       MAX_MAPPED_REGIONS, __FILE__, __LINE__);
                             exit(INCORRECT);
                          }
                          region[count].shmid = shmid;
                          region[count].size = size;
                          region[count].shmptr = shmptr;
                          (void)strcpy((char *)region[count].filename, filename);
                          region[count].step_size = STEP_SIZE;
                          while (region[count].step_size >= size)
                          {
                             region[count].step_size /= 10;
                          }
                          region[count].crc_size = size / region[count].step_size;

                          /* Allocate space for CRC buffers */
                          if (((region[count].initial_crc_ptr = calloc(region[count].crc_size, sizeof(char))) == NULL) ||
                              ((region[count].actual_crc_ptr = calloc(region[count].crc_size, sizeof(char))) == NULL))
                          {
                             (void)rec(sys_log_fd, ERROR_SIGN, "calloc() error : %s (%s %d)\n",
                                       strerror(errno), __FILE__, __LINE__);
                             exit(INCORRECT);
                          }
                          read_crc(shmptr, region[count].initial_crc_ptr, region[count].crc_size, region[count].step_size);
                          count++;

                          /* Copy file into memory */
                          if ((fd = open(filename, O_RDONLY)) < 0)
                          {
                             (void)rec(sys_log_fd, FATAL_SIGN, "Failed to open() %s : %s (%s %d)\n",
                                       filename, strerror(errno), __FILE__, __LINE__);
                             exit(INCORRECT);
                          }
                          if (read(fd, (shmptr + MAX_PATH_LENGTH), size) != size)
                          {
                             (void)rec(sys_log_fd, FATAL_SIGN, "read() error : %s (%s %d)\n",
                                       strerror(errno), __FILE__, __LINE__);
                             exit(INCORRECT);
                          }
                          if (close(fd) < 0)
                          {
                             (void)rec(sys_log_fd, ERROR_SIGN, "Failed to close() %s : %s (%s %d)\n",
                                       filename, strerror(errno), __FILE__, __LINE__);
                          }
                       }
                       else
                       {
                          shmid = region[posi].shmid;
                       }

                       /* return shmid to calling process */
                       (void)snprintf(strshmid, 15, "%d\n", shmid);
                       shmid_length = strlen((char *) strshmid);
                       if (write(write_fd, strshmid, shmid_length) != shmid_length)
                       {
                          (void)rec(sys_log_fd, FATAL_SIGN, "write() error : %s (%s %d)\n",
                                    strerror(errno), __FILE__, __LINE__);
                          exit(INCORRECT);
                       }
                       if (close(write_fd) < 0)
                       {
                          (void)rec(sys_log_fd, ERROR_SIGN, "Failed to close() %s : %s (%s %d)\n",
                                    fifoname, strerror(errno), __FILE__, __LINE__);
                       }

                       break;

                    case 2:  /* msync() emmulator */

                       i = 0;
                       while ((*ptr != '\n') && (i < (MAX_PATH_LENGTH - 1)))
                       {
                          filename[i++] = *(ptr++);
                       }
                       if (i == (MAX_PATH_LENGTH - 1))
                       {
                          (void)rec(sys_log_fd, FATAL_SIGN, "MAPPER    : Could not extract filename (to long). (%s %d)\n",
                                    __FILE__, __LINE__);
                          exit(INCORRECT);
                       }
                       filename[i++] = '\0';
                       ptr++;
                       n -= i;
                       (void)strcpy((char *) buf, ptr);
#ifdef _MMAP_EMU_DEBUG
                       (void)fprintf(stderr, "MAPPER    : msyncing\t%s\n", filename);
#endif

                       if ((posi = find_filename((char *)filename)) < 0)
                       {
                          (void)rec(sys_log_fd, ERROR_SIGN, "MAPPER    : Failed to sync %s (%s %d)\n",
                                    filename, __FILE__, __LINE__);
                          break;
                       }

                       /* Copy memory into file */
                       save_region(region[posi].shmid,
                                   filename,
                                   region[posi].size,
                                   region[posi].shmptr,
                                   YES);

#ifdef _MMAP_EMU_DEBUG
                       (void)fprintf(stderr, "MAPPER    : sync file %s\n", filename);
#endif
                       break;

                    case 3:  /* munmap() emmulator */

                       i = 0;
                       while ((*ptr != '\n') && (i < (MAX_PATH_LENGTH - 1)))
                       {
                          filename[i++] = *(ptr++);
                       }
                       if (i == (MAX_PATH_LENGTH - 1))
                       {
                          (void)rec(sys_log_fd, FATAL_SIGN, "MAPPER    : Could not extract filename (to long). (%s %d)\n",
                                    __FILE__, __LINE__);
                          exit(INCORRECT);
                       }
                       filename[i++] = '\0';
                       ptr++;
                       n -= i;
                       (void)strcpy((char *) buf, ptr);
#ifdef _MMAP_EMU_DEBUG
                       (void)fprintf(stderr, "MAPPER    : unmapping\t%s\n", filename);
#endif

                       if ((posi = find_filename((char *) filename)) != -1)
                       {
                          /* Detach shared memory region */
                          if (shmdt(region[posi].shmptr) < 0)
                          {
                             (void)rec(sys_log_fd, FATAL_SIGN, "shmdt() error : %s (%s %d)\n",
                                       strerror(errno), __FILE__, __LINE__);
                             exit(INCORRECT);
                          }

                          /* remove shared memory region */
                          if (shmctl(region[posi].shmid, IPC_RMID, 0) < 0)
                          {
                             (void)rec(sys_log_fd, FATAL_SIGN, "shmctl() [IPC_RMID] error : %s (%s %d)\n",
                                       strerror(errno), __FILE__, __LINE__);
                             exit(INCORRECT);
                          }

                          /* Free CRC buffers */
                          free(region[posi].initial_crc_ptr);
                          free(region[posi].actual_crc_ptr);

                          while (posi < count)
                          {
                             region[posi].shmid = region[posi + 1].shmid;
                             region[posi].size = region[posi + 1].size;
                             region[posi].shmptr = region[posi + 1].shmptr;
                             region[posi].crc_size = region[posi + 1].crc_size;
                             region[posi].step_size = region[posi + 1].step_size;
                             region[posi].initial_crc_ptr = region[posi + 1].initial_crc_ptr;
                             region[posi].actual_crc_ptr = region[posi + 1].actual_crc_ptr;
                             (void)strcpy((char *)region[posi].filename, (char *)region[posi + 1].filename);
                             posi++;
                          }
#ifdef _MMAP_EMU_DEBUG
                          (void)fprintf(stderr, "MAPPER    : Unmaped %s\n", filename);
#endif
                          count--;
                       }

                       break;

                    default: /* Error */

                       (void)rec(sys_log_fd, ERROR_SIGN, "MAPPER    : Unknown type. (Error in fifo)");
                       exit(INCORRECT);
                 }
              }
           }
           else
           {
              (void)rec(sys_log_fd, FATAL_SIGN, "select() error : %s (%s %d)\n",
                        strerror(errno), __FILE__, __LINE__);
              exit(INCORRECT);
           }
   } /* for (;;) */
#else
   return(0);
#endif
}


#ifndef HAVE_MMAP
/*+++++++++++++++++++++++++++++ find_filename() +++++++++++++++++++++++++++++*/
/*                              ---------------                              */
/* Description : Searches for <filename> in global struct <region> and       */
/*               returns the position if found, else -1 is returned.         */
/* Arguments   : char *filename    - Pointer to filename to be searched for. */
/* Returns     : success           - Position no. in struct <region>.        */
/*               failure           - -1                                      */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int
find_filename(char *filename)
{
   int   i;

   for (i = 0; i < count; i++)
   {
      if (my_strcmp(region[i].filename, filename) == 0)
      {
         return(i);
      }
   }

   return(-1);
}


/*++++++++++++++++++++++++++++++ saver() ++++++++++++++++++++++++++++++++++++*/
/*                               -------                                     */
/* Description : This signal handler saves all the shared memory regions in  */
/*               files if ^C is pressed. It then asks you if you want to     */
/*               exit and if you reply 'y' all regions are removed from the  */
/*               system.                                                     */
/* Arguments   : NONE (dummy)                                                */
/* Returns     : Nothing                                                     */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
saver(int dummy)
{
   int   i;

   (void)signal(SIGINT, saver);

   (void)rec(sys_log_fd, INFO_SIGN, "Saving %d shared memory regions ....\n",
             count);
   for (i = 0; i < count; i++)
   {
      /* Copy memory to file */
      save_region(region[i].shmid,
                  (char *)region[i].filename,
                   region[i].size,
                   region[i].shmptr,
                   YES);

      /* Detach and remove shared memory region */
      (void)rec(sys_log_fd, INFO_SIGN, "Removing shared memory region %d\n",
                region[i].shmid);
      if (shmdt(region[i].shmptr) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN, "shmdt() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
      if (shmctl(region[i].shmid, IPC_RMID, 0) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN, "shmctl() [IPC_RMID] error (%d) : %s (%s %d)\n",
                   region[i].shmid, strerror(errno), __FILE__, __LINE__);
      }

      /* Free CRC buffers */
      free(region[i].initial_crc_ptr);
      free(region[i].actual_crc_ptr);
   }

   (void)unlink(request_fifo);
   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ read_crc() ++++++++++++++++++++++++++++++++*/
/*                                ----------                                 */
/* Description :  This function reads every <step_size> character from       */
/*                <source> and stores it in <buf>.                           */
/* Arguments   :  char   *source    - Pointer to source.                     */
/*                char   *buf       - Pointer where every <step_size>        */
/*                                    character is to be stored.             */
/*                size_t crc_size   - Size in byte of <buf>.                 */
/*                size_t step_size  - The distance in bytes to the next      */
/*                                    character to be read.                  */
/* Returns     :  nothing                                                    */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
read_crc(char *source, char *buf, size_t crc_size, size_t step_size)
{
   int   i;

   /* first initialize buf with 0 */
   (void)memset(buf, 0, (int)crc_size);

   for (i = 0; i < crc_size; i++)
   {
      buf[i] = *source;
      source += step_size;
   }

   return;
}



/*---------------------------- save_region() --------------------------------*/
/*                             -------------                                 */
/* Description : Saves everything from a shared memory region into a file.   */
/* Arguments   : int  shmid      - ID of shared memory region.               */
/*               char *filename  - File in which the shared memory region is */
/*                                 to be copied.                             */
/*               int  size       - Size of memory mapped file.               */
/*               char *shmptr    - Pointer to shared memory region.          */
/*               int  sync_file  - Should we sync the file?                  */
/* Returns     : Nothing                                                     */
/*---------------------------------------------------------------------------*/
static void
save_region(int shmid, char *filename, int size, char *shmptr, int sync_file)
{
   int fd;

   if ((fd = open(filename, O_WRONLY)) < 0)
   {
      if (errno != ENOENT)
      {
         (void)rec(sys_log_fd, WARN_SIGN, "Failed to open() %s : %s (%s %d)\n",
                   filename, strerror(errno), __FILE__, __LINE__);
      }
      return;
   }
   if (write(fd, (shmptr + MAX_PATH_LENGTH), size) != size)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "write() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (sync_file == YES)
   {
      if (fsync(fd) == -1)
      {
         (void)rec(sys_log_fd, WARN_SIGN, "Failed to fsync() %s : %s (%s %d)\n",
                   filename, strerror(errno), __FILE__, __LINE__);
      }
   }
   if (close(fd) < 0)
   {
      (void)rec(sys_log_fd, WARN_SIGN, "Failed to close() %s : %s (%s %d)\n",
                filename, strerror(errno), __FILE__, __LINE__);
   }

   return;
}
#endif /* !HAVE_MMAP */
