#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <byteswap.h>

typedef struct {
    uint8_t code[4]; //Code aligned to 32 bits
    uint8_t len; //valid range (without padding)
} compress_dict_node_t;

typedef struct {
    compress_dict_node_t* arr[256];
    size_t cur_pos;
} compress_dict_t;

compress_dict_t* compress_dict_init();
void compress_dict_add(compress_dict_t *cd, uint8_t code[4], uint8_t len);
compress_dict_node_t* compress_dict_get(compress_dict_t *cd, uint8_t key);
void compress_dict_free(compress_dict_t *cd);