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
#include <linux/limits.h> 
#include <sys/sysmacros.h>
#include <fcntl.h>
#include "queue.h"

#define THREAD_POOL_SIZE (8)
#define LISTENING_SIZE (SOMAXCONN)

void config_reader(configuration_t *config, char *config_file_name);
void compression_reader(configuration_t *config);
void* connection_handler(void *arg);
void* thread_handler(void *arg);
int message_header_reader(void* arg);
int echo(void *arg);
int dir_list(void *arg);
int file_size_query(void *arg);
int retrieve_file(connection_data_t *arg);
void error(void *arg);
void server_shutdown(void *arg);
void compression_char(connection_data_t *d, uint8_t **compressed_msg, 
                        uint8_t key, uint64_t *num_of_bytes, uint64_t *num_of_bit);
void send_compression_msg(connection_data_t *d, connection_data_t *res, uint8_t **compressed_msg, 
                            uint8_t header, uint64_t *num_of_bit, uint64_t *num_of_bytes);
void decompression_msg(connection_data_t *d, uint8_t* original_msg, char **decompression_msg, 
                        uint64_t read_len, uint8_t padding);

int main(int argc, char** argv) {
	
	int serversocket_fd = -1;
	int clientsocket_fd = -1;
    
    // 1. Read the server config file
    // 2. Set up compression dictionary and decompression tree
    configuration_t *config = malloc(sizeof(configuration_t));
    config_reader(config, argv[1]);
    compression_reader(config);
    
    session_t *s = session_array_init(); // Store active session information
    session_t *archived_s = session_array_init(); //Store previous sessions 

    // Threads mutex and conditional variable, and shutdown signal
    server_controller_t *con = malloc(sizeof(server_controller_t));
    pthread_mutex_init(&con->mutex, NULL);
    pthread_cond_init(&con->condition_var, NULL);
    con->__shutdown = false;

    // We limit the number of threads to be THREAD_POOL_SIZE
    pthread_t thread_pool[THREAD_POOL_SIZE];
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_create(thread_pool+i, NULL, thread_handler, con);
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
	
    while(!con->__shutdown) {
		uint32_t addrlen = sizeof(struct sockaddr_in);
		clientsocket_fd = accept(serversocket_fd, (struct sockaddr*) &config->address, &addrlen);
		
        if (clientsocket_fd != -1) {
            connection_data_t* d = malloc(sizeof(connection_data_t));
            d->socketfd = clientsocket_fd;
            d->serversocketfd = serversocket_fd;
            d->path = config->path;
            d->con = con;
            d->config = config;
            d->config->s = s;
            d->config->archived_s = archived_s;

            // Enqueue the job to the pool and signify the waiting thread
            pthread_mutex_lock(&con->mutex);
            enqueue(d);
            pthread_cond_signal(&con->condition_var);
            pthread_mutex_unlock(&con->mutex);
        }
	}

    free(config->path);
    compress_dict_free(config->cd);
    binary_tree_destroy(config->root);
    session_array_free(s);
    session_array_free(archived_s);
    free(con);
    free(config);
    exit(0);
}

/* Read the server config info: ip address, port and directory path */
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

/* Build up compression dictionary and decompression tree */
void compression_reader(configuration_t *config) {
    FILE *f = fopen("compression.dict", "rb");
    fseek(f, 0, SEEK_END);
    long file_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *arr = (uint8_t*)malloc(file_len);
    fread(arr, 1, file_len, f);
    fclose(f);

    compress_dict_t *cd = compress_dict_init(); // Key-value pairs for compression
    binary_tree_node *root = new_empty(); //Binary tree for decompression

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
        // We must have read the padding at the end, break the loop
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
        compress_dict_add(cd, code, run_len);

        //The key(count) is assumed to be incremental starting from 0
        insert(root, count, code, run_len);

        count++;
    }

    config->cd = cd;
    config->root = root;
    arr_destroy(arr);
}

/* Thread pool controller to delegate work to the waiting threads */
void* thread_handler(void* arg) {
    server_controller_t *con = (server_controller_t*)arg;
    while (true) {
        connection_data_t* d;
        pthread_mutex_lock(&con->mutex);
        while ((d = dequeue()) == NULL) {
            pthread_cond_wait(&con->condition_var, &con->mutex); 
            if (con->__shutdown) { //If the server should be shut down, thread exits
                pthread_mutex_unlock(&con->mutex);
                pthread_exit(NULL);
            }
        }; 
        pthread_mutex_unlock(&con->mutex);
        
        //We have some work to do
        if (d != NULL) {
            connection_handler(d);
        }
    }
}
/* Thread work handler: reading data from the assigned client and react properly */
void* connection_handler(void* arg) {
	connection_data_t* d = (connection_data_t*) arg;
    
    bool stop = false;
    while(!stop) {

        // If the client refuses to connect, we disconnect
        if (message_header_reader(d) == 0) {
            break;
        }

        // Retrieve the message type
        uint8_t type = (d->msg.header >> 4 & 0xf) | 0x00;

        // All the functions below return 1 as success or 0 as failure
        switch (type) {
            case 0x00:
                if (!echo(d)){
                    stop = true;
                };
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
            case 0x06:
                if (!retrieve_file(d)) {
                    stop = true;
                }
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

    }
    close(d->socketfd);
    free(d);

	return NULL;
}

/* Read the first two field of the message: header and message length
   return 1 for success, otherwise 0 */
int message_header_reader(void* arg) {
    connection_data_t* d = (connection_data_t*) arg;
    // int flags = fcntl(d->socketfd, F_GETFL, 0); 
    // fcntl(d->socketfd, F_SETFL, flags | O_NONBLOCK);
    ssize_t nread = read(d->socketfd, &d->msg, sizeof(d->msg.header)+sizeof(d->msg.p_length));

    if (nread <= 0) {
        return 0;
    } else {
        return 1;
    }
}

/* Compress the given key and add it to compressed payload
    d: connection data which contains all the info about one connection
    compress_msg: compressed payload
    key: key to be compressed
    num_of_bytes: number of bytes of compressed payload
    num_of_bit: number of bits of compressed payload
*/
void compression_char(connection_data_t *d, uint8_t** compressed_msg, uint8_t key, uint64_t *num_of_bytes, uint64_t *num_of_bit) {
    uint8_t run_len = compress_dict_get(d->config->cd, key)->len; //Extract number of bits
    uint8_t code[4] = {0};
    memcpy(code, compress_dict_get(d->config->cd, key)->code, 4); //Extract compression code

    for (int j = 0; j < run_len; j++) {
                
        // We realloc one more byte each time when the current byte is full
        if (*num_of_bit == *num_of_bytes * 8) {
            *compressed_msg = realloc(*compressed_msg, ++(*num_of_bytes));
        }
        
        //Set -> 1 or clear -> 0 bit one each time
        if (get_bit(code, j) == 1) {
            set_bit(*compressed_msg, (*num_of_bit)++);
        } else {
            clear_bit(*compressed_msg, (*num_of_bit)++);
        }
    }
}

/* Send the compressed payload to the client
    d: connection data which contains all the info about one connection
    res: response to the client
    compress_msg: compressed payload
    header: header of the response
    num_of_bit: number of bits of compressed payload
    num_of_bytes: number of bytes of compressed payload
 */
void send_compression_msg(connection_data_t *d, connection_data_t *res, uint8_t **compressed_msg, 
                            uint8_t header, uint64_t *num_of_bit, uint64_t *num_of_bytes) {
    *num_of_bytes += 1;
    res->msg.header = header;
    set_bit(&res->msg.header, 4);
    res->msg.p_length = htobe64(*num_of_bytes);
    write(d->socketfd, &res->msg, sizeof(res->msg.header)+sizeof(res->msg.p_length));

    //Set the padding in the compressed payload
    uint8_t padding = (8-(*num_of_bit)%8) % 8;
       
    for (int i = 0; i < padding; i++) {
        clear_bit(*compressed_msg, (*num_of_bit)++);
    }
    write(d->socketfd, *compressed_msg, (*num_of_bytes)-1);
    write(d->socketfd, &padding, 1);

    free(*compressed_msg);
}

/* Decompress the payload from the client
    d: connection data which contains all the info about one connection
    original_msg: original compressed payload from the client
    decompression_msg: decompressed payload
    read_len: number of bits of the original payload
    padding: number of paddings of the original payload
*/

void decompression_msg(connection_data_t *d, uint8_t* original_msg, char **decompression_msg, 
                        uint64_t read_len, uint8_t padding) {
    uint8_t code[4] = {0};
    uint32_t run_len = 0;
    uint64_t cur_pos = 0;

    for (int i = 0; i < read_len*8-padding; i++) {
        //Copy bits from the original compressed message
        if (get_bit(original_msg, i) == 1) {
            set_bit(code, run_len++);
        } else {
            clear_bit(code, run_len++);
        }

        //We search the binary decompression tree and find the key, if exist(it is the leaf node)
        binary_tree_node *n = search(d->config->root, code, run_len);
        if (n != NULL) {
            if (n->defined) {
                *decompression_msg = realloc(*decompression_msg, cur_pos+1);
                char key = n->key;
                (*decompression_msg)[cur_pos++] = key;
                memset(code, 0, 4);
                run_len = 0; //reset run_len since we have found one key
            }
        }
    }
}

/* Message header 0x00, return 1 for success, otherwise 0
    Note: We don't care decompression here since we can write back
          compressed message directly
*/
int echo(void *arg) {
    connection_data_t* d = (connection_data_t*) arg;
    connection_data_t* res = (connection_data_t*)malloc(sizeof(connection_data_t)); 

    bool compression_bit = get_bit(&d->msg.header, 4) == 0x1; //if payload received is compressed
    bool require_compression = get_bit(&d->msg.header, 5) == 0x1; //if payload sent needs to be compressed

    if (require_compression && !compression_bit) {
        //need compression first
        uint64_t length = be64toh(d->msg.p_length);
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
    } else { // Directly send back payload (either compressed or not)
        res->msg.header = 0x10;
        if (compression_bit) {
            set_bit(&res->msg.header, 4);
        }
        res->msg.p_length = d->msg.p_length;
        uint64_t length = htobe64(res->msg.p_length);

        write(d->socketfd, &res->msg, sizeof(res->msg.header)+sizeof(res->msg.p_length));
        
        res->msg.payload = malloc(length);
        read(d->socketfd, res->msg.payload, length);
        write(d->socketfd, res->msg.payload, length);
    }   

    free(res->msg.payload);
    free(res);

    return 1;
}

/* List directory: return 1 for success, otherwise 0 
    Note: we don't care decompression here since payload length
          is guaranteed to be 0 */
int dir_list(void *arg) {
    connection_data_t* d = (connection_data_t*) arg;
    if (d->msg.p_length != 0) {
        error(d);
        return 0;
    }

    connection_data_t* res = (connection_data_t*)malloc(sizeof(connection_data_t)); 

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

        //If the directory is empty, send NULL byte only
        if (is_empty) {
            compression_char(d, &compressed_msg, '\0', &num_of_bytes, &num_of_bit);
            send_compression_msg(d, res, &compressed_msg, 0x30, &num_of_bit, &num_of_bytes);
        } else { //If the directory is not empty
            send_compression_msg(d, res, &compressed_msg, 0x30, &num_of_bit, &num_of_bytes);
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
            res->msg.p_length = htobe64(length);
            write(d->socketfd, &res->msg,  sizeof(res->msg.header)+sizeof(res->msg.p_length));
            write(d->socketfd, "\0", 1);

            free(res);
            return 1;
        }

        res->msg.header = 0x30;
        res->msg.p_length = htobe64(length);
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

/* Retrieve file size: return 1 for success, otherwise 0 */
int file_size_query(void *arg) {
    connection_data_t* d = (connection_data_t*) arg;
    connection_data_t* res = (connection_data_t*)malloc(sizeof(connection_data_t)); 

    bool compression_bit = get_bit(&d->msg.header, 4) == 0x1; //if payload received is compressed
    bool require_compression = get_bit(&d->msg.header, 5) == 0x1; //if payload sent needs to be compressed

    char *filename;
    if (compression_bit) {
        //Decode first
        uint64_t read_len = be64toh(d->msg.p_length)-1;
        char *decompressed_msg = malloc(1);

        res->msg.payload = malloc(read_len);
        read(d->socketfd, res->msg.payload, read_len);
        uint8_t padding;
        read(d->socketfd, &padding, 1);

        decompression_msg(d, res->msg.payload, &decompressed_msg, read_len, padding);
        filename = decompressed_msg;
        
        free(res->msg.payload);
    } else {
        uint64_t read_len = be64toh(d->msg.p_length);
        filename = (char*)malloc(read_len);
        read(d->socketfd, filename, read_len);
    }

    struct stat sb;
    DIR *dir;
    struct dirent *file;

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

    //Reading ends

    if (require_compression) {
        uint64_t num_of_bit = 0;
        uint64_t num_of_bytes = 1;
        uint8_t *compressed_msg = malloc(1);

        file_len = htobe64(file_len);
        for (int i = 0; i < 8; i++) {
            uint8_t key = *((uint8_t*)(&file_len) + i);
            compression_char(d, &compressed_msg, key, &num_of_bytes, &num_of_bit);
        }
        
        send_compression_msg(d, res, &compressed_msg, 0x50, &num_of_bit, &num_of_bytes);

        free(filename);
        free(res); 
    } else {
        uint64_t num_len = 8;
        file_len = htobe64(file_len);

        res->msg.header = 0x50;
        res->msg.p_length = htobe64(num_len);
        write(d->socketfd, &res->msg, sizeof(res->msg.header)+sizeof(res->msg.p_length));
        write(d->socketfd, &file_len, num_len);

        free(filename);
        free(res);  
    }

    return 1;

}

/* Retrieve file content: return 1 for success, otherwise 0 */
int retrieve_file(connection_data_t *arg) {
    connection_data_t* d = (connection_data_t*) arg;
    connection_data_t* res = (connection_data_t*)malloc(sizeof(connection_data_t)); 

    bool compression_bit = get_bit(&d->msg.header, 4) == 0x1; //if payload received is compressed
    bool require_compression = get_bit(&d->msg.header, 5) == 0x1; //if payload sent needs to be compressed

    uint32_t session; //Session ID
    uint64_t start; //Start offset of the file
    uint64_t len; //Length of the required file data
    char *filename;

    if (compression_bit) {
        //Decode first
        uint64_t read_len = be64toh(d->msg.p_length)-1;
        char *decompressed_msg = malloc(1);

        res->msg.payload = malloc(read_len);
        read(d->socketfd, res->msg.payload, read_len);
        uint8_t padding;
        read(d->socketfd, &padding, 1);

        decompression_msg(d, res->msg.payload, &decompressed_msg, read_len, padding);

        memcpy(&session, decompressed_msg, 4);

        memcpy(&start, decompressed_msg+4, 8);
        start = be64toh(start);

        memcpy(&len, decompressed_msg+12, 8);
        len = be64toh(len);

        filename = strdup(decompressed_msg+20);

        free(decompressed_msg);
        free(res->msg.payload);
    } else {
        read(d->socketfd, &session, sizeof(uint32_t));

        read(d->socketfd, &start, sizeof(uint64_t));
        start = be64toh(start);

        read(d->socketfd, &len, sizeof(uint64_t));
        len = be64toh(len);

        filename = malloc(be64toh(d->msg.p_length));
        read(d->socketfd, filename, be64toh(d->msg.p_length)-20);
    }

    struct stat sb;
    DIR *dir;
    struct dirent *file;

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

    if (start + len > file_len) {
        error(d);
        free(filename);
        free(res);
        return 0;
    }

    char *path = malloc(strlen(d->path)+3+strlen(filename));
    strcpy(path, d->path);
    strcat(path, "/");
    strcat(path, filename);

    //Read target file
    FILE *fd;
    if ((fd = fopen(path, "r")) == NULL) {
        error(d);

        free(path);
        free(filename);
        free(res);
        return 0;
    }

    fseek(fd, start, SEEK_SET);
    uint8_t *file_content = malloc(len);
    fread(file_content, 1, len, fd);

    // Session id cannot be reused with the same file with same byte range 
    if (session_array_is_in_archive(d->config->archived_s, session, start, len, path)) {
        error(d);

        fclose(fd);
        free(filename);
        free(file_content);
        free(path);
        free(res);

        return 0;
    }

    if (session_array_is_in_active(d->config->s, session, start, len, path)) {
        res->msg.header = 0x70;
        res->msg.p_length = 0;
        write(d->socketfd, &res->msg, sizeof(res->msg.header)+sizeof(res->msg.p_length));

        fclose(fd);
        free(filename);
        free(file_content);
        free(path);
        free(res);

        return 1;
    }

    //End of reading

    //Add to the session list
    session_array_add(d->config->s, session, start, len, path);

    if (require_compression) {
        //Need compression
        uint64_t num_of_bit = 0;
        uint64_t num_of_bytes = 1;
        uint8_t *compressed_msg = malloc(1);

        for (int i = 0; i < 4; i++) {
            uint8_t key = *((uint8_t*)&session+i);   
            compression_char(d, &compressed_msg, key, &num_of_bytes, &num_of_bit);
        }
        start = htobe64(start);
        for (int i = 0; i < 8; i++) {
            uint8_t key = *((uint8_t*)&start+i);   
            compression_char(d, &compressed_msg, key, &num_of_bytes, &num_of_bit);
        }
        uint64_t len_temp = htobe64(len);
        for (int i = 0; i < 8; i++) {
            uint8_t key = *((uint8_t*)&len_temp+i);  
            compression_char(d, &compressed_msg, key, &num_of_bytes, &num_of_bit);
        }
        for (int i = 0; i < len; i++) {
            compression_char(d, &compressed_msg, file_content[i], &num_of_bytes, &num_of_bit);
        }

        send_compression_msg(d, res, &compressed_msg, 0x70, &num_of_bit, &num_of_bytes);
    } else {
        res->msg.header = 0x70;
        res->msg.p_length = htobe64(len+20);
        write(d->socketfd, &res->msg, sizeof(res->msg.header)+sizeof(res->msg.p_length));

        write(d->socketfd, &session, 4);
        start = htobe64(start);
        write(d->socketfd, &start, 8);
        long len_temp = htobe64(len);
        write(d->socketfd, &len_temp, 8);
        write(d->socketfd, file_content, len);
    }

    start = be64toh(start);

    //Delete from the active session array
    session_array_delete(d->config->s, session, start, len, path);
    
    //Add to archive session array which should not be used later
    session_array_add(d->config->archived_s, session, start, len, path);

    fclose(fd);
    free(filename);
    free(file_content);
    free(path);
    free(res);
     
    return 1;
}

/* Send the shutdown signal to the server and all threads */
void server_shutdown(void *arg) {
    connection_data_t* d = (connection_data_t*) arg;
    d->con->__shutdown = true;
    close(d->socketfd);
    pthread_cond_broadcast(&d->con->condition_var); //Signal to all waiting threads
    shutdown(d->serversocketfd, SHUT_RDWR);
}

/* Error: always right and does not need to be compressed */
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