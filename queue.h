#ifndef QUEUE
#define QUEUE

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>
#include "compress_dict.h"
#include "binary_tree.h"
#include "bit_array.h"
#include "session.h"
#define PAYLOAD_CHUNK (1024)

typedef struct {
    struct sockaddr_in address;
    char *path;
    compress_dict_t *cd;
    binary_tree_node *root;
    session_t *s;
    session_t *archived_s;
} configuration_t;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition_var;
    bool __shutdown;
} server_controller_t;

#pragma pack(1)
typedef struct {
    uint8_t header;
    uint64_t p_length;
    uint8_t *payload;
} message_t;

typedef struct {
	int socketfd;
    int serversocketfd;
    char *path;
    server_controller_t *con;
    configuration_t *config;
	message_t msg;
} connection_data_t;

struct node {
    connection_data_t* d;
    struct node* next;
};
typedef struct node node_t;

void enqueue();

connection_data_t* dequeue();

#endif