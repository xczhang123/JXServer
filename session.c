#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "session.h"

#define DYN_ARRAY_DEF_CAPACITY (2)

void session_array_resize(session_t *s) {
    s->capacity *= 2;
    s->sessions = realloc(s->sessions, sizeof(*(s->sessions)) * s->capacity);
}

session_t * session_array_init() {
    session_t *s = malloc(sizeof(session_t));
    s->capacity = DYN_ARRAY_DEF_CAPACITY;
    s->size = 0;
    s->sessions = malloc(sizeof(*(s->sessions)) * s->capacity);
    pthread_mutex_init(&s->lock, NULL);

    return s;
}

void session_array_add(session_t *s, uint32_t id, uint64_t start, uint64_t len, char *filename) {
    pthread_mutex_lock(&s->lock);
    if (s->capacity == s->size) {
        session_array_resize(s);
    }

    session_segment_t seg = {
        .id = id,
        .start = start,
        .len = len,
        .filename = filename
    }; 

    s->sessions[s->size++] = seg;

    pthread_mutex_unlock(&s->lock);
}

void session_array_delete(session_t *s, uint32_t id, uint64_t start, uint64_t len, char *filename) {

    pthread_mutex_lock(&s->lock);

    for (int i = 0; i < s->size; i++) {
        session_segment_t *seg = session_array_get(s, i);
        if (seg->id == id && seg->start == start && 
            seg->len == len && strcmp(seg->filename, filename) == 0) {
                s->sessions[i] = s->sessions[--s->size];
                puts("DELETED");
                break;
        }
    }
    
    pthread_mutex_unlock(&s->lock);
}

session_segment_t* session_array_get(session_t *s, int index) {
    if (index < 0 || index >= s->size) {
        return NULL;
    }

    return (&s->sessions[index]);
}

bool session_array_is_in(session_t *s, uint32_t id, uint64_t start, uint64_t len, char *filename) {
    for (int i = 0; i < s->size; i++) {
        session_segment_t *seg = session_array_get(s, i);
        if (seg->id == id && seg->start == start && 
            seg->len == len && strcmp(seg->filename, filename) == 0) {
                return true;
        }
    }

    return false;
}


void session_array_free(session_t *s) {
    free(s->sessions);
    free(s);
}
