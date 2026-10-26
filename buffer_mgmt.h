#ifndef BUFFER_MGMT_H
#define BUFFER_MGMT_H

#include <cstddef>
#include <cstdint>

extern const int MAX_BUFF;

int32_t parse_len(int fd);
int read_all(int fd, char *buff, int n);
int write_all(int fd, const char *buff, int n);

#endif // BUFFER_MGMT_H
