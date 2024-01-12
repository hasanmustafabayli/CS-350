/*******************************************************************************
* FIFO Order Client Implementation
*
* Description:
*     A client designed to send requests to the FIFO Order Server running on
*     localhost. The client allows users to specify parameters related to
*     the rate of packet arrival, service rate, and the total number of
*     packets to be sent.
*
* Usage:
*     client_executable <port_number> [-a arrival_rate] [-s service_rate] [-n num_packets]
*
* Parameters:
*     port_number    - The port number the server on localhost is bound to.
*     -a             - Optional. Specifies the arrival rate of packets.
*     -s             - Optional. Specifies the service rate for processing packets.
*     -n             - Optional. Specifies the total number of packets to be sent.
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
*     The client communicates with a server running on localhost. If optional
*     parameters are not specified, default values will be used. For more
*     details or troubleshooting, refer to the accompanying documentation.
*
*******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "common.h"

#define PREFIX "[#CLIENT#] "

#define USAGE_STRING							\
	"Missing or unrecognized parameter. Exiting.\n"			\
	"Usage: %s [-a <arrival rate>] [-s <service rate>]"		\
	" [-n <nr. of packets>] <port number>\n"

#define DISTR_EXP   0
#define DISTR_CONST 1
#define DISTR_NORM  2
#define NUM_DISTR   3

#define CLIENT_VERSION    3
#define CLIENT_SUBVERSION 0

struct request_metadata {
	uint64_t req_id;
	struct timespec send_timestamp;
	struct timespec req_length;
};

struct response_metadata {
	uint64_t req_id;
	uint8_t ack;
	struct timespec recv_timestamp;
};

struct client_params {
	int conn_socket;
	uint8_t distr;
	uint64_t num_requests;
	double arr_rate;
	double serv_rate;
	struct request_metadata * script;
};

struct request_metadata * requests = NULL;
struct response_metadata * responses = NULL;
unsigned long num_responses = 0;

void generate_report(void)
{
	unsigned long i;
	struct timespec previous;

	memset(&previous, 0, sizeof(struct timespec));

	printf(PREFIX "==== REPORT ====\n");

	for (i = 0; i < num_responses; ++i) {
		struct request_metadata * req = &requests[i];
		struct response_metadata * resp = &responses[i];

		struct timespec expected = req->send_timestamp;
		struct timespec act_expected;

		if (resp->ack == RESP_COMPLETED) {
			/* If we sent the next request AFTER the previous was
			 * completed, then add the length to the send
			 * timestamp */
			if (timespec_cmp(&expected, &previous) != -1) {
				timespec_add(&expected, &req->req_length);
			} else {
				expected = previous;
				timespec_add(&expected, &req->req_length);
			}
			previous = resp->recv_timestamp;
			act_expected = expected;
		} else {
			/* Rejected requests should not take any time */
			act_expected = req->send_timestamp;
		}

		printf(PREFIX "R[%ld]: Sent: %ld.%09ld Recv: %ld.%09ld"
		       " Exp: %ld.%09ld Len: %ld.%09ld Rejected: %s\n",
		       i, req->send_timestamp.tv_sec, req->send_timestamp.tv_nsec,
		       resp->recv_timestamp.tv_sec, resp->recv_timestamp.tv_nsec,
		       act_expected.tv_sec, act_expected.tv_nsec,
		       req->req_length.tv_sec, req->req_length.tv_nsec,
		       (resp->ack == RESP_REJECTED ? "Yes" : "No"));

	}
}

void get_response(int conn_socket)
{
	/* Attempt to non-blockigly receive responses */
	struct response resp;
	int res = recv(conn_socket, &resp, sizeof(struct response), MSG_DONTWAIT);

	if (res > 0) {
		if (resp.ack == RESP_COMPLETED) {
			printf(PREFIX "RESP REQ %ld\n", resp.req_id);
		} else {
			printf(PREFIX "REJ REQ %ld\n", resp.req_id);
		}
		num_responses++;
		struct response_metadata * to_fill = &responses[resp.req_id];
		clock_gettime(CLOCK_MONOTONIC, &to_fill->recv_timestamp);
		to_fill->req_id = resp.req_id;
		to_fill->ack = resp.ack;
	} else if (res == 0) {
		printf(PREFIX "Connection closed by the server.\n");
		exit(EXIT_FAILURE);
	} else if (res == -1 && errno != EAGAIN) {
		printf(PREFIX "Connection error.\n");
		exit(EXIT_FAILURE);
	}
}

uint64_t busywait_timespec_recv(struct timespec delay, int conn_socket)
{
	uint64_t start, end;
	struct timespec now;

	/* Measure the current system time */
	clock_gettime(CLOCK_MONOTONIC, &now);
	timespec_add(&delay, &now);

	/* Get the start timestamp */
	get_clocks(start);

	/* Busy wait until enough time has elapsed */
	do {
		clock_gettime(CLOCK_MONOTONIC, &now);
		get_response(conn_socket);
	} while (delay.tv_sec > now.tv_sec || delay.tv_nsec > now.tv_nsec);

	/* Get end timestamp */
	get_clocks(end);

	return (end - start);
}

/* Generate an exponentially distributed sample with rate in events
 * per second */
struct timespec get_next_arrival(struct client_params * params, unsigned long idx)
{
	if (params->script) {
		return params->script[idx].send_timestamp;
	} else {
		/* Generate next inter-arrival using exponential
		 * distribution */
		double x = (double)random()/RAND_MAX;
		double sample = log(1-x)/(-params->arr_rate);
		return dtotspec(sample);
	}
}

struct timespec get_next_length(struct client_params * params, unsigned long idx)
{
	if (params->script) {
		return params->script[idx].req_length;
	} else if (params->distr == DISTR_CONST) {
		return dtotspec(1/params->serv_rate);
	} else if (params->distr == DISTR_NORM) {
		double mu = 1/params->serv_rate;
		double si = mu / 4;
		double x1 = ( (double)(random()) + 1. )/( (double)(RAND_MAX) + 1. );
		double x2 = ( (double)(random()) + 1. )/( (double)(RAND_MAX) + 1. );
		double sample = cos(2*3.14*x2)*sqrt(-2.*log(x1));
		sample = fmax((sample*si) + mu, 0);
		return dtotspec(sample);
	} else {
		/* Generate next request length using exponential
		 * distribution */
		double x = (double)random()/RAND_MAX;
		double sample = log(1-x)/(-params->serv_rate);
		return dtotspec(sample);
	}
}

void handle_connection(struct client_params * params)
{
	/* Retrieve all the client parameters */
	int conn_socket = params->conn_socket;
	unsigned long i, num_requests = params->num_requests;
	int res;

	/* Initialize buffer where to accumulate responses */
	responses = (struct response_metadata *)malloc(num_requests *
						       sizeof(struct response_metadata));

	requests = (struct request_metadata *)malloc(num_requests *
						       sizeof(struct request_metadata));

	for (i = 0; i < num_requests; ++i) {
		struct timespec inter_arrival;
		struct request req_payload;
		struct request_metadata * req_meta = &requests[i];

		printf(PREFIX "PREP REQ %ld\n", i);
		inter_arrival = get_next_arrival(params, i);
		req_payload.req_id = i;
		req_payload.req_length = get_next_length(params, i);

		req_meta->req_id = i;
		req_meta->req_length = req_payload.req_length;
		clock_gettime(CLOCK_MONOTONIC, &req_meta->send_timestamp);
		req_payload.req_timestamp = req_meta->send_timestamp;

		res = send(conn_socket, &req_payload, sizeof(struct request), 0);
		printf(PREFIX "SENT REQ %ld\n", i);
		if (res == -1) {
			printf(PREFIX "Connection closed by the server.\n");
			exit(EXIT_FAILURE);
		}
		busywait_timespec_recv(inter_arrival, conn_socket);
	}

	while (num_responses < num_requests) {
		get_response(conn_socket);
	}

	generate_report();

	free(responses);
	free(requests);
	printf(PREFIX "DONE!\n");
}

void parse_req_script(char * script_txt, struct client_params * params)
{
	uint64_t len, i, num_requests = 0;
	char * tok, * saveptr = NULL;
	struct request_metadata * reqs;

	/* First off, let's count the number of requests in the
	 * script */
	len = strlen(script_txt);
	for(i = 0; i < len; ++i) {
		if (script_txt[i] == ':') {
			++num_requests;
		}
	}

	/* Now allocate enough room for the parsed requests */
	params->num_requests = num_requests;
	reqs = (struct request_metadata *)malloc(num_requests *
						 sizeof(struct request_metadata));
	params->script = reqs;

	/* Let's get started! The fist token will be a relative
	 * request timestamp */
	i = 0;
	tok = strtok_r(script_txt, ":,", &saveptr);

	for(;tok;++i,++reqs) {
		reqs->req_id = i;
		/* Read in next inter-arrival */
		reqs->send_timestamp = dtotspec(strtod(tok, NULL));

		/* Read in request length */
		tok = strtok_r(NULL, ":,", &saveptr);
		reqs->req_length = dtotspec(strtod(tok, NULL));

		printf("TS: %ld.%09ld - LEN: %ld.%09ld \n",
		       reqs->send_timestamp.tv_sec, reqs->send_timestamp.tv_nsec,
		       reqs->req_length.tv_sec, reqs->req_length.tv_nsec);

		/* Read in next inter-arrival, if any */
		tok = strtok_r(NULL, ":,", &saveptr);
	}
}

int main (int argc, char ** argv) {
	int sockfd, retval, opt;
	in_port_t socket_port;
	struct sockaddr_in addr;
	struct in_addr localhost_addr;
	int conn_attempts = 100;

	/* These are the parameters that will be passed to the
	 * handle_connection procedure */
	struct client_params params;

	/* Initialize default parameters */
	params.num_requests = 100;
	params.arr_rate = 10;
	params.serv_rate = 12;
	params.distr = DISTR_EXP;
	params.script = NULL;

	printf(PREFIX "INFO: CS350 Client Version %d.%d\n", CLIENT_VERSION, CLIENT_SUBVERSION);

	/* Parse command line parameters */
	while((opt = getopt(argc, argv, "d:s:a:n:P:")) != -1) {
		switch (opt) {
		case 's':
			params.serv_rate = strtod(optarg, NULL);
			break;
		case 'a':
			params.arr_rate = strtod(optarg, NULL);
			break;
		case 'n':
			params.num_requests = strtoul(optarg, NULL, 0);
			break;
		case 'd':
			params.distr = strtoul(optarg, NULL, 0) % NUM_DISTR;
			break;
		case 'P':
			parse_req_script(optarg, &params);
			break;
		default: /* '?' */
			fprintf(stderr, PREFIX USAGE_STRING, argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (optind < argc) {
		socket_port = strtol(argv[optind], NULL, 10);
	} else {
		socket_port = 2222;
	}

	printf(PREFIX "INFO: setting client port as: %d\n", socket_port);
	printf(PREFIX "INFO: setting distribution: %d\n", params.distr);

	/* Now onward to create the right type of socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		ERROR_INFO();
		perror(PREFIX "Unable to create socket");
		return EXIT_FAILURE;
	}

	/* Convert INADDR_LOOPBACK into network byte order */
	localhost_addr.s_addr = htonl(INADDR_LOOPBACK);

	/* Time to bind the socket to the right port  */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(socket_port);
	addr.sin_addr = localhost_addr;

	/* Ready to connect! */
	printf(PREFIX "INFO: Initiating connection...\n");
	do {
	    retval = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
	    if (retval == -1) {
		/* Allow 10 more ms for the server to come online */
		usleep(10000);
	    }
	} while (retval == -1 && conn_attempts-- > 0);

	/* If all the attempts fail, just give up. */
	if (retval == -1) {
		ERROR_INFO();
		perror(PREFIX "Unable to initiate connection.");
		return EXIT_FAILURE;
	}

	/* If everything looks good so far, go ahead and generate
	 * packets. */
	params.conn_socket = sockfd;
	handle_connection(&params);

	/* All done. Shutdown socket and exit gracefully. */
	shutdown(sockfd, SHUT_RDWR);
	close(sockfd);
	return EXIT_SUCCESS;

}
