#include "protocol.h"

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include "netutil.h"

#include "../server_c/engine.h"

bool proto_send_frame(int fd, uint8_t type, const void *payload,
                      uint32_t payload_len) {
    uint32_t total = 1u + payload_len;
    uint32_t be = htonl(total);
    if (!write_exact(fd, &be, sizeof(be))) {
        return false;
    }
    if (!write_exact(fd, &type, 1)) {
        return false;
    }
    if (payload_len > 0 && !write_exact(fd, payload, payload_len)) {
        return false;
    }
    return true;
}

bool proto_read_frame(int fd, uint8_t *type, uint8_t *payload,
                      uint32_t *payload_len) {
    uint32_t be = 0;
    if (!read_exact(fd, &be, sizeof(be))) {
        return false;
    }
    uint32_t total = ntohl(be);
    if (total < 1 || total > PROTO_MAX_FRAME) {
        return false;
    }
    if (!read_exact(fd, type, 1)) {
        return false;
    }
    uint32_t plen = total - 1;
    if (plen > 0 && !read_exact(fd, payload, plen)) {
        return false;
    }
    *payload_len = plen;
    return true;
}

bool proto_send_empty(int fd, uint8_t type) {
    return proto_send_frame(fd, type, NULL, 0);
}

bool proto_send_u8(int fd, uint8_t type, uint8_t value) {
    return proto_send_frame(fd, type, &value, 1);
}

bool proto_send_i8(int fd, uint8_t type, int8_t value) {
    return proto_send_frame(fd, type, &value, 1);
}

bool proto_read_u8_payload(const uint8_t *payload, uint32_t len,
                           uint8_t *value) {
    if (len != 1) {
        return false;
    }
    *value = payload[0];
    return true;
}

bool proto_read_i8_payload(const uint8_t *payload, uint32_t len,
                           int8_t *value) {
    if (len != 1) {
        return false;
    }
    *value = (int8_t)payload[0];
    return true;
}

static bool append_bytes(uint8_t *buf, uint32_t cap, uint32_t *pos,
                         const void *data, size_t n) {
    if (*pos + n > cap) {
        return false;
    }
    memcpy(buf + *pos, data, n);
    *pos += (uint32_t)n;
    return true;
}

bool proto_write_ships(uint8_t *buf, uint32_t cap, uint32_t *len,
                       const struct Ship *ships) {
    uint32_t pos = 0;
    for (int i = 0; i < 4; i++) {
        const struct Ship *s = &ships[i];
        size_t clen = strlen(s->coordinate);
        if (clen == 0 || clen > PROTO_MAX_COORD) {
            return false;
        }
        uint8_t coord_len = (uint8_t)clen;
        if (!append_bytes(buf, cap, &pos, &coord_len, 1)) {
            return false;
        }
        if (!append_bytes(buf, cap, &pos, s->coordinate, clen)) {
            return false;
        }
        if (!append_bytes(buf, cap, &pos, &s->length, 1)) {
            return false;
        }
        uint8_t dir = (uint8_t)s->direction;
        if (!append_bytes(buf, cap, &pos, &dir, 1)) {
            return false;
        }
    }
    *len = pos;
    return true;
}

bool proto_read_ships(const uint8_t *payload, uint32_t len,
                      struct Ship *ships_out,
                      char coord_storage[4][PROTO_MAX_COORD + 1]) {
    uint32_t pos = 0;
    for (int i = 0; i < 4; i++) {
        if (pos + 1 > len) {
            return false;
        }
        uint8_t coord_len = payload[pos++];
        if (coord_len == 0 || coord_len > PROTO_MAX_COORD || pos + coord_len + 2 > len) {
            return false;
        }
        memcpy(coord_storage[i], payload + pos, coord_len);
        coord_storage[i][coord_len] = '\0';
        pos += coord_len;
        ships_out[i].coordinate = coord_storage[i];
        ships_out[i].length = payload[pos++];
        ships_out[i].direction = (enum Direction)payload[pos++];
    }
    if (pos != len) {
        return false;
    }
    return true;
}

bool proto_write_coord(uint8_t *buf, uint32_t cap, uint32_t *len,
                       const char *coordinate) {
    size_t clen = strlen(coordinate);
    if (clen == 0 || clen > PROTO_MAX_COORD) {
        return false;
    }
    uint32_t pos = 0;
    uint8_t coord_len = (uint8_t)clen;
    if (!append_bytes(buf, cap, &pos, &coord_len, 1)) {
        return false;
    }
    if (!append_bytes(buf, cap, &pos, coordinate, clen)) {
        return false;
    }
    *len = pos;
    return true;
}

bool proto_read_coord(const uint8_t *payload, uint32_t len, char *out,
                      size_t out_cap) {
    if (len < 1) {
        return false;
    }
    uint8_t coord_len = payload[0];
    if (coord_len == 0 || coord_len > PROTO_MAX_COORD || len != 1u + coord_len) {
        return false;
    }
    if ((size_t)coord_len + 1 > out_cap) {
        return false;
    }
    memcpy(out, payload + 1, coord_len);
    out[coord_len] = '\0';
    return true;
}

bool proto_write_opp_move(uint8_t *buf, uint32_t cap, uint32_t *len,
                          const char *coordinate, int8_t result) {
    uint32_t pos = 0;
    size_t clen = strlen(coordinate);
    if (clen == 0 || clen > PROTO_MAX_COORD) {
        return false;
    }
    uint8_t coord_len = (uint8_t)clen;
    if (!append_bytes(buf, cap, &pos, &coord_len, 1)) {
        return false;
    }
    if (!append_bytes(buf, cap, &pos, coordinate, clen)) {
        return false;
    }
    if (!append_bytes(buf, cap, &pos, &result, 1)) {
        return false;
    }
    *len = pos;
    return true;
}

bool proto_read_opp_move(const uint8_t *payload, uint32_t len, char *coord_out,
                         size_t coord_cap, int8_t *result_out) {
    if (len < 2) {
        return false;
    }
    uint8_t coord_len = payload[0];
    if (coord_len == 0 || coord_len > PROTO_MAX_COORD || len != 2u + coord_len) {
        return false;
    }
    if ((size_t)coord_len + 1 > coord_cap) {
        return false;
    }
    memcpy(coord_out, payload + 1, coord_len);
    coord_out[coord_len] = '\0';
    *result_out = (int8_t)payload[1 + coord_len];
    return true;
}

bool proto_write_ext_result(uint8_t *buf, uint32_t cap, uint32_t *len,
                            uint16_t data_len, const void *data) {
    if (2u + data_len > cap) {
        return false;
    }
    uint16_t be = htons(data_len);
    memcpy(buf, &be, 2);
    if (data_len > 0 && data != NULL) {
        memcpy(buf + 2, data, data_len);
    }
    *len = 2u + data_len;
    return true;
}

bool proto_read_ext_result(const uint8_t *payload, uint32_t len,
                           uint16_t *data_len, const void **data_ptr) {
    if (len < 2) {
        return false;
    }
    uint16_t be;
    memcpy(&be, payload, 2);
    uint16_t dlen = ntohs(be);
    if (2u + dlen != len) {
        return false;
    }
    *data_len = dlen;
    *data_ptr = payload + 2;
    return true;
}
