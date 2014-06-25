/* Gavin Langdon
 * Network Programming
 *
 * Project #2--TFTP Server
 * Spring 2013
 *
 */

#pragma once 

#define TFTP_PORT             "69"
#define TFTP_MAX_BUF_SIZE     512
#define TFTP_MAX_REQ_BUF_SIZE 514
#define TFTP_STD_HEADER_SIZE  4
#define TFTP_REQ_HEADER_SIZE  2

#define TIMEOUT      5 // five second timeout
#define MAX_TIMEOUTS 5

#define OP_RRQ   1
#define OP_WRQ   2
#define OP_DATA  3
#define OP_ACK   4
#define OP_ERROR 5

#define ERRCODE_UNKNOWN  0
#define ERRCODE_NOTFOUND 1
#define ERRCODE_ACCESS   2
#define ERRCODE_DISK     3
#define ERRCODE_ILLEGAL  4
#define ERRCODE_TID      5
#define ERRCODE_EXISTS   6
#define ERRCODE_USER     7

#define RETURN_STD       1
#define RETURN_CLOSECONN 2
#define RETURN_IGNORE    3
#define RETURN_ERR       -1

