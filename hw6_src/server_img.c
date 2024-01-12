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
	"Usage: %s -q <queue size> "		\
	"-w <workers: 1> "			\
	"-p <policy: FIFO> "			\
	"<port_number>\n"

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
struct image ** img_arr=NULL;

struct request_meta {  
	struct request req;
	struct timespec reciept;
};


struct queue {
	int head;
	int tail;
	struct request_meta items[QUEUE_MAX];
	int size;
};

struct connection_params{
	long queue_size;
	int thread_num;
	int policy;
};


struct worker_params{ 
	struct queue * q;
	int images_count; 
    int thread_id;
	int socket;
    int worker_done;
	int policy;
};

struct timespec reciept;
int add_to_queue(struct request_meta to_add, struct queue * the_queue)
{
	int retval = 0;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */
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

struct request_meta get_from_queue_fifo(struct queue * the_queue)
{
	struct request_meta retval;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_notify);
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */
	
	if(the_queue->size==0){
		printf("empty queue\n");
	       
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


/* Implement this method to correctly dump the status of the queue
 * following the format Q:[R<request ID>,R<request ID>,...] */
void dump_queue_status(struct queue * the_queue)
{
	int i;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */

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


int worker_main(void *args) {
	struct worker_params* worker_params=(struct worker_params*)args; 
	int num_imgs=worker_params->images_count;
	struct queue *queue=worker_params->q;
	int conn_socket=worker_params->socket;
	struct request_meta curreq;
	struct timespec start,completion;
	struct response res;
	while(!worker_params->worker_done | (queue->size!=0)){
		if(worker_params->policy==0){
			curreq=get_from_queue_fifo(queue);
		}
		if (worker_params->worker_done) {
			break;
		}
		clock_gettime(CLOCK_MONOTONIC,&start);
		enum img_opcode opcode=curreq.req.img_op;
		struct image * operation;
		switch(opcode){
		case IMG_ROT90CLKW:
			operation=rotate90Clockwise(img_arr[curreq.req.img_id],NULL);
			break;
		case IMG_BLUR:
			operation=blurImage(img_arr[curreq.req.img_id]);
			break;
		case IMG_SHARPEN:
			operation=sharpenImage(img_arr[curreq.req.img_id]);
			break;
		case IMG_VERTEDGES:
			operation=detectVerticalEdges(img_arr[curreq.req.img_id]);
			break;
		case IMG_HORIZEDGES:
			operation=detectHorizontalEdges(img_arr[curreq.req.img_id]);
			break;
		case IMG_RETRIEVE:

			break;
		default:
			printf("No opcode\n");
		}
		uint64_t img_code=curreq.req.img_id;
		if(opcode!=IMG_RETRIEVE && !curreq.req.overwrite){
			img_arr=realloc(img_arr,(num_imgs+1)*sizeof(struct image*));
			img_arr[num_imgs]=operation;
			img_code=num_imgs;
			num_imgs++; 
		}else if(opcode!=IMG_RETRIEVE){
			deleteImage(img_arr[curreq.req.img_id]);
			img_arr[curreq.req.img_id]=operation; 
		}
		
		clock_gettime(CLOCK_MONOTONIC,&completion);
		res.req_id=curreq.req.req_id;
		res.ack=0;
		res.img_id=img_code;
		write(conn_socket,(struct response *) &res,sizeof(struct response));
		if(opcode==IMG_RETRIEVE){
			sendImage(img_arr[curreq.req.img_id],conn_socket);
		}
		int id=curreq.req.req_id;
		double sent_time=TSPEC_TO_DOUBLE(curreq.req.req_timestamp);
		const char* op=OPCODE_TO_STRING(opcode);
		uint8_t overwr=curreq.req.overwrite; 
		uint64_t image_id=curreq.req.img_id;
		sync_printf("T%d R%d:%.6f,%s,%d,%ld,%ld,%.6f,%.6f,%.6f\n",worker_params->thread_id,id,sent_time,op,overwr,image_id,img_code,TSPEC_TO_DOUBLE(curreq.reciept),TSPEC_TO_DOUBLE(start),TSPEC_TO_DOUBLE(completion));
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
	void* stacks[conn_params.thread_num];
	struct worker_params* params[conn_params.thread_num];
	int i;
	volatile int num_imgs=0;
	int worker_done=0;
	for(i=0;i<conn_params.thread_num;i++){
		params[i]=malloc(sizeof(struct worker_params));
		params[i]->worker_done=worker_done;
		params[i]->q=the_queue;
		params[i]->socket=conn_socket;
		params[i]->thread_id=i;
		params[i]->policy=conn_params.policy;
		params[i]->images_count=num_imgs;
		stacks[i]=malloc(STACK_SIZE);
		int res=clone(worker_main,stacks[i]+STACK_SIZE,(CLONE_THREAD | CLONE_VM | CLONE_SIGHAND),(void *)params[i]);
		if(res<0){
		  ERROR_INFO();
		  perror("Thread was not started");
		  free(the_queue);
		  return;
		}
	}
	int data;
	int client_size=sizeof(struct request);

	struct request_meta clientreq;
	while(1){
		data=read(conn_socket, &clientreq.req,client_size);
		if(data<=0){
			break;
		}
		clock_gettime(CLOCK_MONOTONIC,&clientreq.reciept);
		if(clientreq.req.img_op==1){
			struct timespec start, completion;
			clock_gettime(CLOCK_MONOTONIC,&start);
			img_arr=realloc(img_arr,(num_imgs+1)*sizeof(struct image*));
			img_arr[num_imgs]=recvImage(conn_socket);
			struct response regresp={clientreq.req.req_id,num_imgs,0};
			write(conn_socket, &regresp,sizeof(struct response));
			clock_gettime(CLOCK_MONOTONIC,&completion);
			double sent_time=TSPEC_TO_DOUBLE(clientreq.req.req_timestamp);
			const char* op=OPCODE_TO_STRING(clientreq.req.img_op);
			uint8_t ow=0;
			uint64_t clim=0;
			double rectime=TSPEC_TO_DOUBLE(clientreq.reciept);
			double starttime=TSPEC_TO_DOUBLE(start);
			double comptime=(TSPEC_TO_DOUBLE(completion));
			sync_printf("T0 R%ld:%.6f,%s,%d,%ld,%d,%.6f,%.6f,%.6f\n",clientreq.req.req_id,sent_time,op,ow,clim,num_imgs,rectime,starttime,comptime);
			num_imgs++;			
			continue;
		}
		
		int availability=add_to_queue(clientreq,the_queue);
		struct response rej_res;
		if(availability ==-1){
			rej_res.req_id=clientreq.req.req_id;
			rej_res.img_id=clientreq.req.img_id;
			rej_res.ack=1;
			write(conn_socket, &rej_res,sizeof(struct response));
			sync_printf("X%lu:%.6f,%.6f\n",clientreq.req.req_id, TSPEC_TO_DOUBLE(clientreq.req.req_timestamp), TSPEC_TO_DOUBLE(clientreq.reciept));
		}
		
	}
	printf("INFO: Asserting termination flag for worker thread...\n");
	
	for(i=0;i<conn_params.thread_num;i++){
		params[i]->worker_done = 1;
		waitpid(-1, NULL, 0);	
		sem_post(queue_notify);
	}
	
	printf("INFO: Worker thread exited.\n");
	free(the_queue);
	for(i=0;i<conn_params.thread_num;i++){
		free(stacks[i]);
		free(params[i]);
	}

	shutdown(conn_socket, SHUT_RDWR);
	close(conn_socket);
	printf("INFO: Client disconnected.\n");
}



	
/* Template implementation of the main function for the FIFO
 * server. The server must accept in input a command line parameter
 * with the <port number> to bind the server to. */
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
			conn_params.thread_num = strtol(optarg,NULL,0);
			break;
		case 'p':
			if(strcmp(optarg,"FIFO")==0){
				conn_params.policy=0;
				printf("FIFO POLICY LIVE\n");
			/* }else if(strcmp(optarg,"SJN")==0){ */
			/* 	printf("SJN POLICY LIVE\n"); */
			/* 	conn_params.policy=1; */
			}else{
				printf("invalid policy entered. Please use SJN or FIFO\n");
				return EXIT_FAILURE;
			}
			break;
		default:
			printf("YOU USED IT WRONG. LEAVE.");
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
