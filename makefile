CC = gcc
CFLAGS = -g -I /usr/local/include -I .
LDFLAGS = -L /usr/local/lib -lmsquic

CLIENT_SRC = client.c
SERVER_SRC = server.c

CLIENT_OBJ = $(CLIENT_SRC:.c=.o)
SERVER_OBJ = $(SERVER_SRC:.c=.o)

all: client server

client: $(CLIENT_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

server: $(SERVER_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f client server $(CLIENT_OBJ) $(SERVER_OBJ)

