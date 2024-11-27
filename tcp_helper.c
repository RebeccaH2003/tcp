#include "tcp_helper.h"

uint32_t sliding_window(int sockfd, uint32_t pkt_size, int duration, uint32_t max_window_size, uint32_t step_size) {
    char *packet = malloc(pkt_size);
    printf("server exit here1\n");
    if (!packet) {
        perror("Failed to allocate packet memory");
        return 0; // Return 0 to indicate failure
    }
    memset(packet, 'A', pkt_size); // Fill packet with dummy data

    printf("server exit here2\n");

    uint32_t sequence_number = 1; // Sequence number starts at 1
    uint32_t current_window_size = 1; // Start with a small window size
    long double best_bandwidth = 0.0; // To track the best bandwidth
    uint32_t best_window = 1;         // To track the best window size

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    uint64_t test_end_time = start_time.tv_sec + duration;

    printf("Starting Sliding Window Bandwidth Test...\n");

    for (uint32_t current_window_size = 1; current_window_size <= max_window_size; current_window_size++) {
        uint32_t packets_sent = 0;
        uint64_t bytes_sent = 0;
        uint32_t acked_packets = 0;
        uint32_t packets_in_flight = 0;
        

        printf("\nTesting with Window Size: %u packets (%u bytes)\n", current_window_size, current_window_size * pkt_size);

        // Run the sliding window test for the current window size
        while (time(NULL) < test_end_time) {
            
            // Send packets in the current window
            while (packets_in_flight < current_window_size) {
                uint32_t seq_num_net_order = htonl(sequence_number);
                memcpy(packet, &seq_num_net_order, sizeof(seq_num_net_order));

                ssize_t sent = send(sockfd, packet, pkt_size, 0);
                if (sent < 0) {
                    perror("Failed to send packet");
                    continue;
                }

                packets_in_flight++;
                packets_sent++;
                bytes_sent += sent;
                sequence_number++;
            }

            // Wait for ACKs
            printf("waiting for acks\n");
            while (acked_packets < packets_sent) {
                uint32_t ack_seq_num;
                ssize_t recv_status = recv(sockfd, &ack_seq_num, sizeof(ack_seq_num), 0);

                if (recv_status > 0) {
                    ack_seq_num = ntohl(ack_seq_num);
                    if (ack_seq_num >= sequence_number - packets_in_flight && ack_seq_num < sequence_number) {
                        acked_packets++;
                        packets_in_flight--;
                    }
                } else if (recv_status < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        printf("Timeout occurred, assuming packet loss\n");
                        packets_in_flight=0;
                        break;
                    } else {
                        perror("recv failed");
                        free(packet);
                        return 0; // Return 0 to indicate failure
                    }
                }
            }
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            double elapsed_time = (end_time.tv_sec - start_time.tv_sec) +
                                (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

            // Calculate average bandwidth for this window size
            double bandwidth = (bytes_sent * 8.0) / (elapsed_time * 1024 * 1024); // Mbps
            printf("Average Bandwidth: %.2f Mbps for Window Size: %u packets\n", bandwidth, current_window_size);

            // Check if this is the best bandwidth so far
            if (bandwidth > best_bandwidth) {
                best_bandwidth = bandwidth;
                best_window = current_window_size;
            } else {
                // Stop testing further if bandwidth decreases
                printf("Bandwidth decreased. Stopping test.\n");
                break;
            }


        }

    }

    printf("\nBest Window Size: %u packets (%u bytes) with Bandwidth: %.2f Mbps\n", best_window, best_window * pkt_size, best_bandwidth);

    free(packet);
    return best_window; // Return the best window size
}