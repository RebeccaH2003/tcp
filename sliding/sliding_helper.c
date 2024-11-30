#include "sliding_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

// Global variables
int tcp_socket;                      // TCP socket
pthread_mutex_t queue_lock;          // Mutex for queue operations
pthread_cond_t queue_cond;           // Condition variable for queue synchronization
pthread_mutex_t byte_count_lock;          // Mutex for queue operations
pthread_cond_t byte_count_cond;           // Condition variable for queue synchronization
struct packet_queue recv_queue;      // Queue for received packets
struct ack_queue ack_queue;          // Queue for ACK packets
struct send_queue send_queue;        // Queue for packets to send
struct rtt_queue rtt_queue;   // Queue for RTT
struct bw_queue bw_queue;     // Queue for bandwidth
uint64_t TOTAL_BYTES_SENT = 0; // Total bytes sent during the test
uint32_t RWND = 65535; // Default 64 KB
uint64_t TIMEOUT = 3e9;   // RTO GLobal
uint32_t SEQNUM;
uint64_t INPUT_TESTING_PERIOD;
uint64_t TESTING_PERIOD;

// Function to initialize the TCP socket
void initialize_tcp_socket(uint16_t src_port) {
    struct sockaddr_in src_addr;

    // Create the socket
    tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0) {
        perror("Failed to create TCP socket");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the source port
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.sin_family = AF_INET;
    src_addr.sin_port = htons(src_port);
    src_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(tcp_socket, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
        perror("Failed to bind TCP socket");
        close(tcp_socket);
        exit(EXIT_FAILURE);
    }
}

// Function to initialize queues and locks
void initialize_queues_and_locks() {
    // Initialize queues
    TAILQ_INIT(&recv_queue);
    TAILQ_INIT(&ack_queue);
    TAILQ_INIT(&send_queue);

    // Initialize RTT and Bandwidth queues
    TAILQ_INIT(&rtt_queue);
    TAILQ_INIT(&bw_queue);

    // Initialize mutex and condition variable
    if (pthread_mutex_init(&queue_lock, NULL) != 0) {
        perror("Failed to initialize queue mutex");
        exit(EXIT_FAILURE);
    }

    if (pthread_cond_init(&queue_cond, NULL) != 0) {
        perror("Failed to initialize queue condition variable");
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&byte_count_lock, NULL) != 0) {
        perror("Failed to initialize byte count mutex");
        exit(EXIT_FAILURE);
    }

    if (pthread_cond_init(&byte_count_cond, NULL) != 0) {
        perror("Failed to initialize byte count condition variable");
        exit(EXIT_FAILURE);
    }
}

void establish_connection(uint16_t dst_port, uint32_t dst_ip) {
    struct sockaddr_in server_addr;

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(dst_port);
    server_addr.sin_addr.s_addr = dst_ip;

    // Connect to the server
    if (connect(tcp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to connect to the server");
        exit(EXIT_FAILURE);
    }
    printf("Connected to the server at %s:%d\n", inet_ntoa(*(struct in_addr *)&dst_ip), dst_port);
}

// Cleanup function
void cleanup() {
    // Close the socket
    close(tcp_socket);

    // Destroy mutex and condition variable
    pthread_mutex_destroy(&queue_lock);
    pthread_cond_destroy(&queue_cond);
}

// Function to get the current time in nanoseconds
uint64_t get_current_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

// Add RTT entry
void update_rtt(uint64_t start_time, uint64_t end_time) {
    pthread_mutex_lock(&queue_lock);

    rtt_entry_t *entry = malloc(sizeof(rtt_entry_t));
    if (entry) {
        entry->rtt = end_time - start_time;
        TAILQ_INSERT_TAIL(&rtt_queue, entry, entries);
    }

    pthread_mutex_unlock(&queue_lock);
}

// Add Bandwidth entry
void update_bw(long double bandwidth) {
    pthread_mutex_lock(&queue_lock);

    bw_entry_t *entry = malloc(sizeof(bw_entry_t));
    if (entry) {
        entry->bandwidth = bandwidth;
        TAILQ_INSERT_TAIL(&bw_queue, entry, entries);
    }

    pthread_mutex_unlock(&queue_lock);
}

void *packet_receiver(void *arg) {
    char buffer[PKT_SIZE];
    ssize_t bytes_received;

    while (1) {
        // Receive data from the socket
        bytes_received = recv(tcp_socket, buffer, PKT_SIZE, 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("Connection closed by server.\n");
            } else {
                perror("Receive failed");
            }
            break; // Exit the thread
        }

        buffer[bytes_received] = '\0'; // Null-terminate the received string

         // Parse the JSON string
        cJSON *json_stats = cJSON_Parse(buffer);
        if (json_stats == NULL) {
            fprintf(stderr, "Failed to parse JSON statistics\n");
            continue; // Skip invalid JSON
        }

        // Extract the acknowledgment number
        cJSON *ack_num_item = cJSON_GetObjectItem(json_stats, "ack_num");
        if (ack_num_item == NULL) {
            fprintf(stderr, "Failed to extract ack_num from JSON\n");
            cJSON_Delete(json_stats);
            continue; // Skip if "ack_num" is missing
        }

        // Lock the queue and add the packet
        pthread_mutex_lock(&queue_lock);

        packet_entry_t *packet = malloc(sizeof(packet_entry_t));
        if (!packet) {
            perror("Failed to allocate memory for packet");
            pthread_mutex_unlock(&queue_lock);
            continue; // Skip this packet
        }

        packet->ack_num = (uint32_t)ack_num_item->valueint;
        TAILQ_INSERT_TAIL(&recv_queue, packet, entries);

        pthread_cond_signal(&queue_cond); // Signal that a new packet has been added
        pthread_mutex_unlock(&queue_lock);

        // Clean up
        cJSON_Delete(json_stats);
        printf("Received ack_num: %u and added to recv_queue.\n", packet->ack_num);
    }

    return NULL;
}

void *timeout_handler(void *arg) {
    uint64_t prev_time = get_current_time_ns();

    pthread_mutex_lock(&byte_count_lock);
    uint32_t prev_total_bytes = TOTAL_BYTES_SENT;
    pthread_mutex_unlock(&byte_count_lock);

    uint64_t current_time;
    ack_entry_t *entry;

    while (1) {
        pthread_mutex_lock(&queue_lock);
        current_time = get_current_time_ns();

        TAILQ_FOREACH(entry, &ack_queue, entries) {
            if (entry->timeout <= current_time) {
                printf("Timeout detected for seq_num: %u\n", entry->seq_num);
                pthread_cond_signal(&queue_cond); // Notify waiting threads
            }
        }

        // Bandwidth calculation every second
        current_time = get_current_time_ns();
        if (current_time - prev_time >= 1e9) {
            pthread_mutex_lock(&byte_count_lock);
            uint32_t bytes_sent = TOTAL_BYTES_SENT - prev_total_bytes; // Calculate bytes sent in the last second
            long double bandwidth = 8.0 * bytes_sent / 1000.0; // Convert to Kbits/s

            printf("Bandwidth: %Lf Kbits/s\n", bandwidth);
            update_bw(bandwidth); // Add to bandwidth queue

            prev_total_bytes = TOTAL_BYTES_SENT; // Update the last byte count
            prev_time = current_time;  // Update the timestamp
            pthread_mutex_unlock(&byte_count_lock);
        }

        pthread_mutex_unlock(&queue_lock);
        // Sleep briefly to reduce CPU usage
        usleep(1000);
    }
    return NULL;
}

void init_send_packets(uint32_t *seq_num, uint32_t num_packets) {
    for (uint32_t i = 0; i < num_packets; i++) {
        // Allocate memory for the send entry
        send_entry_t *send_entry = malloc(sizeof(send_entry_t));
        if (!send_entry) {
            perror("Failed to allocate memory for send_entry");
            continue; // Skip if allocation fails
        }

        // Set up the packet data
        send_entry->seq_num = (*seq_num)++;
        send_entry->length = PKT_SIZE;
        send_entry->retransmitted = false;

        TAILQ_INSERT_TAIL(&send_queue, send_entry, entries);
    }
}

uint32_t get_max_ack(uint32_t current_max_ack) {
    packet_entry_t *entry;
    uint32_t max_ack = current_max_ack;
    TAILQ_FOREACH(entry, &recv_queue, entries) {
        uint32_t ack = entry->ack_num; // Extract ACK number
        if(max_ack < ack){
            max_ack = ack;
        }
    }
    return max_ack;
}

void process_ack_queue(uint32_t max_ack, uint32_t *unack_bytes, uint64_t current_time) {
    ack_entry_t *entry, *temp;
    TAILQ_FOREACH_SAFE(entry, &ack_queue, entries, temp) {
        if (entry->seq_num < max_ack) {
            // Packet is acknowledged
            printf("Acknowledged packet seq_num: %u\n", entry->seq_num);

            // Update unacknowledged bytes
            if (*unack_bytes >= entry->length) {
                *unack_bytes -= entry->length;
            } else {
                *unack_bytes = 0; // Handle edge case
            }

            // Calculate and log RTT
            update_rtt(entry->sent_time, current_time);

            pthread_mutex_lock(&byte_count_lock);
            TOTAL_BYTES_SENT = TOTAL_BYTES_SENT + entry->length;
            pthread_mutex_unlock(&byte_count_lock);

            // Remove the entry from the ack_queue
            TAILQ_REMOVE(&ack_queue, entry, entries);
            free(entry); // Free memory for the entry
        }
    }
}

void fast_retransmit(uint32_t max_ack, uint32_t *unack_bytes) {
    ack_entry_t *ack_entry = NULL;
    packet_entry_t *recv_entry = NULL;

    if (!TAILQ_EMPTY (&ack_queue))
    {
      ack_entry = TAILQ_FIRST (&ack_queue);
    }

    while (!TAILQ_EMPTY (&recv_queue))
    {
      recv_entry = TAILQ_FIRST (&recv_queue);
      uint32_t recv_ack = recv_entry->ack_num;

      if (ack_entry && max_ack == ack_entry->seq_num && recv_ack == max_ack)
        {
          ack_entry->repeated_ack++;
        }

      TAILQ_REMOVE (&recv_queue, recv_entry, entries);
      free (recv_entry);
    }

    if (ack_entry && ack_entry->repeated_ack >= 3)
    {
    //   ack_entry = TAILQ_FIRST (&ack_queue);
    //   TIMEOUT *= 2;
    //   SEQNUM -= ckq_e->len;
      // perror ("FASTRRRRR");
      send_entry_t *send_entry;
      send_entry = (send_entry_t *)calloc (1, sizeof (send_entry_t));
      send_entry->length = ack_entry->length;
      send_entry->seq_num = ack_entry->seq_num;
      send_entry->retransmitted = true;
      TAILQ_INSERT_HEAD (&send_queue, send_entry, entries);

    // Update unacknowledged bytes
    if (*unack_bytes >= ack_entry->length) {
        *unack_bytes -= ack_entry->length;
    } else {
        *unack_bytes = 0; // Handle edge case
    }

      TAILQ_REMOVE (&ack_queue, ack_entry, entries);
      free (ack_entry);
    }
}

void timeout_retransmit(uint32_t *unack_bytes, uint64_t current_time) {
    ack_entry_t *ack_entry = NULL;
    ack_entry_t *temp = NULL;

    TAILQ_FOREACH_SAFE(ack_entry, &ack_queue, entries, temp) {
        if (ack_entry->timeout <= current_time) {
            // Timeout detected for the packet
            printf("Timeout detected for seq_num: %u\n", ack_entry->seq_num);

            // Retransmit the packet
            send_entry_t *send_entry = (send_entry_t *)calloc(1, sizeof(send_entry_t));
            if (!send_entry) {
                perror("Failed to allocate memory for send_entry");
                continue; // Skip this packet
            }
            send_entry->seq_num = ack_entry->seq_num;
            send_entry->length = ack_entry->length;
            send_entry->retransmitted = true;
            // send_entry->sent_time = get_current_time_ns(); // Update send time
            TAILQ_INSERT_HEAD(&send_queue, send_entry, entries);

            // Update unacknowledged bytes
            if (*unack_bytes >= ack_entry->length) {
                *unack_bytes -= ack_entry->length;
            } else {
                *unack_bytes = 0; // Handle edge case
            }

            // Remove the timed-out entry from the ack_queue
            TAILQ_REMOVE(&ack_queue, ack_entry, entries);
            free(ack_entry);
        }
    }
}

void clear_queues() {
    ack_entry_t *ack_entry, *ack_temp;
    TAILQ_FOREACH_SAFE(ack_entry, &ack_queue, entries, ack_temp) {
        TAILQ_REMOVE(&ack_queue, ack_entry, entries);
        free(ack_entry);
    }

    send_entry_t *send_entry, *send_temp;
    TAILQ_FOREACH_SAFE(send_entry, &send_queue, entries, send_temp) {
        TAILQ_REMOVE(&send_queue, send_entry, entries);
        free(send_entry);
    }

    packet_entry_t *recv_entry, *recv_temp;
    TAILQ_FOREACH_SAFE(recv_entry, &recv_queue, entries, recv_temp) {
        TAILQ_REMOVE(&recv_queue, recv_entry, entries);
        free(recv_entry);
    }
}

void sliding_window_test(uint32_t window_size_packet) {
    pthread_mutex_lock(&queue_lock);
    SEQNUM = get_max_ack(0);
    clear_queues();
    pthread_mutex_unlock(&queue_lock);
    uint32_t max_ack = SEQNUM;      // Highest acknowledged sequence number
    uint32_t window_size_bytes = window_size_packet * PKT_SIZE; // Window size in bytes
    uint32_t unack_bytes = 0;     // Bytes sent but not yet acknowledged
    uint32_t seq_num = SEQNUM;

    uint64_t end_time = get_current_time_ns() + TESTING_PERIOD; // End time
    send_entry_t *send_entry = NULL;
    init_send_packets(&seq_num, 2 * (RWND / PKT_SIZE));

    while (get_current_time_ns() <= end_time) {
        pthread_mutex_lock(&queue_lock);
        uint64_t current_time = get_current_time_ns();
        // Wait if CWND is full
        if (unack_bytes + PKT_SIZE > window_size_bytes) {
            pthread_cond_wait(&queue_cond, &queue_lock);
        }

        // Process ACKs
        max_ack = get_max_ack(max_ack); // Update MAX_ACK
        process_ack_queue(max_ack, &unack_bytes, current_time);

        fast_retransmit(max_ack, &unack_bytes);

        timeout_retransmit(&unack_bytes, current_time);

        while (unack_bytes + PKT_SIZE <= window_size_bytes)
        {
          send_entry = TAILQ_FIRST (&send_queue);
          char *packet = malloc(PKT_SIZE);
          if (!packet) {
            perror("Failed to allocate packet memory");
            continue; // Skip this packet
          }
          memset(packet, 0, PKT_SIZE); // Zero out the packet buffer

          cJSON *json_packet = cJSON_CreateObject();
          if (!json_packet) {
              perror("Failed to create JSON packet");
              continue; // Skip this packet
          }
          cJSON_AddNumberToObject(json_packet, "seq_num", send_entry->seq_num);
          char *json_string = cJSON_PrintUnformatted(json_packet);
          if (!json_string) {
              perror("Failed to serialize JSON packet");
              cJSON_Delete(json_packet);
              free(packet);
              continue; // Skip this packet
          }

          size_t json_length = strlen(json_string);
          memcpy(packet, json_string, json_length);
          ssize_t sent = send(tcp_socket, packet, PKT_SIZE, 0);
          if (sent < 0) {
            perror("Failed to send packet");
            free(json_string);
            cJSON_Delete(json_packet);
            free(packet);
            continue; // Skip this packet but keep it in the send queue
          }

          printf("Sent packet seq_num: %u, size: %u bytes\n", send_entry->seq_num, PKT_SIZE);
          send_entry->sent_time = get_current_time_ns(); // Update sent time
          unack_bytes += PKT_SIZE;

          // Move the entry to the ACK queue for tracking
          ack_entry_t *ack_entry = (ack_entry_t *)calloc(1, sizeof(ack_entry_t));
          if (!ack_entry) {
            perror("Failed to allocate memory for ack_entry");
            free(json_string);
            cJSON_Delete(json_packet);
            free(packet);
            continue; // Skip adding this entry to ack_queue
           }
           ack_entry->seq_num = send_entry->seq_num;
           ack_entry->length = PKT_SIZE;
           ack_entry->timeout = send_entry->sent_time + TIMEOUT;
           ack_entry->retransmitted = send_entry->retransmitted;
           TAILQ_INSERT_TAIL(&ack_queue, ack_entry, entries);

           TAILQ_REMOVE(&send_queue, send_entry, entries);
           free(send_entry);

           // Clean up
            free(json_string);
            cJSON_Delete(json_packet);
            free(packet);
        }
        pthread_mutex_unlock(&queue_lock);
    }
    
}

void optimize_window_size() {
    printf("\n***** FIXED WINDOW FINDING OPTIMAL WND\n");
    printf("\n***** STOP WHEN BANDWIDTH NO LONGER INCREASES\n");
    long double best_band = 0;
    int best_band_size = 1;
    SEQNUM = 1;

    /* Runs the sliding window algorithm until we see a decrease in performance */
    for (int i = 1; i < RWND / PKT_SIZE; i += 5) {
        printf("\n***** INCREASING WND TO: %u bytes\n", i * PKT_SIZE);
        TESTING_PERIOD = 3e9; // 3 seconds in nanoseconds
        sliding_window_test(i);

        // Calculate bandwidth and clean up bw_queue
        bw_entry_t *bw_entry = NULL;
        long double bw = 0;
        int bandwidth_count = 0;

        pthread_mutex_lock(&queue_lock);

        while (!TAILQ_EMPTY(&bw_queue)) {
            bw_entry = TAILQ_FIRST(&bw_queue);
            bw += bw_entry->bandwidth;
            bandwidth_count++;
            TAILQ_REMOVE(&bw_queue, bw_entry, entries);
            free(bw_entry);
        }

        // Clean up RTT queue
        rtt_entry_t *rtt_entry = NULL;
        while (!TAILQ_EMPTY(&rtt_queue)) {
            rtt_entry = TAILQ_FIRST(&rtt_queue);
            TAILQ_REMOVE(&rtt_queue, rtt_entry, entries);
            free(rtt_entry);
        }
        pthread_mutex_unlock(&queue_lock);

        // Calculate average bandwidth
        if (bandwidth_count > 0) {
            bw /= bandwidth_count;
        } else {
            printf("No bandwidth records available.\n");
            break;
        }

        printf("\n***** BW vs BEST Bandwidth: %Lf, %Lf \n", bw, best_band);

        // Stop if performance drops
        if (best_band != 0 && best_band > bw) {
            printf("Performance dropped. Stopping further tests.\n");
            break;
        }

        // Update best_band and best_band_size if performance improves
        if (bw > best_band) {
            best_band = bw;
            best_band_size = i;
        }
    }
    printf("\n***** Best Bandwidth: %Lf\n", best_band);
    printf("\n***** RUNNING FINAL TEST WITH WINDOW SIZE: %u packets\n", best_band_size);

    // Run the final test with the optimal window size
    TESTING_PERIOD = INPUT_TESTING_PERIOD; // 5 seconds in nanoseconds
    sliding_window_test(best_band_size);
}

void final_result(){
    printf("\n***** TEST RESULTS *****\n");
    FILE *fp;

    // Open RTT file for writing
    fp = fopen("tcprtt.txt", "w");
    if (!fp) {
        perror("Failed to open tcprtt.txt for writing");
        return;
    }

    // Calculate RTT stats and write entries to file for graphing
    rtt_entry_t *rtt_entry = NULL;
    int rtt_count = 1;
    long double rtt_avg = 0;
    long double rtt_max = 0;
    long double rtt_min = 1e12; // Set high for finding the minimum

    pthread_mutex_lock(&queue_lock);

    TAILQ_FOREACH(rtt_entry, &rtt_queue, entries) {
        long double rtt_ms = (long double)rtt_entry->rtt / 1e6; // Convert nanoseconds to milliseconds
        rtt_avg += rtt_ms;
        rtt_max = rtt_max > rtt_ms ? rtt_max : rtt_ms;
        rtt_min = rtt_min < rtt_ms ? rtt_min : rtt_ms;

        // Write RTT data to the file
        fprintf(fp, "%d %Lf\n", rtt_count, rtt_ms);
        rtt_count++;
    }

    rtt_avg = (rtt_count > 1) ? rtt_avg / (rtt_count - 1) : 0; // Avoid division by zero

    pthread_mutex_unlock(&queue_lock);
    fclose(fp); // Close the RTT file

    // Calculate bandwidth stats
    bw_entry_t *bw_entry = NULL;
    long double bw_avg = 0;
    int bw_count = 0;

    pthread_mutex_lock(&queue_lock);

    TAILQ_FOREACH(bw_entry, &bw_queue, entries) {
        bw_avg += bw_entry->bandwidth; // Bandwidth is already in Kbits/s
        bw_count++;
    }

    bw_avg = (bw_count > 0) ? bw_avg / bw_count : 0; // Avoid division by zero

    pthread_mutex_unlock(&queue_lock);

    // Print results
    printf("***** \n");
    printf("***** Average RTT: %Lf ms\n", rtt_avg);
    printf("***** Maximum RTT: %Lf ms\n", rtt_max);
    printf("***** Minimum RTT: %Lf ms\n", rtt_min);
    printf("***** Average Bandwidth: %Lf Kbits/s\n", bw_avg);
    printf("***** Sliding Window Size Best Estimate Based on Bandwidth: %Lf bytes\n",
           (rtt_avg / 1000) * (bw_avg * 1000) / 8); // Formula for optimal window size
    printf("***** \n");
    printf("*******************************\n");
}




