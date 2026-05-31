#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client.h"

#include "../common/netutil.h"
#include "../common/protocol.h"

typedef struct Client {
    int fd;
    uint32_t game_id;
    bool connected;
    bool ready;
} Client;

static bool recv_frame(Client *c, uint8_t *type, uint8_t *payload,
                       uint32_t *payload_len) {
    if (c == NULL || c->fd < 0) {
        return false;
    }
    return proto_read_frame(c->fd, type, payload, payload_len);
}

ClientImplementation *client_init(void) {
    static bool sigpipe_ignored = false;
    if (!sigpipe_ignored) {
        signal(SIGPIPE, SIG_IGN);
        sigpipe_ignored = true;
    }

    Client *c = malloc(sizeof(Client));
    if (c == NULL) {
        return NULL;
    }
    c->fd = -1;
    c->game_id = 0;
    c->connected = false;
    c->ready = false;
    return (ClientImplementation *)c;
}

bool client_connect(ClientImplementation *client, const char *addr,
                    uint16_t port, uint32_t game_id) {
    if (client == NULL) {
        return false;
    }
    Client *c = (Client *)client;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", port);

    struct addrinfo *res = NULL;
    if (getaddrinfo(addr, port_str, &hints, &res) != 0) {
        return false;
    }

    int fd = -1;
    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd < 0) {
        return false;
    }

    c->fd = fd;
    c->game_id = game_id;

    if (!write_u32_be(c->fd, game_id)) {
        close(c->fd);
        c->fd = -1;
        return false;
    }

    uint8_t payload[PROTO_MAX_FRAME];
    uint32_t plen = 0;
    uint8_t type = 0;
    if (!recv_frame(c, &type, payload, &plen)) {
        close(c->fd);
        c->fd = -1;
        return false;
    }

    if (type == MSG_JOIN_FAIL || type == MSG_ERROR) {
        close(c->fd);
        c->fd = -1;
        return false;
    }
    if (type != MSG_JOIN_OK) {
        close(c->fd);
        c->fd = -1;
        return false;
    }

    c->connected = true;
    return true;
}

bool client_wait_for_opponent(ClientImplementation *client) {
    if (client == NULL) {
        return false;
    }
    Client *c = (Client *)client;
    if (!c->connected || c->fd < 0) {
        return false;
    }

    uint8_t payload[PROTO_MAX_FRAME];
    uint32_t plen = 0;
    uint8_t type = 0;
    if (!recv_frame(c, &type, payload, &plen)) {
        return false;
    }

    if (type == MSG_ERROR || type == MSG_JOIN_FAIL) {
        return false;
    }
    if (type != MSG_READY) {
        return false;
    }

    c->ready = true;
    return true;
}

int8_t client_send_ships(ClientImplementation *client,
                         const struct Ship (*ships)[4]) {
    if (client == NULL || ships == NULL) {
        return -1;
    }
    Client *c = (Client *)client;
    if (!c->connected || c->fd < 0) {
        return -1;
    }

    uint8_t buf[256];
    uint32_t blen = 0;
    if (!proto_write_ships(buf, sizeof(buf), &blen, &(*ships)[0])) {
        return -1;
    }

    if (!proto_send_frame(c->fd, MSG_SHIPS, buf, blen)) {
        return -1;
    }

    uint8_t payload[PROTO_MAX_FRAME];
    uint32_t plen = 0;
    uint8_t type = 0;
    if (!recv_frame(c, &type, payload, &plen)) {
        return -1;
    }

    if (type == MSG_SHIPS_FAIL || type == MSG_ERROR) {
        return -1;
    }
    if (type != MSG_SHIPS_ACK) {
        return -1;
    }

    uint8_t player_num = 0;
    if (!proto_read_u8_payload(payload, plen, &player_num)) {
        return -1;
    }
    return (int8_t)player_num;
}

TurnResult client_send_move(ClientImplementation *client,
                            const char *coordinate) {
    if (client == NULL || coordinate == NULL) {
        return Invalid;
    }
    Client *c = (Client *)client;
    if (!c->connected || c->fd < 0) {
        return Invalid;
    }

    uint8_t buf[PROTO_MAX_COORD + 4];
    uint32_t blen = 0;
    if (!proto_write_coord(buf, sizeof(buf), &blen, coordinate)) {
        return Invalid;
    }

    if (!proto_send_frame(c->fd, MSG_MOVE, buf, blen)) {
        return Invalid;
    }

    uint8_t payload[PROTO_MAX_FRAME];
    uint32_t plen = 0;
    uint8_t type = 0;
    if (!recv_frame(c, &type, payload, &plen)) {
        return Invalid;
    }

    if (type == MSG_ERROR) {
        return Invalid;
    }
    if (type != MSG_MOVE_RESULT) {
        return Invalid;
    }

    int8_t result = Invalid;
    if (!proto_read_i8_payload(payload, plen, &result)) {
        return Invalid;
    }
    return (TurnResult)result;
}

ExtendedTurnResult client_send_move_extended(ClientImplementation *client,
                                             const char *coordinate) {
    ExtendedTurnResult empty = {.length = 0, .data = NULL};
    if (client == NULL || coordinate == NULL) {
        return empty;
    }
    Client *c = (Client *)client;
    if (!c->connected || c->fd < 0) {
        return empty;
    }

    uint8_t buf[PROTO_MAX_COORD + 4];
    uint32_t blen = 0;
    if (!proto_write_coord(buf, sizeof(buf), &blen, coordinate)) {
        return empty;
    }

    if (!proto_send_frame(c->fd, MSG_MOVE_EXT, buf, blen)) {
        return empty;
    }

    uint8_t payload[PROTO_MAX_FRAME];
    uint32_t plen = 0;
    uint8_t type = 0;
    if (!recv_frame(c, &type, payload, &plen)) {
        return empty;
    }

    if (type == MSG_ERROR) {
        return empty;
    }
    if (type != MSG_MOVE_EXT_RESULT) {
        return empty;
    }

    uint16_t data_len = 0;
    const void *data_ptr = NULL;
    if (!proto_read_ext_result(payload, plen, &data_len, &data_ptr)) {
        return empty;
    }

    void *copy = NULL;
    if (data_len > 0) {
        copy = malloc(data_len);
        if (copy == NULL) {
            abort();
        }
        memcpy(copy, data_ptr, data_len);
    }

    ExtendedTurnResult result = {.length = data_len, .data = copy};
    return result;
}

MoveResult client_receive_move(ClientImplementation *client) {
    MoveResult fail = {.coordinate = NULL, .result = Invalid};
    if (client == NULL) {
        return fail;
    }
    Client *c = (Client *)client;
    if (!c->connected || c->fd < 0) {
        return fail;
    }

    uint8_t payload[PROTO_MAX_FRAME];
    uint32_t plen = 0;
    uint8_t type = 0;
    if (!recv_frame(c, &type, payload, &plen)) {
        return fail;
    }

    if (type == MSG_ERROR) {
        return fail;
    }
    if (type != MSG_OPP_MOVE) {
        return fail;
    }

    char coord_buf[PROTO_MAX_COORD + 1];
    int8_t result = Invalid;
    if (!proto_read_opp_move(payload, plen, coord_buf, sizeof(coord_buf),
                             &result)) {
        return fail;
    }

    char *coord_copy = malloc(strlen(coord_buf) + 1);
    if (coord_copy == NULL) {
        abort();
    }
    strcpy(coord_copy, coord_buf);

    MoveResult mr = {.coordinate = coord_copy, .result = (TurnResult)result};
    return mr;
}

void client_free_extended_result(ExtendedTurnResult result) {
    free((void *)result.data);
}

void client_free_move_result(MoveResult result) {
    free((void *)result.coordinate);
}

void client_close(ClientImplementation *client) {
    if (client == NULL) {
        return;
    }
    Client *c = (Client *)client;
    if (c->fd >= 0) {
        close(c->fd);
        c->fd = -1;
    }
    c->connected = false;
    c->ready = false;
    free(c);
}
