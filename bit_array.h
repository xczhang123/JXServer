#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

void show(uint8_t *code, uint8_t len);
void set_bit(uint8_t *arr, size_t k);
uint8_t get_bit(uint8_t *arr, size_t k);
void clear_bit(uint8_t *arr, size_t k);
void copy_bit_32(uint8_t *dest, uint8_t *src, size_t k);
void arr_destroy(uint8_t *arr);