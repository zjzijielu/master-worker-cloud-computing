#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#define MASTERSERV 1111
#define FILESIZE 1024
#define QUEUE_SIZE 3 // max number of requests served at the same time
#define BUFSIZE 1024
#define FILENAMESZ 128
#define MAXSRC 5
#define TAILMSG 128
#define WORKERNUM 3
#define MAXJOBS 16
#define NAMESZ 128

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_jobid = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_deploy = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_deploy = PTHREAD_COND_INITIALIZER;

int* udp_ports;
int* tcp_ports;
int* jobs;
int* job_status;
int* job_replica;
int* job_workers;
int mysc;
int flag = 0;
int job_id = 0;
int deployed[WORKERNUM];
int worker_num = 0;
int port_curr = 0;


typedef struct {
    struct sockaddr *tgtsock;
    unsigned int tgtlen;
} socket_info;


int* find_aval_ports(int n) {
    int i;
    int* ports = (int *)malloc(sizeof(int) * n);
    for (i = 0; i < n; i++) {
        ports[i] = tcp_ports[port_curr];
        port_curr = (port_curr + 1) % WORKERNUM;
    }
    return ports;
}

void* deploy_request(void* info) {
    
    int available = 1;
    int replica, i;
    socklen_t fromlen = sizeof(struct sockaddr_in);
    int* aval_ports;
    socket_info* sc_info = (socket_info*)info;
    printf("received deploy request\n");

    /* Send confirm */
	if ( sendto(mysc,&available,sizeof(int),0,(struct sockaddr *)sc_info->tgtsock,
				sizeof(struct sockaddr_in)) == -1) {
		perror("sendto");
		exit(1);
	}

    /* receive replica */
    pthread_mutex_lock(&mutex);
    // printf("wait for replica\n");
    if ( recvfrom(mysc,&replica,sizeof(int),0,0, &fromlen) == -1) {
		perror("recvfrom");
		exit(1);
	}
    pthread_cond_signal(&cond);
    flag = 1;
    aval_ports = find_aval_ports(replica);
    pthread_mutex_unlock(&mutex);
    // printf("replica requested: %d\n", replica);

    /* send available ports */
    for (i = 0; i < replica; i++) {
        printf("ports[%d]: %d\n", i, aval_ports[i]);
    }
    if ( sendto(mysc, aval_ports,sizeof(int) * replica,0,(struct sockaddr *)sc_info->tgtsock,
				sizeof(struct sockaddr_in)) == -1) {
		perror("sendto");
		exit(1);
	}

    /* send job id */
    pthread_mutex_lock(&mutex_jobid);
    printf("send job id to client\n");
    if ( sendto(mysc, &job_id,sizeof(int),0,(struct sockaddr *)sc_info->tgtsock,
				sizeof(struct sockaddr_in)) == -1) {
		perror("sendto");
		exit(1);
	}
    printf("sent job id\n");
    /* store job id */
    int index = 0;
    printf("jobs[0]: %d\n", jobs[index]);
    printf("jobs_status[0]: %d\n", job_status[index]);
    printf("jobs_replica[0]: %d\n", job_replica[index]);

    while (jobs[index] != -1) {
        index = (index + 1) % MAXJOBS;
        printf("current index: %d\n", index);
    }
    jobs[index] = job_id;
    job_status[index] = 0;
    job_replica[index] = replica;
    printf("job_replica: %d\n", replica);
    job_id++;
    for (i = 0; i < replica; i++) {
        job_workers[i * MAXJOBS + index] = aval_ports[i];
    }
    pthread_mutex_unlock(&mutex_jobid);
    // printf("after sending job id to client\n");

    pthread_mutex_lock(&mutex_deploy);
    // printf("lock to change deploy\n");
    for (i = 0; i < WORKERNUM; i++) {
        deployed[i] = 1;
    }
    pthread_cond_broadcast(&cond_deploy);
    // printf("sent cond_deploy signal\n");
    pthread_mutex_unlock(&mutex_deploy);

    // printf("finish sending ports\n");
    printf("leaving deploy request\n");
    pthread_exit(0);
}

void* status_request(void* info) {
    socklen_t fromlen = sizeof(struct sockaddr_in);
    socket_info* sc_info = (socket_info*)info;
    int id, i;
    char* status;

    status = (char*)malloc(sizeof(char) * 10);
    // printf("fromlen : %ud\n", sc_info->tgtlen);

    /* receive job id */
	if ( recvfrom(mysc,&id,sizeof(int),0,0,&fromlen) == -1) {
		perror("sendto");
		exit(1);
	}

    /* check if id is jobs list */
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i] == id) {
            if (job_replica[i] == job_status[i]) {
                strcpy(status, "COMPLETED");
                if ( sendto(mysc, status,sizeof(char) * 10,0,(struct sockaddr *)sc_info->tgtsock,
                    sizeof(struct sockaddr_in)) == -1) {
                    perror("sendto");
                    exit(1);
                } 
            } else {
                strcpy(status, "RUNNING");
                if ( sendto(mysc, status,sizeof(char) * 10,0,(struct sockaddr *)sc_info->tgtsock,
                    sizeof(struct sockaddr_in)) == -1) {
                    perror("sendto");
                    exit(1);
                }
            }
            break;
        }
    }

    if (i == MAXJOBS) {
        strcpy(status, "INVALID");
        if ( sendto(mysc, status,sizeof(char) * 10,0,(struct sockaddr *)sc_info->tgtsock,
				sizeof(struct sockaddr_in)) == -1) {
            perror("sendto");
            exit(1);
        }
    }
    pthread_mutex_lock(&mutex);
    flag = 1;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    pthread_exit(0);
}

void* results_request(void* info) {
    printf("in results request thread\n");
    socklen_t fromlen = sizeof(struct sockaddr_in);
    socket_info* sc_info = (socket_info*)info;
    int id, i, j;
    int replica;
    int* ports;

    /* receive job id */
	if ( recvfrom(mysc,&id,sizeof(int),0,0,&fromlen) == -1) {
		perror("sendto");
		exit(1);
	}

    printf("received job: %d\n", id);

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i] == id) {
            if (job_replica[i] == job_status[i]) {
                replica = job_replica[i];
                    ports = (int*)malloc(sizeof(int) * replica);
                for (j = 0; j < replica; j++) {
                    ports[j] = job_workers[j * MAXJOBS + i];
                    printf("port : %d\n", ports[j]);
                }
                /* send replica */
                if ( sendto(mysc, &replica,sizeof(int),0,(struct sockaddr *)sc_info->tgtsock,
                    sizeof(struct sockaddr_in)) == -1) {
                    perror("sendto");
                    exit(1);
                } 
                /* send ports */
                if ( sendto(mysc, ports,sizeof(int) * replica,0,(struct sockaddr *)sc_info->tgtsock,
                    sizeof(struct sockaddr_in)) == -1) {
                    perror("sendto");
                    exit(1);
                } 
                pthread_mutex_lock(&mutex);
                jobs[i] = -1;
                job_replica[i] = 0;
                job_status[i] = 0;
                for (j = 0; j < WORKERNUM; j++) {
                    job_workers[j * MAXJOBS + i] = 0;
                } 
                pthread_mutex_unlock(&mutex);
                printf("removed all job info\n");
            } else {
                replica = -1;
                if ( sendto(mysc, &replica,sizeof(int),0,(struct sockaddr *)sc_info->tgtsock,
                    sizeof(struct sockaddr_in)) == -1) {
                    perror("sendto");
                    exit(1);
                }
            }
            break;
        }
    }

    if (i == MAXJOBS) {
        replica = -2;
        if ( sendto(mysc, &replica,sizeof(int),0,(struct sockaddr *)sc_info->tgtsock,
				sizeof(struct sockaddr_in)) == -1) {
            perror("sendto");
            exit(1);
        }
    }

    pthread_mutex_lock(&mutex);
    flag = 1;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    printf("leaving request thread\n");
    pthread_exit(0);
}

void* connect_worker(void* portnum) {
    int local_worker_num; 
    pthread_mutex_lock(&mutex);
    local_worker_num = worker_num;
    worker_num++;
    pthread_mutex_unlock(&mutex);
    int udpport = *(int*)portnum;
    int reply, i;
	struct sockaddr_in dest;
	int sock;
	socklen_t fromlen = sizeof(dest);
	char  message[TAILMSG];
	struct addrinfo *result;
    char host[NAMESZ];

    printf("master UDP client> try to connect to worker %d\n", udpport);
    gethostname(host, NAMESZ);
    
    /* Create socket */
	if ((sock = socket(AF_INET,SOCK_DGRAM,0)) == -1) {
		perror("socket");
		exit(1);
	}

    // printf("master UDP socket created\n");

    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_ADDRCONFIG | AI_CANONNAME;
    hints.ai_protocol = 0;
	
	if (getaddrinfo(host, 0, &hints, &result) != 0) {
		perror("getaddrinfo");
		exit(EXIT_FAILURE);
    }

    // printf("address - %s\n", inet_ntoa(((struct sockaddr_in*)result->ai_addr)->sin_addr));
	// printf("canon name - %s\n", result->ai_canonname);

    memset((void *)&dest,0, sizeof(dest));
	memcpy((void*)&((struct sockaddr_in*)result->ai_addr)->sin_addr,(void*)&dest.sin_addr,sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(udpport);

    /* Write message ...*/
	strcpy(message,"Message from master");

    /* Send message */
	if ( sendto(sock,message,strlen(message)+1,0,(struct sockaddr *)&dest,
				sizeof(dest)) == -1) {
		perror("sendto");
		exit(1);
	}

    /* Receive reply */
	if ( recvfrom(sock,&reply,sizeof(reply),0,0,&fromlen) == -1) {
		perror("recvfrom");
		exit(1);
	}

    if (reply == 1) {
        printf("Worker @ %d available\n", udpport);
    }

    while (1) {
        int job_id;
        pthread_mutex_lock(&mutex_deploy);
        // printf("wait for worker completes deployment\n");
        while(!deployed[local_worker_num]) {
            pthread_cond_wait(&cond_deploy, &mutex_deploy);
        }
        printf("wait for %d worker's notification\n", udpport);
        if ( recvfrom(sock,&job_id,sizeof(job_id),0,0,&fromlen) == -1) {
            perror("recvfrom");
            exit(1);
        }
        deployed[local_worker_num] = 0;

        for (i = 0; i < MAXJOBS; i++) {
            if (jobs[i] == job_id) {
                printf("job_status[%d]: %d\n", i, job_status[i]);
                job_status[i]++;
                break;
            }
        }
        
        int j;
        pthread_mutex_unlock(&mutex_deploy);
        // printf("job[%d] : %d \n", i, jobs[i]);
        // printf("job_status[%d] : %d\n", i, job_status[i]);
        // printf("job_replica[%d] : %d\n", i, job_replica[i]);
        // printf("receive a job completion msg\n");
        // printf("job id %d is done\n", job_id);
    }

    close(sock);
}

int main(int argc, char* argv[]) {
    char* line = NULL;
    int i = 0;
    int num = 0;
    size_t len = 0;
    ssize_t read;
    FILE * fp;
    pthread_t worker_connect[WORKERNUM];
    jobs = (int*)malloc(sizeof(int) * MAXJOBS);
    job_status = (int*)malloc(sizeof(int) * MAXJOBS);
    job_replica = (int*)malloc(sizeof(int) * MAXJOBS);
    job_workers = (int*)malloc(sizeof(int) * MAXJOBS * WORKERNUM);
    for (i = 0; i < MAXJOBS; i++) {
        jobs[i] = -1;
        job_status[i] = 0;
        job_replica[i] = 0;
    }

    for (i = 0; i < MAXJOBS * WORKERNUM; i++) {
        job_workers[i] = 0;
    }

    udp_ports = (int*)malloc(sizeof(int) * WORKERNUM);
    tcp_ports = (int*)malloc(sizeof(int) * WORKERNUM);

    /* read port value */
    fp = fopen("/home/oscreader/OS/project/cloud-nodes.txt", "r");
    if (fp == NULL) {
        perror("read cloud-nodes.txt");
        exit(1);
    }
    
    i = 0;
    while ((read = getline(&line, &len, fp)) != -1 ) {
        // printf("%s", line);
        if (i % 2 == 0) {
            udp_ports[i / 2] = atoi(line);
        } else {
            tcp_ports[i / 2] = atoi(line);
        }
        // printf("%d\n", atoi(line));
        i++;
    }
    fclose(fp);
    
    for (i = 0; i < WORKERNUM; i++) {
        deployed[i] = 0;
    }

    /* check if the workers are available */
    for (i = 0; i < WORKERNUM; i++) {
        pthread_create(&worker_connect[i], 0, connect_worker, &udp_ports[i]);
    }

    /* start to receive request */
    struct sockaddr_in sin;  /* Server address */
	struct sockaddr_in exp;	/* Client address */
    socklen_t fromlen = sizeof(exp);
	char message[TAILMSG];
    pthread_t tid[QUEUE_SIZE];

    /* Create socket */
	if ((mysc = socket(AF_INET,SOCK_DGRAM,0)) < 0) {
		perror("creation socket");
		exit(1);
	}

	// printf("socket created\n");
    /* Prepare server name and bind to DNS service */
	memset((void*)&sin, 0, sizeof(sin));
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(MASTERSERV);
	sin.sin_family = AF_INET;
	// printf("Server name: %s\n", (struct sockaddr *)&sin.sa_data);
	if (bind(mysc,(struct sockaddr *)&sin,sizeof(sin)) < 0) {
		perror("bind");
		exit(2);
	}

    socket_info sc_info;

    while (1) {
        /* create a new thread whenever receive a request */
        printf("wait to receive message \n");
        if (num > 0) {
            pthread_mutex_lock(&mutex);
            while(!flag) {
                pthread_cond_wait(&cond, &mutex);
            }
            if ( recvfrom(mysc, message, sizeof(message), 0, (struct sockaddr *) &exp, &fromlen) == -1) { 
                perror("recvfrom");
                exit(2);
            }
            flag = 0;
            pthread_mutex_unlock(&mutex);
        } else {
            if ( recvfrom(mysc, message, sizeof(message), 0, (struct sockaddr *) &exp, &fromlen) == -1) { 
                perror("recvfrom");
                exit(2);
            }
        }
        
        num++;
        sc_info.tgtsock = &exp;
        sc_info.tgtlen = fromlen;
        
        if (strcmp(message, "DEPLOY")== 0) {
            pthread_create(&tid[i], 0, deploy_request, &sc_info);
        } else if (strcmp(message, "STATUS") == 0) {
            pthread_create(&tid[i], 0, status_request, &sc_info);
        } else if (strcmp(message, "RESULTS") == 0) {
            pthread_create(&tid[i], 0, results_request, &sc_info);
        }
        
        i = (i + 1) % QUEUE_SIZE;

        // printf("received request: %s\n", message);
    }
    
	return(0);

}