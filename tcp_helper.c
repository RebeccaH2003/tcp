#include "tcp_helper.h"


void stop_and_wait(int sockfd, uint32_t packet_size, uint64_t duration) {
    char buffer[packet_size];
    memset(buffer, 'A', packet_size); // Fill buffer with dummy data
    uint64_t total_rtt_ns = 0; // Use nanoseconds for total RTT
    uint32_t sequence_number = 1; // Start sequence numbers from 1
    int num_of_packet = 0;

    // Get start time in microseconds
    struct timeval start, end, curr;
    gettimeofday(&start, NULL);  // Get current time
    uint64_t start_time_in_microseconds = (uint64_t)start.tv_sec * 1000000 + start.tv_usec;
    uint64_t duration_in_microseconds = duration * 1000000;
    uint64_t test_end_time = start_time_in_microseconds + duration_in_microseconds;

    printf("Starting time: %ld seconds, %ld microseconds\n", start.tv_sec, start.tv_usec);
    printf("End time (target): %lu microseconds\n", test_end_time);
    printf("Current time in seconds (time(NULL)): %lu\n", time(NULL));

    uint64_t current_time = start_time_in_microseconds;

    while (current_time < test_end_time) {
        printf("Inside the interval loop...\n");
        int retries = 0;
        int max_retries = 5; // Maximum number of retransmissions
        
        gettimeofday(&start, NULL);  // Get current time

        while (retries < max_retries) {
            printf("Attempt number: %d\n", retries + 1);
            printf("Expecting sequence number: %u \n", sequence_number);
            
            // Add sequence number to the packet
            memcpy(buffer, &sequence_number, sizeof(sequence_number));

            // Send the packet
            printf("Sending the packet...\n");
            if (send(sockfd, buffer, packet_size, 0) < 0) {
                perror("send failed");
                close(sockfd);
                exit(EXIT_FAILURE);
            }

            // Wait for ACK
            uint32_t ack_seq_num;
            ssize_t recv_status = recv(sockfd, &ack_seq_num, sizeof(ack_seq_num), 0);

            if (recv_status > 0) {
                // Convert ACK sequence number from network byte order
                ack_seq_num = ntohl(ack_seq_num);
                printf("Received ACK, ack_seq_num: %u \n", ack_seq_num);

                if (ack_seq_num == sequence_number) {
                    printf("Received successfully!\n");

                    gettimeofday(&end, NULL);  // Get current time

                    // Calculate RTT in microseconds
                    uint64_t rtt_ns = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
                    printf("RTT: %lu microseconds\n", rtt_ns);
                    total_rtt_ns += rtt_ns;
                    num_of_packet++;
                    break; // Exit retry loop
                } else {
                    printf("Sequence mismatch: Expected %u, got %u. Retransmitting...\n", sequence_number, ack_seq_num);
                    retries++;
                }
            } else if (recv_status == 0) {
                // Connection closed by the server
                printf("Server closed the connection.\n");
                close(sockfd);
                exit(EXIT_FAILURE);
            } else {
                // Timeout or error
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    printf("Timeout: Retransmitting packet for packet %u...\n", sequence_number);
                    retries++;
                } else {
                    perror("recv failed");
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
            }
        }

        if (retries == max_retries) {
            printf("Max retries reached for packet %u. Exiting.\n", sequence_number);
            close(sockfd);
            return; // Exit after max retries
        }

        sequence_number++; // Increment sequence number for the next packet
        gettimeofday(&curr, NULL);
        current_time = curr.tv_sec * 1000000 + curr.tv_usec;
    }

    // Print total RTT
    printf("Total RTT: %.6f seconds\n", (double)total_rtt_ns / 1000000);
    
    // Calculate and print average RTT
    if (num_of_packet > 0) {
        double avg_rtt = (double)total_rtt_ns / num_of_packet;
        printf("Average RTT: %.6f microseconds\n", avg_rtt);
    } else {
        printf("No packets received successfully.\n");
    }
}


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

    printf("\nBest Window Size: %u packets (%u bytes) with Bandwidth: %.2Lf Mbps\n", best_window, best_window * pkt_size, best_bandwidth);

    free(packet);
    return best_window; // Return the best window size
}