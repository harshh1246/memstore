#include "buffer_mgmt.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

static void buf_append(std::vector<uint8_t> &buff, const uint8_t *data, size_t len) {
    buff.insert(buff.end(), data, data + len);
}

void communicate(int connection_fd) {
    std::vector<uint8_t> send_buff;

    while (true) {
        std::cout << "Enter your message: ";
        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << "\nExiting...\n";
            break;
        }

        std::istringstream iss(line);
        std::string arg;
        std::vector<std::string> args;

        while (iss >> arg) {
            args.push_back(arg);
        }

        if (args.empty()) {
            continue;
        }

        uint32_t n_args = static_cast<uint32_t>(args.size());
        uint32_t cmd_size = 4; // 4 bytes for n_args
        for (const auto &a : args) {
            cmd_size += 4 + static_cast<uint32_t>(a.size());
        }

        send_buff.clear();
        buf_append(send_buff, reinterpret_cast<const uint8_t *>(&cmd_size), 4);
        buf_append(send_buff, reinterpret_cast<const uint8_t *>(&n_args), 4);

        for (const auto &a : args) {
            uint32_t arg_len = static_cast<uint32_t>(a.size());
            buf_append(send_buff, reinterpret_cast<const uint8_t *>(&arg_len), 4);
            buf_append(send_buff, reinterpret_cast<const uint8_t *>(a.data()), arg_len);
        }

        if (write_all(connection_fd, reinterpret_cast<const char *>(send_buff.data()), send_buff.size()) < 0) {
            std::cerr << "Error writing to socket\n";
            break;
        }

        int32_t response_size = parse_len(connection_fd);
        if (response_size < 4) {
            std::cerr << "Connection closed or error retrieving response size\n";
            break;
        }

        int32_t status_code = parse_len(connection_fd);
        if (status_code < 0) {
            std::cerr << "Connection closed or error retrieving status code\n";
            break;
        }

        std::cout << "Status Code: " << status_code << "\n";

        uint32_t payload_len = static_cast<uint32_t>(response_size - 4);
        std::string read_buff(payload_len, '\0');

        if (payload_len > 0) {
            if (read_all(connection_fd, &read_buff[0], payload_len) < 0) {
                std::cerr << "Error reading payload from socket\n";
                break;
            }
        }

        std::cout << "server says: " << read_buff << "\n";
    }
}

int main() {
    int connection_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (connection_fd < 0) {
        perror("socket() failed");
        return 1;
    }

    int optval = 1;
    setsockopt(connection_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1

    int rv = connect(connection_fd, reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr));
    if (rv < 0) {
        perror("connect() failed");
        close(connection_fd);
        return 1;
    }

    std::cout << "Successfully connected with the server!\n";
    communicate(connection_fd);
    close(connection_fd);
    return 0;
}