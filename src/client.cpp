#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <iostream>

#include "sockutils.hpp"
#include "unistd.h"

#define PORT "12321"
#define MAXDATASIZE 100

/**
 * @brief connect to a server described by an addrinfo struct
 *
 * @return socket file descriptor, or -1 on error
 */
int server_connect(addrinfo* addresses) {
  int connection_fd;
  constexpr int non_blocking = 1;
  addrinfo* it = nullptr;
  // loop over the addresses to find the first one available to use
  for (it = addresses; it != nullptr; it = it->ai_next) {
    if ((connection_fd =
             socket(it->ai_family, it->ai_socktype, it->ai_protocol)) == -1) {
      perror("socket");
      continue;
    }

    if (connect(connection_fd, it->ai_addr, it->ai_addrlen) == -1) {
      perror("connect");
      close(connection_fd);
      continue;
    }

    if (fcntl(connection_fd, O_NONBLOCK, &non_blocking)) {
      perror("fcntl");
      close(connection_fd);
      exit(-1);
    }
    return connection_fd;
  }
  std::cerr << "failed to connect the socket\n";
  return -1;
}

/**
 * @brief connect to a given address
 * @param addr address or resolvable hostname
 *
 * @return socket file descriptor, or -1 on error
 */
int get_connection(const char* addr) {
  // Get the list of possible server addresses to use (we use the first one)
  auto [server_addresses, err_code] = get_address(addr, PORT);
  if (err_code != 0) {
    std::cerr << "getaddrinfo:" << gai_strerror(err_code) << '\n';
    return -1;
  }

  // file descriptor of the connection socket
  int connection_fd = server_connect(server_addresses);

  return connection_fd;
}

/**
 * @brief put bytes from a stream socket into a buffer and add null termination
 * @param socket_fd socket file descriptor
 *
 * @return number of bytes received, or -1 on error
 */
int receive_bytes(int socket_fd, char buffer[], size_t buf_size) {
  int bytes_received = recv(socket_fd, buffer, buf_size - 1, 0);
  if (bytes_received == -1 && errno != EAGAIN) {
    perror("recv");
    return -1;
  }
  if (*buffer) std::cout << "received: " << buffer << '\n';
  buffer[bytes_received] = '\0';
  return bytes_received;
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "usage: client <hostname>\n";
    exit(1);
  }

  int connection_fd = get_connection(argv[1]);
  if (connection_fd == -1) {
    exit(1);
  }

  // File descriptors of all used sockets
  constexpr int POLL_SIZE = 2;
  std::array<pollfd, POLL_SIZE> fd_poll{
      {{STDIN_FILENO, POLLIN, 0}, {connection_fd, POLLIN, 0}}};

  char buffer[MAXDATASIZE];
  std::string input;

  bool quit = false;
  while (!quit) {
    int n_events = poll(fd_poll.data(), POLL_SIZE, 10000);
    if (n_events == -1) {
      perror("poll");
      exit(1);
    }
    if (n_events == 0) continue;
    for (auto& sock : fd_poll) {
      if (sock.revents == 0) continue;
      if (sock.revents != POLLIN) {
        std::cerr << "unexpected event: " << sock.revents << '\n';
        quit = true;
        break;
      }
      if (sock.fd == connection_fd) {
        int bytes_received = 0;
        while (true) {
          bytes_received = receive_bytes(connection_fd, buffer, MAXDATASIZE);
          if (bytes_received == -1) {
            break;
          }
        }
      } else {
        // It's stdin, not the connection socket
        std::string input;
        std::cin >> input;
        int bytes_sent = send(connection_fd, input.c_str(), MAXDATASIZE, 0);
        if (bytes_sent < 0) {
          perror("send");
          quit = true;
        }
      }
    }
  }

  close(connection_fd);

  return 0;
}