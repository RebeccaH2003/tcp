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


// double stop_and_wait(int sockfd) {
//     char buffer[BUFFER_SIZE];
//     memset(buffer, 'A', BUFFER_SIZE); // Fill buffer with dummy data
//     char ack[BUFFER_SIZE];
//     double total_rtt = 0.0;
//     for (int i = 0; i < TIME_INTERVAL; i++) {  // Correct iteration to TEST_ROUNDS
//         struct timeval start, end;

//         // Record start time
//         gettimeofday(&start, NULL);

//         // Send a packet
//         if (send(sockfd, buffer, BUFFER_SIZE, 0) < 0) {
//             perror("send failed");
//             close(sockfd);
//             return -1;  // Return error indicator
//         }

//         // Wait for ACK
//         if (recv(sockfd, ack, BUFFER_SIZE, 0) <= 0) {
//             perror("recv failed");
//             close(sockfd);
//             return -1;  // Return error indicator
//         }

//         // Record end time
//         gettimeofday(&end, NULL);

//         // Calculate RTT
//         double rtt = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
//         printf("RTT for round %d: %.6f seconds\n", i + 1, rtt);

//         total_rtt += rtt;
//     }
//     return total_rtt;
// }

double stop_and_wait(int sockfd) {
    char buffer[PKT_SIZE];
    memset(buffer, 'A', PKT_SIZE); // Fill buffer with dummy data
    char ack[BUFFER_SIZE];
    double total_rtt = 0.0;

    for (int i = 0; i < TEST_ROUNDS; i++) {
        struct timeval start, end;
        int retries = 0;
        int max_retries = 5; // Maximum number of retransmissions

        while (retries < max_retries) {
            // Record start time
            gettimeofday(&start, NULL);

            // Send a packet
            if (send(sockfd, buffer, PKT_SIZE, 0) < 0) {
                perror("send failed");
                close(sockfd);
                return -1;
            }

            // Wait for ACK
            int recv_status = recv(sockfd, ack, BUFFER_SIZE, 0);
            if (recv_status > 0) {
                // ACK received successfully
                gettimeofday(&end, NULL);
                double rtt = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
                printf("RTT for round %d: %.6f seconds\n", i + 1, rtt);

                total_rtt += rtt;
                break; // Exit retry loop
            } else if (recv_status == 0) {
                // Connection closed by the server
                printf("Server closed the connection.\n");
                close(sockfd);
                return -1;
            } else {
                // Timeout or error
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    printf("Timeout: Retransmitting packet for round %d...\n", i + 1);
                    retries++;
                } else {
                    perror("recv failed");
                    close(sockfd);
                    return -1;
                }
            }
        }

        if (retries == max_retries) {
            printf("Max retries reached for round %d. Exiting.\n", i + 1);
            close(sockfd);
            return -1;
        }
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
        else if (strstr(argv[i], "--timeout=") != NULL || strstr(argv[i], "-t=") != NULL) {
            char *value = strchr(argv[i], '=');
            if (value) TIMEOUT = strtol(value + 1, NULL, 10);
            continue;
        }
        else if (strstr(argv[i], "--packe_size=") != NULL || strstr(argv[i], "-ps=") != NULL) {
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
    timeout.tv_sec = TIMEOUT;  // 1 second timeout
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
        total_rtt = stop_and_wait(sockfd);
        if (total_rtt < 0) {
            printf("Error during stop-and-wait.\n");
            close(sockfd);
            return 1;
        }
    }
    else if(MODE == 2) {

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
