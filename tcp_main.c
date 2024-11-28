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
uint32_t PKT_SIZE = 1024;
uint64_t TIME_INTERVAL;
int MODE;
uint64_t TIMEOUT;


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
        else if (strstr(argv[i], "--packet_size=") != NULL || strstr(argv[i], "-ps=") != NULL) {
            char *value = strchr(argv[i], '=');
            if (value) PKT_SIZE = strtol(value + 1, NULL, 10);
            continue;
        }
        
    }

    printf("======================Create Raw Socket==================\n");
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return 1;
    }

    printf("======================Setup For TimeOut==================\n");
    // Set timeout for receiving ACKs
    struct timeval timeout;
    timeout.tv_sec = 3;  // 1 second timeout
    timeout.tv_usec = 0; // No additional microseconds
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt failed");
        close(sockfd);
        return 1;
    }

    printf("======================Setup Raw Socket==================\n");
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DST_PORT);
    if (inet_pton(AF_INET, DST_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        close(sockfd);
        return 1;
    }
    printf("======================connect to the server==================\n");
    // Connect to the server with handshake
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connection failed");
        close(sockfd);
        return 1;
    }

    double total_rtt = 0.0;
    if (MODE == 1) {
        printf("======================stop and wait==================\n");
        total_rtt = stop_and_wait(sockfd, PKT_SIZE, TIME_INTERVAL);
        if (total_rtt < 0) {
            printf("Error during stop-and-wait.\n");
            close(sockfd);
            return 1;
        }
         // Calculate average RTT
        double avg_rtt = total_rtt / TIME_INTERVAL;
        printf("Average RTT: %.6f nanoseconds\n", avg_rtt);

        // Convert average RTT to seconds for bandwidth estimation
        double avg_rtt_sec = avg_rtt / 1e9;
        // Estimate optimal window size (assuming 100 Mbps bandwidth)
        double bandwidth = 100 * 1e6; // 100 Mbps in bits per second
        double optimal_window = bandwidth * avg_rtt_sec / 8; // Convert bits to bytes
        printf("Optimal window size: %.2f bytes\n", optimal_window);
    }
    else if(MODE == 2) {
        printf("======================sliding window==================\n");
        // Define parameters for the sliding window test
        uint32_t max_window_size = 20; // Maximum window size (packets)
        uint32_t step_size = 1;        // Increment step for window size
        uint32_t best_window_size = 0; // Store the best window size
        printf("here\n");
        // Run the sliding window function
        best_window_size = sliding_window(sockfd, PKT_SIZE, TIME_INTERVAL, max_window_size, step_size);

        if (best_window_size > 0) {
            printf("Best Window Size: %u packets\n", best_window_size);
        } else {
            printf("Sliding Window Test Failed.\n");
        }
    }
    close(sockfd);
    free(DST_IP);  // Free allocated memory
    return 0;
}
