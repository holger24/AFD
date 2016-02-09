/*************************************************************************/
/* This function was taken from the book Advanced Programming in the     */
/* UNIX Environment by W. Richard Stevens. (page 66)                     */
/*************************************************************************/
#include "afddefs.h"

DESCR__S_M3
/*
 ** NAME
 **   set_fl - sets a file descriptor flag
 **
 ** SYNOPSIS
 **   void clr_fl(int fd, int flags)
 **        fd    - file descriptor of the fifo, etc
 **        flags - file status flags to turn on
 **
 ** DESCRIPTION
 **   The function set_fl() adds a status flag from the file
 **   descriptor 'fd'.
 **
 ** RETURN VALUES
 **   None. Will exit with INCORRECT if fcntl() fails.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   13.10.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>                      /* strerror()                   */
#include <stdlib.h>                      /* exit()                       */
#include <unistd.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>


/*############################# set_fl() ################################*/
void
set_fl(int fd, int flags)
{
   int val;

   if ((val = fcntl(fd, F_GETFL, 0)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("fcntl() error : %s"), strerror(errno));
      exit(INCORRECT);
   }
   
   val |= flags; /* Turn on flags. */

   if (fcntl(fd, F_SETFL, val) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("fcntl() error : %s"), strerror(errno));
      exit(INCORRECT);
   }

   return;
}
