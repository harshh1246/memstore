#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>
#include <iostream>

#include "buffer_mgmt.h"

void communicate(int connection_fd){
    while (true){
        char payload[4+MAX_BUFF] = {};

        char text[MAX_BUFF];
        std::cout << "Enter your message: " << std::endl;
        std::cin.getline(text, MAX_BUFF);

        int text_size = strlen(text);

        memcpy(payload, &text_size, 4);
        memcpy(&payload[4], text, text_size);

        write_all(connection_fd, payload, 4+text_size);

        int32_t response_size = parse_len(connection_fd);

        if(response_size<0){
            std::cerr << "Error in retriving response size!";
        }
        
        char read_buff[MAX_BUFF] = {};

        if (read_all(connection_fd, read_buff, response_size) < 0) {
            std::cerr << "Error reading from socket";
            return;
        }

        printf("server says %s\n", read_buff);
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