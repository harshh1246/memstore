#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cassert>

#include "buffer_mgmt.h"

int communicate(int client_fd){
    while(true){
        int32_t request_size = parse_len(client_fd);

        if(request_size == -1){
            std::cerr << "Client disconnected or error reading request size\n";
            return -1;
        }

        char read_buff[MAX_BUFF] = {};
        if (read_all(client_fd, read_buff, request_size) < 0) {
            std::cerr << "Error in reading from buffer\n";
            return -1;
        }

        printf("client says %s\n", read_buff);

        char response_buff[4+MAX_BUFF] = {};

        char reply[MAX_BUFF];

        std::cin.getline(reply, MAX_BUFF);
        int reply_size = strlen(reply);

        memcpy(response_buff, &reply_size, 4);
        memcpy(&response_buff[4], reply, reply_size);

        int err = write_all(client_fd, response_buff, 4 + reply_size);

        if(err==-1){
            std::cout << "Error occured while reading the output" << std::endl;
            return -1;
        }
    }
    return -1;
}

int main(){
    int listener_fd = socket(AF_INET, SOCK_STREAM, 0);  // AF_INET is for IPv4, SOCK_STREAM is for TCP | For IPV6, use AF_INET6 and for UDP, use SOCK_DGRAM | 0 defines IP protocol
    int optval = 1;

    // Socket options are configuration settings used in network programming to control the behavior, performance, and features of a network socket

    setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    // SO_REUSEADDR is a socket option that allows a socket to bind to a port that is already in use by another socket.
    // SOL_SOCKET is the socket level that defines the specific layer of the network stack where a socket option is applied
    // There are three primary levels: socket level (SOL_SOCKET), IP level (IPPROTO_IP), and TCP level (IPPROTO_TCP). Each level has its own set of socket options that can be configured to control the behavior of sockets at that specific layer.
    // Optval is a pointer to the value of the option being set, and sizeof(optval) specifies the size of the option value in bytes.
    // In this case we are setting optval to 1, which enables the SO_REUSEADDR option, allowing the socket to bind to a port that is already in use. But for other options, the value of optval may vary depending on the specific option being set (e.g., for SO_RCVBUF, optval would specify the size of the receive buffer).

    // struct sockaddr_in is the data structure used to store and manage IPv4 addresses and port numbers.
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    // htons is a function that converts a 16-bit integer from host byte order to network byte order. It is used to ensure that the port number is represented in the correct byte order for network communication, as different computer architectures may use different byte orders (endianness). Network protocols typically use big-endian byte order, so htons is necessary to convert the port number to this format before sending it over the network. 
    addr.sin_addr.s_addr = htonl(0);    // wildcard IP 0.0.0.0; Ex. 1.2.3.4 is represented by htonl(0x01020304).
    int rv = bind(listener_fd, (const struct sockaddr *)&addr, sizeof(addr));
    // Here we have typecasted sockaddr_in to sockaddr

    // if (rv) { die("bind()"); }
    if (rv){
        std::cerr << "bind() failed";
    }

    rv = listen(listener_fd, SOMAXCONN);
    // Creates a socket queue and marks the socket as a passive socket that will be used to accept incoming connection requests. The second argument specifies the maximum number of pending connections that can be queued for this socket. SOMAXCONN is a constant that represents the maximum value allowed by the system for the backlog parameter, which is typically defined in the system headers. 

    if (rv < 0) {
        std::cerr << "listen() failed";
    }   
    std::cout << "Server started listening on port 1234" << std::endl;
    while (true) {
        // accept
        struct sockaddr_in client_addr = {};

        socklen_t addrlen = sizeof(client_addr);

        int client_fd = accept(listener_fd, (struct sockaddr *)&client_addr, &addrlen);


        if (client_fd<0){
            std::cerr << "Error in creating client connection";
            continue;
        }
        std::cout << "A new client connected!\n" << std::endl;

        communicate(client_fd);
        std::cout << "Connection Closed!" << std::endl;
        close(client_fd);
    }
}