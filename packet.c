/* Gavin Langdon
 * Network Programming
 *
 * Project #2--TFTP Server
 * Spring 2013
 *
 */


#include "packet.h"
#include "client.h"

#include "debug.h"
#include "defines.h"

#include <libgen.h>

const char *opcodes[5] = {
  "Read request",
  "Write request",
  "Data",
  "Acknowledgement",
  "Error"
};

int (*op_handlers[5])(char *buf, int pack_size, struct clientinfo *client) = {
  handle_rrq,
  handle_wrq,
  handle_data,
  handle_ack,
  handle_error
};

static int get_buf_val(char *buf, int offset) {
  short netshort;
  memcpy(&netshort, &buf[offset], 2);
  return ntohs(netshort);
}

static void set_buf_val(char *buf, int offset, int value) {
  short netshort = htons(value);
  memcpy(&buf[offset], &netshort, 2);
}

// Get a value from the first 2 bytes
static int get_op(char *buf) {
  return get_buf_val(buf, 0);
}

static void set_op(char *buf, int value) {
  set_buf_val(buf, 0, value);
}

static int get_block(char *buf) {
  return get_buf_val(buf, 2);
}

static void set_block(char *buf, int value) {
  set_buf_val(buf, 2, value);
}

static char *get_datablock(char *buf) {
  return &buf[TFTP_STD_HEADER_SIZE];
}

static int handle_base_size(int pack_size) {
  // We should not have to worry about pack_size being larger since recvfrom should handle this. But let's check anyway.
  return pack_size <= TFTP_STD_HEADER_SIZE + TFTP_MAX_BUF_SIZE && pack_size >= TFTP_STD_HEADER_SIZE;
}


/* Attempt to recognize the packet type and handle base errors */
int handle_packet(char *buf, int pack_size, struct clientinfo *client) {
  int op;

  if (!handle_base_size(pack_size)) {
    ERROR_MSG("Could not parse packet! Packet size (%i) not in range (%i <-> %i)", pack_size, TFTP_STD_HEADER_SIZE, TFTP_STD_HEADER_SIZE + TFTP_MAX_BUF_SIZE);
    send_error(ERRCODE_ILLEGAL, "Invalid packet length received", *client);
    return RETURN_ERR;
  }

  buf[pack_size + 1] = '\0'; // buffer is 1 byte larger than max recv amount. here we avoid overflows in printf/etc

  // Determine which operation we're using
  op = get_op(buf);

  // ensure operation in valid range
  if (op > OP_ERROR || op < OP_RRQ) {
    ERROR_MSG("Invalid opcode: %i", op);
    send_error(ERRCODE_ILLEGAL, "Invalid opcode given.", *client);
    return RETURN_ERR;
  }

  LOG(2, "Client %i::Got opcode %i: %s", client_get_tid(*client), op, opcodes[op-1]);
  
  // Call corresponding handler function
  return op_handlers[op-1](buf, pack_size, client);
}

// Remove all paths from the requested filename, for sandboxing
void strip_path(char *path, char *buf) {
  char *base;
  strncpy(path, buf, TFTP_MAX_REQ_BUF_SIZE);
  base = basename(path);
  strncpy(path, base, TFTP_MAX_REQ_BUF_SIZE);
}

/* Handle a request (RRQ or WRQ) by parsing filename and data mode.
 * This function performs error checking and preparation common to both RRQ and WRQ operations */
int handle_request(char *buf, char *path, int pack_size, struct clientinfo *client) {
  int filename_len;
  char *mode;

  /* Ensure this is a fresh client */
  if (!require_connection(*client, 0)) {
    return RETURN_ERR;
  }

  /* Get the filename from the 3rd byte onwards, at a maximum of the request buffer size, minus 2 bytes of padding.
   * The padding is so we don't illegally go out of buffer range when determining mode (\0 + first char of mode) */
  filename_len = strnlen(&buf[TFTP_REQ_HEADER_SIZE], TFTP_MAX_REQ_BUF_SIZE-2);

  // Filename has to be at least 1 byte long
  if (filename_len < 1) {
    send_error(ERRCODE_ILLEGAL, "Malformed filename.", *client);
    ERROR_MSG("Malformed filename found");
    return RETURN_ERR;
  }

  strip_path(path, &buf[TFTP_REQ_HEADER_SIZE]);

  /* Get the mode by looking at the string after the filename */
  mode = &buf[TFTP_REQ_HEADER_SIZE + filename_len + 1];
  int mode_len = strnlen(mode, TFTP_MAX_REQ_BUF_SIZE - filename_len - 1);

  if (mode_len < 1) {
    send_error(ERRCODE_ILLEGAL, "Malformed mode specification", *client);
    ERROR_MSG("Malformed mode specification");
    return RETURN_ERR;
  }

  /* We only support octet (binary) mode right now */
  if (strcmp(mode, "octet") != 0) {
    ERROR_MSG("Server does not support requested mode");
    send_error(ERRCODE_UNKNOWN, "Server does not support requested mode", *client);
    return RETURN_ERR;
  }

  LOG(2, "File specified: '%s', using mode: %s", path, mode);

  return RETURN_STD;
}

/* Handle a read request */
int handle_rrq(char *buf, int pack_size, struct clientinfo *client) {
  char path[TFTP_MAX_REQ_BUF_SIZE];

  if (handle_request(buf, path, pack_size, client) == RETURN_ERR) {
    return RETURN_ERR;
  }

  client_set_request(OP_RRQ, client);

  LOG(1, "Opening file '%s' for reading", path);

  /* Handle error cases with fopen */
  if ((client->file = fopen(path, "rb")) == NULL) {
    perror("Error opening file");
    if (errno == EACCES) {
      send_error(ERRCODE_ACCESS, "Access violation.", *client);
      return RETURN_ERR;
    }
    else if (errno == ENOENT) {
      send_error(ERRCODE_NOTFOUND, "File not found.", *client);
      return RETURN_ERR;
    }
    else if (errno == ENOMEM || errno == ENOSPC || errno == ENFILE) {
      send_error(ERRCODE_DISK, "Cannot allocate space for the file", *client);
      return RETURN_ERR;
    }

    send_error(ERRCODE_UNKNOWN, strerror(errno), *client);
    return RETURN_ERR;
  }

  LOG(2, "Opened file. Sending initial packet");

  return send_data(client);
}


/* Handle a write request */
int handle_wrq(char *buf, int pack_size, struct clientinfo *client) {
  char path[TFTP_MAX_REQ_BUF_SIZE];

  if (!handle_request(buf, path, pack_size, client) == RETURN_ERR) {
    return RETURN_ERR;
  }

  client_set_request(OP_WRQ, client);

  LOG(1, "Opening file '%s' for writing", path);
  // Check if file exists (atomically, for security/race conditions)
  int fd;
  // Open file only if it can be created. Open in write only mode, ensuring it creates the file
  fd = open(path, O_CREAT | O_WRONLY | O_EXCL, 0644);
  if (fd < 0) {
    if (errno == EEXIST) {
      send_error(ERRCODE_EXISTS, "File already exists.", *client);
      return RETURN_ERR;
    }
    else if (errno == ENOMEM || errno == ENOSPC || errno == ENFILE) {
      send_error(ERRCODE_DISK, "Cannot create file", *client);
      return RETURN_ERR;
    }
    perror("Could not create file.");
    send_error(ERRCODE_UNKNOWN, strerror(errno), *client);
    return RETURN_ERR;
  }

  // Create FILE from fd
  if (( client->file = fdopen(fd, "wb")) == NULL) {
    perror("fdopen");
    send_error(ERRCODE_UNKNOWN, strerror(errno), *client); 
    return RETURN_ERR;
  }

  // Ready for data!
  return send_ack(0, *client);
}

/* Handle an incoming data packet */
int handle_data(char *buf, int pack_size, struct clientinfo *client) {
  int block;
  int buf_size = pack_size - TFTP_STD_HEADER_SIZE;

  if (!require_connection(*client, OP_WRQ)) {
    return RETURN_ERR;
  }

  block = get_block(buf);

  LOG(2, "Got packet %i with size %i", block, buf_size);

  // If we get an invalid block, we might have had a timeout. The client sending the data needs to be acknowledged,
  // even if the data is to be ignored. Otherwise it will not continue sending data.
  // If there are more than UINT_MAX packets, the client seems to expect integer wrapping
  unsigned next_block = client_get_next_block(client);
  if (next_block != block) {
    ERROR_MSG("(client %i) Got unexpected block #%i (expected #%i), acknowledging and discarding", client_get_tid(*client), block, next_block);
    if (send_ack(block, *client) == RETURN_ERR) {
      return RETURN_ERR;
    }
    return RETURN_IGNORE;
  }

  /* Only actually write if the packet has data */
  if (buf_size) {
    if (fwrite(get_datablock(buf), 1, buf_size, client->file) != buf_size) {
      if (ferror(client->file) != 0) {
        perror("error writing to file");
        send_error(ERRCODE_UNKNOWN, strerror(errno), *client);
        return RETURN_ERR;
      }
    }
    LOG(2, "Wrote %i bytes.", buf_size);
  }

  if (send_ack(block, *client) == RETURN_ERR) {
    return RETURN_ERR;
  }

  client_update_block(buf_size, client);

  if (buf_size < TFTP_MAX_BUF_SIZE) {
    LOG(1, "Packet smaller than max size. Closing connection");
    return RETURN_CLOSECONN;
  }
  return RETURN_STD;
}


/* Handle an Acknowledgement packet */
int handle_ack(char *buf, int pack_size, struct clientinfo *client) {
  if (!require_connection(*client, OP_RRQ)) {
    return RETURN_ERR;
  }

  int block = get_block(buf);
  if (client->last_block != block) {
    // If we're sending the data, we are required to ignore any invalid acks to avoid SAS
    ERROR_MSG("(client %i) Got invalid block id %i, ignoring", client_get_tid(*client), block);
    return RETURN_IGNORE;
  }

  return send_data(client);
}

/* Handle an error packet */
int handle_error(char *buf, int pack_size, struct clientinfo *client) {
  int error = get_block(buf);
  ERROR_MSG("Got error %i: %s", error, get_datablock(buf));
  return RETURN_ERR;
}

/* Ensure the client is reading/writing/nothing to avoid state errors */
int require_connection(const struct clientinfo client, int type) {
  if (client.request != type) {
    ERROR_MSG("Got a packet for the wrong type of connection");
    send_error(ERRCODE_ILLEGAL, "Expected a different type of packet for this TID.", client);
    return 0;
  }
  return 1;
}


/* Send an error message to the client */
/* Returns void because we don't want to handle errors within errors */
void send_error(int code, char *message, const struct clientinfo client) {
  char buf[TFTP_STD_HEADER_SIZE + TFTP_MAX_BUF_SIZE];
  int buflen;
  set_op(buf, OP_ERROR);
  set_block(buf, code);

  LOG(1, "Sending error: %s", message);

  // Ensure we don't overflow our buffer
  // snprintf returns amount of bytes sent EXCLUDING null character, and we need to send the null char too
  buflen = snprintf(get_datablock(buf), TFTP_MAX_BUF_SIZE, "%s", message);
  sendto_client(buf, TFTP_STD_HEADER_SIZE+buflen+1, client); // send null byte too, hence the +1
}

/* Send an acknowledgement packet to the client */
int send_ack(int block, const struct clientinfo client) {
  char buf[TFTP_STD_HEADER_SIZE];
  set_op(buf, OP_ACK);
  set_block(buf, block);

  LOG(2, "Acknowledging block #%i", block);

  if (sendto_client(buf, TFTP_STD_HEADER_SIZE, client) == -1) {
    return RETURN_ERR;
  }
  return RETURN_STD;
}


/* Send a data packet to the client */
int send_data(struct clientinfo *client) {
  char buf[TFTP_MAX_BUF_SIZE + TFTP_STD_HEADER_SIZE];
  int size;

  LOG(2, "Sending data block #%i", client->last_block + 1);
  set_op(buf, OP_DATA);
  set_block(buf, client->last_block + 1);

  if ((size = fread(get_datablock(buf), 1, TFTP_MAX_BUF_SIZE, client->file)) < TFTP_MAX_BUF_SIZE) {
    if (ferror(client->file) != 0) {
      perror("Error reading file");
      send_error(ERRCODE_UNKNOWN, strerror(errno), *client);
      return RETURN_ERR;
    }
  }

  LOG(2,"Sending buf with size %i to client %i", size, client_get_tid(*client));

  if (sendto_client(buf, size + TFTP_STD_HEADER_SIZE, *client) == -1) {
    return RETURN_ERR;
  }

  client_update_block(size, client);

  if (size < TFTP_MAX_BUF_SIZE) {
    LOG(2, "Got last ack for this file. Closing connection");
    return RETURN_CLOSECONN;
  }

  return RETURN_STD;
}
