/*******************************************************************************
* Single-Threaded FIFO Image Server Implementation w/ Queue Limit
*
* Description:
*     A server implementation designed to process client
*     requests for image processing in First In, First Out (FIFO)
*     order. The server binds to the specified port number provided as
*     a parameter upon launch. It launches a secondary thread to
*     process incoming requests and allows to specify a maximum queue
*     size.
*
* Usage:
*     <build directory>/server -q <queue_size> -w <workers> -p <policy> <port_number>
*
* Parameters:
*     port_number - The port number to bind the server to.
*     queue_size  - The maximum number of queued requests.
*     workers     - The number of parallel threads to process requests.
*     policy      - The queue policy to use for request dispatching.
*
* Author:
*     Renato Mancuso
*
* Affiliation:
*     Boston University
*
* Creation Date:
*     October 31, 2023
*
* Notes:
*     Ensure to have proper permissions and available port before running the
*     server. The server relies on a FIFO mechanism to handle requests, thus
*     guaranteeing the order of processing. If the queue is full at the time a
*     new request is received, the request is rejected with a negative ack.
*
*******************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <assert.h>

/* Needed for wait(...) */
#include <sys/types.h>
#include <sys/wait.h>

/* Needed for semaphores */
#include <semaphore.h>

/* Include struct definitions and other libraries that need to be
 * included by both client and server */
#include "common.h"

#define BACKLOG_COUNT 100
#define USAGE_STRING				\
	"Missing parameter. Exiting.\n"		\
	"Usage: %s -q <queue size> "		\
	"-w <workers: 1> "			\
	"-p <policy: FIFO> "			\
	"<port_number>\n"

/* 4KB of stack for the worker thread */
#define STACK_SIZE (4096)

/* Mutex needed to protect the threaded printf. DO NOT TOUCH */
sem_t * printf_mutex;

/* Synchronized printf for multi-threaded operation */
#define sync_printf(...)			\
	do {					\
		sem_wait(printf_mutex);		\
		printf(__VA_ARGS__);		\
		sem_post(printf_mutex);		\
	} while (0)

/* START - Variables needed to protect the shared queue. DO NOT TOUCH */
sem_t * queue_mutex;
sem_t * queue_notify;
/* END - Variables needed to protect the shared queue. DO NOT TOUCH */

struct image ** images = NULL;
uint64_t image_counter = 0;

sem_t arr_mutex;
sem_t * img_semaphores = NULL; //Array for handling semaphores for each image
struct img_req_array *wait_op;
sem_t * operation_mutex;

sem_t conn_socket_mutex;

struct request_meta {
	struct request request;
	struct timespec receipt_timestamp;
	struct timespec start_timestamp;
	struct timespec completion_timestamp;
};

enum queue_policy {
	QUEUE_FIFO,
	QUEUE_SJN
};

struct queue {
	size_t wr_pos;
	size_t rd_pos;
	size_t max_size;
	size_t available;
	enum queue_policy policy;
	struct request_meta * requests;
};

struct connection_params {
	size_t queue_size;
	size_t workers;
	enum queue_policy queue_policy;
};

struct worker_params {
	int conn_socket;
	int worker_done;
	struct queue * the_queue;
	int worker_id;
};

enum worker_command {
	WORKERS_START,
	WORKERS_STOP
};

struct img_req_array {
    uint64_t *request_ids; 
    size_t count;           
    size_t capacity; 
};


void queue_init(struct queue * the_queue, size_t queue_size, enum queue_policy policy)
{
	the_queue->rd_pos = 0;
	the_queue->wr_pos = 0;
	the_queue->max_size = queue_size;
	the_queue->requests = (struct request_meta *)malloc(sizeof(struct request_meta)
						     * the_queue->max_size);
	the_queue->available = queue_size;
	the_queue->policy = policy;
}

/* Add a new request <request> to the shared queue <the_queue> */
int add_to_queue(struct request_meta to_add, struct queue * the_queue)
{
	int retval = 0;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */

	/* Make sure that the queue is not full */
	if (the_queue->available == 0) {
		retval = 1;
	} else {
		the_queue->requests[the_queue->wr_pos] = to_add;
		the_queue->wr_pos = (the_queue->wr_pos + 1) % the_queue->max_size;
		the_queue->available--;

		sem_wait(operation_mutex);
		struct img_req_array *img_arr = &wait_op[to_add.request.img_id];
		if (img_arr->count == img_arr->capacity) {
			img_arr->capacity = (img_arr->capacity == 0) ? 1 : img_arr->capacity * 2;
			img_arr->request_ids = realloc(img_arr->request_ids, img_arr->capacity * sizeof(uint64_t));
		}
		img_arr->request_ids[img_arr->count++] = to_add.request.req_id;
		sem_post(operation_mutex);

		/* QUEUE SIGNALING FOR CONSUMER --- DO NOT TOUCH */
		sem_post(queue_notify);
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
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */
	retval = the_queue->requests[the_queue->rd_pos];
	the_queue->rd_pos = (the_queue->rd_pos + 1) % the_queue->max_size;
	the_queue->available++;

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
	return retval;
}

void dump_queue_status(struct queue * the_queue)
{
	size_t i, j;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */
	sem_wait(printf_mutex);
	printf("Q:[");

	for (i = the_queue->rd_pos, j = 0; j < the_queue->max_size - the_queue->available;
	     i = (i + 1) % the_queue->max_size, ++j)
	{
		printf("R%ld%s", the_queue->requests[i].request.req_id,
		       ((j+1 != the_queue->max_size - the_queue->available)?",":""));
	}

	printf("]\n");
	sem_post(printf_mutex);
	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
}

void register_new_image(int conn_socket, struct request * req)
{
    sem_wait(&arr_mutex);

    image_counter++;

    images = realloc(images, image_counter * sizeof(struct image *));
    img_semaphores = realloc(img_semaphores, image_counter * sizeof(sem_t));

	sem_wait(operation_mutex);

	wait_op = realloc(wait_op, image_counter * sizeof(struct img_req_array));
	wait_op[image_counter - 1].request_ids = NULL;
	wait_op[image_counter - 1].count = 0;
	wait_op[image_counter - 1].capacity = 0;

	sem_post(operation_mutex);

    struct image * new_img = recvImage(conn_socket);

    images[image_counter - 1] = new_img;

    sem_init(&img_semaphores[image_counter - 1], 0, 1);
    sem_post(&arr_mutex);

    struct response resp;
    resp.req_id = req->req_id;
    resp.img_id = image_counter - 1;
    resp.ack = RESP_COMPLETED;

    sem_wait(&conn_socket_mutex);
	send(conn_socket, &resp, sizeof(struct response), 0);
	sem_post(&conn_socket_mutex);
}


/* Main logic of the worker thread */
int worker_main (void * arg) {
    struct timespec now;
    struct worker_params * params = (struct worker_params *)arg;

    /* Print the first alive message. */
    clock_gettime(CLOCK_MONOTONIC, &now);
    sync_printf("[#WORKER#] %lf Worker Thread Alive!\n", TSPEC_TO_DOUBLE(now));

    while (!params->worker_done) {
        struct request_meta req = get_from_queue(params->the_queue);

        struct response resp;
		uint64_t img_id = req.request.img_id;
        if (params->worker_done)
            break;

		sem_wait(&img_semaphores[img_id]); 
		struct img_req_array *img_array = &wait_op[img_id];
		while (img_array->count > 0 && img_array->request_ids[0] != req.request.req_id) {
			sem_post(&img_semaphores[img_id]); 
			sem_wait(&img_semaphores[img_id]); 
		}

		sem_wait(operation_mutex);
		memmove(img_array->request_ids, img_array->request_ids + 1, (img_array->count - 1) * sizeof(uint64_t));
		img_array->count--;
		sem_post(operation_mutex);

		clock_gettime(CLOCK_MONOTONIC, &req.start_timestamp);

		struct image * img = NULL;
		img = images[img_id];
		assert(img != NULL);

		/* Image processing operations */
		switch (req.request.img_op) {
			case IMG_ROT90CLKW:
				img = rotate90Clockwise(img, NULL);
				break;
			case IMG_BLUR:
				img = blurImage(img);
				break;
			case IMG_SHARPEN:
				img = sharpenImage(img);
				break;
			case IMG_VERTEDGES:
				img = detectVerticalEdges(img);
				break;
			case IMG_HORIZEDGES:
				img = detectHorizontalEdges(img);
				break;
		}

		if (req.request.img_op != IMG_RETRIEVE) {
			sem_wait(&arr_mutex); 
			if (req.request.overwrite) {
				images[img_id] = img;        
			} else {
				/* Generate new ID and Increase the count of registered images */
				img_id = image_counter++;
				images = realloc(images, image_counter * sizeof(struct image *));
				images[img_id] = img;
			}
			sem_post(&arr_mutex); 
		}
		sem_post(&img_semaphores[img_id]); 

        clock_gettime(CLOCK_MONOTONIC, &req.completion_timestamp);

        /* Response to the client */
        resp.req_id = req.request.req_id;
        resp.ack = RESP_COMPLETED;
        resp.img_id = img_id;

		sem_wait(&conn_socket_mutex);
		send(params->conn_socket, &resp, sizeof(struct response), 0);
		sem_post(&conn_socket_mutex);

        /* Send the actual image payload for IMG_RETRIEVE operation */
        if (req.request.img_op == IMG_RETRIEVE) {
			sem_wait(&conn_socket_mutex);
            uint8_t err = sendImage(img, params->conn_socket);
            if(err) {
                ERROR_INFO();
                perror("Unable to send image payload to client.");
            }
			sem_post(&conn_socket_mutex);
        }

        sync_printf("T%d R%ld:%lf,%s,%d,%ld,%ld,%lf,%lf,%lf\n",
                   params->worker_id, req.request.req_id,
                   TSPEC_TO_DOUBLE(req.request.req_timestamp),
                   OPCODE_TO_STRING(req.request.img_op),
                   req.request.overwrite, req.request.img_id, img_id,
                   TSPEC_TO_DOUBLE(req.receipt_timestamp),
                   TSPEC_TO_DOUBLE(req.start_timestamp),
                   TSPEC_TO_DOUBLE(req.completion_timestamp));

        dump_queue_status(params->the_queue);
	}

    return EXIT_SUCCESS;
}


/* This function will start/stop all the worker threads wrapping
 * around the clone()/waitpid() system calls */
int control_workers(enum worker_command cmd, size_t worker_count,
		    struct worker_params * common_params)
{
	/* Anything we allocate should we kept as static for easy
	 * deallocation when the STOP command is issued */
	static char ** worker_stacks = NULL;
	static struct worker_params ** worker_params = NULL;
	static int * worker_ids = NULL;

	/* Start all the workers */
	if (cmd == WORKERS_START) {
		size_t i;
		/* Allocate all stacks and parameters */
		worker_stacks = (char **)malloc(worker_count * sizeof(char *));
		worker_params = (struct worker_params **)
		malloc(worker_count * sizeof(struct worker_params *));
		worker_ids = (int *)malloc(worker_count * sizeof(int));

		if (!worker_stacks || !worker_params) {
			ERROR_INFO();
			perror("Unable to allocate descriptor arrays for threads.");
			return EXIT_FAILURE;
		}

		/* Allocate and initialize as needed */
		for (i = 0; i < worker_count; ++i) {
			worker_ids[i] = -1;

			worker_stacks[i] = malloc(STACK_SIZE);
			worker_params[i] = (struct worker_params *)
				malloc(sizeof(struct worker_params));

			if (!worker_stacks[i] || !worker_params[i]) {
				ERROR_INFO();
				perror("Unable to allocate memory for thread.");
				return EXIT_FAILURE;
			}

			worker_params[i]->conn_socket = common_params->conn_socket;
			worker_params[i]->the_queue = common_params->the_queue;
			worker_params[i]->worker_done = 0;
			worker_params[i]->worker_id = i;
		}

		/* All the allocations and initialization seem okay,
		 * let's start the threads */
		for (i = 0; i < worker_count; ++i) {
			worker_ids[i] = clone(worker_main, worker_stacks[i] + STACK_SIZE,
					      CLONE_THREAD | CLONE_VM | CLONE_SIGHAND |
					      CLONE_FS | CLONE_FILES | CLONE_SYSVSEM,
					      worker_params[i]);

			if (worker_ids[i] < 0) {
				ERROR_INFO();
				perror("Unable to start thread.");
				return EXIT_FAILURE;
			} else {
				sync_printf("INFO: Worker thread %ld (TID = %d) started!\n",
				       i, worker_ids[i]);
			}
		}
	}

	else if (cmd == WORKERS_STOP) {
		size_t i;

		/* Command to stop the threads issues without a start
		 * command? */
		if (!worker_stacks || !worker_params || !worker_ids) {
			return EXIT_FAILURE;
		}

		/* First, assert all the termination flags */
		for (i = 0; i < worker_count; ++i) {
			if (worker_ids[i] < 0) {
				continue;
			}

			/* Request thread termination */
			worker_params[i]->worker_done = 1;
		}

		/* Next, unblock threads and wait for completion */
		for (i = 0; i < worker_count; ++i) {
			if (worker_ids[i] < 0) {
				continue;
			}

			sem_post(queue_notify);
			waitpid(-1, NULL, __WCLONE);
			sync_printf("INFO: Worker thread exited.\n");
		}

		/* Finally, do a round of deallocations */
		for (i = 0; i < worker_count; ++i) {
			free(worker_stacks[i]);
			free(worker_params[i]);
		}

		free(worker_stacks);
		worker_stacks = NULL;

		free(worker_params);
		worker_params = NULL;

		free(worker_ids);
		worker_ids = NULL;
	}

	else {
		ERROR_INFO();
		perror("Invalid thread control command.");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/* Main function to handle connection with the client. This function
 * takes in input conn_socket and returns only when the connection
 * with the client is interrupted. */
void handle_connection(int conn_socket, struct connection_params conn_params)
{
	struct request_meta * req;
	struct queue * the_queue;
	size_t in_bytes;

	/* The connection with the client is alive here. Let's start
	 * the worker thread. */
	struct worker_params common_worker_params;
	int res;

	/* Now handle queue allocation and initialization */
	the_queue = (struct queue *)malloc(sizeof(struct queue));
	queue_init(the_queue, conn_params.queue_size, conn_params.queue_policy);

	common_worker_params.conn_socket = conn_socket;
	common_worker_params.the_queue = the_queue;
	res = control_workers(WORKERS_START, conn_params.workers, &common_worker_params);

	/* Do not continue if there has been a problem while starting
	 * the workers. */
	if (res != EXIT_SUCCESS) {
		free(the_queue);

		/* Stop any worker that was successfully started */
		control_workers(WORKERS_STOP, conn_params.workers, NULL);
		return;
	}

	/* We are ready to proceed with the rest of the request
	 * handling logic. */
    req = (struct request_meta *)malloc(sizeof(struct request_meta));

    do {
        in_bytes = recv(conn_socket, &req->request, sizeof(struct request), 0);
        clock_gettime(CLOCK_MONOTONIC, &req->receipt_timestamp);

		/* Don't just return if in_bytes is 0 or -1. Instead
		 * skip the response and break out of the loop in an
		 * orderly fashion so that we can de-allocate the req
		 * and resp varaibles, and shutdown the socket. */
		if (in_bytes > 0) {

			/* Handle image registration right away! */
			if(req->request.img_op == IMG_REGISTER) {
				clock_gettime(CLOCK_MONOTONIC, &req->start_timestamp);

				register_new_image(conn_socket, &req->request);

				clock_gettime(CLOCK_MONOTONIC, &req->completion_timestamp);

				sync_printf("T%ld R%ld:%lf,%s,%d,%ld,%ld,%lf,%lf,%lf\n",
				       conn_params.workers, req->request.req_id,
				       TSPEC_TO_DOUBLE(req->request.req_timestamp),
				       OPCODE_TO_STRING(req->request.img_op),
				       req->request.overwrite, req->request.img_id,
				       image_counter - 1,
				       TSPEC_TO_DOUBLE(req->receipt_timestamp),
				       TSPEC_TO_DOUBLE(req->start_timestamp),
				       TSPEC_TO_DOUBLE(req->completion_timestamp));

				dump_queue_status(the_queue);
				continue;
			}

			res = add_to_queue(*req, the_queue);

			/* The queue is full if the return value is 1 */
			if (res) {
				struct response resp;
				/* Now provide a response! */
				resp.req_id = req->request.req_id;
				resp.ack = RESP_REJECTED;
				sem_wait(&conn_socket_mutex);
				send(conn_socket, &resp, sizeof(struct response), 0);
				sem_post(&conn_socket_mutex);

				sync_printf("X%ld:%lf,%lf,%lf\n", req->request.req_id,
				       TSPEC_TO_DOUBLE(req->request.req_timestamp),
				       TSPEC_TO_DOUBLE(req->request.req_length),
				       TSPEC_TO_DOUBLE(req->receipt_timestamp)
					);

			}
		}
	} while (in_bytes > 0);
    free(req);
	free(the_queue->requests);
	free(the_queue);
    shutdown(conn_socket, SHUT_RDWR);
    close(conn_socket);
    sync_printf("INFO: Client disconnected.\n");
}


/* Template implementation of the main function for the FIFO
 * server. The server must accept in input a command line parameter
 * with the <port number> to bind the server to. */
int main (int argc, char ** argv) {
    int sockfd, retval, accepted, optval, opt;
    in_port_t socket_port;
    struct sockaddr_in addr, client;
    struct in_addr any_address;
    socklen_t client_len;
    struct connection_params conn_params;
    conn_params.queue_size = 0;
    conn_params.queue_policy = QUEUE_FIFO;
    conn_params.workers = 1;

    /* Parse all the command line arguments */
    while((opt = getopt(argc, argv, "q:w:p:")) != -1) {
        switch (opt) {
        case 'q':
            conn_params.queue_size = strtol(optarg, NULL, 10);
            printf("INFO: setting queue size = %ld\n", conn_params.queue_size);
            break;
        case 'w':
            conn_params.workers = strtol(optarg, NULL, 10);
            printf("INFO: setting worker count = %ld\n", conn_params.workers);
            break;
        case 'p':
            if (!strcmp(optarg, "FIFO")) {
                conn_params.queue_policy = QUEUE_FIFO;
            } else {
                ERROR_INFO();
                fprintf(stderr, "Invalid queue policy.\n" USAGE_STRING, argv[0]);
                return EXIT_FAILURE;
            }
            printf("INFO: setting queue policy = %s\n", optarg);
            break;
        default: /* '?' */
            fprintf(stderr, USAGE_STRING, argv[0]);
        }
    }

    if (!conn_params.queue_size) {
        ERROR_INFO();
        fprintf(stderr, USAGE_STRING, argv[0]);
        return EXIT_FAILURE;
    }

    if (optind < argc) {
        socket_port = strtol(argv[optind], NULL, 10);
        printf("INFO: setting server port as: %d\n", socket_port);
    } else {
        ERROR_INFO();
        fprintf(stderr, USAGE_STRING, argv[0]);
        return EXIT_FAILURE;
    }

    /* Socket creation and binding */
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

    /* Initialize semaphores */
    printf_mutex = (sem_t *)malloc(sizeof(sem_t));
    retval = sem_init(printf_mutex, 0, 1);
    if (retval < 0) {
        ERROR_INFO();
        perror("Unable to initialize printf mutex");
        return EXIT_FAILURE;
    }

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
	retval = sem_init(&conn_socket_mutex, 0, 1);
	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to initialize connection socket mutex");
		return EXIT_FAILURE;
	}
    operation_mutex = malloc(sizeof(sem_t));
    if (sem_init(operation_mutex, 0, 1) != 0) {
        perror("Unable to initialize mutex for operation on image");
        return EXIT_FAILURE;
    }
    sem_init(&arr_mutex, 0, 1);
    img_semaphores = NULL; 
    /* Handle connection */
    handle_connection(accepted, conn_params);

	if (wait_op) {
        for (size_t i = 0; i < image_counter; i++) {
			if (wait_op[i].request_ids) {
            	free(wait_op[i].request_ids);
			}
        }
        free(wait_op);
    }

	free(queue_mutex);
    free(queue_notify);
    free(printf_mutex);
	free(operation_mutex);
    close(sockfd);
}