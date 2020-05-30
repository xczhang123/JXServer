#ifndef SESSION
#define SESSION

#include <stdint.h>
#include <pthread.h>
#include <linux/limits.h> 

#define DYN_ARRAY_DEF_CAPACITY (2)

typedef struct session_segment {
    uint32_t id;
    uint64_t start;
    uint64_t len;
    char filename[PATH_MAX];
} session_segment_t;

typedef struct session{
    session_segment_t *sessions;
    uint64_t capacity;
    uint64_t size;
    pthread_mutex_t lock;
} session_t;

void session_array_resize(session_t *s);

session_t * session_array_init();

void session_array_add(session_t *s, uint32_t id, uint64_t start, uint64_t len, char *filename);

void session_array_delete(session_t *s, uint32_t id, uint64_t start, uint64_t len, char *filename);

session_segment_t* session_array_get(session_t *s, int index);

bool session_array_is_in_active(session_t *s, uint32_t id, uint64_t start, uint64_t len, char *filename);

bool session_array_is_in_archive(session_t *archive, uint32_t id, uint64_t start, uint64_t len, char *filename);

void session_array_free(session_t *s);



#endif