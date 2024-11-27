#include "tcp_helper.h"

// TCP Parameters
uint16_t SRC_PORT;
uint16_t DST_PORT;
uint32_t DST_IP;
uint32_t SRC_IP;
uint32_t PKT_SIZE;
uint64_t TIME_INTERVAL;
int MODE;


double stop_and_wait(int sockfd) {

    char buffer[BUFFER_SIZE];
    memset(buffer, 'A', BUFFER_SIZE); // Fill buffer with dummy data
    char ack[BUFFER_SIZE];
    double total_rtt = 0.0;
    for (int i = 0; i < TIME_INTERVAL; i++) {
        struct timeval start, end;

        // Record start time
        gettimeofday(&start, NULL);

        // Send a packet
        if (send(sockfd, buffer, BUFFER_SIZE, 0) < 0) {
            perror("send failed");
            close(sockfd);
            return 1;
        }

        // Wait for ACK
        if (recv(sockfd, ack, BUFFER_SIZE, 0) <= 0) {
            perror("recv failed");
            close(sockfd);
            return 1;
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
    // clean ip table
    system ("iptables -P OUTPUT ACCEPT");
    system ("iptables -F OUTPUT");
    system ("iptables -A OUTPUT -p tcp --tcp-flags RST RST -j DROP");
    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strstr(argv[i], "--mode=") != NULL || strstr(argv[i], "-m=") != NULL) {
            MODE = strtol(strchr(argv[i], '=') + 1, NULL, 10);
            continue;
        } else if (strstr(argv[i], "--dst_port=") != NULL || strstr(argv[i], "-sp=") != NULL) {
            DST_PORT = strtol(strchr(argv[i], '=') + 1, NULL, 10);
            continue;
        } else if (strstr(argv[i], "--dest_ip=") != NULL || strstr(argv[i], "-dp=") != NULL) {
            DST_IP = strtol(strchr(argv[i], '=') + 1, NULL, 10);
            continue;
        } else if (strstr(argv[i], "--testing_interval=") != NULL || strstr(argv[i], "-i=") != NULL) {
            TIME_INTERVAL = strtol(strchr(argv[i], '=') + 1, NULL, 10);
            continue;
        }
        else if (strstr(argv[i], "--src_port=") != NULL || strstr(argv[i], "-sp=") != NULL) {
            SRC_PORT = strtol(strchr(argv[i], '=') + 1, NULL, 10);
            continue;
        }
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(DST_PORT));
    if (inet_pton(AF_INET, DST_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        close(sockfd);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connection failed");
        close(sockfd);
        return 1;
    }

    double total_rtt = 0.0;
    if(MODE == 1) {
        total_rtt = stop_and_wait(sockfd);
    }

    // Calculate average RTT
    double avg_rtt = total_rtt / TEST_ROUNDS;
    printf("Average RTT: %.6f seconds\n", avg_rtt);

    // Estimate optimal window size (assuming 100 Mbps bandwidth)
    double bandwidth = 100 * 1e6; // 100 Mbps in bits per second
    double optimal_window = bandwidth * avg_rtt / 8; // Convert bits to bytes
    printf("Optimal window size: %.2f bytes\n", optimal_window);

    close(sockfd);
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
