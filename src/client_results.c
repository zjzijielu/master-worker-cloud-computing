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
#define MAXWORKER 3
#define PATH_MAX 256

int job_id;
int result_index = 0; 
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
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
    printf("in file transf thread\n");
    struct sockaddr_in dest; /* Server address */
	struct addrinfo *result;
	int sock;
	int n; 
    int fd1;
    void* buffer = malloc(BUFSZ);
    int worker_port = *(int*) port;
    char* type;
    int fromlen = sizeof(dest);
    char host[NAMESZ];

    gethostname(host, NAMESZ);

    type = (char*)malloc(sizeof(char) * 10);
    strcpy(type, "RESULTS");
    /* create TCP socket */
    if ((sock = socket(AF_INET,SOCK_STREAM,0)) == -1) {
		perror("client socket");
		exit(1);
	}

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

    printf("worker_port : %d\n", worker_port);
    memset((void *)&dest,0, sizeof(dest));
	memcpy((void*)&((struct sockaddr_in*)result->ai_addr)->sin_addr,(void*)&dest.sin_addr,sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(worker_port);

    printf("connect to worker\n");
    /* Establish connection */
	if (connect(sock, (struct sockaddr *) &dest, fromlen) == -1) {
		perror("connect");
		exit(1);
	}

    printf("type: %s\n", type);
    /* send request type to worker */
    if (write(sock, type, strlen(type)) == -1) {
		perror("request confimr recvfrom");
		exit(1);
	}
    check_confirm(sock);

    /* send job id */
    if (write(sock, &job_id, sizeof(int)) == -1) {
		perror("request confimr recvfrom");
		exit(1);
	}
    check_confirm(sock);
    /* make dir */
    pthread_mutex_lock(&mutex);
    char result_dir[128];
    char result_dir_pt2[128];
    sprintf(result_dir_pt2, "/myapp/results/%d/result%d", job_id, result_index);
    strcpy(result_dir, cwd);
    strcat(result_dir, result_dir_pt2);
    // sprintf(result_dir, "/home/oscreader/OS/project/myapp/results/%d/result%d", job_id, result_index);
    printf("result_dir: %s\n", result_dir);
    result_index++;
    pthread_mutex_unlock(&mutex);
    if (mkdir(result_dir, 0777) == -1) {
        perror("open directory");
        exit(1);
    }
    /* copy result.txt */
    n = BUFSZ;
    char file_name[128];
    strcpy(file_name, result_dir);
    strcat(file_name, "/result.txt");
    fd1 = open(file_name, O_RDWR|O_CREAT|O_TRUNC, 0777);
    while(n == BUFSZ) {
        if (read(sock, buffer, BUFSZ) == -1) {
            perror("recev file error");
            exit(1);
        }
        // printf("buffer: %s\n", buffer);
        if ((n = write(fd1, buffer, strlen(buffer))) == -1) {
            perror("write");
            exit(1);
        }
        printf("chars written: %d\n", n);
        printf("write file\n");
    }
    close(fd1);

    shutdown(sock,2);
	close(sock);
    pthread_exit(0);
}

int main(int argc, char* argv[]) {
	struct sockaddr_in dest;
	int sock, i;
	unsigned int fromlen = sizeof(dest);
	struct addrinfo *result;
    char* request_type = "RESULTS";
    int replica;
    int* ports;

    job_id = atoi(argv[1]);

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
       printf("Current working dir: %s\n", cwd);
    }

    if (argc < 3) {
		fprintf(stderr, "results needs (id, path)\n");
		exit(1);
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
    printf("request_type : %s\n", request_type);
	if ( sendto(sock,request_type,strlen(request_type)+1,0,(struct sockaddr *)&dest,
				sizeof(dest)) == -1) {
		perror("sendto");
		exit(1);
	}

    /* send job id */
    if ( sendto(sock,&job_id,sizeof(int),0,(struct sockaddr *)&dest,
				sizeof(dest)) == -1) {
		perror("sendto");
		exit(1);
	}

    printf("sent job\n");
    
    /* Receive replica */
	if ( recvfrom(sock,&replica,sizeof(int),0,0,&fromlen) == -1) {
		perror("recvfrom");
		exit(1);
	}

    printf("receive replica: %d\n", replica);
    pthread_t tid[replica];
    ports = (int*)malloc(sizeof(int) * replica);

    if (replica == -1) {
        printf("Job %d is still RUNNING\n", job_id);
        exit(1);
    } else if (replica == -2) {
        printf(" Job %d doesn't exist\n", job_id);
        exit(1);
    }

    /* make dir for the result of job_id */
    char dir_name[128]; 
    char dir_name_pt2[128];
    sprintf(dir_name_pt2, "/myapp/results/%d", job_id);
    strcpy(dir_name, cwd);
    strcat(dir_name, dir_name_pt2);
    // sprintf(dir_name, "/home/oscreader/OS/project/myapp/results/%d", job_id);
    if (mkdir(dir_name, 0777) == -1) {
        perror("open directory");
        exit(1);
    }

    /* Receive ports */
    if ( recvfrom(sock,ports,sizeof(int) * MAXWORKER,0,0,&fromlen) == -1) {
		perror("recvfrom");
		exit(1);
	}

    for (i = 0; i < replica; i++) {
        pthread_create(&tid[i], 0, file_transf, &ports[i]);
    }

    for (i = 0; i < replica; i++) {
        pthread_join(tid[i], NULL);
    }

    printf("job %d results retrieved\n", job_id);

    return 0;
}