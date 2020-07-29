#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "42186245_1524069388.h"
#define TRCLEN 4096
#define DEBUG 0

static void sig_pipe(int); /* our signal handler */

int main (int argc, char **argv) {
	int n,nread,nwrite,fd1[2], fd2[2];
	pid_t pid;
  	char line[TRCLEN];
	FILE *input, *output;
	int totread=0,totwrite=0,totpiperead=0,totpipewrite=0;
	const char **childargv;


	if(signal(SIGPIPE, sig_pipe) == SIG_ERR)
		err_sys("signal error");

	if(pipe(fd1) < 0) /* half duplex stream pipe */
		err_sys("fd1 pipe error");

	if(pipe(fd2) < 0) /* half duplex stream pipe */
		err_sys("fd2 pipe error");

	if( (pid = fork() ) < 0 )
		err_sys("fork error");

	else if (pid > 0 ) { /* parent */
		close(fd1[0]);
		close(fd2[1]);
  		input  = fopen ("test.su", "r");
		output = fopen ("output.su","w");
		if (!input) 
			err_sys("Unable to open parent input file.");
		if (!output)
			err_sys("Unable to open parent output file.");

		fcntl(fd2[0],F_SETFL,O_NONBLOCK);
flag:
   		while((nread=(fread(line,1,TRCLEN,input))) > 0 ) {
			totread += nread;
			if(DEBUG)printf("parent:%d:nread=%d totread=%d\n",__LINE__,nread,totread);
			if ((nwrite=write(fd1[1],line,nread)) != nread) {
				err_sys("parent write error to pipe");
			} else {
				totpipewrite+=nwrite;
				if(DEBUG)printf("parent:%d:nwrite-pipe=%d totpipwrite=%d\n",__LINE__,nwrite,totpipewrite);
			}
			if ( (nread = read(fd2[0],line,TRCLEN) ) < 0 ) {
				if(errno == EAGAIN)  {
				if(DEBUG)err_msg("EAGAIN on read pipe in parent");
				goto flag;}
				/* goto mainloopend; */
				err_sys("parent read error from pipe 58");
			}
			totpiperead+=nread;
			if(DEBUG)printf("parent:%d:nread-pipe=%d totpiperead=%d\n",__LINE__,nread,totpiperead);
			if (nread == 0 ) {
				if(DEBUG)err_msg("from parent: child closed pipe");
				break;
			}
			if ((nwrite=fwrite(line,1,nread,output)) != nread ){
				err_sys("parent write to output error");
			} else {
				totwrite+=nwrite;
				if(DEBUG)printf("parent:%d:nwrite=%d totwrite=%d\n",__LINE__,nwrite,totwrite);
			}
		}
mainloopend:
		close(fd1[1]);
		if(DEBUG)printf("parent: outside loop\n");
		if(ferror(input))
			err_sys("parent read error from input");

		fcntl(fd2[0],F_SETFL,~O_NONBLOCK);
redo:
		while ( (nread = read(fd2[0],line,TRCLEN) ) > 0 ) {
			totpiperead+=nread;
			if(DEBUG)printf("parent:%d:nread-pipe=%d totpiperead=%d\n",__LINE__,nread,totpiperead);
			if ((nwrite=fwrite(line,1,nread,output)) != nread ){	
				err_sys("parent write to output error");
			} else {
				totwrite+=nwrite;
				if(DEBUG)printf("parent:%d:nwrite=%d totwrite=%d\n",__LINE__,nwrite,totwrite);
			}
		}
		if (nread == 0 ) {
			if(DEBUG)err_msg("from parent: child closed pipe");
			goto exitpoint;
		}
		if (nread < 0 ) goto redo;
exitpoint:
		fclose(input);
		fclose(output);
		exit(0);
	} else { /* child who will actually do the work */
		close(fd1[1]);
		close(fd2[0]);
		if(fd1[0] != STDIN_FILENO) {
			if(dup2(fd1[0],STDIN_FILENO) != STDIN_FILENO)
				err_sys("dup2 error on stdin");
			close(fd1[0]);
		}
		if(fd2[1] != STDOUT_FILENO) {
			if(dup2(fd2[1],STDOUT_FILENO) != STDOUT_FILENO)
				err_sys("dup2 error on stdout");
			close(fd2[1]);
		}
		/*if (execl("/apps/cwp/SeisUnix/bin/suagc","tpow=.5", "agc=1","wagc=.25", NULL) < 0 )*/
		switch(argc) {
			case 0|1: 
				err_sys("Not enough arguments");;
			case 2:	if (execl( (const char *) argv[1],(char *) NULL) < 0 ) err_sys("execl error");;
			case 3:	if (execl( (const char *) argv[1],argv[2],(char *) NULL) < 0 ) err_sys("execl error");;
			case 4:	if (execl( (const char *) argv[1],argv[2],argv[3],(char *) NULL) < 0 ) err_sys("execl error");;
			case 5:	if (execl( (const char *) argv[1],argv[2],argv[3],argv[4],
					(char *) NULL) < 0 ) err_sys("execl error");;
			case 6:	if (execl( (const char *) argv[1],argv[2],argv[3],argv[4], argv[5],
					(char *) NULL) < 0 ) err_sys("execl error");;
			case 7:	if (execl( (const char *) argv[1],argv[2],argv[3],argv[4], argv[5],argv[6],
					(char *) NULL) < 0 ) err_sys("execl error");;
			case 8:	if (execl( (const char *) argv[1],argv[2],argv[3],argv[4], argv[5],argv[6],argv[7],
					(char *) NULL) < 0 ) err_sys("execl error");;
			case 9:	if (execl( (const char *) argv[1],argv[2],argv[3],argv[4], argv[5],argv[6],argv[7],argv[8],
					(char *) NULL) < 0 ) err_sys("execl error");;
			case 10: if (execl( (const char *) argv[1],argv[2],argv[3],argv[4], argv[5],argv[6],argv[7],argv[8],
					argv[9], (char *) NULL) < 0 ) err_sys("execl error");;
			default: 
				err_sys("Too many arguments");;
		}
	}
}

static void sig_pipe(int signo) {
	err_msg("SIGPIPE caught\n");
	exit (1);
}
