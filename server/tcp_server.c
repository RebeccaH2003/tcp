#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_port>\n", argv[0]);
        return 1;
    }

    int server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        perror("socket creation failed");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_sockfd);
        return 1;
    }

    if (listen(server_sockfd, 5) < 0) {
        perror("listen failed");
        close(server_sockfd);
        return 1;
    }

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_len);
    if (client_sockfd < 0) {
        perror("accept failed");
        close(server_sockfd);
        return 1;
    }

    printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    char buffer[BUFFER_SIZE];
    char ack[BUFFER_SIZE] = "ACK";

    while (1) {
        // Receive data
        ssize_t received = recv(client_sockfd, buffer, BUFFER_SIZE, 0);
        if (received <= 0) {
            if (received == 0) {
                printf("Client disconnected.\n");
            } else {
                perror("recv failed");
            }
            break;
        }

        // Send acknowledgment
        if (send(client_sockfd, ack, BUFFER_SIZE, 0) < 0) {
            perror("send ACK failed");
            break;
        }
    }

    close(client_sockfd);
    close(server_sockfd);
    return 0;
}
