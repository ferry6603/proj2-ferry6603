CC=cc
CFLAGS=-Wall -Wextra -std=c11 -I.
LDFLAGS=-pthread

LIBENGINE=project2-bin/libengine.a
LIBRUNNER=project2-bin/librunner.a

COMMON_SRCS=common/netutil.c common/protocol.c
COMMON_OBJS=common/netutil.o common/protocol.o

CLIENT_OBJS=client_c/client.o $(COMMON_OBJS)
SERVER_OBJS=server_c/server.o server_c/game.o $(COMMON_OBJS)

all: client.a server.a

client: $(LIBRUNNER) client.a
	$(CC) $(CFLAGS) -o $@ $(LIBRUNNER) client.a $(LDFLAGS)

server: server.a $(LIBENGINE)
	$(CC) $(CFLAGS) -o $@ server.a $(LIBENGINE) $(LDFLAGS)

client.a: $(CLIENT_OBJS)
	rm -f $@
	ar rcs $@ $^

server.a: $(SERVER_OBJS)
	rm -f $@
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f client server client.a server.a
	rm -f client_c/*.o server_c/*.o common/*.o

.PHONY: all clean
