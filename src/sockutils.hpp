#ifndef SOCKUTILS_HPP
#define SOCKUTILS_HPP

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

#include <utility>

/**
 * @brief Extract an universal pointer to IP/IPv6 from a socket's address
 *
 * @param address pointer to a sockaddr_storage address, which could hold a IPv6
 * address or an old address
 * @return an universal void pointer to the IP/IPv6 address. We want void*
 * because inet_ntop uses it that way
 */
inline void *get_sinaddr(sockaddr_storage *address) {
  return ((sockaddr *)address)->sa_family == AF_INET
             ? (void *)&(((sockaddr_in *)address)->sin_addr)
             : (void *)&(((sockaddr_in6 *)address)->sin6_addr);
}

/**
 * @brief Get the address object into an addrinfo struct
 *
 * @param ip IP address. If null, let the system get its own one (AI_PASSIVE)
 * @param port port
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

#endif