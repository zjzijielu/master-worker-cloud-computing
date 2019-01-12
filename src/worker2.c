#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>
#include <time.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>

#define UDPPORT 5678
#define TCPPORT 5679
#define FILENAMESZ 256
#define NAMESZ 128
#define MAXSRC 5
#define TAILMSG 128
#define QUEUE_SIZE 5
#define BUFSZ 1024
#define PATH_MAX 256

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int executed = 0;
int id_to_send = -1;
char cwd[PATH_MAX];

void send_confirm(int sock) {
    int confirm = 1;
    if (write(sock, &confirm, sizeof(int)) == -1) {
        perror("write");
        exit(2);
    }
    // printf("after send confirm\n");
}

void* result_request (void* sock_) {
    int fd1;
    int sock = *(int*) sock_;
    int job_id;
    int n;
    char* buffer = (char*)malloc(BUFSZ);
    
    /* receive job id */
    if (read(sock, &job_id, sizeof(int)) < 0) {
        perror("request type read error");
        exit(1);
    }
    printf("received job id: %d\n", job_id);
    send_confirm(sock);
    

    /* copy file */
    char file_name[256];
    char file_name_pt2[128];
    sprintf(file_name_pt2, "/worker2/%d/myapp/result/result.txt", job_id);
    strcpy(file_name, cwd);
    strcat(file_name, file_name_pt2);
    // printf("file_name: %s\n", file_name);
    // sprintf(file_name, "/home/oscreader/OS/project/worker2/%d/myapp/result/result.txt", job_id);
    n = BUFSZ;
    fd1 = open(file_name, O_RDWR, 0600);
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

    printf("done file writing\n");

    int pid = fork();
    if (pid == 0) {
        char dir_name[256];
        char dir_name_pt2[128];
        sprintf(dir_name_pt2, "/worker2/%d", job_id);
        strcpy(dir_name, cwd);
        strcat(dir_name, dir_name_pt2);
        // printf("dir_name: %s\n", dir_name);
        // sprintf(dir_name, "/home/oscreader/OS/project/worker2/%d", job_id);
        execl("/bin/rm", "rm", "-rf", dir_name, NULL);
    }
    wait(NULL);
    pthread_exit(0);
}

void* exec_request(void* sock_){
    int source_num;
    int fd1, i;
    int sock = *(int*) sock_;
    int job_id;
    int n;
    char* buffer = (char*)malloc(BUFSZ);

    printf("received exec request\n");
    /* receive job is */
    if (read(sock, &job_id, sizeof(int)) < 0) {
        perror("request type read error");
        exit(1);
    }
    printf("received job_id: %d\n", job_id);
    send_confirm(sock);

    /* receive source file num */
    if (read(sock, &source_num, sizeof(int)) < 0) {
        perror("request type read error");
        exit(1);
    }
    printf("received source_num : %d\n", source_num);
    send_confirm(sock);


    /* Get file name */
    char* f_names[source_num];
    for (i = 0; i < source_num; i++) {
        f_names[i] = (char*)malloc(sizeof(char) * FILENAMESZ);
    }
    for (i = 0; i < source_num; i++) {
        if (read(sock, f_names[i], sizeof(char) * FILENAMESZ) < 0) {
            perror("read");
            exit(1);
        }
        printf("received file: %s\n", f_names[i]);
    }
    send_confirm(sock);

    /* Create directory and copy file */ 
    // char dir_name[128];   
    char src_dir[128];
    char bin_dir[128];   
    char obj_dir[128]; 

    char dir_name[256];
    char dir_name_pt2[128];
    sprintf(dir_name_pt2, "/worker2/%d", job_id);
    strcpy(dir_name, cwd);
    strcat(dir_name, dir_name_pt2);
    // printf("dir_name: %s\n", dir_name);
    // sprintf(dir_name, "/home/oscreader/OS/project/worker2/%d", job_id);
    if (mkdir(dir_name, 0777) == -1) {
        perror("open directory root");
        exit(1);
    }
    strcat(dir_name, "/myapp");
    if (mkdir(dir_name, 0777) == -1) {
        perror("open directory myapp");
        exit(1);
    }

    strcpy(src_dir, dir_name);
    strcat(src_dir, "/src");
    if (mkdir(src_dir, 0777) == -1) {
        perror("open directory src");
        exit(1);
    }

    strcpy(obj_dir, dir_name);
    strcat(obj_dir, "/obj");
    if (mkdir(obj_dir, 0777) == -1) {
        perror("open directory obj");
        exit(1);
    }

    strcpy(obj_dir, dir_name);
    strcat(obj_dir, "/result");
    if (mkdir(obj_dir, 0777) == -1) {
        perror("open directory result");
        exit(1);
    }

    strcpy(bin_dir, dir_name);
    strcat(bin_dir, "/bin");
    if (mkdir(bin_dir, 0777) == -1) {
        perror("open directory bin");
        exit(1);
    }

    // char* file_dir; 
    for (i = 0; i < source_num; i++) {
        n = BUFSZ;
        // file_dir = (char*)malloc(sizeof(char) * 128);
        buffer = (char*)malloc(BUFSZ);
        char file_dir[256];
        char file_dir_pt2[128];
        sprintf(file_dir_pt2, "/worker2/%d/", job_id);
        strcpy(file_dir, cwd);
        strcat(file_dir, file_dir_pt2);
        // printf("file_dir: %s\n", file_dir);
        // sprintf(file_dir, "/home/oscreader/OS/project/worker2/%d/", job_id);
        strcat(file_dir, f_names[i]);
        // printf("file_name : %s\n", file_dir);
        fd1 = open(file_dir, O_RDWR|O_CREAT|O_TRUNC, 0600);
        while (n == BUFSZ) {
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
        send_confirm(sock);
        close(fd1);
    }

    /* fork to compile and execute */
    int pid;
    pid = fork();
    
    if (pid == 0) {
        // char* make_addr;
        // make_addr = (char*)malloc(sizeof(char) * FILENAMESZ);
        char make_addr[256];
        char make_addr_pt2[128];
        sprintf(make_addr_pt2, "/worker2/%d/myapp", job_id);
        strcpy(make_addr, cwd);
        strcat(make_addr, make_addr_pt2);
        // printf("make_addr: %s\n", make_addr);
        // sprintf(make_addr, "/home/oscreader/OS/project/worker2/%d/myapp", job_id);
        execl("/usr/bin/make", "make", "-C", make_addr,  NULL);
        perror("execl");
    }
    wait(NULL);
    printf("%d> done compiling\n", TCPPORT);
    pid = fork();

    if (pid == 0) {
        // char* exec_addr;
        // char* store_addr;
        // exec_addr = (char*)malloc(sizeof(char) * FILENAMESZ);
        // store_addr = (char*)malloc(sizeof(char) * FILENAMESZ);
        char exec_addr[256];
        char exec_addr_pt2[128];
        sprintf(exec_addr_pt2, "/worker2/%d/myapp/bin/test", job_id);
        strcpy(exec_addr, cwd);
        strcat(exec_addr, exec_addr_pt2);
        // printf("exec_addr: %s\n", exec_addr);
        // sprintf(exec_addr, "/home/oscreader/OS/project/worker2/%d/myapp/bin/test", job_id);

        char store_addr[256];
        char store_addr_pt2[128];
        sprintf(store_addr_pt2, "/worker2/%d/myapp/result/result.txt", job_id);
        strcpy(store_addr, cwd);
        strcat(store_addr, store_addr_pt2);
        // printf("store_addr: %s\n", store_addr);
        // sprintf(store_addr, "/home/oscreader/OS/project/worker2/%d/myapp/result/result.txt", job_id);
        int out_fd = open(store_addr, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP);
        dup2(out_fd, STDOUT_FILENO);
        close(out_fd);
        execl(exec_addr, exec_addr, NULL);
        perror("execl");
    }

    wait(NULL);
    printf("%d> done executing\n", TCPPORT);
    pthread_mutex_lock(&mutex);
    id_to_send = job_id;
    printf("id_to_send: %d\n", id_to_send);
    executed = 1;
    pthread_cond_signal(&cond);
    // printf("after signaling\n")
    pthread_mutex_unlock(&mutex);

    
}

void* udp_thread(void* arg) {
    printf("in udp thread\n");
    struct sockaddr_in sin;  /* Server address */
	struct sockaddr_in exp;	/* Client address */
	int sc;
	socklen_t fromlen = sizeof(exp);
	int rsp = 1; 
	char message[TAILMSG];

    /* Create socket */
	if ((sc = socket(AF_INET,SOCK_DGRAM,0)) < 0) {
		perror("creation socket");
		exit(1);
	}

    printf("UDP socket created\n");

    /* Prepare server name and bind to DNS service */
	memset((void*)&sin, 0, sizeof(sin));
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(UDPPORT);
	sin.sin_family = AF_INET;
	// printf("Server name: %s\n", (struct sockaddr *)&sin.sa_data);
	if (bind(sc,(struct sockaddr *)&sin,sizeof(sin)) < 0) {
		perror("udp bind");
		exit(2);
	}

    /*** Receive message ***/
	printf("wait to receive message \n");
	if ( recvfrom(sc, message, sizeof(message), 0, (struct sockaddr *) &exp, &fromlen) == -1) { 
		perror("recvfrom");
		exit(2);
	}

    /*** Display client address ***/
	
	printf("Master: <IP = %s, PORT = %d> \n", inet_ntoa(exp.sin_addr), ntohs(exp.sin_port));

    /*** Send reply ***/
	if (sendto(sc,&rsp,sizeof(rsp),0,(struct sockaddr *)&exp,fromlen) == -1) {
		perror("sendto");
		exit(2);
	}
    
    int local_id;

    while(1) {
        pthread_mutex_lock(&mutex);
        while(!executed) {
            pthread_cond_wait(&cond, &mutex);
        }
        executed = 0;
        local_id = id_to_send;
        pthread_mutex_unlock(&mutex);

        printf("UDP socket > send id\n");
        if (sendto(sc,&id_to_send,sizeof(int),0,(struct sockaddr *)&exp,fromlen) == -1) {
            perror("sendto");
            exit(2);
        }
        
    } 

    close(sc);
    pthread_exit(0);
}

void* tcp_thread(void* arg) {
    printf("in tcp thread\n");
    struct sockaddr_in sin;  /* Name of the connection socket (Server address) */
    struct sockaddr_in exp;  /* Client address */
    int sc ;                 /* Connection socket */
    int scom;		      /* Communication socket */
    socklen_t fromlen = sizeof (exp);
    int i = 0;
    char* request;
    pthread_t tid[QUEUE_SIZE];

    request = (char*)malloc(sizeof(char) * 10);
    /* Create socket */
    if ((sc = socket(AF_INET,SOCK_STREAM,0)) < 0) {
        perror("socket");
        exit(1);
    }

    printf("TCP socket created\n");

    memset((void*)&sin, 0, sizeof(sin));
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(TCPPORT);
    sin.sin_family = AF_INET;

    int reuse = 1;
    setsockopt(sc, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));

    /* Register server on DNS svc */
    if (bind(sc,(struct sockaddr *)&sin,sizeof(sin)) < 0) {
        perror("tcp bind");
        exit(2);
    }

    listen(sc, QUEUE_SIZE);

    while (1) {
        request = (char*)malloc(sizeof(char) * 10);
        /* wait for connection request */
        if ( (scom = accept(sc, (struct sockaddr *)&exp, &fromlen))== -1) {
            perror("accept");
            exit(2);
        }

        // /* wait for request */
        if (read(scom, request, sizeof(char) * 10) < 0) {
            perror("request type read error");
            exit(1);
        }
        printf("request: %s\n", request);
        send_confirm(scom);

        /* Create a thread to handle the newly connected client */
        int* tscom = (int*)malloc(sizeof(int));
        *tscom = scom;
        if (strcmp(request, "DEPLOY") == 0) {
            pthread_create(&tid[i], 0, exec_request, tscom);
        } else if (strcmp(request, "RESULTS") == 0) {
            pthread_create(&tid[i], 0, result_request, tscom);
        }

        i = (i + 1) % QUEUE_SIZE;
    }

    close(sc);
    pthread_exit(0);
}

int main(int argc, char* argv[]) {
    pthread_t udp_thr;
    pthread_t tcp_thr;
    int arg = 1;

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
       printf("Current working dir: %s\n", cwd);
    }

    pthread_create(&udp_thr, 0, udp_thread, &arg);
    pthread_create(&tcp_thr, 0, tcp_thread, &arg);   
    pthread_join(udp_thr, NULL);
    pthread_join(tcp_thr, NULL);

    return 0;
}