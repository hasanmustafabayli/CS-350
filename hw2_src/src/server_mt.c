/*******************************************************************************
* Simple FIFO Order Server Implementation
*
* Description:
*     A server implementation designed to process client requests in First In,
*     First Out (FIFO) order. The server binds to the specified port number
*     provided as a parameter upon launch.
*
* Usage:
*     <build directory>/server <port_number>
*
* Parameters:
*     port_number - The port number to bind the server to.
*
* Author:
*     Renato Mancuso
*
* Affiliation:
*     Boston University
*
* Creation Date:
*     September 10, 2023
*
* Notes:
*     Ensure to have proper permissions and available port before running the
*     server. The server relies on a FIFO mechanism to handle requests, thus
*     guaranteeing the order of processing. For debugging or more details, refer
*     to the accompanying documentation and logs.
*
*******************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "common.h"

#define BACKLOG_COUNT 100
#define USAGE_STRING "Missing parameter. Exiting.\nUsage: %s <port_number>\n"

#define STACK_SIZE (4096)


int worker_main(void *arg) {
    (void) arg;
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    double newtime = TSPEC_TO_DOUBLE(now);

    printf("[#WORKER#] %.6f Worker Thread Alive!\n", newtime);

    do {
        get_elapsed_busywait(1, 0);
        clock_gettime(CLOCK_REALTIME, &now);
        newtime = TSPEC_TO_DOUBLE(now);

        printf("[#WORKER#] %.6f Still Alive!\n", newtime);
        sleep(1);
    } while(1);
    return 0;
}

void handle_connection(int conn_socket) 
{
    struct request creq;
    struct timespec receipt_timestamp, completion_timestamp;
    ssize_t bytes_in;

    int rec = clone(&worker_main, malloc(4096) + 4096, (CLONE_THREAD | CLONE_VM | CLONE_SIGHAND | CLONE_FS | CLONE_FILES | CLONE_SYSVSEM), &bytes_in);

    (void)rec;
    do {
        bytes_in = recv(conn_socket, &creq, sizeof(struct request), 0);

        if (bytes_in < 0) {
            ERROR_INFO();
            perror("Error receiving request from client");
            break;
        } else if (bytes_in == 0) {
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &receipt_timestamp);
        get_elapsed_busywait(creq.req_length.tv_sec, creq.req_length.tv_nsec);
        clock_gettime(CLOCK_MONOTONIC, &completion_timestamp);

        struct response c_response;
        c_response.req_id=creq.req_id;

        ssize_t sent_bytes = send(conn_socket, &creq.req_id, sizeof(uint64_t), 0);

        if (sent_bytes < 0) {
            ERROR_INFO();
            perror("Error sending response to client");
            break;
        }

        double sent_timestamp = TSPEC_TO_DOUBLE(creq.req_timestamp);
        double request_length = TSPEC_TO_DOUBLE(creq.req_length);
        double receipt_time = TSPEC_TO_DOUBLE(receipt_timestamp);
        double completion = TSPEC_TO_DOUBLE(completion_timestamp);

        printf("R%" PRIu64 ":%.6f,%.6f,%.6f,%.6f\n",
               creq.req_id, sent_timestamp, request_length, receipt_time, completion);
    } while(1);

    close(conn_socket);
}

int main (int argc, char ** argv) {
    int sockfd, retval, accepted, optval;
    in_port_t socket_port;
    struct sockaddr_in addr, client;
    struct in_addr any_address;
    socklen_t client_len;

    /* Get port to bind our socket to */
    if (argc > 1) {
        socket_port = strtol(argv[1], NULL, 10);
        printf("INFO: setting server port as: %d\n", socket_port);
    } else {
        ERROR_INFO();
        fprintf(stderr, USAGE_STRING, argv[0]);
        return EXIT_FAILURE;
    }

    /* Now onward to create the right type of socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        ERROR_INFO();
        perror("Unable to create socket");
        return EXIT_FAILURE;
    }

    /* Before moving forward, set socket to reuse address */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(optval));

    /* Convert INADDR_ANY into network byte order */
    any_address.s_addr = htonl(INADDR_ANY);

    /* Time to bind the socket to the right port  */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(socket_port);
    addr.sin_addr = any_address;

    /* Attempt to bind the socket with the given parameters */
    retval = bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

    if (retval < 0) {
        ERROR_INFO();
        perror("Unable to bind socket");
        return EXIT_FAILURE;
    }

    /* Let us now proceed to set the server to listen on the selected port */
    retval = listen(sockfd, BACKLOG_COUNT);

    if (retval < 0) {
        ERROR_INFO();
        perror("Unable to listen on socket");
        return EXIT_FAILURE;
    }

    /* Ready to accept connections! */
    printf("INFO: Waiting for incoming connection...\n");
    client_len = sizeof(struct sockaddr_in);
    accepted = accept(sockfd, (struct sockaddr *)&client, &client_len);

    if (accepted == -1) {
        ERROR_INFO();
        perror("Unable to accept connections");
        return EXIT_FAILURE;
    }

    /* Ready to handle the new connection with the client. */
    handle_connection(accepted);

    close(sockfd);
    return EXIT_SUCCESS;

}