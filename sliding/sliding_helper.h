#ifndef SLIDING_HELPER_H
#define SLIDING_HELPER_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/queue.h>

#include "libs/cJSON.h"

// Queue for received packets
typedef struct packet_entry {
    TAILQ_ENTRY(packet_entry) entries;
    // uint8_t *data;      // Pointer to the packet data
    uint32_t length;    // Length of the packet
    uint32_t ack_num;
} packet_entry_t;

// Queue for packets waiting for acknowledgment
typedef struct ack_entry {
    TAILQ_ENTRY(ack_entry) entries;
    uint8_t *data;
    uint32_t seq_num;    // Sequence number of the packet
    uint64_t timeout;    // Timeout value for retransmission
    bool retransmitted;  // Flag for retransmission
    uint32_t length;     // Length of the packet
    uint64_t sent_time; // time the packet is sent
    uint32_t repeated_ack;
} ack_entry_t;

typedef struct send_entry {
    TAILQ_ENTRY(send_entry) entries;
    uint8_t *data;      // Pointer to the packet data
    uint32_t length;    // Length of the packet
    uint32_t seq_num;   // Sequence number of the packet
    uint64_t sent_time; // time the packet is sent
    bool retransmitted;  // Flag for retransmission
} send_entry_t;

// Define the queues
TAILQ_HEAD(packet_queue, packet_entry);
TAILQ_HEAD(ack_queue, ack_entry);
TAILQ_HEAD(send_queue, send_entry);

// Structure for RTT tracking
typedef struct rtt_entry {
    TAILQ_ENTRY(rtt_entry) entries; // Queue linkage
    uint32_t seq_num;               // Sequence number of the packet
    uint64_t rtt;            // Time packet was sent
} rtt_entry_t;

// Structure for Bandwidth tracking
typedef struct bw_entry {
    TAILQ_ENTRY(bw_entry) entries;  // Queue linkage
    long double bandwidth;          // Bandwidth in Kbits/s
} bw_entry_t;

// RTT and Bandwidth queues
TAILQ_HEAD(rtt_queue, rtt_entry);   // RTT Queue
TAILQ_HEAD(bw_queue, bw_entry);    // Bandwidth Queue


// Global variables
extern int tcp_socket;                  // TCP socket
extern pthread_mutex_t queue_lock;      // Mutex for queue operations
extern pthread_cond_t queue_cond;       // Condition variable for queue synchronization
extern struct packet_queue recv_queue;  // Queue for received packets
extern struct ack_queue ack_queue;      // Queue for ACK packets
extern struct send_queue send_queue;    // Queue for packets to send
extern struct rtt_queue rtt_queue;      // Queue for RTT
extern struct bw_queue bw_queue;        // Queue for Bandwidth
extern uint32_t PKT_SIZE;               // Global packet size
extern uint64_t TESTING_PERIOD;
extern uint64_t INPUT_TESTING_PERIOD;
extern uint32_t RWND; // Receiver window size
extern uint64_t TIMEOUT;
extern uint32_t SEQNUM;    // Sequence number global

// Function prototypes
void initialize_tcp_socket(uint16_t src_port);
void initialize_queues_and_locks();
void establish_connection(uint16_t dst_port, uint32_t dst_ip);
void cleanup();
void *packet_receiver(void *arg);
void update_rtt(uint64_t start_time, uint64_t end_time);
void update_bw(long double bandwidth);
void *timeout_handler(void *arg);
void init_send_packets(uint32_t *seq_num, uint32_t num_packets);
uint32_t get_max_ack(uint32_t current_max_ack);
void process_ack_queue(uint32_t max_ack, uint32_t *unack_bytes, uint64_t current_time);
void fast_retransmit(uint32_t max_ack, uint32_t *unack_bytes);
void timeout_retransmit(uint32_t *unack_bytes, uint64_t current_time);
void sliding_window_test(uint32_t window_size_packet);
void optimize_window_size();
void final_result();

#endif // SLIDING_HELPER_H