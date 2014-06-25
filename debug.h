/* Gavin Langdon
 * Network Programming
 * Homework #2
 * Spring 2013
 */


#pragma once

#include "stdio.h"

#ifdef DEBUG_MODE /* Compiler flag */

#define DEBUG 1

#define LOG(level, message, ...) if (DEBUG_MODE >= level) { fprintf(stdout, "LOG %i (%s:%i): " message "\n", level, __FILE__, __LINE__, ##__VA_ARGS__); }

#else

#define DEBUG 0

#define LOG(message, ...) /* Un-define so the lines become empty */

#endif

#define ERROR_MSG(message, ...) fprintf(stderr, "ERROR (%s:%i): " message "\n", __FILE__, __LINE__, ##__VA_ARGS__)
