#include <string>
#include <vector>
#include <poll.h>
#include <assert.h>

#include "socket.hh"
#include "rate-estimate.hh"

using namespace std;

Socket::Address get_nat_addr( const Socket & sender, const Socket::Address & dest,
			      const Socket & receiver )
{
  char buf[ 10 ];
  for ( int i = 0; i < 10; i++ ) {
    buf[ i ] = rand() % 256;
  }
  
  string to_send( buf, 10 );

  sender.send( Socket::Packet( dest, to_send ) );
  Socket::Packet received( receiver.recv() );

  if ( received.payload != to_send ) {
    fprintf( stderr, "Bad packet received while getting NAT addresses.\n" );
    exit( 1 );
  }

  return received.addr;
}

double hread( uint64_t in )
{
  return (double) in / 1.e9;
}

int main( void )
{
  /* Create and bind Ethernet socket */
  Socket ethernet_socket;
  Socket::Address ethernet_address( "128.30.76.255", 9000 );
  ethernet_socket.bind( ethernet_address );
  ethernet_socket.bind_to_device( "eth0" );

  /* Create and bind LTE socket */
  Socket lte_socket;

  lte_socket.bind( Socket::Address( "10.100.1.1", 9001 ) );
  lte_socket.bind_to_device( "usb0" );

  /* Figure out the NAT addresses of each of the three LTE sockets */
  Socket::Address target( get_nat_addr( lte_socket, ethernet_address, ethernet_socket ) );
  fprintf( stderr, "LTE = %s\n", target.str().c_str() );

  RateEstimate anirudh( 1.0, 20 );
  
  const unsigned int MAX_PACKETS_OUTSTANDING = 500;
  const unsigned int PACKET_SIZE = 1000;
  unsigned int packets_outstanding = 0;
  
  while ( 1 ) {
    while ( packets_outstanding < MAX_PACKETS_OUTSTANDING ) {
      
    }
  }
}
