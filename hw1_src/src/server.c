#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <inttypes.h>


#include "common.h"

#define BACKLOG_COUNT 100
#define USAGE_STRING \
    "Missing parameter. Exiting.\n" \
    "Usage: %s <port_number>\n"

static void handle_connection(int conn_socket)
{
    struct request client_request;
    struct timespec receipt_timestamp, completion_timestamp;
    ssize_t bytesr;
    double total = 0;
    double diff; 

    while (1) {

        bytesr = recv(conn_socket, &client_request, sizeof(struct request), 0);

        if (bytesr < 0) {
            ERROR_INFO();
            perror("Error receiving request from client");
            break; 
        } else if (bytesr == 0) {
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &receipt_timestamp);


        get_elapsed_busywait(client_request.request_length.tv_sec, client_request.request_length.tv_nsec);


        clock_gettime(CLOCK_MONOTONIC, &completion_timestamp);


        ssize_t bytes_sent = send(conn_socket, &client_request.request_id, sizeof(uint64_t), 0);

        if (bytes_sent < 0) {
            ERROR_INFO();
            perror("Error sending response to client");
            break; 
        }

        double sent_timestamp = TSPEC_TO_DOUBLE(client_request.timestamp);
        double request_length = TSPEC_TO_DOUBLE(client_request.request_length);
        double receipt_time = TSPEC_TO_DOUBLE(receipt_timestamp);
        double completion = TSPEC_TO_DOUBLE(completion_timestamp);

        diff = completion - receipt_time;
        total = total + diff;
        printf("R%" PRIu64 ":%.6f,%.6f,%.6f,%.6f\n",
            client_request.request_id, sent_timestamp, request_length, receipt_time, completion);
    }
    printf("total is: %.6f\n",total);

    close(conn_socket);
}

int main(int argc, char **argv)
{
    int sockfd, retval, accepted, optval;
    in_port_t socket_port;
    struct sockaddr_in addr, client;
    struct in_addr any_address;
    socklen_t client_len;

    if (argc > 1) {
        socket_port = strtol(argv[1], NULL, 10);
        printf("INFO: setting server port as: %d\n", socket_port);
    } else {
        ERROR_INFO();
        fprintf(stderr, USAGE_STRING, argv[0]);
        return EXIT_FAILURE;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        ERROR_INFO();
        perror("Unable to create socket");
        return EXIT_FAILURE;
    }

    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(optval));

    any_address.s_addr = htonl(INADDR_ANY);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(socket_port);
    addr.sin_addr = any_address;

    retval = bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

    if (retval < 0) {
        ERROR_INFO();
        perror("Unable to bind socket");
        return EXIT_FAILURE;
    }

    retval = listen(sockfd, BACKLOG_COUNT);

    if (retval < 0) {
        ERROR_INFO();
        perror("Unable to listen on socket");
        return EXIT_FAILURE;
    }

    printf("INFO: Waiting for incoming connection...\n");
    client_len = sizeof(struct sockaddr_in);
    accepted = accept(sockfd, (struct sockaddr *)&client, &client_len);

    if (accepted == -1) {
        ERROR_INFO();
        perror("Unable to accept connections");
        return EXIT_FAILURE;
    }

    handle_connection(accepted);

    close(sockfd);
    return EXIT_SUCCESS;
}
