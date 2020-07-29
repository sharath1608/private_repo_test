#include <errno.h>
#include <stdarg.h>
#include "42186245_1524069388.h"

static void err_doit(int, const char *, va_list);

char *pname = NULL; /* caller can set this from argv[0] */

/* nonfatal error related to system call
   print message and return */

void err_ret (const char *fmt,...) {
	va_list ap;

	va_start(ap,fmt);
	err_doit(1,fmt,ap);
	va_end(ap);
	return;
}

/* fatal error related to system call
   print message and terminate */

void err_sys(const char *fmt, ...) {
	va_list	ap;
	
	va_start(ap,fmt);
	err_doit(1,fmt,ap);
	va_end(ap);
	exit(1);
}

/* fatal error related to system call
   print message and dump core, terminate */

void err_dump(const char *fmt, ...) {
	va_list ap;

	va_start(ap,fmt);
	err_doit(1,fmt,ap);
	va_end(ap);
	abort(); /* dump core and terminate */
	exit(1); /* should never get here */
}

/* nonfatal error unrelated to a system call 
   print a message and return */

void err_msg(const char *fmt, ...) {
	va_list ap;

	va_start(ap,fmt);
	err_doit(0,fmt,ap);
	va_end(ap);
	return;
}

/* fatal error unrelated to a system call 
   print a message and terminate */

void err_quit(const char *fmt, ...) {
	va_list(ap);

	va_start(ap,fmt);
	err_doit(0,fmt,ap);
	va_end(ap);
	exit(1);
}

/* Print a message and return to caller.
   caller specifies "errnoflag". */

static void err_doit(int errnoflag, const char *fmt, va_list ap) {
	int	errno_save;
	char	buf[MAXLINE];

	errno_save = errno; /* value caller might want printer */
	vsprintf(buf,fmt,ap);
	if(errnoflag) 
		sprintf(buf+strlen(buf), ": %s", strerror(errno_save));
	strcat(buf, "\n");
	fflush(stdout); /* in case stdout and stderr are the same */
	fputs(buf,stderr);
	fflush(NULL);	/* flushes all stdio output streams */
	return;
}
