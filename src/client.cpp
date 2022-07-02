#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "sockutils.hpp"
#include "unistd.h"

/**
 * @brief Connect to a server described by an addrinfo struct.
 * @param addrinfo linked list of possible server addresses
 * @return socket file descriptor or -1 on error.
 */
int server_connect(addrinfo* addresses) {
  int connection_fd = 0;
  addrinfo* it = nullptr;
  constexpr int NON_BLOCKING = 1;
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

    if (fcntl(connection_fd, O_NONBLOCK, &NON_BLOCKING) != 0) {
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
 * @brief Connect to a given address.
 * @param address address or resolvable hostname
 * @return socket file descriptor or -1 on error.
 */
int get_connection(const char* address) {
  // Get the list of possible server addresses to use (we use the first one)
  auto [server_addresses, err_code] = get_address(address, PORT);
  if (err_code != 0) {
    std::cerr << "getaddrinfo:" << gai_strerror(err_code) << '\n';
    return -1;
  }

  // file descriptor of the connection socket
  int connection_fd = server_connect(server_addresses);

  return connection_fd;
}

/**
 * @brief Put bytes coming from a socket into a buffer.
 * @param socket_fd socket file descriptor
 * @param buffer char buffer to receive byte
 * @return number of bytes received or -1 on error.
 */
ssize_t receive_bytes(int socket_fd, char buffer[]) {
  ssize_t bytes_received = recv(socket_fd, buffer, BUF_BYTES_SIZE, 0);
  if (bytes_received == -1) {
    if (errno != EAGAIN) {
      perror("recv");
    }
    return -1;
  }
  const auto [sock, address, msg] = decode_server_msg(buffer);
  if (!sock.empty()) {
    std::cout << '[' << sock << "][" << address << "] " << msg << '\n';
  }
  *buffer = 0;

  return bytes_received;
}

/**
 * @brief Handle an event on a the server file descriptor.
 * In this client, we know it's data ready to be read from the server.
 * @param connection_fd server connection's file descriptor
 * @param buffer char buffer to receive bytes
 * @return number of bytes received from the server or -1 on error.
 */
ssize_t handle_server_event(int connection_fd, char buffer[]) {
  ssize_t bytes_received = 0;
  while (true) {
    bytes_received = receive_bytes(connection_fd, buffer);
    if (bytes_received == -1) {
      break;
    }
  }
  return bytes_received;
}

/**
 * @brief Send input from stdin to the server.
 * @param connection_fd server connection's file descriptor
 * @return number of bytes sent or -1 on error.
 */
ssize_t send_stdin(int connection_fd) {
  std::string input;
  std::cin >> input;
  input.push_back('\n');
  ssize_t bytes_sent =
      send(connection_fd, (input + '\n').c_str(), BUF_BYTES_SIZE, 0);
  if (bytes_sent < 0) {
    perror("send");
    return -1;
  }
  return bytes_sent;
}

/**
 * @brief Handle poll events in a loop.
 * @param fd_poll socket file descriptor
 * @param connection_fd server connection file descriptor
 */
void handle_events(client_poll_arr& fd_poll, int connection_fd) {
  char buffer[BUF_BYTES_SIZE];
  bool quit = false;
  while (!quit) {
    int n_events = poll(fd_poll.data(), fd_poll.size(), POLL_TIMEOUT);
    if (n_events == -1) {
      perror("poll");
      exit(1);
    }
    if (n_events == 0) {
      continue;
    }
    for (auto& sock : fd_poll) {
      if (sock.revents == 0) {
        continue;
      };
      if (sock.revents != POLLIN) {
        std::cerr << "unexpected event: " << sock.revents << '\n';
        quit = true;
        break;
      }
      if (sock.fd == connection_fd) {
        // Server connection is ready to read from
        handle_server_event(connection_fd, buffer);
      } else {
        // Else it's stdin, not the server connection socket
        ssize_t bytes_sent = send_stdin(connection_fd);
        if (bytes_sent < 0) {
          perror("send");
          quit = true;
        }
      }
    }
  }
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

  // File descriptors of stdin and the server
  std::array<pollfd, CLIENT_POLL_SIZE> fd_poll{
      {{STDIN_FILENO, POLLIN, 0}, {connection_fd, POLLIN, 0}}};

  handle_events(fd_poll, connection_fd);

  close(connection_fd);

  return 0;
}