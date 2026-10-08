#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <vector>
#include <sstream>

#include "buffer_mgmt.h"

static void buf_append(std::vector<uint8_t> &buff, const uint8_t *data, size_t len) {
    buff.insert(buff.end(), data, data + len);
}

static void buf_prepend(std::vector<uint8_t> &buff, const uint8_t *data, size_t len) {

    buff.insert(buff.begin(), data, data + len);
}

static void buf_consume(std::vector<uint8_t> &buff, size_t n) {
    buff.erase(buff.begin(), buff.begin() + n);
}

void communicate(int connection_fd){
    std::vector<uint8_t> send_buff;

    while (true){
        char payload[4+MAX_BUFF] = {};

        char cmd[MAX_BUFF];
        std::cout << "Enter your message: ";
        std::cin.getline(cmd, MAX_BUFF);

        std::istringstream iss(cmd);

        std::string arg;

        uint32_t n_args = 0;
        uint32_t cmd_size = 0;
        
        // 2. Extract words separated by spaces
        while (iss >> arg) {
            uint32_t arg_len = arg.length();
            buf_append(send_buff, (uint8_t *)&arg_len, 4);
            buf_append(
                send_buff,
                reinterpret_cast<const uint8_t*>(arg.data()),
                arg_len
            );
            n_args++;
            cmd_size += 4 + arg_len;
        }
        buf_prepend(send_buff, (uint8_t *)&n_args, 4);

        cmd_size += 4;
        buf_prepend(send_buff, (uint8_t *)&cmd_size, 4);

        while(send_buff.size()!=0){
            int rv = write(connection_fd, send_buff.data(), send_buff.size());
            buf_consume(send_buff, rv);
        }

        int32_t response_size = parse_len(connection_fd);
        int32_t status_code = parse_len(connection_fd);

        std::cout << status_code << "Status Code \n";

        if(response_size<0){
            std::cerr << "Error in retriving response size!";
        }
        
        char read_buff[MAX_BUFF] = {};

        if (read_all(connection_fd, read_buff, response_size-4) < 0) {
            std::cerr << "Error reading from socket";
            return;
        }

        printf("server says: %s\n", read_buff);
    }

}

int main(){
    int connection_fd = socket(AF_INET, SOCK_STREAM, 0);  // AF_INET is for IPv4, SOCK_STREAM is for TCP | For IPV6, use AF_INET6 and for UDP, use SOCK_DGRAM | 0 defines IP protocol
    int optval = 1;

    setsockopt(connection_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);    // INADDR_LOOPBACK is a constant that represents the loopback address (127.0.0.1)
    int rv = connect(connection_fd, (const struct sockaddr *)&addr, sizeof(addr));

    if (rv<0){
        printf("connect() failed\n");
    }else if(rv==0){
        printf("Successfully connected with the server!\n");
    }

    communicate(connection_fd);
}