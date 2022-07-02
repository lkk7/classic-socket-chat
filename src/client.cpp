#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "sockutils.hpp"

#define PORT "12321"
#define MAXDATASIZE 100

/**
 * @brief connect to a server described by an addrinfo struct
 *
 * @return socket file descriptor, or -1 on error
 */
int server_connect(addrinfo* addresses) {
  int connection_fd;
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
    return connection_fd;
  }
  fprintf(stderr, "failed to connect the socket");
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
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err_code));
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
  if (bytes_received == -1) {
    perror("recv");
    return -1;
  }
  printf("received '%s'\n", buffer);
  buffer[bytes_received] = '\0';
  return bytes_received;
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: client <hostname>\n");
    exit(1);
  }

  int connection_fd = get_connection(argv[1]);
  if (connection_fd == -1) {
    exit(1);
  }

  char buffer[MAXDATASIZE] = "Test message!";
  int bytes_sent = send(connection_fd, buffer, MAXDATASIZE, 0);
  if (bytes_sent < 0) {
    perror("send");
    exit(1);
  }
  printf("Sent %d bytes\n", bytes_sent);
  int bytes_received = receive_bytes(connection_fd, buffer, MAXDATASIZE);
  if (bytes_received == -1) {
    exit(1);
  }
  printf("Received %d bytes\n", bytes_received);

  close(connection_fd);

  return 0;
}