CC=gcc
CFLAGS=-g -O2 -Wall -Werror -I. -g -lpthread

all: client server

client: client.c message.c message.h
	$(CC) -o $@ client.c message.c $(CFLAGS)

server: server.c joblist.c joblist.h message.c message.h
	$(CC) -o $@ server.c message.c joblist.c $(CFLAGS)

sample: b.out c.out

%.out: %.c
	$(CC) -o $@ $<

clean:
	rm -f b.out
	rm -f c.out
	rm -f client
	rm -f server
	rm -f socket_pipe