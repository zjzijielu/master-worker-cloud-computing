#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define MASTERSERV 1111
#define FILENAMESZ 128
#define NAMESZ 128
#define MAXSRC 5
#define BUFSZ 1024

int main(int argc, char* argv[]) {
	struct sockaddr_in dest;
	int sock;
	unsigned int fromlen = sizeof(dest);
	struct addrinfo *result;
    char* request_type = "STATUS";
    int id;
    char* status;

    status = (char*)malloc(sizeof(char) * 10);

    if (argc < 2) {
		fprintf(stderr, "Status needs job id\n");
		exit(1);
	}
    
    id = atoi(argv[1]);
	// printf("job: %d\n", source_num);

    /* Server name and port nb are passed as arguments */
	if ((sock = socket(AF_INET,SOCK_DGRAM,0)) == -1) {
		perror("socket");
		exit(1);
	}

    /* Find server address and use it to fill in structure dest */
	
	struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_ADDRCONFIG | AI_CANONNAME;
    hints.ai_protocol = 0;
	
	if (getaddrinfo(argv[1], 0, &hints, &result) != 0) {
		perror("getaddrinfo");
		exit(EXIT_FAILURE);
    }

    memset((void *)&dest,0, sizeof(dest));
	memcpy((void*)&((struct sockaddr_in*)result->ai_addr)->sin_addr,(void*)&dest.sin_addr,sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(MASTERSERV);

    /* Send request type */
	if ( sendto(sock,request_type,strlen(request_type)+1,0,(struct sockaddr *)&dest,
				sizeof(dest)) == -1) {
		perror("sendto");
		exit(1);
	}

    /* Send job id */
    if ( sendto(sock,&id,sizeof(int),0,(struct sockaddr *)&dest,
				sizeof(dest)) == -1) {
		perror("sendto");
		exit(1);
	} 

    /* receive status */
    if ( recvfrom(sock,status, sizeof(char) * 10,0,0,&fromlen) == -1) {
		perror("recvfrom");
		exit(1);
	}

    printf("received status: %s\n", status);


    return 0;
} 