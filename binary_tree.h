#ifndef BINARY_TREE
#define BINARY_TREE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdint.h>
#include <byteswap.h>
#include <arpa/inet.h>

typedef struct {
    uint8_t key;
    struct binary_tree_node *l;
    struct binary_tree_node *r;
    uint8_t code[4];
    uint8_t len;
    bool defined;
} binary_tree_node;

binary_tree_node* new_empty();

binary_tree_node* new(uint8_t key, uint8_t *code, uint8_t len);

void insert(binary_tree_node *root, uint8_t v, uint8_t* code, uint8_t len);

binary_tree_node* search(binary_tree_node *root, uint8_t* code, uint8_t len);

void pointer_search(binary_tree_node *root, uint8_t direction, binary_tree_node **save);

void post_traversal(binary_tree_node * n);

void pre_traversal(binary_tree_node * n);

void in_traversal(binary_tree_node *n);

void traversal(binary_tree_node *n);

void binary_tree_destroy(binary_tree_node *n);

#endif