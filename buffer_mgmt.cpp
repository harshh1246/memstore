#include <unistd.h>
#include <iostream>
#include <cassert>
#include <cstdint>

#include "buffer_mgmt.h"

const int MAX_BUFF = 4096;

int32_t parse_len(int fd){
    int n = 4;

    int32_t request_size;

    int rv = read(fd, &request_size, n);

    if(rv<0){
        std::cerr << "Error in reading the request size from receive buffer";
        return -1;
    }

    return request_size;
}

int read_all(int fd, char * buff, int n){
    while(n>0){
        int rv = read(fd, buff, n);
        // it attempts to read up to count bytes from file descriptor fd into the buffer starting at buf. It creates a buffer of size 64 bytes and reads data from the socket connection represented by connfd into this buffer. The sizeof(rbuf) - 1 ensures that there is space for a null terminator at the end of the string, allowing it to be treated as a C-style string. It also maintains an internal file position indicator for the socket, which keeps track of where the next read operation will start from. Each time read() is called, it updates this position indicator based on the number of bytes successfully read, allowing subsequent reads to continue from where the previous one left off.


        if (rv<0){
            std::cerr << "Error in reading the receive buffer";
            return -1;
        }
        else if (rv==0){
            std::cout << "End of file" << std::endl;
            return -1;
        }

        assert(rv <= n);

        n -= rv;
        buff += rv;
    }
    return 0;
}

int write_all(int fd, char *buff, int n){
    while(n>0){
        ssize_t rv = write(fd, buff, n);

        if (rv<0){
            std::cerr << "Error in writing to the send buffer";
            return -1;
        }

        assert(rv<=n);
        n-=rv;
        buff += rv;
    }
    return 0;
}
