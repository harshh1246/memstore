#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cassert>
#include <fcntl.h>
#include <vector>
#include <poll.h>

#include "buffer_mgmt.h"

struct Connection {
    int fd = -1;
    bool want_read = false;
    bool want_write = false;
    bool closed = false;
    std::vector<uint8_t> receive_buff;
    std::vector<uint8_t> send_buff;  
};


// Both there functions together make a queue
static void buf_append(std::vector<uint8_t> &buff, const uint8_t *data, size_t len) {
    buff.insert(buff.end(), data, data + len);
}

static void buf_consume(std::vector<uint8_t> &buff, size_t n) {
    buff.erase(buff.begin(), buff.begin() + n);
}

bool parse_write(Connection * connection){
    char response[MAX_BUFF] = {};

    std::cout << "Please type your response: ";
    std::cin.getline(response, MAX_BUFF);

    uint32_t len = strlen(response); 
    
    buf_append(connection->send_buff, (const uint8_t *)&len, 4);
    buf_append(connection->send_buff, (const uint8_t *)response, len);

    return true;

}

bool parse_read(Connection *connection){
    if (connection->receive_buff.size() < 4) {
        return false;   // Didn't receive the header bytes
    }

    uint32_t len = 0;

    memcpy(&len, connection->receive_buff.data(), 4);
    
    if (len > MAX_BUFF) {  // Message length is higher than allowed buffer size
        connection->closed = true;
        return false;   // want close
    }

    if (4 + len > connection->receive_buff.size()) {
        return false;   // Full payload hasn't arrived yet
    }

    const char *request = reinterpret_cast<const char *>(connection->receive_buff.data() + 4);

    printf("Client said: %.*s\n", len, request);

    buf_consume(connection->receive_buff, 4 + len);

    parse_write(connection);
    return true;
}

static void set_nb(int fd){
    int rv = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

    if (rv==-1){
        std::cout << "Some error occured while changing file descriptor settings\n";
    }
}


Connection* handle_accept(int listener_fd){
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(listener_fd, (struct sockaddr *)&client_addr, &addrlen);

        if (client_fd<0){
            std::cerr << "Error in creating client connection";
            return NULL;
        }
        std::cout << "A new client connected!\n" << std::endl;

        set_nb(client_fd);

        Connection *connection = new Connection();
        connection->fd = client_fd;
        connection->want_read = true;

        return connection;
}

static void handle_read(Connection * connection){
    uint8_t read_buff[64 * 1024];
    
    int rv = read(connection->fd, read_buff, sizeof(read_buff));
    
    if (rv<=0){
        // std::cout << "Either connection is closed or there is some error\n" << std::endl;
        connection->closed = true;
        return;
    }

    buf_append(connection->receive_buff, read_buff, rv);
    parse_read(connection);
    
    if (connection->send_buff.size() > 0) {    // has a response
        connection->want_read = false;
        connection->want_write = true;
    }   // else: want read
}

static void handle_write(Connection *connection) {
    assert(connection->send_buff.size() > 0);
    ssize_t rv = write(connection->fd, connection->send_buff.data(), connection->send_buff.size());
    
    if (rv <= 0) {
        connection->closed = true;    // error handling
        return;
    }
    
    buf_consume(connection->send_buff, (size_t)rv);

    // parse_write(connection);
    if (connection->send_buff.size() == 0) {   // all data written
        connection->want_read = true;
        connection->want_write = false;
    } // else: want write
}


void event_loop(int listener_fd){
    std::vector <Connection *> connections;
    std::vector <struct pollfd> poll_args;

    while(true){
        poll_args.clear();

        struct pollfd pfd = {listener_fd, POLLIN, 0};

        poll_args.push_back(pfd);
        for(Connection * conn : connections){

            if (!conn){
                continue;
            }
            struct pollfd pfd = {conn->fd, POLLERR, 0};

            if(conn->want_read){
                pfd.events |= POLLIN;
            }

            if(conn->want_write){
                pfd.events |= POLLOUT;
            }

            poll_args.push_back(pfd);
        }

        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);

        if (rv < 0 && errno == EINTR) {
            continue;
        }
        if (rv < 0) {
            std::cerr << "Some error occured while polling the fds";
        }

        // Checking for the new connections
        if(poll_args[0].revents & POLLIN){
            if(Connection * connection = handle_accept(listener_fd)){
                if (connections.size() <= connection->fd) {
                    connections.resize(connection->fd + 1);
                }
                connections[connection->fd] = connection;
            }
        }

        for(int i=1; i<poll_args.size(); i++){
            int ready = poll_args[i].revents;
            Connection * connection = connections[poll_args[i].fd];
            
            if(ready & POLLIN){
                // Read the data
                handle_read(connection);
            }

            if(ready & POLLOUT){
                // Write the data
                handle_write(connection);
            }

            if((ready & POLLERR) || connection->closed){
                // Connection closed; delete the connection
                std::cout << "closing connection\n";
                int rv = close(connection->fd);
                if(rv==-1){
                    std::cerr << "Error in closing connection: " << connection->fd << "\n" ;
                }
                connections[connection->fd] = NULL;

                delete connection;
            }
        }
    }
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

    set_nb(listener_fd); // Setting listener file descriptor as non-blocking

    if (rv < 0) {
        std::cerr << "listen() failed";
    }   
    std::cout << "Server started listening on port 1234" << std::endl;
        // accept
    event_loop(listener_fd);
    std::cout << "Connection died\n";
}