#include <arpa/inet.h>
#include <netdb.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstring>

#include "sockutils.hpp"

/**
 * @brief Attempt to create a listener socket with correct settings
 * and return its file descriptor.
 * @return file descriptor of the listener socket or -1 on error.
 */
int get_listener_socket() {
  // iterator used for choosing the first right server info from getaddrinfo()
  addrinfo* it = nullptr;
  // file descriptor of the listener socket
  int listener_fd = 0;
  constexpr int REUSABLE = 1;
  constexpr int NON_BLOCKING = 1;

  // Get the list of possible server addresses to use (we use the first one)
  const auto [server_addresses, err_code] = get_address(nullptr, PORT);
  if (err_code != 0) {
    std::cerr << "getaddrinfo:" << gai_strerror(err_code) << '\n';
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

    if (setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &REUSABLE,
                   sizeof(REUSABLE)) == -1) {
      perror("setsockopt");
      close(listener_fd);
      continue;
    }

    if (fcntl(listener_fd, O_NONBLOCK, &NON_BLOCKING) != 0) {
      perror("fcntl");
      close(listener_fd);
      exit(-1);
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
    std::cerr << "failed to create/bind the socket\n";
    return -1;
  }

  if (listen(listener_fd, LISTENER_BACKLOG) == -1) {
    perror("listen");
    return -1;
  }

  return listener_fd;
}

/**
 * @brief Put bytes coming from a socket into a buffer.
 * @param sock socket to read from
 * @param data_buffer char buffer to receive byte
 * @param conn_map map of addresses based on file descriptors
 * @return message received or "" on error, or "\nblock" on EWOULDBLOCK.
 */
std::string receive_message(const pollfd& sock, char data_buffer[],
                            const address_map& conn_map) {
  ssize_t bytes_received = recv(sock.fd, data_buffer, BUF_BYTES_SIZE, 0);
  if (bytes_received < 0) {
    if (errno != EWOULDBLOCK) {
      perror("recv");
      return "";
    }
    return "\nblock";
  }
  if (bytes_received == 0) {
    std::cout << "connection closed\n";
    return "";
  }
  std::cout << bytes_received << " bytes received from " << '[' << sock.fd
            << ']' << conn_map.at(sock.fd) << "\n";

  return encode_server_msg(sock.fd, conn_map.at(sock.fd), (char*)data_buffer);
}

/**
 * @brief Send a message to all connected clients (except the listener socket).
 * @param fd_poll the event poll
 * @param listener_fd listener socket's file descriptor
 * @param msg string message to send
 * @param conn_map map of addresses based on file descriptors
 * @return number of failed send attempts.
 */
int broadcast_message(const server_poll_arr& fd_poll, int listener_fd,
                      const std::string& msg, address_map& conn_map) {
  ssize_t bytes_sent = -1;
  int bad_sends = 0;
  for (const auto& sock : fd_poll) {
    if (sock.fd != listener_fd && sock.fd != -1) {
      bytes_sent = send(sock.fd, msg.c_str(), BUF_BYTES_SIZE, 0);
      std::cout << "sent " << bytes_sent << " bytes to [" << sock.fd << ']'
                << conn_map.at(sock.fd) << "\n";
      if (bytes_sent < 0) {
        perror("send");
        bad_sends++;
      }
    }
  }
  return bad_sends;
}

/**
 * @brief Handle an event on a the client's file descriptor (receive and
 * broadcast the message).
 * @param n_socks variable to increment on succesful client socket creation
 * @param listener_fd listener socket's file descriptor
 * @param fd_poll the event poll
 * @param conn_map map of addresses based on file descriptors
 * @return new client's file descriptor or -1 on error.
 */
int handle_listener_event(size_t& n_socks, int listener_fd,
                          server_poll_arr& fd_poll, address_map& conn_map) {
  // holds client information
  sockaddr_storage client{};
  socklen_t client_size = sizeof(client);
  char client_addr[INET6_ADDRSTRLEN];

  int current_fd = accept(listener_fd, (sockaddr*)&client, &client_size);
  if (current_fd < 0) {
    if (errno != EWOULDBLOCK) {
      perror("accept");
    }
    return -1;
  }

  fd_poll.at(n_socks).fd = current_fd;
  fd_poll.at(n_socks++).events = POLLIN;
  inet_ntop(client.ss_family, get_sinaddr(&client), client_addr, client_size);
  conn_map.insert_or_assign(current_fd, client_addr);
  std::cout << "connected: [" << current_fd << ']' << client_addr << '\n';

  return current_fd;
}

/**
 * @brief Handle an event on a the client's file descriptor (receive and
 * broadcast the message).
 * @param pollfd a socket with the event
 * @param fd_poll the event poll
 * @param listener_fd listener socket's file descriptor
 * @param conn_map map of addresses based on file descriptors
 * @return 0 on success, -1 on error.
 */
int handle_client_event(const pollfd& sock, const server_poll_arr& fd_poll,
                        int listener_fd, address_map& conn_map) {
  char data_buffer[BUF_BYTES_SIZE];
  const auto msg = receive_message(sock, data_buffer, conn_map);
  if (msg.empty()) {
    return -1;
  }
  if (msg != "\nblock") {
    const int bad_sends =
        broadcast_message(fd_poll, listener_fd, msg, conn_map);
    if (bad_sends > 1) {
      return -1;
    };
  }
  return 0;
}

/**
 * @brief Handle poll events in a loop.
 * @param fd_poll the event poll
 * @param listener_fd listener socket's file descriptor
 */
void handle_events(server_poll_arr& fd_poll, int listener_fd) {
  // utility connection map (file descriptor -> address)
  address_map conn_map{};
  size_t n_socks = 1;

  while (true) {
    int n_events = poll(fd_poll.data(), n_socks, POLL_TIMEOUT);
    if (n_events == -1) {
      perror("poll");
      exit(1);
    }
    if (n_events == 0) {
      std::cout << "poll() timeout, waiting for events...\n";
      continue;
    }
    for (auto& sock : fd_poll) {
      if (sock.revents == 0) {
        continue;
      };
      if ((sock.revents & POLLHUP) != 0) {
        std::cerr << "client hanged up, closing their socket\n";
        close(sock.fd);
        sock.fd = -1;
        break;
      }
      if (sock.revents != POLLIN) {
        std::cerr << "unexpected event: " << sock.revents << '\n';
        break;
      }

      if (sock.fd == listener_fd) {
        // we're in the listener socket
        handle_listener_event(n_socks, listener_fd, fd_poll, conn_map);
        continue;
      }
      // we're listening to the client's socket
      if (handle_client_event(sock, fd_poll, listener_fd, conn_map) < 0) {
        close(sock.fd);
        sock.fd = -1;
      }
    }
  }
}

int main() {
  int listener_fd = get_listener_socket();
  if (listener_fd == -1) {
    return 1;
  }

  // client info

  // poll that handles events, the first entry is the listener socket
  server_poll_arr fd_poll{};
  fd_poll.fill({.fd = -1});
  fd_poll.at(0) = {.fd = listener_fd, .events = POLLIN};

  handle_events(fd_poll, listener_fd);

  for (const auto& sock : fd_poll) {
    if (sock.fd >= 0) {
      close(sock.fd);
    }
  }
  return 0;
}
