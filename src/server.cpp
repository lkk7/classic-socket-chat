#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cstring>

#include "sockutils.hpp"

#define PORT "12321"
#define BACKLOG 100

int get_listener_socket() {
  // iterator used for choosing the first right server info from getaddrinfo()
  addrinfo* it = nullptr;
  // file descriptor of the listener socket
  int listener_fd;

  // Get the list of possible server addresses to use (we use the first one)
  auto [server_addresses, err_code] = get_address(nullptr, PORT);
  if (err_code != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err_code));
    return -1;
  }

  // loop over the addresses to find the first one available to bind
  for (it = server_addresses; it != nullptr; it = it->ai_next) {
    // try creating the socket, if it fails, use the next server info
    listener_fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
    if (listener_fd == -1) {
      perror("socket");
      continue;
    }

    int should_reuse = 1;
    if (setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &should_reuse,
                   sizeof(should_reuse)) == -1) {
      perror("setsockopt");
      close(listener_fd);
      continue;
    }

    if (bind(listener_fd, it->ai_addr, it->ai_addrlen) == -1) {
      perror("bind");
      close(listener_fd);
      continue;
    }

    break;
  }

  freeaddrinfo(server_addresses);

  if (it == nullptr) {
    fprintf(stderr, "failed to create/bind the socket");
    return -1;
  }

  if (listen(listener_fd, BACKLOG) == -1) {
    perror("listen");
    return -1;
  }

  return listener_fd;
}

int main(void) {
  int listener_fd = get_listener_socket();
  if (listener_fd == -1) {
    return 1;
  }

  // file descriptor of the actual connection
  int connection_fd;

  // client info
  sockaddr_storage client;
  socklen_t client_size = sizeof(client);
  char client_addr[INET6_ADDRSTRLEN];

  // File descriptors of all used sockets
  std::array<pollfd, 10> fd_poll;
  fd_poll[0] = {.fd = listener_fd, .events = POLLIN};
  size_t n_socks = 1;

  while (true) {
    int n_events = poll(fd_poll.data(), n_socks, 5000);
    if (n_events == -1) {
      perror("poll");
      exit(1);
    }
    if (n_events == 0) {
      printf("polt() timeout, waiting for events...\n");
      continue;
    }
    for (auto& sock : fd_poll) {
      if (sock.revents == 0) continue;
      if (sock.revents != POLLIN) {
        fprintf(stderr, "unexpected event: %d\n", sock.revents);
        break;
      }

      if (sock.fd == listener_fd) {
        connection_fd = accept(listener_fd, (sockaddr*)&client, &client_size);
        if (connection_fd < 0) {
          if (errno != EWOULDBLOCK) {
            perror("accept");
          }
          break;
        }
        fd_poll[n_socks].fd = connection_fd;
        fd_poll[n_socks++].events = POLLIN;
        inet_ntop(client.ss_family, get_sinaddr(&client), client_addr,
                  sizeof(client_addr));
        printf("connected: %s\n", client_addr);
      } else {
        int bytes_received;
        char data_buffer[128];
        bool close_connection = false;
        size_t str_len;
        do {
          bytes_received = recv(sock.fd, data_buffer, sizeof(data_buffer), 0);
          if (bytes_received < 0) {
            if (errno != EWOULDBLOCK) {
              perror("recv");
              close_connection = true;
            }
            break;
          }
          if (bytes_received == 0) {
            printf("Connection closed\n");
            close_connection = true;
            break;
          }
          str_len = bytes_received;
          printf("%lu bytes received from client\n", str_len);
          bytes_received = send(sock.fd, data_buffer, str_len, 0);
          if (bytes_received < 0) {
            perror("send");
            close_connection = true;
            break;
          }
        } while (true);
        if (close_connection) {
          close(sock.fd);
          sock.fd = -1;
        }
      }
    }
  }

  for (size_t i = 0; i < n_socks; i++) {
    if (fd_poll[i].fd >= 0) close(fd_poll[i].fd);
  }
  return 0;
}
