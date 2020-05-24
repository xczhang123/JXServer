#ifndef QUEUE
#define QUEUE

#include <stdint.h>

#pragma pack(1)
typedef struct {
    uint8_t header;
    uint64_t p_length;
    uint8_t *payload;
} message_t;

typedef struct {
	int socketfd;
    char *path;
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