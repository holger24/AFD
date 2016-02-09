/*
 * Get's the value of linger time in seconds of a SOCK_STREAM.
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>


int
main(void)
{
   int           sock_fd,
                 length;
   struct linger l;

   if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      (void)fprintf(stderr, "ERROR   : Failed to create socket() : %s\n",
                    strerror(errno));
      exit(-1);
   }

   length = sizeof(struct linger);
   if (getsockopt(sock_fd, SOL_SOCKET, SO_LINGER, (char *)&l, &length) < 0)
   {
      (void)fprintf(stderr, "ERROR   : getsockopt() error : %s\n",
                    strerror(errno));
      exit(-1);
   }

   (void)printf("l_onoff = %d\nl_linger = %d\n", l.l_onoff, l.l_linger);

   l.l_onoff = 1; l.l_linger = 100;
   if (setsockopt(sock_fd, SOL_SOCKET, SO_LINGER, (char *)&l, sizeof(struct linger)) < 0)
   {
      (void)fprintf(stderr, "ERROR   : setsockopt() error : %s\n",
                    strerror(errno));
      exit(-1);
   }

   length = sizeof(struct linger);
   if (getsockopt(sock_fd, SOL_SOCKET, SO_LINGER, (char *)&l, &length) < 0)
   {
      (void)fprintf(stderr, "ERROR   : getsockopt() error : %s\n",
                    strerror(errno));
      exit(-1);
   }

   (void)printf("l_onoff = %d\nl_linger = %d\n", l.l_onoff, l.l_linger);

   (void)close(sock_fd);

   exit(0);
}
