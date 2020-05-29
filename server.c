#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <byteswap.h>
#include <endian.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/sysmacros.h>
#include "queue.h"

#define THREAD_POOL_SIZE (20)
#define LISTENING_SIZE (100)

pthread_t thread_pool[THREAD_POOL_SIZE];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;
bool __shutdown = false;

void config_reader(configuration_t *config, char* config_file_name);
void compression_reader(configuration_t *config);
void* connection_handler(void* arg);
void* thread_handler();
int message_header_reader(void* arg);
int echo(void *arg);
int dir_list(void *arg);
int file_size_query(void *arg);
void error(void *arg);
void server_shutdown(void *arg);
void compression_char(connection_data_t *d, uint8_t** compressed_msg, 
                        uint8_t key, uint64_t *num_of_bytes, uint64_t *num_of_bit);
void send_compression_msg(connection_data_t *d, connection_data_t *res, uint8_t **compressed_msg, 
                            uint8_t header, uint64_t *num_of_bit, uint64_t *num_of_bytes);

int main(int argc, char** argv) {
	
	int serversocket_fd = -1;
	int clientsocket_fd = -1;
 
    configuration_t *config = malloc(sizeof(configuration_t));
    config_reader(config, argv[1]);
    compression_reader(config);

    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_create(thread_pool+i, NULL, thread_handler, NULL);
    }

	int option = 1; 
	serversocket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(serversocket_fd < 0) {
		puts("This failed!");
		exit(1);
	}

	setsockopt(serversocket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &option, sizeof(int));

	if(bind(serversocket_fd, (struct sockaddr*) &config->address, sizeof(struct sockaddr_in))) {
		puts("This broke! :(");
		exit(1);
	}

	listen(serversocket_fd, LISTENING_SIZE);
	
    while(!__shutdown) {
		uint32_t addrlen = sizeof(struct sockaddr_in);
		clientsocket_fd = accept(serversocket_fd, (struct sockaddr*) &config->address, &addrlen);
		
        if (clientsocket_fd != -1) {
            connection_data_t* d = malloc(sizeof(connection_data_t));
            d->socketfd = clientsocket_fd;
            d->serversocketfd = serversocket_fd;
            d->path = config->path;
            d->config = config;

            pthread_mutex_lock(&mutex);
            enqueue(d);
            pthread_cond_signal(&condition_var);
            pthread_mutex_unlock(&mutex);
        }
	}

    free(config->path);
    compress_dict_free(config->cd);
    binary_tree_destroy(config->root);
    free(config);
    exit(0);
}

void config_reader(configuration_t *config, char* config_file_name) {
    FILE *fp;
    if ((fp = fopen(config_file_name, "rb")) == NULL) {
        perror("error");
        exit(1);
    }

    // Read 1st - address
    struct in_addr inaddr;
    fread(&inaddr.s_addr, sizeof(uint32_t), 1, fp);
    config->address.sin_addr = inaddr;
    config->address.sin_family = AF_INET;

    // Read 2nd - port
    fread(&config->address.sin_port, sizeof(uint16_t), 1, fp);

    // Read 3rd - absolute path to the target directory
    long pos = ftell(fp);
    fseek(fp, 0, SEEK_END);
    long file_length = ftell(fp)-pos;
    fseek(fp, pos, SEEK_SET);

    char *path = (char*)calloc((file_length+1)*sizeof(char), 1);
    strcat(path, "\0");
    fread(path, file_length, 1, fp);
    config->path = path;

    fclose(fp);
}

void compression_reader(configuration_t *config) {
    FILE *f = fopen("compression.dict", "rb");
    fseek(f, 0, SEEK_END);
    long file_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *arr = (uint8_t*)malloc(file_len);
    fread(arr, 1, file_len, f);
    fclose(f);

    compress_dict_t *cd = compress_dict_init();
    binary_tree_node *root = new_empty();

    int count = 0;
    int i = 0;
    while (i < file_len*8){
        uint8_t run_len = 0;

        // Read code length
        for (int j = 0; j < 8 && i < file_len*8; j++){
            if (get_bit(arr, i++) == 1){
                set_bit(&run_len, j);
            }
        }
        // We must have read the padding at the end
        if (run_len == 0) {
            break;
        }

        // Read code
        uint8_t code[4] = {0};
        for (int j = 0; j < run_len; j++){
            if (get_bit(arr, i++) == 1){
                set_bit(code, j);
            }
        }

        //Add to the compression dictionary
        //The key is assumed to be incremental starting from 0
        compress_dict_add(cd, code, run_len);

        insert(root, count, code, run_len);

        // show(cd->arr[cd->cur_pos-1]->code, run_len);

        count++;

        // binary_tree_node *n = search(root, code, run_len);
        // show(n->code, n->len);
        // break;
    }

    config->cd = cd;
    config->root = root;
    arr_destroy(arr);
}

void* thread_handler() {
    while (true) {
        connection_data_t* d;
        pthread_mutex_lock(&mutex);
        while ((d = dequeue()) == NULL) {
            pthread_cond_wait(&condition_var, &mutex); 
            if (__shutdown) {
                pthread_mutex_unlock(&mutex);
                // puts("Thread exiting...");
                pthread_exit(NULL);
            }
        }; 
        pthread_mutex_unlock(&mutex);

        if (d != NULL) {
            //We have some work to do
            connection_handler(d);
        }
    }

}

void* connection_handler(void* arg) {
	connection_data_t* d = (connection_data_t*) arg;
    bool stop = false;
    while(!stop) {

        // If the client refuses to connect, we disconnect it
        if (message_header_reader(d) == 0) {
            // error(d);
            break;
        }

        uint8_t type = (d->msg.header >> 4 & 0xf) | 0x00;

        // printf("Type is : %d\n", type);

        switch (type) {
            case 0x00:
                if (!echo(d)){
                    stop = true;
                };
                // echo(d);
                break;
            case 0x02:
                if (!dir_list(d)) {
                    stop = true;
                };
                break;
            case 0x04:
                if (!file_size_query(d)) {
                    stop = true;
                };
                break;
            case 0x08:
                server_shutdown(d);
                stop = true;
                break;
            default:
                error(d);
                stop = true;
                break;
        }

        //dummy reader to clear the buffer
        // message_header_reader(d);
    }

    free(d);

	return NULL;
}

/* return 1 for success, otherwise 0 */
int message_header_reader(void* arg) {
    connection_data_t* d = (connection_data_t*) arg;
    ssize_t nread = read(d->socketfd, &d->msg, sizeof(d->msg.header)+sizeof(d->msg.p_length));

    // printf("nread %ld\n", nread);
    if (nread <= 0) {
        return 0;
    } else {
        return 1;
    }
}

void compression_char(connection_data_t *d, uint8_t** compressed_msg, uint8_t key, uint64_t *num_of_bytes, uint64_t *num_of_bit) {
    uint8_t run_len = compress_dict_get(d->config->cd, key)->len;
    uint8_t code[4] = {0};
    memcpy(code, compress_dict_get(d->config->cd, key)->code, 4);

    for (int j = 0; j < run_len; j++) {
                
        // Realloc one mor byte
        if (*num_of_bit == *num_of_bytes * 8) {
            *compressed_msg = realloc(*compressed_msg, ++(*num_of_bytes));
        }
        
        if (get_bit(code, j) == 1) {
            set_bit(*compressed_msg, (*num_of_bit)++);
        } else {
            clear_bit(*compressed_msg, (*num_of_bit)++);
        }
    }
}

void send_compression_msg(connection_data_t *d, connection_data_t *res, uint8_t **compressed_msg, 
                            uint8_t header, uint64_t *num_of_bit, uint64_t *num_of_bytes) {
    *num_of_bytes += 1;
    res->msg.header = header;
    set_bit(&res->msg.header, 4);
    res->msg.p_length = bswap_64(*num_of_bytes);
    write(d->socketfd, &res->msg, sizeof(res->msg.header)+sizeof(res->msg.p_length));

    uint8_t padding = (8-(*num_of_bit)%8) % 8;
       
    for (int i = 0; i < padding; i++) {
        clear_bit(*compressed_msg, (*num_of_bit)++);
    }
    write(d->socketfd, *compressed_msg, (*num_of_bytes)-1);
    // padding = bswap_64(padding);
    write(d->socketfd, &padding, 1);

    free(*compressed_msg);
}

int echo(void *arg) {
    connection_data_t* d = (connection_data_t*) arg;
    connection_data_t* res = (connection_data_t*)malloc(sizeof(connection_data_t)); 

    bool compression_bit = get_bit(&d->msg.header, 4) == 0x1; //if payload received is compressed
    bool require_compression = get_bit(&d->msg.header, 5) == 0x1; //if payload sent needs to be compressed

    if (require_compression && !compression_bit) {
        //need compression
        uint64_t length = bswap_64(d->msg.p_length);
        res->msg.payload = malloc(length);
        read(d->socketfd, res->msg.payload, length);
        uint64_t num_of_bit = 0;
        uint64_t num_of_bytes = 1;
        uint8_t *compressed_msg = malloc(1);

        for (int i = 0; i < length; i++) {
            uint8_t key = res->msg.payload[i];
            compression_char(d, &compressed_msg, key, &num_of_bytes, &num_of_bit);
        }
        send_compression_msg(d, res, &compressed_msg, 0x10, &num_of_bit, &num_of_bytes);
    } else {
        res->msg.header = 0x10;
        if (compression_bit) {
            set_bit(&res->msg.header, 4);
        }
        res->msg.p_length = d->msg.p_length;
        uint64_t length = bswap_64(res->msg.p_length);

        write(d->socketfd, &res->msg, sizeof(res->msg.header)+sizeof(res->msg.p_length));
        
        res->msg.payload = malloc(length);
        read(d->socketfd, res->msg.payload, length);
        write(d->socketfd, res->msg.payload, length);
    }   

    free(res->msg.payload);
    free(res);

    return 1;
}

int dir_list(void *arg) {
    connection_data_t* d = (connection_data_t*) arg;
    if (d->msg.p_length != 0) {
        error(d);
        return 0;
    }

    connection_data_t* res = (connection_data_t*)malloc(sizeof(connection_data_t)); 

    // bool compression_bit = get_bit(&d->msg.header, 4) == 0x1; //if payload received is compressed
    bool require_compression = get_bit(&d->msg.header, 5) == 0x1; //if payload sent needs to be compressed

    if (require_compression) {
        struct stat sb;
        DIR *dir;
        struct dirent *file;

        uint64_t num_of_bit = 0;
        uint64_t num_of_bytes = 1;
        uint8_t *compressed_msg = malloc(1);

        bool is_empty = true;
        if ((dir=opendir(d->path)) != NULL) {
            while ((file = readdir(dir)) != NULL) {
                stat(file->d_name, &sb);
                if (file->d_type == DT_REG) {

                    for (int i = 0; i < strlen(file->d_name); i++) {
                        uint8_t key = file->d_name[i];
                        compression_char(d, &compressed_msg, key, &num_of_bytes, &num_of_bit);
                    }
                    compression_char(d, &compressed_msg, '\0', &num_of_bytes, &num_of_bit);
                    is_empty = false;
                }
            }
            closedir(dir);
        } else {
            error(d);
            free(res);
            return 0;
        }

        //If the directory is empty
        if (is_empty) {
            compression_char(d, &compressed_msg, '\0', &num_of_bytes, &num_of_bit);
            uint8_t header = 0x30;
            set_bit(&header, 4);
            send_compression_msg(d, res, &compressed_msg, header, &num_of_bit, &num_of_bytes);
            return 1;
        } else { //If the directory is not empty
            uint8_t header = 0x30;
            set_bit(&header, 4);
            send_compression_msg(d, res, &compressed_msg, header, &num_of_bit, &num_of_bytes);
        }

        free(res);

    } else { // do not need compression
        struct stat sb;
        DIR *dir;
        struct dirent *file;

        bool is_empty = true;
        uint64_t length = 0;
        if ((dir=opendir(d->path)) != NULL) {
            while ((file = readdir(dir)) != NULL) {
                stat(file->d_name, &sb);
                if (file->d_type == DT_REG) {
                    is_empty = false;
                    length += strlen(file->d_name) + 1;
                }
            }
            closedir(dir);
        } else {
            error(d);
            free(res);
            return 0;
        }

        if (is_empty) {
            length = 1;
            res->msg.header = 0x30;
            res->msg.p_length = bswap_64(length);
            write(d->socketfd, &res->msg,  sizeof(res->msg.header)+sizeof(res->msg.p_length));
            write(d->socketfd, "\0", 1);

            free(res);
            return 1;
        }

        res->msg.header = 0x30;
        res->msg.p_length = bswap_64(length);
        write(d->socketfd, &res->msg,  sizeof(res->msg.header)+sizeof(res->msg.p_length));

        if ((dir=opendir(d->path)) != NULL) {
            while ((file = readdir(dir)) != NULL) {
                stat(file->d_name, &sb);
                if (file->d_type == DT_REG) {
                    write(d->socketfd, file->d_name, strlen(file->d_name));
                    write(d->socketfd, "\0", 1);
                }
            }
            closedir(dir);
        }

        free(res);

    }

    return 1;
}

int file_size_query(void *arg) {
    connection_data_t* d = (connection_data_t*) arg;
    connection_data_t* res = (connection_data_t*)malloc(sizeof(connection_data_t)); 

    uint64_t read_len = bswap_64(d->msg.p_length);
    char *filename = (char*)malloc(read_len);
    read(d->socketfd, filename, read_len);;

    struct stat sb;
    DIR *dir;
    struct dirent *file;

    // puts(filename);

    bool found = false;
    uint64_t file_len = 0;
    if ((dir=opendir(d->path)) != NULL) {
        while ((file = readdir(dir)) != NULL) {
            char *file_name_full = malloc(strlen(d->path)+3+strlen(file->d_name));
            strcpy(file_name_full, d->path);
            strcat(file_name_full, "/");
            strcat(file_name_full, file->d_name);
            stat(file_name_full, &sb);
            free(file_name_full);
            if (file->d_type == DT_REG && strcmp(file->d_name, filename) == 0) {
                found = true;
                file_len = sb.st_size;
                // puts("LAter");
                // puts(file->d_name);
                break;
            }
        }
        closedir(dir);
    } else {
        error(d);
        free(filename);
        free(res); 
        return 0;
    }

    //If the file is not found
    if (!found) {
        error(d);
        free(filename);
        free(res); 
        return 0;
    }


    // uint64_t target_filename_length = strlen(d->path)+1+2+strlen(filename);
    uint64_t num_len = 8;

    // printf("%ld\n", file_len);
    file_len = bswap_64(file_len);

    // printf("%ld\n", file_len);

    // res->msg.payload = malloc(target_filename_length);
    // strcpy((char*)res->msg.payload, d->path);
    // strcat((char*)res->msg.payload, "/");
    // strcat((char*)res->msg.payload, filename);
    // target_filename_length = bswap_64(target_filename_length);
    // num_len = bswap_64(num_len);

    // printf("%ld\n", target_filename_length);
    // printf("%ld\n", num_len);
    
    res->msg.header = 0x50;
    res->msg.p_length = bswap_64(num_len);
    write(d->socketfd, &res->msg, sizeof(res->msg.header)+sizeof(res->msg.p_length));
    // write(d->socketfd, res->msg.payload, );
    // for (int i = 7; i >= 0; i++) {
    write(d->socketfd, &file_len, num_len);
   
    free(filename);
    free(res);  

    return 1;

}

void server_shutdown(void *arg) {
    connection_data_t* d = (connection_data_t*) arg;
    __shutdown = true;
    close(d->socketfd);
    pthread_cond_broadcast(&condition_var);
    shutdown(d->serversocketfd, SHUT_RDWR);
}


void error(void *arg) {
    connection_data_t* d = (connection_data_t*) arg;
    connection_data_t* res = (connection_data_t*)malloc(sizeof(connection_data_t)); 

    res->socketfd = d->socketfd;
    res->msg.header = 0xf0;
    res->msg.p_length = 0;

    write(d->socketfd, &res->msg,  sizeof(res->msg.header)+sizeof(res->msg.p_length));

    //Close the connection
    close(res->socketfd);

    free(res);
}