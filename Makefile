# Gavin Langdon
# Network Programming
# 
# Project #2--TFTP Server
# Spring 2013

default:
	gcc main.c client.c packet.c -Wall -o tftp-server -DDEBUG_MODE=1
debug:
	gcc main.c client.c packet.c -Wall -g -DDEBUG_MODE=2 -o tftp-server

