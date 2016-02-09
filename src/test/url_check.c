
#include "afddefs.h"
#include <stdio.h>
#include <unistd.h>

// char *url_strings = "mailto://hafen@schleswiger-stadtwerke.de;server=mailhub.dwd.de";
// char *url_strings = "wmo://afd_ftp:dwd@davidmss:10100;type=a";
// char *url_strings = "ftp://anonymous:dwd.de@uni-goet/ftproot;type=a";

int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir,
           *url_strings = "mailto://mailhub;server=mailhub.dwd.de";
const char *sys_log_name = SYSTEM_LOG_FIFO;

int
main(int argc, char *argv[])
{
   unsigned int  scheme;
   int           port,
                 ret;
   char          *p_url,
                 user[MAX_USER_NAME_LENGTH + 1],
                 smtp_user[MAX_USER_NAME_LENGTH + 1],
#ifdef WITH_SSH_FINGERPRINT
                 fingerprint[MAX_FINGERPRINT_LENGTH + 1],
                 key_type,
#endif
                 password[MAX_USER_NAME_LENGTH + 1],
                 hostname[MAX_REAL_HOSTNAME_LENGTH + 1],
                 path[MAX_RECIPIENT_LENGTH + 1],
                 transfer_type,
                 server[MAX_REAL_HOSTNAME_LENGTH + 1],
                 work_dir[MAX_PATH_LENGTH];
   unsigned char protocol_version,
                 smtp_auth;

   p_work_dir = work_dir;
   if (argc == 1)
   {
      p_url = url_strings;
   }
   else
   {
      p_url = argv[1];
   }

   ret = url_evaluate(p_url, &scheme, user, &smtp_auth, smtp_user,
#ifdef WITH_SSH_FINGERPRINT
                      fingerprint, &key_type,
#endif
                      password, YES, hostname, &port, path, NULL, NULL,
                      &transfer_type, &protocol_version, server);

   (void)printf("url: %s\n", p_url);
   (void)printf("\nscheme           = %u\n", scheme);
   (void)printf("user             = %s\n", user);
   if (smtp_auth == SMTP_AUTH_NONE)
   {
      (void)printf("SMTP auth        = None\n");
   }
   else if (smtp_auth == SMTP_AUTH_LOGIN)
        {
           (void)printf("SMTP auth        = Login\n");
        }
   else if (smtp_auth == SMTP_AUTH_PLAIN)
        {
           (void)printf("SMTP auth        = Plain\n");
        }
        else
        {
           (void)printf("SMTP auth        = Unknown\n");
        }
   (void)printf("SMTP user        = %s\n", smtp_user);
#ifdef WITH_SSH_FINGERPRINT
   if (fingerprint[0] != '\0')
   {
      (void)printf("fingerprint      = %s (key_type = %d)\n",
                   fingerprint, key_type);
   }
#endif
   (void)printf("password         = %s\n", password);
   (void)printf("hostname         = %s\n", hostname);
   if (port != -1)
   {
      (void)printf("port             = %d\n", port);
   }
   (void)printf("path             = %s\n", path);
   (void)printf("transfer type    = %c\n", transfer_type);
   if (protocol_version != 0)
   {
      (void)printf("protocol version = %d\n", (int)protocol_version);
   }
   if (server[0] != '\0')
   {
      (void)printf("server           = %s\n", server);
   }
   (void)printf("===============================\n");
   (void)printf("Result           = %d\n", ret);

   return(0);
}
