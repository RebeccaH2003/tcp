#include "tcp_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

// TCP Parameters
uint16_t SRC_PORT;
uint16_t DST_PORT;
char *DST_IP;  // Correct type for inet_pton
uint32_t SRC_IP;
uint32_t PKT_SIZE;
uint64_t TIME_INTERVAL;
int MODE;

#define BUFFER_SIZE 1024  // Define BUFFER_SIZE
#define TEST_ROUNDS 10    // Define number of test rounds

double stop_and_wait(int sockfd) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 'A', BUFFER_SIZE); // Fill buffer with dummy data
    char ack[BUFFER_SIZE];
    double total_rtt = 0.0;
    for (int i = 0; i < TEST_ROUNDS; i++) {  // Correct iteration to TEST_ROUNDS
        struct timeval start, end;

        // Record start time
        gettimeofday(&start, NULL);

        // Send a packet
        if (send(sockfd, buffer, BUFFER_SIZE, 0) < 0) {
            perror("send failed");
            close(sockfd);
            return -1;  // Return error indicator
        }

        // Wait for ACK
        if (recv(sockfd, ack, BUFFER_SIZE, 0) <= 0) {
            perror("recv failed");
            close(sockfd);
            return -1;  // Return error indicator
        }

        // Record end time
        gettimeofday(&end, NULL);

        // Calculate RTT
        double rtt = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
        printf("RTT for round %d: %.6f seconds\n", i + 1, rtt);

        total_rtt += rtt;
    }
    return total_rtt;
}

int main(int argc, char *argv[]) {
    // Clean IP table
    system("iptables -P OUTPUT ACCEPT");
    system("iptables -F OUTPUT");
    system("iptables -A OUTPUT -p tcp --tcp-flags RST RST -j DROP");

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strstr(argv[i], "--mode=") != NULL || strstr(argv[i], "-m=") != NULL) {
            char *value = strchr(argv[i], '=');
            if (value) MODE = strtol(value + 1, NULL, 10);
            continue;
        } else if (strstr(argv[i], "--dst_port=") != NULL || strstr(argv[i], "-sp=") != NULL) {
            char *value = strchr(argv[i], '=');
            if (value) DST_PORT = strtol(value + 1, NULL, 10);
            continue;
        } else if (strstr(argv[i], "--dst_ip=") != NULL || strstr(argv[i], "-dp=") != NULL) {
            char *value = strchr(argv[i], '=');
            if (value) DST_IP = strdup(value + 1);  // Copy string safely
            continue;
        } else if (strstr(argv[i], "--testing_interval=") != NULL || strstr(argv[i], "-i=") != NULL) {
            char *value = strchr(argv[i], '=');
            if (value) TIME_INTERVAL = strtol(value + 1, NULL, 10);
            continue;
        }
    }

    printf("I am here now\n");

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return 1;
    }

    printf("I am here now2\n");
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DST_PORT);
    if (inet_pton(AF_INET, DST_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        close(sockfd);
        return 1;
    }
    printf("I am here now3\n");

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connection failed");
        close(sockfd);
        return 1;
    }
    printf("I am here now4\n");

    double total_rtt = 0.0;
    if (MODE == 1) {
        printf("I am in stop and wait now\n");
        total_rtt = stop_and_wait(sockfd);
        if (total_rtt < 0) {
            printf("Error during stop-and-wait.\n");
            close(sockfd);
            return 1;
        }
    }

    // Calculate average RTT
    double avg_rtt = total_rtt / TEST_ROUNDS;
    printf("Average RTT: %.6f seconds\n", avg_rtt);

    // Estimate optimal window size (assuming 100 Mbps bandwidth)
    double bandwidth = 100 * 1e6; // 100 Mbps in bits per second
    double optimal_window = bandwidth * avg_rtt / 8; // Convert bits to bytes
    printf("Optimal window size: %.2f bytes\n", optimal_window);

    close(sockfd);
    free(DST_IP);  // Free allocated memory
    return 0;
}


// int main(int argc, char *argv[]) {
//     if (argc != 3) {
//         fprintf(stderr, "Usage: %s <server_ip> <server_port>\n", argv[0]);
//         return 1;
//     }

//     int sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     if (sockfd < 0) {
//         perror("socket creation failed");
//         return 1;
//     }

//     struct sockaddr_in server_addr;
//     memset(&server_addr, 0, sizeof(server_addr));
//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(atoi(argv[2]));
//     if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
//         perror("inet_pton failed");
//         close(sockfd);
//         return 1;
//     }

//     if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
//         perror("connection failed");
//         close(sockfd);
//         return 1;
//     }

//     char buffer[BUFFER_SIZE];
//     memset(buffer, 'A', BUFFER_SIZE); // Fill buffer with dummy data
//     char ack[BUFFER_SIZE];

//     double total_rtt = 0.0;

//     for (int i = 0; i < TEST_ROUNDS; i++) {
//         struct timeval start, end;

//         // Record start time
//         gettimeofday(&start, NULL);

//         // Send a packet
//         if (send(sockfd, buffer, BUFFER_SIZE, 0) < 0) {
//             perror("send failed");
//             close(sockfd);
//             return 1;
//         }

//         // Wait for ACK
//         if (recv(sockfd, ack, BUFFER_SIZE, 0) <= 0) {
//             perror("recv failed");
//             close(sockfd);
//             return 1;
//         }

//         // Record end time
//         gettimeofday(&end, NULL);

//         // Calculate RTT
//         double rtt = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
//         printf("RTT for round %d: %.6f seconds\n", i + 1, rtt);

//         total_rtt += rtt;
//     }

//     // Calculate average RTT
//     double avg_rtt = total_rtt / TEST_ROUNDS;
//     printf("Average RTT: %.6f seconds\n", avg_rtt);

//     // Estimate optimal window size (assuming 100 Mbps bandwidth)
//     double bandwidth = 100 * 1e6; // 100 Mbps in bits per second
//     double optimal_window = bandwidth * avg_rtt / 8; // Convert bits to bytes
//     printf("Optimal window size: %.2f bytes\n", optimal_window);

//     close(sockfd);
//     return 0;
// }
