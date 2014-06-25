/* Gavin Langdon
 * Network Programming
 *
 * Project #2--TFTP Server
 * Spring 2013
 *
 */


#pragma once

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>

#include "client.h"

const char* opcodes[5];

int (*op_handlers[5])(char *buf, int pack_size, struct clientinfo *client);

int handle_packet(char *buf, int pack_size, struct clientinfo *client);

int handle_request(char *buf, char *path, int pack_size, struct clientinfo *client);
int handle_rrq(char *buf, int pack_size, struct clientinfo *client);
int handle_wrq(char *buf, int pack_size, struct clientinfo *client);

int handle_data(char *buf, int pack_size, struct clientinfo *client);
int handle_ack(char *buf, int pack_size, struct clientinfo *client);
int handle_error(char *buf, int pack_size, struct clientinfo *client);

int require_connection(const struct clientinfo client, int type);

int send_ack(int block, const struct clientinfo client);
void send_error(int code, char *message, const struct clientinfo client);
int send_data(struct clientinfo *client);
