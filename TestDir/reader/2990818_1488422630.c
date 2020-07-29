/* A tool to detect gap durations during network configuration */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/ip.h>

#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <signal.h>

#define UDP_PORT    1900
#define MAX_SENDERS 16

#define ANSI_CLS "\x1b[2J"
#define ANSI_HOME "\x1b[1;1H"

static const char *progname;

unsigned long count_master[2][MAX_SENDERS];
unsigned long count_idx = 0;
volatile unsigned long *count = count_master[0];

static void reader_stats_rotate( int signo )
{
    unsigned i;
    unsigned long total = 0ul;
    float total_f;
    unsigned long *count_prev = &count_master[count_idx][0];
    
    alarm(1);
    count_idx = 1-count_idx;
    count = &count_master[count_idx][0];
    /* printf( ANSI_CLS ANSI_HOME ); */

    /* Totalise and display */
    for( i=0; i < MAX_SENDERS; i++ )
    {
	total += count_prev[i];
    }
    total_f = (float)total;
    printf( "%lu Mbytes of UDP payload received\n\n", total >> 20 );

    for( i=0; i < MAX_SENDERS; i++ )
    {
	if( count_prev[i] != 0ul )
	{
	    printf( "Writer %u: %lu bytes: %.1f%%\n", i, count_prev[i], (100.0 * (float)count_prev[i]) / total_f );
	    count_prev[i] = 0ul;
	}
    }
}

static int reader_rx( int sock_fd )
{
    /* NOTE: implicitly only support one tx partner at present: otherwise use recvfrom+getaddrinfo */
    int result;
    unsigned char packet[10000];
    struct sigaction action_new, action_old;

    memset( &action_new, 0x00, sizeof(action_new) );
    action_new.sa_handler = reader_stats_rotate;

    sigaction(SIGALRM, &action_new, &action_old);
    alarm(1);

    while( 1 )
    {
	result = recv( sock_fd, &packet, sizeof(packet), 0 );	/* Receive the next packet from our partner */
	
	if( result <= 0 )
	{
	    if( errno == EINTR )
		continue;

	    printf( "Receive failed: %s\n", strerror(errno) );
	    break;
	}
	
	/* Note: we are returned the UDP payload size received, minus protocol overhead,
	 * so bandwidth estimates are not accurate.  Ratios between them should be */
	count[packet[0]] += result;
    }

    alarm(0);
    sigaction(SIGALRM, &action_old, NULL);
    return result;
}

int main( int argc, char *argv[] )
{
    int result;
    progname = basename( argv[0] );

    int sock_fd = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP ); 
    if( sock_fd < 0 )
    {
	fprintf( stderr, "%s: cannot create socket: %s\n", progname, strerror(errno) );
	exit( -1 );
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons( UDP_PORT );
    dest_addr.sin_addr.s_addr = 0;
    result = bind( sock_fd, (struct sockaddr *)&dest_addr, sizeof(dest_addr) );
    if( result < 0 )
    {
	fprintf( stderr, "%s: cannot bind to port %u: %s\n", progname, UDP_PORT, strerror(errno) );
	exit( -1 );
    }

    printf( "%s: waiting for frames\n", progname );
    return reader_rx( sock_fd );
}
