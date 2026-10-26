#include "buffer_mgmt.h"

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <iostream>
#include <unistd.h>

const int MAX_BUFF = 4096;

int32_t parse_len(int fd) {
    int32_t len = 0;
    if (read_all(fd, reinterpret_cast<char *>(&len), sizeof(len)) < 0) {
        return -1;
    }
    return len;
}

int read_all(int fd, char *buff, int n) {
    while (n > 0) {
        ssize_t rv = read(fd, buff, n);

        if (rv < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "Error reading from receive buffer: " << errno << "\n";
            return -1;
        } else if (rv == 0) {
            return -1; // End of file / connection closed
        }

        assert(rv <= n);
        n -= static_cast<int>(rv);
        buff += rv;
    }
    return 0;
}

int write_all(int fd, const char *buff, int n) {
    while (n > 0) {
        ssize_t rv = write(fd, buff, n);

        if (rv < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "Error writing to send buffer: " << errno << "\n";
            return -1;
        } else if (rv == 0) {
            return -1;
        }

        assert(rv <= n);
        n -= static_cast<int>(rv);
        buff += rv;
    }
    return 0;
}
