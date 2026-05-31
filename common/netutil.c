#include "netutil.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stddef.h>
#include <unistd.h>

bool read_exact(int fd, void *buf, size_t n) {
    uint8_t *p = buf;
    size_t remaining = n;

    while (remaining > 0) {
        ssize_t got = read(fd, p, remaining);
        if (got <= 0) {
            return false;
        }
        p += (size_t)got;
        remaining -= (size_t)got;
    }
    return true;
}

bool write_exact(int fd, const void *buf, size_t n) {
    const uint8_t *p = buf;
    size_t remaining = n;

    while (remaining > 0) {
        ssize_t sent = write(fd, p, remaining);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (sent == 0) {
            return false;
        }
        p += (size_t)sent;
        remaining -= (size_t)sent;
    }
    return true;
}

bool read_u32_be(int fd, uint32_t *value) {
    uint32_t raw = 0;
    if (!read_exact(fd, &raw, sizeof(raw))) {
        return false;
    }
    *value = ntohl(raw);
    return true;
}

bool write_u32_be(int fd, uint32_t value) {
    uint32_t raw = htonl(value);
    return write_exact(fd, &raw, sizeof(raw));
}
