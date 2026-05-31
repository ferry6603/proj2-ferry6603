#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../common/protocol.h"
#include "../common/netutil.h"
#include "engine.h"
#include "game.h"

#define READ_BUF_SIZE (PROTO_MAX_FRAME + 8)

typedef struct ClientBuffer {
    bool in_use;
    int client_index;
    uint8_t data[READ_BUF_SIZE];
    size_t len;
} ClientBuffer;

static ClientBuffer client_bufs[MAX_CLIENTS];

static Engine *global_engine = NULL;

static void shutdown_server(int sig) {
    (void)sig;
    if (global_engine != NULL) {
        engine_free(global_engine);
        global_engine = NULL;
    }
    _exit(0);
}

static int find_buf_by_fd(ServerState *state, int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_bufs[i].in_use && state->clients[client_bufs[i].client_index].fd == fd) {
            return i;
        }
    }
    return -1;
}

static int alloc_buf(int client_index) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!client_bufs[i].in_use) {
            client_bufs[i].in_use = true;
            client_bufs[i].client_index = client_index;
            client_bufs[i].len = 0;
            return i;
        }
    }
    return -1;
}

static void free_buf(int bi) {
    if (bi >= 0 && bi < MAX_CLIENTS) {
        client_bufs[bi].in_use = false;
        client_bufs[bi].len = 0;
    }
}

static void reap_stale_bufs(ServerState *state) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!client_bufs[i].in_use) {
            continue;
        }
        int ci = client_bufs[i].client_index;
        if (ci < 0 || ci >= MAX_CLIENTS || !state->clients[ci].active) {
            free_buf(i);
        }
    }
}

static void process_client_buffer(ServerState *state, int bi) {
    ClientBuffer *cb = &client_bufs[bi];
    while (cb->len >= 4) {
        uint32_t be;
        memcpy(&be, cb->data, 4);
        uint32_t total = ntohl(be);
        if (total < 1 || total > PROTO_MAX_FRAME) {
            server_on_client_disconnect(state, cb->client_index);
            free_buf(bi);
            return;
        }
        size_t frame_size = 4 + total;
        if (cb->len < frame_size) {
            return;
        }

        uint8_t type = cb->data[4];
        const uint8_t *payload = cb->data + 5;
        uint32_t plen = total - 1;

        server_handle_client_message(state, cb->client_index, type, payload, plen);

        if (!cb->in_use) {
            return;
        }

        memmove(cb->data, cb->data + frame_size, cb->len - frame_size);
        cb->len -= frame_size;
    }
}

static void handle_client_read(ServerState *state, int ci) {
    int bi = find_buf_by_fd(state, state->clients[ci].fd);
    if (bi < 0) {
        return;
    }
    ClientBuffer *cb = &client_bufs[bi];
    if (cb->len >= READ_BUF_SIZE) {
        server_on_client_disconnect(state, ci);
        free_buf(bi);
        return;
    }

    ssize_t n = read(state->clients[ci].fd, cb->data + cb->len,
                     READ_BUF_SIZE - cb->len);
    if (n <= 0) {
        server_on_client_disconnect(state, ci);
        free_buf(bi);
        return;
    }
    cb->len += (size_t)n;
    process_client_buffer(state, bi);
}

static int create_listen_socket(uint16_t port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        close(sockfd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "bind failed on port %u: %s\n", port, strerror(errno));
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 32) < 0) {
        fprintf(stderr, "listen failed on port %u: %s\n", port, strerror(errno));
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        return 1;
    }

    char *end = NULL;
    long port_long = strtol(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || port_long <= 0 || port_long > 65535) {
        return 1;
    }
    uint16_t port = (uint16_t)port_long;

    Engine *engine = engine_init();
    if (engine == NULL) {
        return 2;
    }
    global_engine = engine;

    signal(SIGTERM, shutdown_server);
    signal(SIGINT, shutdown_server);
    signal(SIGPIPE, SIG_IGN);

    int listen_fd = create_listen_socket(port);
    if (listen_fd < 0) {
        engine_free(engine);
        return 3;
    }

    ServerState state;
    server_state_init(&state, engine, listen_fd);

    fprintf(stderr,
            "Battleship server listening on 0.0.0.0:%u (TCP — use ./client, not a "
            "browser)\n",
            port);

    for (;;) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listen_fd, &readfds);
        int maxfd = listen_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (state.clients[i].active && state.clients[i].fd >= 0) {
                FD_SET(state.clients[i].fd, &readfds);
                if (state.clients[i].fd > maxfd) {
                    maxfd = state.clients[i].fd;
                }
            }
        }

        int ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            continue;
        }

        if (FD_ISSET(listen_fd, &readfds)) {
            int fd = accept(listen_fd, NULL, NULL);
            if (fd < 0) {
                continue;
            }
            if (!server_handle_new_connection(&state, fd)) {
                continue;
            }
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (state.clients[i].active && state.clients[i].fd == fd) {
                    alloc_buf(i);
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!state.clients[i].active || state.clients[i].fd < 0) {
                continue;
            }
            if (FD_ISSET(state.clients[i].fd, &readfds)) {
                handle_client_read(&state, i);
            }
        }

        reap_stale_bufs(&state);
    }

    return 0;
}
