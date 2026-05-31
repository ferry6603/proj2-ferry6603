#include "game.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/netutil.h"
#include "../common/protocol.h"
#include "engine.h"

/* Required for Task 5 marking when multiple concurrent games are supported. */
static const char MULTIPLE_GAMES[] = "MULTIPLE_GAMES";

static pthread_mutex_t engine_lock = PTHREAD_MUTEX_INITIALIZER;

static void close_client_fd(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

static bool send_to_client(const Client *c, uint8_t type, const void *payload,
                           uint32_t len) {
    if (!c->active || c->fd < 0) {
        return false;
    }
    return proto_send_frame(c->fd, type, payload, len);
}

static bool send_empty(const Client *c, uint8_t type) {
    return send_to_client(c, type, NULL, 0);
}

static Game *get_game(ServerState *state, int gi) {
    if (gi < 0 || gi >= MAX_GAMES || !state->games[gi].active) {
        return NULL;
    }
    return &state->games[gi];
}

static Client *get_client(ServerState *state, int ci) {
    if (ci < 0 || ci >= MAX_CLIENTS || !state->clients[ci].active) {
        return NULL;
    }
    return &state->clients[ci];
}

void server_state_init(ServerState *state, Engine *engine, int listen_fd) {
    memset(state, 0, sizeof(*state));
    state->engine = engine;
    state->listen_fd = listen_fd;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        state->clients[i].fd = -1;
    }
    (void)MULTIPLE_GAMES;
}

int server_find_free_client(ServerState *state) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!state->clients[i].active) {
            return i;
        }
    }
    return -1;
}

int server_find_game(ServerState *state, uint32_t game_id) {
    for (int i = 0; i < MAX_GAMES; i++) {
        if (state->games[i].active && state->games[i].game_id == game_id) {
            return i;
        }
    }
    return -1;
}

int server_create_game(ServerState *state, uint32_t game_id) {
    for (int i = 0; i < MAX_GAMES; i++) {
        if (!state->games[i].active) {
            Game *g = &state->games[i];
            memset(g, 0, sizeof(*g));
            g->active = true;
            g->game_id = game_id;
            g->phase = PHASE_WAITING;
            g->client_slots[0] = -1;
            g->client_slots[1] = -1;
            g->current_turn = 1;
            return i;
        }
    }
    return -1;
}

void server_remove_client(ServerState *state, int ci) {
    Client *c = get_client(state, ci);
    if (c == NULL) {
        return;
    }
    close_client_fd(c->fd);
    c->fd = -1;
    c->active = false;
    c->phase = CLIENT_DEAD;
    if (state->num_clients > 0) {
        state->num_clients--;
    }
}

void server_end_game(ServerState *state, int gi, bool notify) {
    Game *g = get_game(state, gi);
    if (g == NULL) {
        return;
    }

    if (notify) {
        for (int p = 0; p < 2; p++) {
            Client *c = get_client(state, g->client_slots[p]);
            if (c != NULL) {
                send_empty(c, MSG_ERROR);
            }
        }
    }

    for (int p = 0; p < 2; p++) {
        int slot = g->client_slots[p];
        if (slot >= 0) {
            Client *c = get_client(state, slot);
            if (c != NULL) {
                c->game_index = -1;
                c->phase = CLIENT_DEAD;
                close_client_fd(c->fd);
                c->fd = -1;
                c->active = false;
                if (state->num_clients > 0) {
                    state->num_clients--;
                }
            }
            g->client_slots[p] = -1;
        }
    }

    pthread_mutex_lock(&engine_lock);
    engine_end_game(state->engine, g->game_id);
    pthread_mutex_unlock(&engine_lock);

    g->active = false;
}

static void notify_ready(ServerState *state, int gi) {
    Game *g = get_game(state, gi);
    if (g == NULL) {
        return;
    }
    for (int p = 0; p < 2; p++) {
        Client *c = get_client(state, g->client_slots[p]);
        if (c != NULL) {
            c->phase = CLIENT_PLACEMENT;
            send_empty(c, MSG_READY);
        }
    }
}

static void send_ships_ack(ServerState *state, int gi) {
    Game *g = get_game(state, gi);
    if (g == NULL) {
        return;
    }
    for (int p = 0; p < 2; p++) {
        Client *c = get_client(state, g->client_slots[p]);
        if (c != NULL) {
            uint8_t num = (uint8_t)(p + 1);
            send_to_client(c, MSG_SHIPS_ACK, &num, 1);
            c->phase = CLIENT_PLAYING;
        }
    }
    g->phase = PHASE_PLAYING;
    g->current_turn = 1;
}

static void handle_ships(ServerState *state, int ci, const uint8_t *payload,
                         uint32_t len) {
    Client *c = get_client(state, ci);
    if (c == NULL || c->game_index < 0) {
        return;
    }
    Game *g = get_game(state, c->game_index);
    if (g == NULL || g->phase != PHASE_PLACEMENT) {
        send_empty(c, MSG_ERROR);
        return;
    }

    struct Ship ships[4];
    char coord_bufs[4][PROTO_MAX_COORD + 1];
    if (!proto_read_ships(payload, len, ships, coord_bufs)) {
        server_end_game(state, c->game_index, true);
        return;
    }

    pthread_mutex_lock(&engine_lock);
    int8_t result = engine_place_ships(state->engine, g->game_id, &ships);
    pthread_mutex_unlock(&engine_lock);

    if (result < 0) {
        server_end_game(state, c->game_index, true);
        return;
    }

    int slot = c->player_num - 1;
    if (slot < 0 || slot > 1 || g->ships_received[slot]) {
        send_empty(c, MSG_SHIPS_FAIL);
        return;
    }

    g->ships_received[slot] = true;

    if (g->ships_received[0] && g->ships_received[1]) {
        send_ships_ack(state, c->game_index);
    }
}

static void notify_opponent_move(ServerState *state, int gi, int mover_slot,
                                 const char *coordinate, int8_t result) {
    Game *g = get_game(state, gi);
    if (g == NULL) {
        return;
    }
    int opp_slot = 1 - mover_slot;
    Client *opp = get_client(state, g->client_slots[opp_slot]);
    if (opp == NULL) {
        return;
    }
    uint8_t buf[PROTO_MAX_COORD + 4];
    uint32_t blen = 0;
    if (!proto_write_opp_move(buf, sizeof(buf), &blen, coordinate, result)) {
        return;
    }
    send_to_client(opp, MSG_OPP_MOVE, buf, blen);
}

static void handle_move(ServerState *state, int ci, const uint8_t *payload,
                        uint32_t len, bool extended) {
    Client *c = get_client(state, ci);
    if (c == NULL || c->game_index < 0) {
        return;
    }
    Game *g = get_game(state, c->game_index);
    if (g == NULL || g->phase != PHASE_PLAYING) {
        send_empty(c, MSG_ERROR);
        return;
    }

    char coord[PROTO_MAX_COORD + 1];
    if (!proto_read_coord(payload, len, coord, sizeof(coord))) {
        server_end_game(state, c->game_index, true);
        return;
    }

    int slot = c->player_num - 1;

    if (extended) {
        ExtendedTurnResult ext;
        pthread_mutex_lock(&engine_lock);
        ext = engine_take_turn_extended(state->engine, g->game_id,
                                        c->player_num, coord);
        pthread_mutex_unlock(&engine_lock);

        if (ext.length == 0 && ext.data == NULL) {
            uint8_t zero[2] = {0, 0};
            send_to_client(c, MSG_MOVE_EXT_RESULT, zero, 2);
            return;
        }

        TurnResult tr;
        pthread_mutex_lock(&engine_lock);
        tr = engine_extract_turn_result(ext);
        pthread_mutex_unlock(&engine_lock);

        uint8_t buf[PROTO_MAX_FRAME];
        uint32_t blen = 0;
        if (!proto_write_ext_result(buf, sizeof(buf), &blen, ext.length,
                                    ext.data)) {
            pthread_mutex_lock(&engine_lock);
            engine_free_extended_result(ext);
            pthread_mutex_unlock(&engine_lock);
            server_end_game(state, c->game_index, true);
            return;
        }

        if (!send_to_client(c, MSG_MOVE_EXT_RESULT, buf, blen)) {
            pthread_mutex_lock(&engine_lock);
            engine_free_extended_result(ext);
            pthread_mutex_unlock(&engine_lock);
            server_end_game(state, c->game_index, true);
            return;
        }

        notify_opponent_move(state, c->game_index, slot, coord, (int8_t)tr);

        pthread_mutex_lock(&engine_lock);
        engine_free_extended_result(ext);
        pthread_mutex_unlock(&engine_lock);

        if (tr == Win) {
            g->phase = PHASE_WAITING;
            pthread_mutex_lock(&engine_lock);
            engine_end_game(state->engine, g->game_id);
            pthread_mutex_unlock(&engine_lock);
            for (int p = 0; p < 2; p++) {
                Client *pc = get_client(state, g->client_slots[p]);
                if (pc != NULL) {
                    send_empty(pc, MSG_GAME_OVER);
                    pc->phase = CLIENT_DEAD;
                    close_client_fd(pc->fd);
                    pc->fd = -1;
                    pc->active = false;
                    g->client_slots[p] = -1;
                    if (state->num_clients > 0) {
                        state->num_clients--;
                    }
                }
            }
            g->active = false;
            return;
        }

        g->current_turn = (g->current_turn == 1) ? 2 : 1;
        return;
    }

    TurnResult tr;
    pthread_mutex_lock(&engine_lock);
    tr = engine_take_turn(state->engine, g->game_id, c->player_num, coord);
    pthread_mutex_unlock(&engine_lock);

    if (tr == Invalid) {
        int8_t result_byte = (int8_t)Invalid;
        send_to_client(c, MSG_MOVE_RESULT, &result_byte, 1);
        return;
    }

    int8_t result_byte = (int8_t)tr;
    if (!send_to_client(c, MSG_MOVE_RESULT, &result_byte, 1)) {
        server_end_game(state, c->game_index, true);
        return;
    }

    notify_opponent_move(state, c->game_index, slot, coord, result_byte);

    if (tr == Win) {
        g->phase = PHASE_WAITING;
        pthread_mutex_lock(&engine_lock);
        engine_end_game(state->engine, g->game_id);
        pthread_mutex_unlock(&engine_lock);
        for (int p = 0; p < 2; p++) {
            Client *pc = get_client(state, g->client_slots[p]);
            if (pc != NULL) {
                send_empty(pc, MSG_GAME_OVER);
                pc->phase = CLIENT_DEAD;
                close_client_fd(pc->fd);
                pc->fd = -1;
                pc->active = false;
                g->client_slots[p] = -1;
                if (state->num_clients > 0) {
                    state->num_clients--;
                }
            }
        }
        g->active = false;
        return;
    }

    g->current_turn = (g->current_turn == 1) ? 2 : 1;
}

void server_handle_client_message(ServerState *state, int ci, uint8_t type,
                                  const uint8_t *payload, uint32_t len) {
    Client *c = get_client(state, ci);
    if (c == NULL) {
        return;
    }

    switch (type) {
    case MSG_SHIPS:
        handle_ships(state, ci, payload, len);
        break;
    case MSG_MOVE:
        handle_move(state, ci, payload, len, false);
        break;
    case MSG_MOVE_EXT:
        handle_move(state, ci, payload, len, true);
        break;
    default:
        if (c->game_index >= 0) {
            server_end_game(state, c->game_index, true);
        } else {
            send_empty(c, MSG_ERROR);
            server_remove_client(state, ci);
        }
        break;
    }
}

bool server_handle_new_connection(ServerState *state, int fd) {
    uint32_t game_id = 0;
    if (!read_u32_be(fd, &game_id)) {
        close(fd);
        return false;
    }

    int gi = server_find_game(state, game_id);
    if (gi < 0) {
        gi = server_create_game(state, game_id);
        if (gi < 0) {
            proto_send_empty(fd, MSG_JOIN_FAIL);
            close(fd);
            return false;
        }
        pthread_mutex_lock(&engine_lock);
        bool ok = engine_init_game(state->engine, game_id);
        pthread_mutex_unlock(&engine_lock);
        if (!ok) {
            state->games[gi].active = false;
            proto_send_empty(fd, MSG_JOIN_FAIL);
            close(fd);
            return false;
        }
    }

    Game *g = &state->games[gi];
    int slot = -1;
    if (g->client_slots[0] < 0) {
        slot = 0;
    } else if (g->client_slots[1] < 0) {
        slot = 1;
    } else {
        proto_send_empty(fd, MSG_JOIN_FAIL);
        close(fd);
        return false;
    }

    int ci = server_find_free_client(state);
    if (ci < 0) {
        proto_send_empty(fd, MSG_JOIN_FAIL);
        close(fd);
        return false;
    }

    Client *c = &state->clients[ci];
    memset(c, 0, sizeof(*c));
    c->active = true;
    c->fd = fd;
    c->game_id = game_id;
    c->player_num = (uint8_t)(slot + 1);
    c->phase = CLIENT_WAITING;
    c->game_index = gi;
    state->num_clients++;

    g->client_slots[slot] = ci;

    if (!proto_send_empty(fd, MSG_JOIN_OK)) {
        g->client_slots[slot] = -1;
        server_remove_client(state, ci);
        return false;
    }

    if (g->client_slots[0] >= 0 && g->client_slots[1] >= 0) {
        g->phase = PHASE_PLACEMENT;
        notify_ready(state, gi);
    }

    return true;
}

void server_on_client_disconnect(ServerState *state, int ci) {
    Client *c = get_client(state, ci);
    if (c == NULL) {
        return;
    }
    int gi = c->game_index;
    if (gi >= 0) {
        server_end_game(state, gi, true);
    } else {
        server_remove_client(state, ci);
    }
}
