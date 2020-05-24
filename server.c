#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <byteswap.h>
#include <endian.h>
#include <stdbool.h>
#include "queue.h"

#define SERVER_MSG ("Hello User! Welcome to my server!")
#define THREAD_POOL_SIZE (20)
#define LISTENING_SIZE (100)

typedef struct {
    struct sockaddr_in address;
    char *path;
} configuration;

pthread_t thread_pool[THREAD_POOL_SIZE];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;

void config_reader(configuration *config, char* config_file_name);
void* connection_handler(void* arg);
void* thread_handler();
int message_reader(void* arg);
void echo(void *arg);
void error(void *arg);

int main(int argc, char** argv) {
	
	int serversocket_fd = -1;
	int clientsocket_fd = -1;

    configuration *config = malloc(sizeof(configuration));
    config_reader(config, argv[1]);

    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_create(thread_pool+i, NULL, thread_handler, NULL);
    }

	int option = 1; 
	serversocket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(serversocket_fd < 0) {
		puts("This failed!");
		exit(1);
	}

	setsockopt(serversocket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &option, sizeof(int));

	if(bind(serversocket_fd, (struct sockaddr*) &config->address, sizeof(struct sockaddr_in))) {
		puts("This broke! :(");
		exit(1);
	}

	listen(serversocket_fd, LISTENING_SIZE);
    // puts("HEYYY!");
	while(true) {
		uint32_t addrlen = sizeof(struct sockaddr_in);
		clientsocket_fd = accept(serversocket_fd, (struct sockaddr*) &config->address, &addrlen);
		
		connection_data_t* d = malloc(sizeof(connection_data_t));
		d->socketfd = clientsocket_fd;
        d->path = config->path;

        // printf("serversocket_fd %d\n", serversocket_fd);
        // printf("clientsocket_fd %d\n", clientsocket_fd);
        pthread_mutex_lock(&mutex);
        enqueue(d);
        pthread_cond_signal(&condition_var);
        pthread_mutex_unlock(&mutex);
        // puts("HEYYY!");

		// pthread_t thread;
		// pthread_create(&thread, NULL, &connection_handler, d);
	}
    
    // pthread_join(thread, NULL);
    free(config->path);
    free(config);
	close(serversocket_fd);
    
    return 0;

}

void config_reader(configuration *config, char* config_file_name) {
    FILE *fp;
    if ((fp = fopen(config_file_name, "rb")) == NULL) {
        perror("error");
        exit(1);
    }

    // Read 1st - address
    struct in_addr inaddr;
    fread(&inaddr.s_addr, sizeof(uint32_t), 1, fp);
    config->address.sin_addr = inaddr;
    config->address.sin_family = AF_INET;

    // Read 2nd - port
    fread(&config->address.sin_port, sizeof(uint16_t), 1, fp);

    // Read 3rd - absolute path to the target directory
    long pos = ftell(fp);
    fseek(fp, 0, SEEK_END);
    long file_length = ftell(fp)-pos;
    fseek(fp, pos, SEEK_SET);

    char *path = (char*)calloc((file_length+1)*sizeof(char), 1);
    strcat(path, "\0");
    fread(path, file_length, 1, fp);
    config->path = path;

    fclose(fp);
}

void* thread_handler() {
    while (true) {
        connection_data_t* d;
        pthread_mutex_lock(&mutex);
        if ((d = dequeue()) == NULL) {
            pthread_cond_wait(&condition_var, &mutex); 
            //try again
            d = dequeue(); 
        }; 
        pthread_mutex_unlock(&mutex);

        if (d != NULL) {
            //We have some work to do
            connection_handler(d);
        }
    }

}

void* connection_handler(void* arg) {
	connection_data_t* d = (connection_data_t*) arg;
    while(true) {

        if (message_reader(d) == 0) {
            // sleep(1);
            error(d);
            break;
        }

        // printf("header received %hhx\n", (d->msg.header));
        // printf("header received %hhx\n", (d->msg.header>>4));

        if (((d->msg.header >> 4) & 0xf) == 0x0) {
            // sleep(1);
            echo(d);
        } else {
            // sleep(1);
            error(d);
            break;
        }


        free(d->msg.payload);
    }

    free(d);

	return NULL;
}

/* return 1 for success, otherwise 0 */
int message_reader(void* arg) {
    connection_data_t* d = (connection_data_t*) arg;
    ssize_t nread = read(d->socketfd, &d->msg, sizeof(d->msg.header)+sizeof(d->msg.p_length));

    if (nread == 0) {
        return 0;
    }
    // for (int i = 0; i < 8; i++) {
    //     printf("The length is: %hhx\n", *((char*)(&(d->msg.p_length))+i));
    // }
    // puts("");

    // printf("The converted length is %zu\n", be64toh(d->msg.p_length));
    
    uint64_t length = be64toh(d->msg.p_length);
    // Max possible allocable size
    if (length > 0x10000000000) {
        return 0;
    }

    uint8_t *payload = (uint8_t*)malloc(length*sizeof(uint8_t));
    d->msg.payload = payload;
    read(d->socketfd, d->msg.payload, length*sizeof(uint8_t));

    // for (int i = 0; i < 1; i++) {
    //     printf("The header is: %hhx\n", (*((char*)(&(d->msg.header)))));
    // }
    // puts("");

    // for (int i = 0; i < 8; i++) {
    //     printf("The length is: %hhx\n", *((char*)(&(d->msg.p_length))+i));
    // }
    // puts("");

    // for (int i = 0; i < length; i++) {
    //     printf("The payload is: %hhx\n", *((char*)(d->msg.payload)+i));
    // }
    // puts("");

    return 1;

}

void echo(void *arg) {
    connection_data_t* d = (connection_data_t*) arg;
    connection_data_t* res = (connection_data_t*)malloc(sizeof(connection_data_t)); 

    // bool p_compressed = ((d->msg.header & 0x08) >> 3) == 0x1; //if payload received is compressed
    // bool r_compressed = ((d->msg.header & 0x04) >> 2) == 0x1; //if payload sent needs to be compressed

    res->socketfd = d->socketfd;
    //***** NOT FULLY IMPLEMENTED YET
	res->msg.header = 0x10;
    res->msg.p_length = d->msg.p_length;

    long length = be64toh(res->msg.p_length)*sizeof(uint8_t);
    res->msg.payload = (uint8_t*)malloc(length*sizeof(uint8_t));
    memcpy(res->msg.payload, d->msg.payload, length); 

    write(d->socketfd, &res->msg, sizeof(d->msg.header)+sizeof(d->msg.p_length));
    write(d->socketfd, res->msg.payload, length);

    free(res->msg.payload);
    free(res);
}

void error(void *arg) {
    connection_data_t* d = (connection_data_t*) arg;
    connection_data_t* res = (connection_data_t*)malloc(sizeof(connection_data_t)); 

    res->socketfd = d->socketfd;
    res->msg.header = 0xf0;
    res->msg.p_length = 0;
    res->msg.payload = NULL;

    write(d->socketfd, &res->msg,  sizeof(d->msg.header)+sizeof(d->msg.p_length));

    //Close the connection
    close(res->socketfd);

    free(res);

    // free(d->msg.payload);
    // free(d);
}
