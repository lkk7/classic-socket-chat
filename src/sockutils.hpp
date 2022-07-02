#ifndef SOCKUTILS_HPP
#define SOCKUTILS_HPP

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/poll.h>
#include <sys/socket.h>

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <utility>

/* Port on which the server and clients operate */
inline constexpr auto PORT = "12333";
/* Buffer size for all sockets, for send() and recv()  */
inline constexpr size_t BUF_BYTES_SIZE = 2048;
/* Timeout for all poll() calls  */
inline constexpr int POLL_TIMEOUT = 10000;
/* Maximum  size of the queue of pending connections for the listener socket */
inline constexpr int LISTENER_BACKLOG = 100;
/* The client only polls the standard input and the server socket */
inline constexpr int CLIENT_POLL_SIZE = 2;
/* The server polls up to that much client sockets, minus one listener socket */
inline constexpr int SERVER_POLL_SIZE = 100;

using client_poll_arr = std::array<pollfd, CLIENT_POLL_SIZE>;
using server_poll_arr = std::array<pollfd, SERVER_POLL_SIZE>;
using address_map = std::unordered_map<int, std::string>;

/**
 * @brief Extract an universal pointer to IP/IPv6 from a socket's address.
 * @param address pointer to a sockaddr_storage address, which could hold a IPv6
 * address or an old address
 * @return an universal void pointer to the IP/IPv6 address. We want void*
 * because inet_ntop uses it that way.
 */
inline void *get_sinaddr(sockaddr_storage *address) {
  return ((sockaddr *)address)->sa_family == AF_INET
             ? (void *)&(((sockaddr_in *)address)->sin_addr)
             : (void *)&(((sockaddr_in6 *)address)->sin6_addr);
}

/**
 * @brief Get the address object into an addrinfo struct.
 * @param ip IP address. If null, let the system get its own one (AI_PASSIVE)
 * @param port
 * @return std::pair containing the address and error code.
 */
inline std::pair<addrinfo *, int> get_address(const char *ip,
                                              const char *port) {
  // linked list of possible server addresses to use (we use the first one)
  addrinfo *address = nullptr;
  addrinfo hints = {
      .ai_family = AF_UNSPEC,
      .ai_socktype = SOCK_STREAM,
      .ai_flags = (ip == nullptr ? AI_PASSIVE : 0),
  };
  int err_code = getaddrinfo(ip, port, &hints, &address);
  return {address, err_code};
}

/**
 * @brief Encode a message from client into a format intended to share with all
 * clients in the chat.
 * @param sock file descriptor of the socket that sent us the message
 * @param address address of the message sender
 * @param client_msg message from the client
 * @return std::string with ready-to-c_str()-and-send content.
 */
inline std::string encode_server_msg(int sock, const std::string &address,
                                     const char *client_msg) {
  // Find the last (which is the only one) '\n' to fill a string only until its
  // occurrence
  const auto *msg_end = strchr(client_msg, '\n');
  if (msg_end == nullptr) {
    std::cerr << "encode_server_msg: wrong message format, '\\n' not found!\n";
    return "";
  }
  const auto str_buffer =
      std::string{client_msg, static_cast<size_t>(msg_end - client_msg)};

  std::ostringstream str_stream;
  str_stream << sock << '\n' << address << '\n' << str_buffer << '\n';
  return str_stream.str();
}

/**
 * @brief Decode a message from the server into printable parts.
 * @param server_msg message from the server
 * @return std::tuple containing all the printable info. It consists of the
 * server's socket descriptor of the original message sender, original sender's
 * address and the message content.
 */
inline std::tuple<std::string, std::string, std::string> decode_server_msg(
    const char *server_msg) {
  if (*server_msg == 0) {
    return {"", "", ""};
  }
  const auto msg_str = std::string{server_msg};
  const auto sock_end = msg_str.find('\n');
  const auto address_end = msg_str.find('\n', sock_end + 1);
  auto msg_end = msg_str.find('\n', address_end + 1);

  return {
      msg_str.substr(0, sock_end),
      msg_str.substr(sock_end + 1, address_end - sock_end - 1),
      msg_str.substr(address_end + 1, msg_end - address_end - 1),
  };
}
#endif