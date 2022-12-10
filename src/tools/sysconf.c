/*************************************************************************/
/* This program was taken from :                                         */
/*                                                                       */
/*   Title       : Advanced Programming in the UNIX Environment          */
/*   Author      : W.Richard Stevens                                     */
/*   Published by: Addison-Wesley 1992                                   */
/*                                                                       */
/* THE BEST BOOK IN THIS WORLD!                                          */                
/*************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

static void	pr_sysconf(char *, int);
static void	pr_pathconf(char *, char *, int);

int
main(int argc, char *argv[])
{
	if (argc != 2)
	{
		(void)fprintf(stderr, "usage: a.out <dirname>\n");
		exit(1);
	}

	pr_sysconf("ARG_MAX            =", _SC_ARG_MAX);
	pr_sysconf("CHILD_MAX          =", _SC_CHILD_MAX);
	pr_sysconf("clock ticks/second =", _SC_CLK_TCK);
	pr_sysconf("NGROUPS_MAX        =", _SC_NGROUPS_MAX);
	pr_sysconf("OPEN_MAX           =", _SC_OPEN_MAX);
#ifdef	_SC_STREAM_MAX
	pr_sysconf("STREAM_MAX         =", _SC_STREAM_MAX);
#endif
#ifdef	_SC_TZNAME_MAX
	pr_sysconf("TZNAME_MAX         =", _SC_TZNAME_MAX);
#endif
	pr_sysconf("_POSIX_JOB_CONTROL =", _SC_JOB_CONTROL);
	pr_sysconf("_POSIX_SAVED_IDS   =", _SC_SAVED_IDS);
	pr_sysconf("_POSIX_VERSION     =", _SC_VERSION);
	pr_pathconf("MAX_CANON       =", "/dev/tty", _PC_MAX_CANON);
	pr_pathconf("MAX_INPUT       =", "/dev/tty", _PC_MAX_INPUT);
	pr_pathconf("_POSIX_VDISABLE =", "/dev/tty", _PC_VDISABLE);
	pr_pathconf("LINK_MAX        =", argv[1], _PC_LINK_MAX);
	pr_pathconf("NAME_MAX        =", argv[1], _PC_NAME_MAX);
	pr_pathconf("PATH_MAX        =", argv[1], _PC_PATH_MAX);
	pr_pathconf("PIPE_BUF        =", argv[1], _PC_PIPE_BUF);
	pr_pathconf("_POSIX_NO_TRUNC =", argv[1], _PC_NO_TRUNC);
	pr_pathconf("_POSIX_CHOWN_RESTRICTED =",
							argv[1], _PC_CHOWN_RESTRICTED);
	exit(0);
}

static void
pr_sysconf(char *mesg, int name)
{
	long	val;

	fputs(mesg, stdout);
	errno = 0;
	if ( (val = sysconf(name)) < 0) {
		if (errno != 0)
		{
			(void)fprintf(stderr, "sysconf error : %s\n", strerror(errno));
			exit(1);
		}
		fputs(" (not defined)\n", stdout);
	} else
		printf(" %ld\n", val);
}

static void
pr_pathconf(char *mesg, char *path, int name)
{
	long	val;

	fputs(mesg, stdout);
	errno = 0;
	if ( (val = pathconf(path, name)) < 0) {
		if (errno != 0)
		{
			(void)fprintf(stderr, "pathconf error, path = %s : %s\n", path, strerror(errno));
			exit(1);
		}
		fputs(" (no limit)\n", stdout);
	} else
		printf(" %ld\n", val);
}
