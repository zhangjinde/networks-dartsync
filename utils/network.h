#ifndef NETWORK_H
#define NETWORK_H

#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>

#define LENGTH_OF_LISTEN_QUEUE 20 
/* The tracker's port in the main thread */
#define SERVER_PORT 7371
/* The peer's port in the main thread */
#define PEER_PORT 7471
/* State */
#define SIGNAL_HEARTBEAT 1
#define SIGNAL_FILE_UPDATE 2
/* Interval */
#define HEARTBEAT_INTERVAL_SEC 10
#define FILE_UPDATE_INTERVAL_SEC 9
#define BUFFER_SIZE 1024

unsigned long get_My_IP();

/*
 * Create a socket listening on ServerPort
 * Return value is the sockfd
 */
int create_server_socket(int ServerPort);

/*
 * Connect to server with ip address ServerIp and port number ServerPort
 * Return value is the client sockfd
 */
int create_client_socket_byIp(unsigned long ServerIp, int ServerPort);

/*
 * Connect to server with ip address "tahoe.cs.dartmouth.edu" and port number ServerPort
 */
int create_client_socket(char* HostName, int ServerPort);

/*
 * This function returns the ip address given socket
 */
char *get_address_from_ip(int socket);
/* 
 *Get the unsigned long type ip of peer from a socket 
 */
unsigned long get_peer_IP(int peer_socket);

#endif
