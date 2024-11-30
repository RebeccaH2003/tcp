#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdbool.h>
#include <getopt.h>

#include "sliding_helper.h"

// Global variables
char *VARIANT = 'SAW';
uint16_t SRC_PORT = 5432;      // Default source port
uint16_t DST_PORT = 0;         // Destination port
uint32_t DST_IP = INADDR_NONE; // Destination IP
uint32_t PKT_SIZE = 1460;      // Default packet size
uint64_t INPUT_TESTING_PERIOD = 5;  // Default testing period in seconds
uint64_t TESTING_PERIOD;

int
main (int argc, char **argv){
    /********************************* Parsing Arguments ***************************************/
    int opt;
    int option_index = 0;

    // Struct for long options
    static struct option long_options[] = {
        {"packet-size", required_argument, 0, 'z'},
        {"duration", required_argument, 0, 't'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "s:d:p:z:t:v:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 's': // Source port
                SRC_PORT = atoi(optarg);
                break;
            case 'd': // Destination IP
                if (inet_pton(AF_INET, optarg, &DST_IP) != 1) {
                    fprintf(stderr, "Invalid destination IP address format: %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'p': // Destination port
                DST_PORT = atoi(optarg);
                break;
            case 'z': // --packet-size
                PKT_SIZE = atoi(optarg);
                break;
            case 't': // --duration
                INPUT_TESTING_PERIOD = atoi(optarg);
                break;
            case 'v': // --variant
                VARIANT = strdup(optarg); // Store the variant
                break;
            default:
                fprintf(stderr, "Usage: %s -d <DST_IP> -p <DST_PORT> [-s <SRC_PORT>] [--packet-size <SIZE>] [--duration <SECONDS>]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (DST_PORT == 0 || DST_IP == INADDR_NONE) {
        fprintf(stderr, "Error: Missing required arguments.\n");
        fprintf(stderr, "Usage: %s -d <DST_IP> -p <DST_PORT> [-s <SRC_PORT>] [--packet-size <SIZE>] [--duration <SECONDS>] [--variant <SAW/SWF>]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Validate variant value
    if (strcmp(VARIANT, "SWF") != 0 && strcmp(VARIANT, "SAW") != 0) {
        fprintf(stderr, "Invalid variant. Use 'SAW' for Stop-and-Wait or 'SWF' for Sliding Window Fixed Size.\n");
        exit(EXIT_FAILURE);
    }

    /************************************************************************************************/

    // Convert TESTING_PERIOD to nanoseconds
    INPUT_TESTING_PERIOD *= 1000000000ULL;

    initialize_tcp_socket(SRC_PORT);
    initialize_queues_and_locks();
    establish_connection(DST_PORT, DST_IP);
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, packet_receiver, NULL) != 0) {
        perror("Failed to create receiving thread");
        cleanup();
        return EXIT_FAILURE;
    }
    printf("Receiving thread started.\n");

    // Create timeout thread
    pthread_t timeout_thread;
    if (pthread_create(&timeout_thread, NULL, timeout_handler, NULL) != 0) {
        perror("Failed to create timeout thread");
        cleanup();
        return EXIT_FAILURE;
    }
    printf("Timeout thread started.\n");

    if (strcmp(VARIANT, "SAW") == 0) {
        printf("\n***** Running Variant Stop-and-Wait...\n");
        stop_and_wait();
    }
    if (strcmp(VARIANT, "SWF") == 0) {
        printf("\n***** Running Variant Fixed Size Sliding Window...\n");
        optimize_window_size();

    }

    final_result ();
    // Wait for threads to finish
    pthread_join(recv_thread, NULL);
    pthread_join(timeout_thread, NULL);

    // Cleanup
    cleanup();
    return 0;

}