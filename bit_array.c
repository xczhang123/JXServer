#include "bit_array.h"

void show(uint8_t *code, uint8_t len) {
    printf("The code bit is: ");
    for (int i = 0; i < len; i++) {
        printf("%d", get_bit(code, i));
    }
    puts("");
}

void set_bit(uint8_t *arr, size_t k) {
    size_t i = k/8; 
    size_t pos = k%8;

    uint8_t flag = 1;

    flag = flag << (8-pos-1);
    arr[i] = arr[i] | flag;
}

uint8_t get_bit(uint8_t *arr, size_t k) {
    size_t i = k/8;
    size_t pos = k%8;

    uint8_t flag = 1;

    flag = flag << (8-pos-1);

    if ((arr[i] & flag) != 0) {
        return 0x1;
    } else {
        return 0x0;
    }
}

void clear_bit(uint8_t *arr, size_t k) {
    size_t i = k/8;
    size_t pos = k%8;

    uint8_t flag = 1; 

    flag = flag << (8-pos-1);
    flag = ~flag;   

    arr[i] = arr[i] & flag;
}

void copy_bit_32(uint8_t *dest, uint8_t *src, size_t k) {
    memset(dest, 0, 4*sizeof(uint8_t));
    for (size_t i = 0; i < k; i++) {
        if (get_bit(src, i) == 1) {
            set_bit(dest, i);
        }
    }
}

void arr_destroy(uint8_t *arr) {
    free(arr);
}