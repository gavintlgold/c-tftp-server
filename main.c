/* Gavin Langdon
 * Network Programming
 *
 * Project #2--TFTP Server
 * Spring 2013
 *
 */


#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "debug.h"
#include "client.h"
#include "packet.h"

#include "defines.h"

int get_local_addr() {
  struct addrinfo hints, *ai, *p;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  int rv;
  if ((rv = getaddrinfo(NULL, TFTP_PORT, &hints, &ai)) != 0) {
    ERROR_MSG("tftpserver: %s", gai_strerror(rv));
    return -1;
  }

  for (p = ai; p!= NULL; p = p->ai_next) {
    if ((rv = socket(p->ai_addr->sa_family, SOCK_DGRAM, 0)) == -1) {
      perror("tftpserver: failed to create local socket");
      continue;
    }

    if (bind(rv, p->ai_addr, p->ai_addrlen) == -1) {
      close(rv);
      perror("tftpserver: bind");
      continue;
    } 
 
    return rv;
  }

  ERROR_MSG("tftpserver: Could not get local address");
  return -1;
}


int main(int argc, char **argv) {
  fd_set master;
  fd_set read_fds;

  int listener;

  char buf[TFTP_STD_HEADER_SIZE + TFTP_MAX_BUF_SIZE + 1]; // add 1 character to ensure we're null-escaped

  struct clientinfo *clients = NULL;

  FD_ZERO(&master);
  FD_ZERO(&read_fds);

  if ((listener = get_local_addr()) == -1) {
    return 3;
  }

  FD_SET(listener, &master);
  int fdmax = listener;

  LOG(1, "TFTP Server bound to port %s. Listening for a connection.", TFTP_PORT);

  while (1) {
    struct timeval tv, curtime;
    int num_fresh_fds;
    int len_data;
    struct sockaddr addrin;
    socklen_t sock_len = sizeof(struct sockaddr_in);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    read_fds = master;
    if ((num_fresh_fds = select(fdmax+1, &read_fds, NULL, NULL, &tv)) == -1) {
      /* Restart on signal */
      if (errno == EINTR) {
        continue;
      }
      perror("select");
      return 4;
    }

    LOG(3, "%i fresh fds selected.", num_fresh_fds);

    /* Get time select returned, for timeouts */
    gettimeofday(&curtime, NULL);

    /* Check if our master is set */
    if (FD_ISSET(listener, &read_fds)) {
      struct clientinfo *client;

      LOG(3, "Establishing a new connection");

      if ((len_data = recvfrom(listener, buf, TFTP_STD_HEADER_SIZE + TFTP_MAX_BUF_SIZE, 0, &addrin, &sock_len)) == -1) {
        perror("recvfrom");
        continue;
      }

      // Establish ephemeral connection with client
      int new_fd;
      if ((new_fd = socket(addrin.sa_family, SOCK_DGRAM, 0)) == -1) {
        continue;
      }

      client = new_client( new_fd, (struct sockaddr_in*)&addrin, sock_len, &clients);

      FD_SET(new_fd, &master);
      fdmax = new_fd;

      LOG(1, "Bound to new client on fd %i with tid %i", new_fd, client->address.sin_port);
      handle_client(client, &curtime, buf, len_data, &clients, &master);

    }
    /* Check if any connected clients are set */
    struct clientinfo* p;
    for (p = clients; p != NULL; p = p->next) {
      if (FD_ISSET(p->sockfd, &read_fds)) {
        struct sockaddr_in addrin;
        socklen_t sock_len = sizeof(struct sockaddr_in);
        LOG(3, "Existing connection.");
        if ((len_data = recvfrom(p->sockfd, buf, TFTP_STD_HEADER_SIZE + TFTP_MAX_BUF_SIZE, 0, (struct sockaddr*)&addrin, &sock_len)) == -1) {
          perror("Error receiving data.");
          continue;
        }
        if (addrin.sin_port != p->address.sin_port) {
          // Construct a temporary client so we can give it the bad news about being unauthorized
          struct clientinfo new_client;
          new_client.address = addrin;
          new_client.sockfd = p->sockfd;
          send_error(ERRCODE_TID, "Unknown transfer ID.", new_client);
          continue;
        }
        if (handle_client(p, &curtime, buf, len_data, &clients, &master) == 1) {
          /* p invalidated, exit for */
          break;
        }
      }
      /* Deal with timeouts */
      else {
        LOG(3, "client %i last time: %li delta: %li", p->address.sin_port, p->last_time.tv_sec, curtime.tv_sec - p->last_time.tv_sec);
        if (curtime.tv_sec - p->last_time.tv_sec >= TIMEOUT) {
          p->timeouts++;
          if (p->timeouts >= MAX_TIMEOUTS) {
            ERROR_MSG("Client tid=%i maximum timeouts. Closing connection", p->address.sin_port);
            close_client_connection(p, &master);
            delete_client(p, &clients);
            continue;
          }
          if (p->request == OP_RRQ) {
            int rv;
            LOG(1, "Client %i timed out. Attempting to resend block.", p->address.sin_port);
            rewind_client_file(p);
            rv = send_data(p);
            if (rv == RETURN_ERR || rv == RETURN_CLOSECONN) { 
              close_client_connection(p, &master);
              delete_client(p, &clients);
              /* p invalidated, exit for */
              break;
            }
          }
          p->last_time = curtime;
        }
      }
    }
  }

  close(listener);

  return 0;
}
