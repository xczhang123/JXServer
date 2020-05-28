CC=gcc
CFLAGS=-O0 -Wall -Werror -Werror=vla -std=gnu11 -lm -lpthread -lrt
CFLAG_SAN=$(CFLAGS) -fsanitize=address -g
DEPS=
OBJ=queue.o binary_tree.o compress_dict.o bit_array.o
TARGET=server

all: $(TARGET)

server: server.c $(OBJ)
	$(CC) -o $@ $^ $(CFLAG_SAN)

test: test.c $(OBJ)
	$(CC) -o $@ $^ $(CFLAG_SAN)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAG_SAN)

client: client-scaffold.c
	$(CC) -o $@ $< $(CFLAG_SAN)

# binary_tree.o: binary_tree.c
# 	$(CC) -c -o $@ $< $(CFLAG_SAN)
	
# bit_array.o: bit_array.c
# 	$(CC) -c -o $@ $< $(CFLAG_SAN)

# compress_dict.o: compress_dict.c
# 	$(CC) -c -o $@ $< $(CFLAG_SAN)

.PHONY: clean

clean:
	rm -f *.o
	rm -f server
	rm -f test
	rm -f client
