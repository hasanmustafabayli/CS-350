/*******************************************************************************
* Multi-Threaded FIFO Server Implementation w/ Queue Limit
*
* Description:
*     A server implementation designed to process client requests in First In,
*     First Out (FIFO) order. The server binds to the specified port number
*     provided as a parameter upon launch. It launches multiple threads to
*     process incoming requests and allows to specify a maximum queue size.
*
* Usage:
*     <build directory>/server -q <queue_size> -w <workers> <port_number>
*
* Parameters:
*     port_number - The port number to bind the server to.
*     queue_size  - The maximum number of queued requests
*     workers     - The number of workers to start to process requests
*
* Author:
*     Renato Mancuso
*
* Affiliation:
*     Boston University
*
* Creation Date:
*     September 29, 2023
*
* Notes:
*     Ensure to have proper permissions and available port before running the
*     server. The server relies on a FIFO mechanism to handle requests, thus
*     guaranteeing the order of processing. If the queue is full at the time a
*     new request is received, the request is rejected with a negative ack.
*     sudo docker run -it --rm -p 5001:5001 -v .:/workspace c-assignment-env
*
*******************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>

/* Needed for wait(...) */
#include <sys/types.h>
#include <sys/wait.h>

/* Needed for semaphores */
#include <semaphore.h>

/* Include struct definitions and other libraries that need to be
 * included by both client and server */
#include "common.h"

#define BACKLOG_COUNT 100
#define USAGE_STRING                \
    "Missing parameter. Exiting.\n"     \
    "Usage: %s -q <queue size> <port_number>\n"

/* 4KB of stack for the worker thread */
#define STACK_SIZE (4096)

/* START - Variables needed to protect the shared queue. DO NOT TOUCH */
sem_t * queue_mutex;
sem_t * queue_notify;
/* END - Variables needed to protect the shared queue. DO NOT TOUCH */

struct request_meta {
    struct request request;

    /* ADD REQUIRED FIELDS */
};

struct queue {
    /* ADD REQUIRED FIELDS */
    struct request_meta* requests; 
    int head; 
    int end;  
    int size; 
    int limit; 
};

struct connection_params {
    /* ADD REQUIRED FIELDS */
    int queue_size; 
    int num_servers;
};

struct worker_params {
    /* ADD REQUIRED FIELDS */
    int worker_done;
    struct queue* the_queue;
    int conn_socket; 
    struct timespec receipt_timestamp;
    uint64_t ID;
    
};

/* Helper function to perform queue initialization */
void queue_init(struct queue * the_queue, size_t queue_size)
{
    /* IMPLEMENT ME !! */
    the_queue->requests = (struct request_meta*)malloc(queue_size * sizeof(struct request_meta)); 
    the_queue->limit=queue_size; 
    the_queue->head = 0;
    the_queue->end = -1; 
    the_queue->size = 0; 
}

/* Add a new request <request> to the shared queue <the_queue> */
int add_to_queue(struct request_meta to_add, struct queue * the_queue)
{
    int retval = 0;
    sem_wait(queue_mutex);
    if (the_queue->size >= the_queue->limit) {
        /* What to do in case of a full queue */
        retval=-1;
        /* DO NOT RETURN DIRECTLY HERE */
    } else {
        /* If all good, add the item in the queue */
        /* IMPLEMENT ME !!*/
        the_queue->end = (the_queue->end + 1) % the_queue->limit;
        the_queue->requests[the_queue->end] = to_add; //added to the end of the queue 
        the_queue->size++;
        sem_post(queue_notify);

        /* QUEUE SIGNALING FOR CONSUMER --- DO NOT TOUCH */
    }
    /* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */ 
    sem_post(queue_mutex);
    /* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
    return retval;
}

/* Add a new request <request> to the shared queue <the_queue> */
struct request_meta get_from_queue(struct queue * the_queue)
{
    struct request_meta retval;
    /* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
    sem_wait(queue_notify);
    sem_wait(queue_mutex);
    if (the_queue->size == 0) {
        perror("Error because queue is empty");
    } else {
        retval = the_queue->requests[the_queue->head];  
        the_queue->head = (the_queue->head + 1) % the_queue->limit; 
        the_queue->size--;
    }
    /* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
    sem_post(queue_mutex);
    /* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
    return retval;
}

void dump_queue_status(struct queue * the_queue)
{
    /* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
    sem_wait(queue_mutex);
    /* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

    int i;
    printf("Q:["); 
    for (i = 0; i < the_queue->size; i++) {
        printf("R%lu", the_queue->requests[(the_queue->head + i) % the_queue->limit].request.req_id);
        if (i < the_queue->size - 1) { //print comma after only if it's not the last element 
            printf(","); 
        }
    }
    printf("]\n");

    /* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
    sem_post(queue_mutex);
    /* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
}

/* Main logic of the worker thread */
int worker_main (void * arg)
{
    struct timespec now, start_timestamp, completion_timestamp;
    struct worker_params * params = (struct worker_params *)arg;
    struct response response;
    struct queue* the_queue=params->the_queue;
    int conn_socket=params->conn_socket;
    /* Print the first alive message. */
    clock_gettime(CLOCK_MONOTONIC, &now);
    printf("[#WORKER#] %lf Worker Thread Alive!\n", TSPEC_TO_DOUBLE(now));

    /* Okay, now execute the main logic. */
    while (!params->worker_done) {
        struct request_meta upcoming_request = get_from_queue(the_queue);
        clock_gettime(CLOCK_MONOTONIC, &start_timestamp);
        get_elapsed_busywait(upcoming_request.request.req_length.tv_sec, upcoming_request.request.req_length.tv_nsec);
        response.req_id=upcoming_request.request.req_id;
        response.ack=0;
        size_t bytes_sent = send(conn_socket, &response, sizeof(struct response), 0);
        if (bytes_sent == -1) {
            perror("Error sending response to the client\n");
            break;
        }
        else{
            clock_gettime(CLOCK_MONOTONIC, &completion_timestamp);
        }
        uint64_t id;
        uint64_t thread_id=params->ID;
        id=upcoming_request.request.req_id;
        double receipt_time=TSPEC_TO_DOUBLE(params->receipt_timestamp);
        double sent_timestamp = TSPEC_TO_DOUBLE(upcoming_request.request.req_timestamp);
        double request_length =TSPEC_TO_DOUBLE(upcoming_request.request.req_length);
        double start_time=TSPEC_TO_DOUBLE(start_timestamp);
        double completion_time = TSPEC_TO_DOUBLE(completion_timestamp);
        printf("T%lu R%lu:%.6f,%.6f,%.6f,%.6f,%.6f\n", thread_id, id, sent_timestamp, request_length, receipt_time, start_time, completion_time);
        dump_queue_status(the_queue);
    }
    return EXIT_SUCCESS;
}

/* This function will start the worker thread wrapping around the
 * clone() system call*/
int start_worker(void * params, void * worker_stack)
{
    /* IMPLEMENT ME !! */
    int process = clone(worker_main, worker_stack + STACK_SIZE, CLONE_THREAD | CLONE_VM | CLONE_SIGHAND | CLONE_FS | CLONE_FILES | CLONE_SYSVSEM, params);
    return process; 
}

/* Main function to handle connection with the client. This function
 * takes in input conn_socket and returns only when the connection
 * with the client is interrupted. */
void handle_connection(int conn_socket, struct connection_params conn_params)
{
    struct request_meta * req;
    struct queue * the_queue;
    size_t in_bytes;

    int worker_id, res;
    int worker_done=0;
    struct timespec reject_timestamp; 

    struct response response;

    the_queue=(struct queue *)malloc(sizeof(struct queue));

    if(the_queue==NULL) {
        perror("No memory was allocated for the queue");
        exit(EXIT_FAILURE);
    }
    queue_init(the_queue, conn_params.queue_size);  

    struct worker_params * worker_params = malloc(sizeof(struct worker_params) * conn_params.num_servers);
    void* stacks[conn_params.num_servers]; 

    for (int i = 0; i < conn_params.num_servers; i++) {
        worker_params[i].the_queue = the_queue;
        worker_params[i].conn_socket = conn_socket;
        worker_params[i].ID = i;
        stacks[i] = malloc(STACK_SIZE);
        worker_id = start_worker(&worker_params[i], stacks[i]);
        if(stacks == NULL){
            perror("No memory allocated\n");
            exit(EXIT_FAILURE);
        }
        if (worker_id < 0) {
            perror("No child process\n");
            exit(EXIT_FAILURE);
        }
        printf("INFO: Worker thread started. Thread ID = %d\n", worker_id);
    }

    
    req = (struct request_meta *)malloc(sizeof(struct request_meta));
    do {
        worker_params->worker_done=0;
        in_bytes = recv(conn_socket, req, sizeof(struct request_meta), 0);
        clock_gettime(CLOCK_MONOTONIC, &worker_params->receipt_timestamp);
        if(in_bytes<0) {
            worker_params->worker_done=1;
            break;
        } 
        res=add_to_queue(*req, the_queue);
        if(res==-1) { 
            clock_gettime(CLOCK_MONOTONIC, &reject_timestamp);
            response.req_id=req->request.req_id;
            response.ack=1;
            double sent_timestamp=TSPEC_TO_DOUBLE(req->request.req_timestamp);
            double length=TSPEC_TO_DOUBLE(req->request.req_length);
            double reject=TSPEC_TO_DOUBLE(reject_timestamp);
            printf("X%lu:%.6f,%.6f,%.6f\n", response.req_id, sent_timestamp, length, reject);
            size_t bytes = send(conn_socket, &response, sizeof(struct response), 0);

        }
    } while (in_bytes > 0);
    /* IMPLEMENT ME!! Write a loop to gracefully terminate all the
     * worker threads ! */
    for (int i = 0; i < conn_params.num_servers; ++i) {
        free(stacks[i]);

    }

    free(the_queue);

    free(req);
    shutdown(conn_socket, SHUT_RDWR);
    close(conn_socket);
    printf("INFO: Client disconnected.\n");
}


/* Template implementation of the main function for the FIFO
 * server. The server must accept in input a command line parameter
 * with the <port number> to bind the server to. */
int main (int argc, char ** argv) {
    int sockfd, retval, accepted, optval, opt, queue_size, num_servers;
    in_port_t socket_port;
    struct sockaddr_in addr, client;
    struct in_addr any_address;
    socklen_t client_len;

    struct connection_params conn_params;

    while ((opt = getopt(argc, argv, "q:w:")) != -1) {
        switch (opt) {
            case 'q':
                queue_size = atoi(optarg);
                break;
            case 'w':
                num_servers=atoi(optarg);
                break;
            case '?':
                if (optopt == 'q') {
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                } else {
                    fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                }
                return EXIT_FAILURE;
            default:
                fprintf(stderr, USAGE_STRING, argv[0]);
                return EXIT_FAILURE;
        }
    }

    conn_params.queue_size=queue_size;
    conn_params.num_servers=num_servers;

    /* 2. Detect the port number to bind the server socket to (see HW1 and HW2) */
    if(optind<argc) {
        socket_port=atoi(argv[optind]);
    }
    else {
        fprintf(stderr, USAGE_STRING, argv[0]);
        return EXIT_FAILURE;
    }

    socket_port = atoi(argv[optind]);
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

    /* Initialize queue protection variables. DO NOT TOUCH. */
    queue_mutex = (sem_t *)malloc(sizeof(sem_t));
    queue_notify = (sem_t *)malloc(sizeof(sem_t));
    retval = sem_init(queue_mutex, 0, 1);
    if (retval < 0) {
        ERROR_INFO();
        perror("Unable to initialize queue mutex");
        return EXIT_FAILURE;
    }
    retval = sem_init(queue_notify, 0, 0);
    if (retval < 0) {
        ERROR_INFO();
        perror("Unable to initialize queue notify");
        return EXIT_FAILURE;
    }
    /* DONE - Initialize queue protection variables */

    /* Ready to handle the new connection with the client. */
    handle_connection(accepted, conn_params);

    free(queue_mutex);
    free(queue_notify);

    close(sockfd);
    return EXIT_SUCCESS;

}
