#include "buffer_mgmt.h"

#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

static std::map<std::string, std::string> g_data;

// Response status codes
constexpr int32_t SUCC   = 200;
constexpr int32_t BADREQ = 400;
constexpr int32_t NOTFND = 404;
constexpr int32_t INTERR = 500;

struct Connection {
    int fd = -1;
    bool want_read = false;
    bool want_write = false;
    bool closed = false;
    std::vector<uint8_t> receive_buff;
    std::vector<uint8_t> send_buff;
};

struct Response {
    int32_t status = SUCC;
    std::string content;
};

static void buf_append(std::vector<uint8_t> &buff, const uint8_t *data, size_t len) {
    buff.insert(buff.end(), data, data + len);
}

static void buf_consume(std::vector<uint8_t> &buff, size_t n) {
    buff.erase(buff.begin(), buff.begin() + n);
}

bool parse_write(Connection *connection, const Response &response) {
    uint32_t content_len = static_cast<uint32_t>(response.content.length());
    uint32_t len = 4 + content_len;

    buf_append(connection->send_buff, reinterpret_cast<const uint8_t *>(&len), 4);
    buf_append(connection->send_buff, reinterpret_cast<const uint8_t *>(&response.status), 4);
    if (content_len > 0) {
        buf_append(connection->send_buff, reinterpret_cast<const uint8_t *>(response.content.data()), content_len);
    }

    return true;
}

bool extract_cmds(const uint8_t *cmd_data, std::vector<std::string> &args, uint32_t len) {
    if (len < 4) {
        return false;
    }

    uint32_t n_args = 0;
    std::memcpy(&n_args, cmd_data, 4);

    uint32_t iter = 4;
    while (args.size() < n_args) {
        if (iter + 4 > len) {
            return false;
        }

        uint32_t arg_len = 0;
        std::memcpy(&arg_len, cmd_data + iter, 4);
        iter += 4;

        if (arg_len > len - iter) {
            return false;
        }

        args.emplace_back(reinterpret_cast<const char *>(cmd_data + iter), arg_len);
        iter += arg_len;
    }

    if (iter != len) {
        return false;
    }

    return true;
}

void run_cmds(const std::vector<std::string> &args, Response &response) {
    if (args.empty()) {
        std::cerr << "Error: empty command received\n";
        response.status = BADREQ;
        response.content = "Bad request";
        return;
    }

    // Supported commands: GET, SET and DELETE
    if (args[0] == "GET" && args.size() == 2) {
        auto res = g_data.find(args[1]);
        if (res != g_data.end()) {
            response.content = res->second;
            response.status = SUCC;
        } else {
            response.content = "No such entry found";
            response.status = NOTFND;
        }
    } else if (args[0] == "SET" && args.size() == 3) {
        g_data[args[1]] = args[2];
        response.status = SUCC;
        response.content = "Data saved successfully";
    } else if (args[0] == "DELETE" && args.size() == 2) {
        g_data.erase(args[1]);
        response.status = SUCC;
        response.content = "Deleted successfully";
    } else {
        std::cerr << "Error in running commands: invalid command or syntax\n";
        response.status = BADREQ;
        response.content = "Bad request";
    }
}

void parse_read(Connection *connection) {
    while (true) {
        if (connection->receive_buff.size() < 4) {
            return; // Header bytes not fully received yet
        }

        uint32_t len = 0;
        std::memcpy(&len, connection->receive_buff.data(), 4);

        if (len > static_cast<uint32_t>(MAX_BUFF)) {
            connection->closed = true;
            return;
        }

        if (4 + len > connection->receive_buff.size()) {
            return; // Full payload hasn't arrived yet
        }

        std::vector<std::string> args;
        bool ok = extract_cmds(connection->receive_buff.data() + 4, args, len);
        if (!ok) {
            std::cerr << "Error in reading commands\n";
            connection->closed = true;
            return;
        }

        Response response;
        run_cmds(args, response);

        buf_consume(connection->receive_buff, 4 + len);
        parse_write(connection, response);
    }
}

static int set_nb(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL failed");
        return -1;
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("fcntl F_SETFL failed");
        return -1;
    }
    return 0;
}

Connection *handle_accept(int listener_fd) {
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int client_fd = accept(listener_fd, reinterpret_cast<struct sockaddr *>(&client_addr), &addrlen);

    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("accept() failed");
        }
        return nullptr;
    }
    std::cout << "A new client connected! (fd: " << client_fd << ")\n";

    set_nb(client_fd);

    Connection *connection = new Connection();
    connection->fd = client_fd;
    connection->want_read = true;

    return connection;
}

static void handle_read(Connection *connection) {
    uint8_t read_buff[64 * 1024];

    ssize_t rv = read(connection->fd, read_buff, sizeof(read_buff));

    if (rv < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return;
        }
        std::cout << "Read error on connection " << connection->fd << "\n";
        connection->closed = true;
        return;
    }

    if (rv == 0) {
        std::cout << "Client closed connection " << connection->fd << "\n";
        connection->closed = true;
        return;
    }

    buf_append(connection->receive_buff, read_buff, static_cast<size_t>(rv));

    parse_read(connection);

    if (!connection->send_buff.empty()) {
        connection->want_read = false;
        connection->want_write = true;
    }
}

static void handle_write(Connection *connection) {
    if (connection->send_buff.empty()) {
        connection->want_read = true;
        connection->want_write = false;
        return;
    }

    ssize_t rv = write(connection->fd, connection->send_buff.data(), connection->send_buff.size());

    if (rv < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return;
        }
        std::cout << "Write error on connection " << connection->fd << "\n";
        connection->closed = true;
        return;
    }

    if (rv == 0) {
        connection->closed = true;
        return;
    }

    buf_consume(connection->send_buff, static_cast<size_t>(rv));

    if (connection->send_buff.empty()) {
        connection->want_read = true;
        connection->want_write = false;
    }
}

void event_loop(int listener_fd) {
    std::vector<Connection *> connections;
    std::vector<struct pollfd> poll_args;

    while (true) {
        poll_args.clear();

        struct pollfd pfd = {listener_fd, POLLIN, 0};
        poll_args.push_back(pfd);

        for (Connection *conn : connections) {
            if (!conn) {
                continue;
            }
            struct pollfd conn_pfd = {conn->fd, 0, 0};

            if (conn->want_read) {
                conn_pfd.events |= POLLIN;
            }
            if (conn->want_write) {
                conn_pfd.events |= POLLOUT;
            }

            poll_args.push_back(conn_pfd);
        }

        int rv = poll(poll_args.data(), static_cast<nfds_t>(poll_args.size()), -1);

        if (rv < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll() failed");
            break;
        }

        // Check for new incoming connections
        if (poll_args[0].revents & POLLIN) {
            while (Connection *connection = handle_accept(listener_fd)) {
                if (connections.size() <= static_cast<size_t>(connection->fd)) {
                    connections.resize(connection->fd + 1, nullptr);
                }
                connections[connection->fd] = connection;
            }
        }

        for (size_t i = 1; i < poll_args.size(); i++) {
            int ready = poll_args[i].revents;
            if (ready == 0) {
                continue;
            }

            int fd = poll_args[i].fd;
            if (fd < 0 || static_cast<size_t>(fd) >= connections.size()) {
                continue;
            }

            Connection *connection = connections[fd];
            if (!connection) {
                continue;
            }

            if (!connection->closed && (ready & POLLIN)) {
                handle_read(connection);
            }

            if (!connection->closed && (ready & POLLOUT)) {
                handle_write(connection);
            }

            if (connection->closed || (ready & (POLLERR | POLLHUP | POLLNVAL))) {
                std::cout << "Closing connection (fd: " << connection->fd << ")\n";
                close(connection->fd);
                connections[connection->fd] = nullptr;
                delete connection;
            }
        }
    }

    for (Connection *conn : connections) {
        if (conn) {
            close(conn->fd);
            delete conn;
        }
    }
}

int main() {
    signal(SIGPIPE, SIG_IGN);

    int listener_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_fd < 0) {
        perror("socket() failed");
        return 1;
    }

    int optval = 1;
    if (setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt() failed");
        close(listener_fd);
        return 1;
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int rv = bind(listener_fd, reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr));
    if (rv < 0) {
        perror("bind() failed");
        close(listener_fd);
        return 1;
    }

    rv = listen(listener_fd, SOMAXCONN);
    if (rv < 0) {
        perror("listen() failed");
        close(listener_fd);
        return 1;
    }

    if (set_nb(listener_fd) < 0) {
        close(listener_fd);
        return 1;
    }

    std::cout << "Server started listening on port 1234\n";
    event_loop(listener_fd);
    close(listener_fd);
    return 0;
}