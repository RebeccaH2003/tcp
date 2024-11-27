#include "tcp_helper.h"

// double stop_and_wait(int sockfd) {

//     char buffer[BUFFER_SIZE];
//     memset(buffer, 'A', BUFFER_SIZE); // Fill buffer with dummy data
//     char ack[BUFFER_SIZE];
//     double total_rtt = 0.0;
//     for (int i = 0; i < TIME_INTERVAL; i++) {
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
//     return total_rtt;
// }