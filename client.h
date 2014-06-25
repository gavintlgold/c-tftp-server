/* Gavin Langdon
 * Network Programming
 *
 * Project #2--TFTP Server
 * Spring 2013
 *
 */


#pragma once

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>


/* The structure for maintaining client state */
struct clientinfo {
  struct sockaddr_in address; // client's address
  socklen_t len; // address memory length
  int sockfd; // our socket's file descriptor for this client
  FILE* file; // the FILE we're reading/writing to
  int last_block; // The last block we sent/received
  int last_amount_written; // how much we wrote last time (so we can rewind the file)
  struct timeval last_time; // timestamp of last message received (for timeouts)
  int request; // type of request (RRQ or WRQ, or 0 if not set yet. Used for error checking)
  int timeouts; // How many times we've timed out--after MAX_TIMEOUTS we'll disconnect

  struct clientinfo *next; // simple linked list
};

int rewind_client_file(struct clientinfo *client);

int sendto_client(char *buf, int length, const struct clientinfo client);
void client_update_block(int size, struct clientinfo *client);
void client_set_request(int req, struct clientinfo *client);

int client_get_tid(const struct clientinfo client);

int close_client_connection(struct clientinfo *client, fd_set *masterset);

struct clientinfo * new_client(int sockfd, 
                              struct sockaddr_in *address,
                              socklen_t len, 
                              struct clientinfo **head);

struct clientinfo * delete_client(struct clientinfo *client, struct clientinfo **head);

int handle_client(struct clientinfo *client,
                  struct timeval *curtime,
                  char *buf,
                  int len_data,
                  struct clientinfo **clients,
                  fd_set *master);
