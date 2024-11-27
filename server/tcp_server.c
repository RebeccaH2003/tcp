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

#define BUFFER_SIZE 1024

// Function to get a timestamp string
void get_timestamp(char *buffer, size_t size) {
    struct timespec ts;
    struct tm *tm_info;
    clock_gettime(CLOCK_REALTIME, &ts);
    tm_info = localtime(&ts.tv_sec);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_port>\n", argv[0]);
        return 1;
    }

    // Create a TCP socket
    int server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Set up the server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the socket to the address
    if (bind(server_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sockfd);
        return 1;
    }

    // Start listening for connections
    if (listen(server_sockfd, 5) < 0) {
        perror("Listen failed");
        close(server_sockfd);
        return 1;
    }

    // Accept a client connection
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_len);
    if (client_sockfd < 0) {
        perror("Accept failed");
        close(server_sockfd);
        return 1;
    }

    printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    // Server-side sliding window setup
    char buffer[BUFFER_SIZE];
    uint32_t expected_seq_num = 1; // Start with sequence number 1

    while (1) {
        // Receive data
        ssize_t received = recv(client_sockfd, buffer, BUFFER_SIZE, 0);
        if (received <= 0) {
            if (received == 0) {
                printf("Client disconnected.\n");
            } else {
                perror("Receive failed");
            }
            break;
        }

        // Extract the sequence number
        uint32_t seq_num;
        memcpy(&seq_num, buffer, sizeof(seq_num));
        seq_num = ntohl(seq_num); // Convert from network byte order to host byte order

        // Get timestamp for logging
        char timestamp[64];
        get_timestamp(timestamp, sizeof(timestamp));

        // Check the sequence number
        if (seq_num != expected_seq_num) {
            printf("[%s] Error: Expected sequence number %u but received %u\n", timestamp, expected_seq_num, seq_num);
            expected_seq_num = seq_num + 1; // Adjust to next sequence
        } else {
            printf("[%s] Received packet with sequence number: %u\n", timestamp, seq_num);
            expected_seq_num++;
        }

        // Send an ACK for the received sequence number
        uint32_t ack_seq_num = htonl(seq_num); // Convert to network byte order
        if (send(client_sockfd, &ack_seq_num, sizeof(ack_seq_num), 0) < 0) {
            perror("Send ACK failed");
            break;
        }
    }

    // Clean up and close sockets
    close(client_sockfd);
    close(server_sockfd);
    return 0;
}
