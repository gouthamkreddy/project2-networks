#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>

#include "proxy_parse.h"


#define MAX_PENDING 10						
#define MAX_SIZE    4096

void response_500(int sockid){
	char buffer[4096];

	strcpy(buffer, "HTTP/1.0 500 Internal Server Error\r\n");
  	write(sockid, buffer, strlen(buffer));
  	strcpy(buffer, "Content-length: 117\r\n");
  	write(sockid, buffer, strlen(buffer));
	strcpy(buffer, "Connection: close\r\n");
	write(sockid, buffer, strlen(buffer));
  	strcpy(buffer, "Content-Type: text/html\r\n\r\n");
	write(sockid, buffer, strlen(buffer));
  	strcpy(buffer, "<html>\n<head>\n<title>Internal Server Error</title>\n</head>\r\n");
  	write(sockid, buffer, strlen(buffer));
  	strcpy(buffer, "<body>\n<p>500 Internal Server Error</p>\n</body>\n</html>\r\n");
  	write(sockid, buffer, strlen(buffer));
}

int main(int argc, char * argv[]) {
  	int sockid, new_sockid;   					
	struct sockaddr_in sin;						
	int status;
	pid_t  pid;
	char send_buf[MAX_SIZE], recv_buf[MAX_SIZE], recv1_buf[MAX_SIZE];						
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

	printf("Proxy Running on Port %d\n", atoi(argv[1]));

	while(1)
	{
		/*--- Waiting to accept connection from a client ---*/
		if((new_sockid = accept(sockid, (struct sockaddr *)&sin, &len)) < 0)
		{
			perror("Error: accept");
			continue;
		} 
		pid = fork();
		if(pid == 0)
		{
			// close(sockid);
			printf("New Connection Created:::waiting\n");
			bzero((char *)recv_buf, MAX_SIZE);
			
			status = 0;
			while(!strstr(recv_buf,"\r\n\r\n")){
				bzero((char *)recv1_buf, MAX_SIZE);
				status = status + recv(new_sockid, recv1_buf, MAX_SIZE-1, 0);
				strcat(recv_buf, recv1_buf);
			}

			printf("status: %d size: %ld\n%s\n", status, strlen(recv_buf), recv_buf);
			if(status < 0)
			{
				perror("Error: recv\n");	
				continue;
			} 
			else if(status == 0)
			{
				printf("Client Disconnected\n");
				continue;
			} 
			// strcat(recv_buf, "\r\n");

			/*--- Parsing ---*/
			struct ParsedRequest * parsed_req;
			parsed_req = ParsedRequest_create();
			int ret = ParsedRequest_parse(parsed_req, recv_buf, strlen(recv_buf));
			if (ret == -1)
			{
				response_500(new_sockid);
				printf("ParsedRequest_parse failed\n");
				close(new_sockid);
				printf("Connection Closed\n");
				exit(0);
			}

			if (parsed_req->port == NULL)
				parsed_req->port = (char*)"80";

			/*--- Making request to server ---*/
			struct sockaddr_in sin1;				
			int sockid1;
			/*--- Setting values of sockaddr_in ---*/
			bzero((char *)&sin1, sizeof(sin1));
			sin1.sin_family = AF_INET;
			sin1.sin_port = htons(atoi(parsed_req->port));
			struct hostent *host_1 = gethostbyname(parsed_req->host);
			bcopy((char *)host_1->h_addr, (char *)&sin1.sin_addr.s_addr, host_1->h_length);
			/*--- Creating a socket to connect to server ---*/
			if ((sockid1 = socket(AF_INET, SOCK_STREAM, 0)) < 0){
				perror("Error: socket");
				response_500(new_sockid);
				close(new_sockid);
				printf("Connection Closed\n");
				exit(0);
			}
			/*--- Connecting to Server Socket ---*/
			if (connect(sockid1, (struct sockaddr *)&sin1, sizeof(sin1)) < 0){
				perror("Error: connect");
				close(sockid1);
				response_500(new_sockid);
				close(new_sockid);
				printf("Connection Closed\n");
				exit(0);
			}
			
			/*--- send_buf to send to server ---*/
			bzero((char *)send_buf, MAX_SIZE);
			snprintf(send_buf, MAX_SIZE, "GET %s HTTP/1.0\r\n", parsed_req->path);
			bzero((char *)recv_buf, MAX_SIZE);
			sprintf(recv_buf, "Host: %s\r\n", parsed_req->host);
			strcat(send_buf, recv_buf);
			for (unsigned int i = 0; i < parsed_req->headersused; ++i)
			{
				bzero((char *)recv_buf, MAX_SIZE);
				if(!strcmp(parsed_req->headers[i].key, "Connection"))
					continue;
				sprintf(recv_buf, "%s: %s\r\n", parsed_req->headers[i].key, parsed_req->headers[i].value);
				strcat(send_buf, recv_buf);
			}
			strcat(send_buf, "Connection: close\r\n\r\n");
			send(sockid1, send_buf, strlen(send_buf), 0);
				
			/*--- Receiving and Sending response ---*/
			while(1)
			{
				bzero((char *)recv_buf, MAX_SIZE);
				int length = recv(sockid1, recv_buf, MAX_SIZE, 0);
				if (length <= 0) break;
				send(new_sockid, recv_buf, length, 0);
			}

			close(sockid1);
			close(new_sockid);
			printf("Child Process Exited\n");
			exit(0);
		}
    	close(new_sockid);	
	}

  return 0;
}
