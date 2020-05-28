#include "compress_dict.h"
#include "bit_array.h"

compress_dict_t* compress_dict_init() {
    compress_dict_t *cd = malloc(sizeof(compress_dict_t));
    cd->cur_pos = 0;
    memset(cd->arr, 0, 256*sizeof(compress_dict_node_t*));
    return cd;
}

void compress_dict_add(compress_dict_t *cd, uint8_t *code, uint8_t len) {
    compress_dict_node_t *t = malloc(sizeof(compress_dict_node_t));
    copy_bit_32(t->code, code, len);
    t->len = len;  
    cd->arr[cd->cur_pos++] = t;
}

compress_dict_node_t* compress_dict_get(compress_dict_t *cd, uint8_t key) {
    return cd->arr[key];
}

void compress_dict_free(compress_dict_t *cd) {
    for (int i = 0; i < cd->cur_pos;i++) {
        free(cd->arr[i]);
    }
    free(cd);
}

