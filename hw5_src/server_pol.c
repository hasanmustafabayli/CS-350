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
	"Usage: %s <port_number> -q <queue_size> -w <worker_threads>\n"

/* 4KB of stack for the worker thread */
#define STACK_SIZE (4096)

/* START - Variables needed to protect the shared queue. DO NOT TOUCH */
sem_t * queue_mutex;
sem_t * queue_notify;
/* END - Variables needed to protect the shared queue. DO NOT TOUCH */
sem_t * printf_mutex;
#define sync_printf(...)			\
  do {						\
    sem_wait(printf_mutex);			\
    printf(__VA_ARGS__);			\
    sem_post(printf_mutex);			\
  } while(0)

#define QUEUE_MAX 1500
int QUEUE_SIZE;

struct request_meta { 
	struct request req;
	struct timespec reciept; 
};

struct connection_params{ 
	long queue_size;
	int threads; 
	int policy;
};

struct worker_params{ 
	struct queue * the_queue; 
	int thread_id;
	int policy;
	int conn_socket; 
    int worker_done;
};
struct queue {
	int head;
	int tail;
	struct request_meta items[QUEUE_MAX];
	int size;
};

int add_to_queue(struct request_meta to_add, struct queue * the_queue)
{
	int retval = 0;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */
	int next_write;
	next_write=(the_queue->tail+1)%QUEUE_SIZE;	
	if(next_write==the_queue->head){
		retval=-1;
	}else{
		if(the_queue->size==0){
			the_queue->head=the_queue->tail=0;
			the_queue->items[0]=to_add;
			the_queue->size+=1;
			retval=0;
		}else{
			the_queue->items[next_write]=to_add;
			the_queue->tail=(the_queue->tail+1)%QUEUE_SIZE;
			retval=0;
			the_queue->size+=1;
			
		}
		sem_post(queue_notify);
	}
	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
	return retval;
}

struct request_meta sjn_get_from_queue(struct queue * the_queue)
{
	struct request_meta retval;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_notify);
	sem_wait(queue_mutex);
	if(the_queue->size==0){
		printf("Error: empty queue\n");
	       
	}else{
		int ind_sjn=the_queue->head; 
		double length_sjn=TSPEC_TO_DOUBLE(the_queue->items[the_queue->head].req.req_length);		
		for(int i=0;i<the_queue->size;i++){
			int index=(the_queue->head+i)%QUEUE_SIZE;
			double cur_length=TSPEC_TO_DOUBLE(the_queue->items[index].req.req_length);
			if(cur_length<length_sjn){
				length_sjn=cur_length;
				ind_sjn=index;
			}				  
		}
		retval=the_queue->items[ind_sjn];
		for(int i=ind_sjn;i!=the_queue->head;i=(i-1+QUEUE_SIZE)%QUEUE_SIZE){
			the_queue->items[i]=the_queue->items[(i-1+QUEUE_SIZE)%QUEUE_SIZE];
		}
		the_queue->head=(the_queue->head+1)%QUEUE_SIZE;
		the_queue->size-=1;
		if(the_queue->size==0){
			the_queue->head=the_queue->tail=-1;
		}
		if(TSPEC_TO_DOUBLE(retval.req.req_length)==0){
			printf("Error: length of the request is equal to 0");
		}
	}
	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
	return retval;
}
struct request_meta fifo_get_from_queue(struct queue * the_queue)
{
	struct request_meta retval;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_notify);
	sem_wait(queue_mutex);
	if(the_queue->size==0){
		printf("ERROR: TRIED TO GET FROM EMPTY QUEUE\n");
	       
	}else{
		retval=the_queue->items[the_queue->head];
		the_queue->head=(the_queue->head+1)%QUEUE_SIZE;
		the_queue->size-=1;
		if(the_queue->size==0){
			the_queue->head=the_queue->tail=-1;
		}
	}
	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */

	return retval;
}
void dump_queue_status(struct queue * the_queue)
{
	int i;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	int end=the_queue->size;
	sync_printf("Q:[");
	int start=the_queue->head;
	int id;
	int j=0;
	
	for (i=start;(j<end);i++){
		i=i%QUEUE_SIZE;
		j++;
		id=the_queue->items[i].req.req_id;
		sync_printf("R%d",id);
		if(j!=end){
			sync_printf(",");
		}
	}
	sync_printf("]\n");
	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
}

struct timespec reciept;
int worker_main(void *args){

	struct worker_params* params=(struct worker_params*)args; 
	struct queue *queue=params->the_queue; 
	int conn_socket=params->conn_socket;
	struct request_meta curreq;
	struct timespec start_timestamp,completion_timestamp;
	struct response client_res; 
	while(!params->worker_done | (queue->size!=0)){
		if(params->policy==0){
			curreq=fifo_get_from_queue(queue);
		}else if(params->policy==1){
			curreq=sjn_get_from_queue(queue);
		}
		if (params->worker_done) {
			break;
		}
		
		clock_gettime(CLOCK_MONOTONIC,&start_timestamp);
		get_elapsed_busywait(curreq.req.req_length.tv_sec,curreq.req.req_length.tv_nsec);
		clock_gettime(CLOCK_MONOTONIC,&completion_timestamp);
		client_res.req_id=curreq.req.req_id;
		client_res.ack=0;
		write(conn_socket,(struct response *) &client_res,sizeof(struct response));
		int id=curreq.req.req_id;
		double sent_time=TSPEC_TO_DOUBLE(curreq.req.req_timestamp);
		double len=TSPEC_TO_DOUBLE(curreq.req.req_length);
		double rec_time=TSPEC_TO_DOUBLE(curreq.reciept);
		double start_time=TSPEC_TO_DOUBLE(start_timestamp);
		double comp_time=(TSPEC_TO_DOUBLE(completion_timestamp));
		sync_printf("T%d R%d:%.6f,%.6f,%.6f,%.6f,%.6f\n",params->thread_id,id,sent_time,len,rec_time,start_time,comp_time);
		dump_queue_status(queue);
	} 
	return 0;
}


void handle_connection(int conn_socket, struct connection_params conn_params)
{
	struct queue * the_queue;
	the_queue=(struct queue*)malloc(sizeof(struct queue));
	the_queue->head=-1;
	the_queue->tail=-1;
	the_queue->size=0;
	void* stacks[conn_params.threads];
	struct worker_params* params[conn_params.threads];
	int i;
	int worker_done=0;
	for(i=0;i<conn_params.threads;i++){ 
		params[i]=malloc(sizeof(struct worker_params));
		params[i]->worker_done=worker_done;
		params[i]->the_queue=the_queue;
		params[i]->conn_socket=conn_socket;
		params[i]->thread_id=i;
		params[i]->policy=conn_params.policy;
		stacks[i]=malloc(STACK_SIZE);
		clone(worker_main,stacks[i]+STACK_SIZE,(CLONE_THREAD | CLONE_VM | CLONE_SIGHAND),(void *)params[i]);
	}
	int client_size;
	int data;
	client_size=sizeof(struct request);
	struct request_meta clientreq;
	while(1){
		data=read(conn_socket, &clientreq.req,client_size);
		if(data<=0){
			break;
		}
		clock_gettime(CLOCK_MONOTONIC,&clientreq.reciept);
		int status=add_to_queue(clientreq,the_queue);
		struct response rejected_response;
		if(status==-1){
			rejected_response.req_id=clientreq.req.req_id;
			rejected_response.ack=1;
			write(conn_socket, &rejected_response,sizeof(struct response));
			sync_printf("X%lu:%.6f,%.6f,%.6f\n",clientreq.req.req_id, TSPEC_TO_DOUBLE(clientreq.req.req_timestamp), TSPEC_TO_DOUBLE(clientreq.req.req_length),	TSPEC_TO_DOUBLE(clientreq.reciept));
		}
	}
	printf("INFO: Asserting termination flag for worker thread...\n");
	for(i=0;i<conn_params.threads;i++){
		params[i]->worker_done = 1;
		waitpid(-1, NULL, 0);	
		sem_post(queue_notify);
	}
	printf("INFO: Worker thread exited.\n");
	free(the_queue);
	for(i=0;i<conn_params.threads;i++){
		free(stacks[i]);
		free(params[i]);
	}
	shutdown(conn_socket, SHUT_RDWR);
	close(conn_socket);
	printf("INFO: Client disconnected.\n");
}
int main (int argc, char ** argv) {
	int sockfd, retval, accepted, optval;
	in_port_t socket_port;
	struct sockaddr_in addr, client;
	struct in_addr any_address;
	socklen_t client_len;
	struct connection_params conn_params;
	int opt;
	while((opt=getopt(argc,argv,"q:w:p:")) != -1){
		switch(opt){
		case 'q':
			conn_params.queue_size=strtol(optarg,NULL,0); 
			break;
		case 'w':
			conn_params.threads = strtol(optarg,NULL,0);
			break;
		case 'p':
			if(strcmp(optarg,"FIFO")==0){
				conn_params.policy=0;
			}else if(strcmp(optarg,"SJN")==0){
				conn_params.policy=1;
			}else{
				return EXIT_FAILURE;
			}
			break;
		default:
			return EXIT_FAILURE;
		}
	}
	QUEUE_SIZE=conn_params.queue_size;
	if (optind<argc) {
		socket_port = strtol(argv[optind], NULL, 10);
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

	/* Initialize queue protection variables. DO NOT TOUCH. */
	queue_mutex = (sem_t *)malloc(sizeof(sem_t));
	queue_notify = (sem_t *)malloc(sizeof(sem_t));
	printf_mutex = (sem_t *)malloc(sizeof(sem_t));
	retval = sem_init(queue_mutex, 0, 1);
	if(sem_init(printf_mutex,0,1)<0){
	  ERROR_INFO();
	  perror("Unable to init printf mutex");
	  return EXIT_FAILURE;
	}
	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to initialize queue mutex");
		return EXIT_FAILURE;
	}
	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to initialize queue notify");
		return EXIT_FAILURE;
	}
	/* DONE - Initialize queue protection variables. DO NOT TOUCH */

	/* Ready to handle the new connection with the client. */
	handle_connection(accepted,conn_params);

	free(queue_mutex);
	free(queue_notify);
	free(printf_mutex);
	close(sockfd);
	return EXIT_SUCCESS;

}