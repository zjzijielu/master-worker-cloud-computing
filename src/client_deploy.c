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
#include <pthread.h>

#define MASTERSERV 1111
#define FILENAMESZ 256
#define NAMESZ 128
#define MAXSRC 5
#define BUFSZ 1024
#define MAXWORKER 2
#define PATH_MAX 256

char **src_files;
int source_num;
int job_id;
char cwd[PATH_MAX];

void check_confirm(int sock) {
	int reply;
	if (read(sock, &reply,sizeof(reply)) == -1) {
		perror("request confirm recvfrom");
		exit(1);
	}

    if (reply == 0) {
        perror("master not available");
        exit(1);
    }
	// printf("after confirm\n");
}

void* file_transf(void* port) {
    struct sockaddr_in dest; /* Server address */
	struct addrinfo *result;
	int sock;
	int i;
    int fd1, n = 1;
    void* buffer = malloc(BUFSZ);
    int worker_port = *(int*) port;
    char* type = "DEPLOY";
    char* host[NAMESZ];
    // printf("worker_port: %d\n", worker_port);
    
    gethostname(host, NAMESZ);
    
    /* create TCP socket */
    if ((sock = socket(AF_INET,SOCK_STREAM,0)) == -1) {
		perror("client socket");
		exit(1);
	}
    // printf("tcp socket created\n");

    /* Find worker address and use it to fill in structure dest */
	
	struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG | AI_CANONNAME;
    hints.ai_protocol = 0;

    if (getaddrinfo(host, 0, &hints, &result) != 0) {
		perror("getaddrinfo");
		exit(EXIT_FAILURE);
    }

    memset((void *)&dest,0, sizeof(dest));
	memcpy((void*)&((struct sockaddr_in*)result->ai_addr)->sin_addr,(void*)&dest.sin_addr,sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(worker_port);

    /* Establish connection */
	if (connect(sock, (struct sockaddr *) &dest, sizeof(dest)) == -1) {
		perror("connect");
		exit(1);
	}
    printf("connected to worker\n");

    /* send request type to worker */
    if (write(sock, type, sizeof(char)*10) == -1) {
		perror("request confimr recvfrom");
		exit(1);
	}
    check_confirm(sock);
    printf("sent request: %s\n", type);
    
    /* send job id */
    if (write(sock, &job_id, sizeof(int)) == -1) {
		perror("request confimr recvfrom");
		exit(1);
	}
    check_confirm(sock);

    /* Send number of files */
	if (write(sock, &source_num, sizeof(int)) == -1) {
		perror("request confimr recvfrom");
		exit(1);
	}
    check_confirm(sock);
    printf("source_num: %d\n", source_num);
    printf("after sending file num\n");
    
    /* Send file path */
	for (i = 0; i < source_num; i++) {
		if (write(sock, src_files[i], sizeof(char) * FILENAMESZ) == -1) {
			perror("write");
			exit(1);
		}
	}
    check_confirm(sock);
    printf("after sending file path\n");

    /* Send file */
    for (i = 0; i < source_num; i++) {
        n = 1;
        printf("send file: %s\n", src_files[i]);
        fd1 = open(src_files[i], O_RDWR, 0600);
        if (fd1 == -1) {
            perror("open");
            exit(1);
        }

        while(n > 0) {
            n = read(fd1, buffer, BUFSZ);
            if (write(sock, buffer, n) == -1) {
                perror('send');
            }
        }

        check_confirm(sock);
    }

    printf("done sending file\n");

    shutdown(sock,2);
	close(sock);

}

int main(int argc, char* argv[]) {
    int reply;
	struct sockaddr_in dest;
	int sock;
	unsigned int fromlen = sizeof(dest);
	struct addrinfo *result;
    char* request_type = "DEPLOY";
    int replica;
    int i;

    if (argc < 3) {
		fprintf(stderr, "Deploy needs (replica, makefile, source code)\n");
		exit(1);
	}

    replica = atoi(argv[1]);
    source_num = argc - 2;

    src_files = (char**)malloc(sizeof(char*) * source_num);
    for (i = 0; i < source_num; i++) {
	    src_files[i] = malloc(sizeof(char) * FILENAMESZ);
	}

    for (i = 0; i < source_num; i++) {
		strcpy(src_files[i], argv[2+i]);
        printf("source files: %s\n", src_files[i]);
    }

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

    /* Receive reply */
	if ( recvfrom(sock,&reply,sizeof(reply),0,0,&fromlen) == -1) {
		perror("recvfrom");
		exit(1);
	}

    printf("replica : %d\n", replica);
    /* send replica */
    if ( sendto(sock,&replica,sizeof(int),0,(struct sockaddr *)&dest,
				sizeof(dest)) == -1) {
		perror("sendto");
		exit(1);
	}

    int* aval_ports = (int *)malloc(sizeof(int) * replica);
    /* receive available worker ports */
    if ( recvfrom(sock, aval_ports,sizeof(int) * replica,0,0,&fromlen) == -1) {
		perror("recvfrom");
		exit(1);
	}

    for (i = 0; i < replica; i++) {
        printf("port %d: %d\n", i, aval_ports[i] );
    }

    if ( recvfrom(sock,&job_id,sizeof(int),0,0,&fromlen) == -1) {
		perror("recvfrom");
		exit(1);
	}
    
    /* create pthread to file exchange with workers */
    
    pthread_t tid[replica];

    for (i = 0; i < replica; i++) {
        pthread_create(&tid[i], 0, file_transf, &aval_ports[i]);
    }
    
    for (i = 0; i < replica; i++) {
        pthread_join(tid[i], NULL);
    }

	close(sock);

    printf("Job deployment successful - JOB TICKET %d\n", job_id);

    return 0;
} 