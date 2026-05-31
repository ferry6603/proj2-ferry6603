#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>

#include "engine.h"

#define MAX_CLIENTS 64
#define MAX_GAMES 32

enum GamePhase {
    PHASE_WAITING = 0,
    PHASE_PLACEMENT = 1,
    PHASE_PLAYING = 2,
};

enum ClientPhase {
    CLIENT_CONNECTING = 0,
    CLIENT_WAITING = 1,
    CLIENT_PLACEMENT = 2,
    CLIENT_PLAYING = 3,
    CLIENT_DEAD = 4,
};

typedef struct Client {
    int fd;
    bool active;
    uint32_t game_id;
    uint8_t player_num;
    enum ClientPhase phase;
    int game_index;
} Client;

typedef struct Game {
    bool active;
    uint32_t game_id;
    enum GamePhase phase;
    int client_slots[2];
    bool ships_received[2];
    uint8_t current_turn;
} Game;

typedef struct ServerState {
    Engine *engine;
    int listen_fd;
    Client clients[MAX_CLIENTS];
    Game games[MAX_GAMES];
    int num_clients;
} ServerState;

void server_state_init(ServerState *state, Engine *engine, int listen_fd);
int server_find_free_client(ServerState *state);
int server_find_game(ServerState *state, uint32_t game_id);
int server_create_game(ServerState *state, uint32_t game_id);
void server_remove_client(ServerState *state, int ci);
void server_end_game(ServerState *state, int gi, bool notify);
void server_handle_client_message(ServerState *state, int ci, uint8_t type,
                                  const uint8_t *payload, uint32_t len);
bool server_handle_new_connection(ServerState *state, int fd);
void server_on_client_disconnect(ServerState *state, int ci);

#endif
