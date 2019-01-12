CC=gcc -Wall -lrt -lpthread
BIN=bin
INC=include
LIB=lib
OBJ=obj
SRC=src

all: clean build

build:
	# workers
	${CC} -c -o ${OBJ}/worker1.o ${SRC}/worker1.c
	${CC} -o ${BIN}/worker1 ${OBJ}/worker1.o -lpthread
	${CC} -c -o ${OBJ}/worker2.o ${SRC}/worker2.c
	${CC} -o ${BIN}/worker2 ${OBJ}/worker2.o -lpthread
	${CC} -c -o ${OBJ}/worker3.o ${SRC}/worker3.c
	${CC} -o ${BIN}/worker3 ${OBJ}/worker3.o -lpthread

	# master 
	${CC} -c -o ${OBJ}/master.o ${SRC}/master.c
	${CC} -o ${BIN}/master ${OBJ}/master.o -lpthread

	# deploy 
	${CC} -c -o ${OBJ}/deploy.o ${SRC}/client_deploy.c
	${CC} -o ${BIN}/deploy ${OBJ}/deploy.o -lpthread

	# status
	${CC} -c -o ${OBJ}/status.o ${SRC}/client_status.c
	${CC} -o ${BIN}/status ${OBJ}/status.o

	# results
	${CC} -c -o ${OBJ}/results.o ${SRC}/client_results.c
	${CC} -o ${BIN}/results ${OBJ}/results.o -lpthread

clean: 
	rm -rf ${OBJ}/*
	rm -rf worker1/*
	rm -rf worker2/*
	rm -rf worker3/*
	rm -rf myapp/results/*