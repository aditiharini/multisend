#include <string>
#include <vector>
#include <poll.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <cpr/cpr.h>

#include "acker.hh"
#include "saturateservo.hh"
#include "payload.hh"

using namespace std;

void send_monitoring(cpr::Url url, cpr::Payload payload) {
  cpr::Response res = cpr::Post(url, cpr::Timeout{2000}, payload); 
  if (res.error.code != cpr::ErrorCode::OK) {
    fprintf( stderr, "Unable to send monitoring\n");
  }
}

int main( int argc, char *argv[] )
{
  if ( argc != 2 && argc != 7 ) {
    fprintf( stderr, "Usage: %s [RELIABLE_IP RELIABLE_DEV TEST_IP TEST_DEV SERVER_IP MODE]\n",
	     argv[ 0 ]);
    exit( 1 );
  }

  bool test = false;
  int packet_size = 1400;

  Socket data_socket, feedback_socket;
  bool server;

  int sender_id = getpid();

  Socket::Address remote_data_address( UNKNOWN ), remote_feedback_address( UNKNOWN );

  string monitoring_url = "";
  uint64_t ts=Socket::timestamp();
  if ( argc == 2 ) { /* server */
    server = true;
    if (strcmp(argv[1], "test") == 0)  {
      test = true;
      packet_size = sizeof(SatPayload);
    }
    data_socket.bind( Socket::Address( "0.0.0.0", 9001 ) );
    feedback_socket.bind( Socket::Address( "0.0.0.0", 9002 ) );
    monitoring_url += "localhost:10000/server/saturatr";
  } else { /* client */
    server = false;
    if (strcmp(argv[6], "test") == 0)  {
      test = true;
      packet_size = sizeof(SatPayload);
    }
    
    const char *reliable_ip = argv[ 1 ];
    const char *reliable_dev = argv[ 2 ];

    const char *test_ip = argv[ 3 ];
    const char *test_dev = argv[ 4 ];

    const char *server_ip = argv[ 5 ];

    sender_id = ( (int)(ts/1e9) );

    data_socket.bind( Socket::Address( test_ip, 9003 ) );
    data_socket.bind_to_device( test_dev );
    remote_data_address = Socket::Address( server_ip, 9001 );

    feedback_socket.bind( Socket::Address( reliable_ip, 9004 ) );
    feedback_socket.bind_to_device( reliable_dev );
    remote_feedback_address = Socket::Address( server_ip, 9002 );
    Socket::Address monitoring_addr = Socket::Address(server_ip, 10000);
    monitoring_url += monitoring_addr.str() + "/drone/saturatr";
  }

  FILE* log_file;
  char log_file_name[50];
  sprintf(log_file_name,"%s-%d-%d",server ? "server" : "client",(int)(ts/1e9),sender_id);
  log_file=fopen(log_file_name,"w");

  SaturateServo saturatr( "OUTGOING", log_file, feedback_socket, data_socket, remote_data_address, server, sender_id, packet_size);
  Acker acker( "INCOMING", log_file, data_socket, feedback_socket, remote_feedback_address, server, sender_id );

  saturatr.set_acker( &acker );
  acker.set_saturatr( &saturatr );
  int acker_packets_received = 0, saturatr_packets_received = 0;
  int acker_packets_sent = 0, saturatr_packets_sent = 0;
  uint64_t next_monitor_send_time = Socket::timestamp();

  while ( 1 ) {
    fflush( NULL );
    if (test) {
      std::string fmt_str = "%s (acker_packets_sent: %d, acker_packets_received: %d, saturatr_packets_sent: %d, saturatr_packets_received: %d)\n";
      if ((acker_packets_received && saturatr_packets_received && saturatr_packets_sent) && (server || acker_packets_sent)) {
        fprintf(stdout, 
        fmt_str.c_str(),
        "SUCCESS",
        acker_packets_sent,
        acker_packets_received,
        saturatr_packets_sent,
        saturatr_packets_received
        );
        return EXIT_SUCCESS;
      } else {
        fprintf(stdout, 
        fmt_str.c_str(),
        "WAITING",
        acker_packets_sent,
        acker_packets_received,
        saturatr_packets_sent,
        saturatr_packets_received
        );
      }
    } else {
      if (next_monitor_send_time < Socket::timestamp()) {
        send_monitoring(
          cpr::Url{monitoring_url}, 
          cpr::Payload{
            {"acker_packets_sent", to_string(acker_packets_sent)},
            {"acker_packets_received", to_string(acker_packets_received)},
            {"saturatr_packets_sent", to_string(saturatr_packets_sent)},
            {"saturatr_packets_received", to_string(saturatr_packets_received)}
          }
        );
        next_monitor_send_time = Socket::timestamp() + 1000000000;
      }
    }

    /* possibly send packet */
    saturatr_packets_sent += saturatr.tick();
    acker_packets_sent += acker.tick();
    
    /* wait for incoming packet OR expiry of timer */
    struct pollfd poll_fds[ 2 ];
    poll_fds[ 0 ].fd = data_socket.get_sock();
    poll_fds[ 0 ].events = POLLIN;
    poll_fds[ 1 ].fd = feedback_socket.get_sock();
    poll_fds[ 1 ].events = POLLIN;

    struct timespec timeout;
    uint64_t next_transmission_delay = std::min( saturatr.wait_time(), acker.wait_time() );

    if ( next_transmission_delay == 0 ) {
      fprintf( stderr, "ZERO %ld %ld\n", saturatr.wait_time(), acker.wait_time() );
    }

    timeout.tv_sec = next_transmission_delay / 1000000000;
    timeout.tv_nsec = next_transmission_delay % 1000000000;
    ppoll( poll_fds, 2, &timeout, NULL );

    if ( poll_fds[ 0 ].revents & POLLIN ) {
      acker.recv();
      acker_packets_received++;
    }

    if ( poll_fds[ 1 ].revents & POLLIN ) {
      saturatr.recv();
      saturatr_packets_received++;
    }
  }
}
