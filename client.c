/* Gavin Langdon
 * Network Programming
 *
 * Project #2--TFTP Server
 * Spring 2013
 *
 */


#include "debug.h"
#include "client.h"
#include "packet.h"
#include "defines.h"

/* Clean up sockets and open files */
int close_client_connection(struct clientinfo *client, fd_set *masterset) {
  LOG(1, "Closing connection to client %i", client->address.sin_port);
  close(client->sockfd);
  FD_CLR(client->sockfd, masterset);

  if (client->file != NULL) {
    fclose(client->file);
  }

  return 0;
}

// go back in the file so we can resend
int rewind_client_file(struct clientinfo *client) {
  if (client->last_block) {
    fseek(client->file, -client->last_amount_written, SEEK_CUR);
    client->last_block--;
  }
  return 0;
}

// Create a new client and put it at the head of the linked list.
struct clientinfo * new_client( int sockfd, struct sockaddr_in *address, socklen_t len, struct clientinfo **head) {
  struct clientinfo *cl;

  // Allocate
  cl = (struct clientinfo*)calloc(1, sizeof(struct clientinfo));

  // Initialize values
  cl->sockfd = sockfd;
  cl->address = *address;
  cl->len = len;

  // Replace head with this item
  cl->next = *head;
  *head = cl;

  return cl;
}

// Delete a client from the linked list. This invalidates the pointer *client!
struct clientinfo * delete_client(struct clientinfo *client, struct clientinfo **head) {
  struct clientinfo *p;

  if (client == *head) {
    *head = (*head)->next;
    free(client);
    return *head;
  }

  for (p = *head; p->next != NULL; p = p->next) {
    if (p->next == client) {
      p->next = p->next->next;
      free(client);
      return p->next;
    }
  }

  free(client);
  return NULL;
}

// Handle a received buffer from a client
int handle_client(struct clientinfo *client, 
                  struct timeval *curtime, 
                  char *buf, 
                  int len_data, 
                  struct clientinfo **clients, 
                  fd_set *master) {
  int rv;

  rv = handle_packet(buf, len_data, client);
  if (rv == RETURN_ERR || rv == RETURN_CLOSECONN) {
    close_client_connection(client, master);
    delete_client(client, clients);
    return 1;
  }
  // Only consider this a timeout if it was successful (invalid acks should not
  // reset the timeout
  if (rv != RETURN_IGNORE) {
    client->last_time = *curtime;
    client->timeouts = 0;
  }
  return 0;
}

void client_update_block(int size, struct clientinfo *client) {
  client->last_block++;
  client->last_amount_written = size;
}

void client_set_request(int req, struct clientinfo *client) {
  client->request = req;
}

int client_get_tid(const struct clientinfo client) {
  return client.address.sin_port;
}

/* Send a packet (buf) to the client with sendto */
int sendto_client(char *buf, int length, const struct clientinfo client) {
  int rv;
  rv = sendto(client.sockfd, buf, length, 0, (struct sockaddr*)&client.address, client.len);
  if (rv != length) {
    ERROR_MSG("There was a problem sending data to client %i: %s", client.address.sin_port, strerror(errno));
    return -1;
  }
  return rv;
}
