#pragma once

#include "network_server.h"
#include <sys/epoll.h>
#include <atomic>
#include <unordered_map>

namespace kvstore {

class EpollServer : public NetworkServer {
public:
    EpollServer();
    ~EpollServer() override;
    
    bool start(uint16_t port) override;
    void stop() override;
    void run() override;
    
private:
    int listen_fd_;
    int epoll_fd_;
    std::atomic<bool> running_;
    std::unordered_map<int, std::unique_ptr<ClientConnection>> clients_;
    
    static constexpr int MAX_EVENTS = 1024;
    static constexpr int BUFFER_SIZE = 4096;
    
    // Socket operations
    bool create_listen_socket(uint16_t port);
    void accept_connection();
    void handle_client_read(ClientConnection* client);
    void handle_client_write(ClientConnection* client);
    void close_client(int fd);
    
    // Utility
    bool set_nonblocking(int fd);
    bool add_to_epoll(int fd, uint32_t events);
    bool modify_epoll(int fd, uint32_t events);
};

} // namespace kvstore
