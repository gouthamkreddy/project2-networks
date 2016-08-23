#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>

#include "proxy_parse.h"


#define MAX_PENDING 10						
#define MAX_SIZE    4096

int child_process_count = 0;

int main(int argc, char * argv[]) {
  	int sockid, new_sockid;   					
	struct sockaddr_in sin;						
	int status;
	pid_t  pid;
	char send_buf[MAX_SIZE], recv_buf[MAX_SIZE];						
	socklen_t len;
	
	/*--- Checking number of arguments ---*/
  	if(argc!=2){
		fprintf(stderr, "Run using ./proxy PORT\n");
		return 1;
	}

	/*--- Setting values of sockaddr_in ---*/
  	bzero((char *)&sin, sizeof(sin));
  	bzero((char *)send_buf, MAX_SIZE);			
  	bzero((char *)recv_buf, MAX_SIZE);			
	sin.sin_family = AF_INET;
	sin.sin_port = htons(atoi(argv[1]));
	sin.sin_addr.s_addr = htons(INADDR_ANY);

	/*--- Creating a socket ---*/
	if((sockid = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("Error: socket");
		return 1;
	}

	/*--- Reusing same port ---*/
	int enable = 1;
    if (setsockopt(sockid, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    	perror("setsockopt(SO_REUSEADDR) failed");

	/*--- Binding socket with ip and port ---*/
	if(bind(sockid, (struct sockaddr *)&sin, sizeof(sin)) < 0){
		perror("Error: bind");
		return 1;
	}

	/*--- Socket listening for a connection ---*/
	if((status = listen(sockid, MAX_PENDING)) != 0){
		perror("Error: listen");
		return 1;
	}

	printf("Proxy Serving Requests on Port %d\n", atoi(argv[1]));

	while(1)
	{
		/*--- Waiting to accept connection from a client ---*/
		if((new_sockid = accept(sockid, (struct sockaddr *)&sin, &len)) < 0)
		{
			perror("Error: accept");
			continue;
		} 
		printf("New Connection Created\n");

		pid = fork();
		child_process_count = child_process_count + 1;
		if(pid == 0)
		{	
			close(sockid);
			FILE *fsp = fdopen(new_sockid, "r+b");
			while(1)
			{
				printf("waiting---");
				bzero((char *)recv_buf, MAX_SIZE);
				status = recv(new_sockid, recv_buf, MAX_SIZE, 0);
				if(status < 0)
				{
					perror("Error: recv\n");	
					// write_response(new_sockid, 500, 0);
					continue;
				} 
				else if(status == 0)
				{
					printf("Client Disconnected\n");
					break;
				} 

				printf("%s\n", recv_buf);

				/*--- Parsing ---*/
				struct ParsedRequest * parse;
				printf("size=%ld\n", sizeof(struct ParsedRequest));

				parse = (struct ParsedRequest *)malloc(1*sizeof(struct ParsedRequest));
				printf("size=%ld %ld\n", sizeof(parse), sizeof(struct ParsedRequest));

				printf("here\n");
				int ret = ParsedRequest_parse(parse, recv_buf, strlen(recv_buf));
				printf("here\n");
				
				printf("ret - %d\n", ret);
				printf("%s\n", parse->port);
				

				/*--- Making request to server ---*/
				
				/*--- Receiving response from server ---*/

				/*--- Sending response to client ---*/
		  		
			}

			fclose(fsp);
			exit(0);
			child_process_count = child_process_count - 1;
			printf("Connection Closed\n");
		}
		
    	close(new_sockid);	
	}

  return 0;
}
