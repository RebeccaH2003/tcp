#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define BUFFER_SIZE 1024  // Smaller buffer for Stop-and-Wait
#define TEST_ROUNDS 10    // Number of RTT measurements

