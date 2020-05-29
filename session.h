#ifndef SESSION
#define SESSION

#include <stdint.h>
#include <pthread.h>

#define DYN_ARRAY_DEF_CAPACITY (2)

typedef struct session_segment {
    uint32_t id;
    uint64_t start;
    uint64_t len;
    char *filename;
} session_segment_t;

typedef struct session{
    session_segment_t *sessions;
    uint64_t capacity;
    uint64_t size;
    pthread_mutex_t lock;
} session_t;




#endif