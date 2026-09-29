#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

int main(){
    int fd = socket(AF_INET, SOCK_STREAM, 0);  // AF_INET is for IPv4, SOCK_STREAM is for TCP | For IPV6, use AF_INET6 and for UDP, use SOCK_DGRAM | 0 defines IP protocol
    int optval = 1;

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);    // INADDR_LOOPBACK is a constant that represents the loopback address (127.0.0.1)
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv){
        printf("connect() failed");
    }
    
    char wbuf[] = "Hey there";
    write(fd, wbuf, sizeof(wbuf));

    char rbuf[64] = {};
    ssize_t n = read(fd, rbuf, sizeof(rbuf) - 1);

    if (n < 0) {
        printf("Error reading from socket\n");
        return -1;
    }
    printf("server says: %s\n", rbuf);
}