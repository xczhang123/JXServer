// #include <stdio.h>
// #include <stdlib.h>
// #include "session.h"

// #define DYN_ARRAY_DEF_CAPACITY (2)

// static void session_array_resize(struct dyn_array *d) {
//     d->capacity *= 2;
//     d->data = realloc(d->data, sizeof(*(d->data)) * d->capacity);
// }

// struct dyn_array* dyn_array_init() {
//     struct dyn_array *d = malloc(sizeof(struct dyn_array));
//     d->capacity = DYN_ARRAY_DEF_CAPACITY;
//     d->size = 0;
//     d->data = malloc(sizeof(*(d->data)) * d->capacity);

//     return d;
// }

// void dyn_array_add(struct dyn_array *dyn, int value) {

//     if (dyn->capacity == dyn->size) {
//         dyn_array_resize(dyn);
//     }

//     dyn->data[dyn->size++] = value;
// }

// void dyn_array_delete(struct dyn_array *dyn, int index) {

//     if (index < 0 || index >= dyn->size) {
//         return;
//     }

//     for (int i = index; i < dyn->size-1; i++) {
//         dyn->data[i] = dyn->data[i+1];
//     }

//     dyn->size--;

// }

// int dyn_array_get(struct dyn_array *dyn, int index) {
//     if (index < 0 || index >= dyn->size) {
//         fprintf(stderr, "Array index out of bounds! We return -1");
//         return -1;
//     }

//     return dyn->data[index];
// }

// void dyn_array_free(struct dyn_array * dyn) {
//     free(dyn->data);
//     free(dyn);
// }

// int main() {
//     struct dyn_array *dyn = dyn_array_init();
//     dyn_array_add(dyn, 1);
//     dyn_array_add(dyn, 3);
//     dyn_array_add(dyn, 5);

//     for(int i = 0; i < dyn->size; i++) {
//         printf("%d ", dyn->data[i]);
//     }
//     puts("");

//     dyn_array_delete(dyn, 2);
//     for(int i = 0; i < dyn->size; i++) {
//         printf("%d ", dyn->data[i]);
//     }
//     puts("");


//     dyn_array_free(dyn);

// }
