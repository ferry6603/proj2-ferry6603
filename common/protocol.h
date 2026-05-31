#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct Ship;

enum MsgType {
    MSG_JOIN_OK = 1,
    MSG_JOIN_FAIL = 2,
    MSG_READY = 3,
    MSG_SHIPS = 4,
    MSG_SHIPS_ACK = 5,
    MSG_SHIPS_FAIL = 6,
    MSG_MOVE = 7,
    MSG_MOVE_RESULT = 8,
    MSG_MOVE_EXT = 9,
    MSG_MOVE_EXT_RESULT = 10,
    MSG_OPP_MOVE = 11,
    MSG_ERROR = 12,
    MSG_GAME_OVER = 13,
};

#define PROTO_MAX_FRAME (65536 + 16)
#define PROTO_MAX_COORD 16

bool proto_send_frame(int fd, uint8_t type, const void *payload, uint32_t payload_len);
bool proto_read_frame(int fd, uint8_t *type, uint8_t *payload, uint32_t *payload_len);

bool proto_send_empty(int fd, uint8_t type);
bool proto_send_u8(int fd, uint8_t type, uint8_t value);
bool proto_send_i8(int fd, uint8_t type, int8_t value);

bool proto_read_u8_payload(const uint8_t *payload, uint32_t len, uint8_t *value);
bool proto_read_i8_payload(const uint8_t *payload, uint32_t len, int8_t *value);

bool proto_write_ships(uint8_t *buf, uint32_t cap, uint32_t *len,
                       const struct Ship *ships);
bool proto_read_ships(const uint8_t *payload, uint32_t len,
                      struct Ship *ships_out,
                      char coord_storage[4][PROTO_MAX_COORD + 1]);

bool proto_write_coord(uint8_t *buf, uint32_t cap, uint32_t *len,
                       const char *coordinate);
bool proto_read_coord(const uint8_t *payload, uint32_t len, char *out,
                      size_t out_cap);

bool proto_write_opp_move(uint8_t *buf, uint32_t cap, uint32_t *len,
                          const char *coordinate, int8_t result);
bool proto_read_opp_move(const uint8_t *payload, uint32_t len, char *coord_out,
                         size_t coord_cap, int8_t *result_out);

bool proto_write_ext_result(uint8_t *buf, uint32_t cap, uint32_t *len,
                            uint16_t data_len, const void *data);
bool proto_read_ext_result(const uint8_t *payload, uint32_t len,
                           uint16_t *data_len, const void **data_ptr);

#endif
