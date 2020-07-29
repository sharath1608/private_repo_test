#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

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

#define PAYLOAD_BYTES	1000
#define UDP_PORT	1900

static const char *progname = "";

static void usage( const char *msg )
{
    fprintf( stderr, "%s: %s\n", progname, msg );
    fprintf( stderr, "usage: %s <destip>\n", progname );
}

static int writer_tx( int sock_fd, const uint8_t writer_id )
{
    int result;
    unsigned char packet[PAYLOAD_BYTES];

    memset( packet, 0x00, sizeof(packet) );
    packet[0] = writer_id;

    while( 1 )
    {
	do
	{
	    result = send( sock_fd, packet, sizeof(packet), MSG_DONTWAIT );
	    if( result < 0 && errno != EAGAIN )
	    {
		fprintf( stderr, "%s: couldn't send packet: %s\n", progname, strerror(errno) );
		exit( -1 );
	    }

	} while( result < 0 && errno == EAGAIN );
    }
}

int main( int argc, char *argv[] )
{
    int result;
    progname = basename( argv[0] );

    if( argc != 3 )
    {
	usage( "Writer ID and Target IP expected" );
	exit( -1 );
    }
    const uint8_t writer_id = atoi(argv[1]);
    const char *dest_ip = argv[2];

    int sock_fd = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP ); 
    if( sock_fd < 0 )
    {
	fprintf( stderr, "%s: cannot create socket: %s\n", progname, strerror(errno) );
	exit( -1 );
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons( UDP_PORT );
    result = inet_aton( dest_ip, &dest_addr.sin_addr );
    if( result < 0 )
    {
	usage( "Cannot parse destination IP" );
	exit( -1 );
    }
    result = connect( sock_fd, (struct sockaddr *)&dest_addr, sizeof(dest_addr) );
    if( result < 0 )
    {
	fprintf( stderr, "%s: cannot connect to %s: %s\n", progname, dest_ip, strerror(errno) );
	exit( -1 );
    }

    printf( "%s %hhu: sending frames to %s\n", progname, writer_id, dest_ip );
    return writer_tx( sock_fd, writer_id );
}
