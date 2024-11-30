#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/queue.h>

#include "../libs/cJSON.h"

#define BUFFER_SIZE 65536 // Maximum buffer size for the receiving window
#define MAX_PENDING_CONNECTIONS 5

// Structure to store buffered sequence numbers
typedef struct buffered_packet {
    int seq_num; // Sequence number of the packet
    TAILQ_ENTRY(buffered_packet) entries; // Linked list pointers
} buffered_packet_t;

// Define the queue type for buffered packets
TAILQ_HEAD(buffered_queue, buffered_packet);

typedef struct {
    int expected_seq;                  // The next expected sequence number
    int recv_window;                   // Available receiving window
    struct buffered_queue buffer;      // Queue to hold out-of-order packets
} ReceiverState;

void process_client(int client_socket) {
    char recv_buffer[BUFFER_SIZE]; // Large enough to hold any reasonable packet
    ReceiverState state = {0};
    state.expected_seq = 1; // Starting sequence number
    state.recv_window = BUFFER_SIZE;
    TAILQ_INIT(&state.buffer); // Initialize the buffer queue

    while (1) {
        int bytes_received = recv(client_socket, recv_buffer, sizeof(recv_buffer), 0);
        if (bytes_received <= 0) {
            printf("Client disconnected or error occurred.\n");
            break;
        }

        cJSON *recv_json = cJSON_Parse(recv_buffer);
        if (!recv_json) {
            printf("Invalid JSON received.\n");
            continue;
        }
        cJSON *seq_item = cJSON_GetObjectItemCaseSensitive(recv_json, "seq_num");
        if (!seq_item || !cJSON_IsNumber(seq_item)) {
            printf("Missing or invalid sequence number in JSON.\n");
            cJSON_Delete(recv_json);
            continue;
        }
        int recv_seq = seq_item->valueint;

        if (recv_seq == state.expected_seq) {
            // Expected sequence received
            printf("Received expected sequence number: %d\n", recv_seq);
            state.expected_seq++;

            // Process buffered packets in sequence
            buffered_packet_t *entry, *temp;
            TAILQ_FOREACH_SAFE(entry, &state.buffer, entries, temp) {
                if (entry->seq_num == state.expected_seq) {
                    printf("Processing buffered packet: %d\n", entry->seq_num);
                    state.expected_seq++;
                    state.recv_window += bytes_received;
                    TAILQ_REMOVE(&state.buffer, entry, entries);
                    free(entry);
                }
            }

            // Send cumulative ACK
            cJSON *ack_json = cJSON_CreateObject();
            cJSON_AddNumberToObject(ack_json, "ack_num", state.expected_seq);
            const char *ack_string = cJSON_PrintUnformatted(ack_json);
            send(client_socket, ack_string, strlen(ack_string), 0);
            free((void *)ack_string);
            cJSON_Delete(ack_json);
        }
        else if (recv_seq > state.expected_seq) {
            printf("Out-of-order packet received: %d. Expected: %d\n", recv_seq, state.expected_seq);

            // Check if the packet is already buffered
            int already_buffered = 0;
            buffered_packet_t *entry;
            TAILQ_FOREACH(entry, &state.buffer, entries) {
                if (entry->seq_num == recv_seq) {
                    already_buffered = 1;
                    break;
                }
            }

            // Buffer the packet if it isn't already buffered
            if (!already_buffered && state.recv_window - bytes_received >= 0) {
                buffered_packet_t *new_packet = malloc(sizeof(buffered_packet_t));
                if (!new_packet) {
                    perror("Failed to allocate memory for buffered packet");
                    cJSON_Delete(recv_json);
                    continue;
                }
                new_packet->seq_num = recv_seq;
                TAILQ_INSERT_TAIL(&state.buffer, new_packet, entries);
                state.recv_window -= bytes_received;

                printf("Buffered packet: %d\n", recv_seq);

                // Send duplicate ACK for the last in-order sequence number
                cJSON *dup_ack_json = cJSON_CreateObject();
                cJSON_AddNumberToObject(dup_ack_json, "ack_num", state.expected_seq);
                const char *dup_ack_string = cJSON_PrintUnformatted(dup_ack_json);
                send(client_socket, dup_ack_string, strlen(dup_ack_string), 0);
                free((void *)dup_ack_string);
                cJSON_Delete(dup_ack_json);
            }
            else if (!already_buffered) {
                printf("Buffer full, cannot buffer packet: %d\n", recv_seq);
            }
        }
        else {
            // Duplicate or stale packet received
            printf("Duplicate or stale packet received: %d. Ignoring.\n", recv_seq);
        }
        cJSON_Delete(recv_json);
    }

    // Free all buffered packets on client disconnect
    buffered_packet_t *entry, *temp;
    TAILQ_FOREACH_SAFE(entry, &state.buffer, entries, temp) {
        TAILQ_REMOVE(&state.buffer, entry, entries);
        free(entry);
    }

    close(client_socket);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number: %d\n", port);
        exit(EXIT_FAILURE);
    }

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_socket, MAX_PENDING_CONNECTIONS) == -1) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", port);

    // Accept and handle one client at a time
    while (1) {
        client_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("Accept failed");
            continue;
        }

        printf("Client connected: %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        process_client(client_socket);
    }
    close(server_socket);
    return 0;
}