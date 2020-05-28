#include <stdlib.h>
#include <pthread.h>
#include "queue.h"

node_t* head = NULL;
node_t* tail = NULL;

void enqueue(connection_data_t* d) {
    node_t *newnode = malloc(sizeof(node_t));
    newnode->d = d;
    newnode->next = NULL;
    if (tail == NULL) {
        head = newnode;
    } else {
        tail->next = newnode;
    }
    tail = newnode;
}

connection_data_t* dequeue() {
    if (head == NULL) {
        return NULL;
    } else {
        connection_data_t *d = head->d;
        node_t *temp = head;
        head = head->next;
        if (head == NULL) {
            tail = NULL;
        }
        free(temp);
        return d;
    }
}
