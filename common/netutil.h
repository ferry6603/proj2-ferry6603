#ifndef NETUTIL_H
#define NETUTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool read_exact(int fd, void *buf, size_t n);
bool write_exact(int fd, const void *buf, size_t n);
bool read_u32_be(int fd, uint32_t *value);
bool write_u32_be(int fd, uint32_t value);

#endif
