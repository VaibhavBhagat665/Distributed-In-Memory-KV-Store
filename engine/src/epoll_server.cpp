#include "epoll_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <iostream>

namespace kvstore {

EpollServer::EpollServer() 
    : listen_fd_(-1), epoll_fd_(-1), running_(false) {
}

EpollServer::~EpollServer() {
    stop();
}

bool EpollServer::set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

bool EpollServer::create_listen_socket(uint16_t port) {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ == -1) {
        return false;
    }
    
    // Set SO_REUSEADDR
    int reuse = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        close(listen_fd_);
        return false;
    }
    
    // Set SO_REUSEPORT
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) == -1) {
        close(listen_fd_);
        return false;
    }
    
    // Disable Nagle's algorithm
    int nodelay = 1;
    setsockopt(listen_fd_, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
    
    // Bind
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) == -1) {
        close(listen_fd_);
        return false;
    }
    
    // Listen
    if (listen(listen_fd_, SOMAXCONN) == -1) {
        close(listen_fd_);
        return false;
    }
    
    // Set non-blocking
    if (!set_nonblocking(listen_fd_)) {
        close(listen_fd_);
        return false;
    }
    
    return true;
}

bool EpollServer::add_to_epoll(int fd, uint32_t events) {
    epoll_event ev{};
    ev.events = events | EPOLLET;  // Edge-triggered
    ev.data.fd = fd;
    return epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) != -1;
}

bool EpollServer::modify_epoll(int fd, uint32_t events) {
    epoll_event ev{};
    ev.events = events | EPOLLET;
    ev.data.fd = fd;
    return epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) != -1;
}

bool EpollServer::start(uint16_t port) {
    if (running_.load()) {
        return false;
    }
    
    // Create listen socket
    if (!create_listen_socket(port)) {
        return false;
    }
    
    // Create epoll instance
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        close(listen_fd_);
        return false;
    }
    
    // Add listen socket to epoll
    if (!add_to_epoll(listen_fd_, EPOLLIN)) {
        close(epoll_fd_);
        close(listen_fd_);
        return false;
    }
    
    running_.store(true);
    return true;
}

void EpollServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    // Close all client connections
    for (auto& [fd, client] : clients_) {
        close(fd);
    }
    clients_.clear();
    
    if (epoll_fd_ != -1) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
    
    if (listen_fd_ != -1) {
        close(listen_fd_);
        listen_fd_ = -1;
    }
}

void EpollServer::accept_connection() {
    while (true) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        
        int client_fd = accept(listen_fd_, (sockaddr*)&client_addr, &addr_len);
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // No more connections
            }
            continue;  // Error, try next
        }
        
        // Set non-blocking
        if (!set_nonblocking(client_fd)) {
            close(client_fd);
            continue;
        }
        
        // Disable Nagle
        int nodelay = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
        
        // Add to epoll
        if (!add_to_epoll(client_fd, EPOLLIN)) {
            close(client_fd);
            continue;
        }
        
        // Create client connection
        clients_[client_fd] = std::make_unique<ClientConnection>(client_fd);
    }
}

void EpollServer::handle_client_read(ClientConnection* client) {
    char buffer[BUFFER_SIZE];
    
    while (true) {
        ssize_t n = recv(client->fd, buffer, BUFFER_SIZE, 0);
        
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // No more data
            }
            client->should_close = true;
            return;
        }
        
        if (n == 0) {
            // Connection closed
            client->should_close = true;
            return;
        }
        
        // Parse RESP commands
        auto value = client->parser.parse(buffer, n);
        while (value) {
            auto cmd = RESPParser::to_command(*value);
            if (cmd && command_handler_) {
                std::string response = command_handler_(*cmd);
                client->write_buffer += response;
            } else {
                client->write_buffer += RESPFormatter::error("ERR invalid command");
            }
            
            // Try to parse next command (pipelined)
            value = client->parser.parse("", 0);
        }
        
        // If we have data to write, enable EPOLLOUT
        if (!client->write_buffer.empty()) {
            modify_epoll(client->fd, EPOLLIN | EPOLLOUT);
        }
    }
}

void EpollServer::handle_client_write(ClientConnection* client) {
    while (!client->write_buffer.empty()) {
        ssize_t n = send(client->fd, client->write_buffer.data(), 
                        client->write_buffer.size(), MSG_NOSIGNAL);
        
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // Would block, try later
            }
            client->should_close = true;
            return;
        }
        
        client->write_buffer.erase(0, n);
    }
    
    // If write buffer empty, disable EPOLLOUT
    if (client->write_buffer.empty()) {
        modify_epoll(client->fd, EPOLLIN);
    }
}

void EpollServer::close_client(int fd) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    clients_.erase(fd);
}

void EpollServer::run() {
    epoll_event events[MAX_EVENTS];
    
    while (running_.load()) {
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100);  // 100ms timeout
        
        if (nfds == -1) {
            if (errno == EINTR) continue;
            break;
        }
        
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;
            
            if (fd == listen_fd_) {
                // New connection
                if (ev & EPOLLIN) {
                    accept_connection();
                }
            } else {
                // Client event
                auto it = clients_.find(fd);
                if (it == clients_.end()) continue;
                
                ClientConnection* client = it->second.get();
                
                if (ev & (EPOLLERR | EPOLLHUP)) {
                    close_client(fd);
                    continue;
                }
                
                if (ev & EPOLLIN) {
                    handle_client_read(client);
                }
                
                if (ev & EPOLLOUT) {
                    handle_client_write(client);
                }
                
                if (client->should_close) {
                    close_client(fd);
                }
            }
        }
    }
}

} // namespace kvstore
