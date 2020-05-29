#include "binary_tree.h"
#include "bit_array.h"

int pre_count = 0;
int in_count = 0;
int post_count = 0;

binary_tree_node* new_empty() {
    binary_tree_node *n = malloc(sizeof(binary_tree_node));
    memset(n->code, 0, 4*sizeof(uint8_t));
    n->key = 0;
    n->len = 0;
    n->l = NULL;
    n->r = NULL;
    n->defined = false;
    return n;
}

binary_tree_node* new(uint8_t key, uint8_t *code, uint8_t len) {
    binary_tree_node *n = malloc(sizeof(binary_tree_node));
    n->key = key;
    n->l = NULL;
    n->r = NULL;
    copy_bit_32(n->code, code, len);
    n->len = len;
    n->defined = true;
    return n;
}

/* root is not NULL */
void insert(binary_tree_node *root, uint8_t key, uint8_t *code, uint8_t len) {
    binary_tree_node *cursor = root;
    for (int i = 0; i < len; i++) {
        // The bit is not set
        if (get_bit(code, i) == 0) {
            if (cursor->l == NULL) {
                if (i == len-1) {  //We have reached the rb of the code
                    cursor->l = new(key, code, len);
                } else {
                    cursor->l = new_empty();
                    cursor = cursor->l;
                }
            } else {
                if (i == len-1) {  //We have reached the rb of the code
                    cursor->l->key = key;
                    copy_bit_32(cursor->l->code, code, len);
                    cursor->l->len = len;
                    cursor->l->defined = true;
                } else {
                    cursor = cursor->l;
                }
            }
        } else {
            if (cursor->r == NULL) {
                if (i == len-1) {  //We have reached the rb of the code
                    cursor->r = new(key, code, len);
                } else {
                    cursor->r = new_empty();
                    cursor = cursor->r;
                }
            } else {
                if (i == len-1) {  //We have reached the rb of the code
                    cursor->r->key = key;
                    copy_bit_32(cursor->r->code, code, len);
                    cursor->r->len = len;
                    cursor->r->defined = true;
                } else {
                    cursor = cursor->r;
                }
            }
        }
    }
}

binary_tree_node* search(binary_tree_node *root, uint8_t *code, uint8_t len) {

    binary_tree_node *cursor = root;

    for (int i = 0; i < len; i++) {
        // The bit is not set
        if (get_bit(code, i) == 0) {
            if (cursor->l != NULL) {
                cursor = cursor->l;
            }
            else {
                return NULL;
            }
        } else {
            if (cursor->r != NULL) {
                cursor = cursor->r;
            } else {
                return NULL;
            }
        }
        // printf("%d\n", i);
    }
    // printf("What is cursor: %p\n", cursor);
    return cursor;
}


void post_traversal(binary_tree_node * n){
	if(n->l) post_traversal(n->l);
	if(n->r) post_traversal(n->r);
	printf("%d ", n->key);
    post_count++;
}

void pre_traversal(binary_tree_node * n){
    pre_count++;
    printf("%d ", n->key);
	if(n->l) pre_traversal(n->l);
	if(n->r) pre_traversal(n->r);
}

void in_traversal(binary_tree_node *n) {
    if (n->l) in_traversal(n->l);
    printf("%d ", n->key);
    in_count++;
    if (n->r) in_traversal(n->r);
}

void traversal(binary_tree_node *root) {
    puts("");
    printf("Preorder: ");
    pre_traversal(root);
    puts("");

    printf("Inorder: ");
    in_traversal(root);
    puts("");

    printf("Postorder: ");
    post_traversal(root);
    puts("");

    printf("Pre_count: %d\n", pre_count);
    printf("In_count: %d\n", in_count);
    printf("Post_count: %d\n", post_count);
    pre_count = 0;
    in_count = 0;
    post_count = 0;

    puts("");

}

void binary_tree_destroy(binary_tree_node *n) {
    if (n->l != NULL) {
        binary_tree_destroy(n->l);
    } 
    
    if (n->r != NULL) {
        binary_tree_destroy(n->r);
    }

    free(n);
}



// int main() {
//     binary_tree_node *root = new_empty();
//     // root->l = new(11, 0x0);
//     // root->r = new(255, 0x1);
//     // root->l->l = new(13, 0x00);
//     // root->l->r = new(14, 0x01) ;
//     uint32_t code = 0xd98b48e;
//     insert(root, 128, code, 28);
//     binary_tree_node * found = search(root, 0xd98b48e, 28);
//     if (found != NULL)
//         printf("Found: %x\n", found->code);
//     else
//         puts("NULL!");

//     uint32_t n_code = htonl(code);

//     FILE *f = fopen("test.tt", "wb");
//     // printf("Network byte order is: %x\n", code_net);
//     fwrite(&n_code, 4, 1, f);
//     fclose(f);

//     // f = fopen("test.tt", "rb");
//     // uint32_t v;
//     // fread(&v, 4, 1, f);

//     // printf("Code is: ");
//     // for (int i = 0; i < 4; i++) {
//     //     printf("%x ", *((uint8_t*)(&v)+i));
//     // }
//     // puts("");
//     // printf("V is %x\n", v);

//     // uint32_t t_v = MID(v, 4, 32);
//     // printf("Truncated v is %x\n", t_v);

//     // printf("Code is: ");
//     // for (int i = 0; i < 4; i++) {
//     //     printf("%x ", *((uint8_t*)(&t_v)+i));
//     // }
//     // puts("");

//     // fclose(f);

//     destroy(root);
// }