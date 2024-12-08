#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

#define BUFFER_SIZE 1024  // Smaller buffer for Stop-and-Wait
#define TEST_ROUNDS 10    // Number of RTT measurements
#define MAX_WINDOW_SIZE 1024
#define CONVERSION 1000000

void stop_and_wait(int sockfd, uint32_t packet_size, uint64_t interval);
uint32_t sliding_window(int sockfd, uint32_t pkt_size, int duration, uint32_t max_window_size, uint32_t step_size);
