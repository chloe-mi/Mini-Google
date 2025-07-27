/*
 * Copyright ©2025 Hal Perkins.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Spring Quarter 2025 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

// Borrowed code from lecture demo:
// https://courses.cs.washington.edu/courses/cse333/25sp/csenetid/solns/ex16/
// SocketUtil.cc.html
// - Listen
// - getnameinfo
// https://courses.cs.washington.edu/courses/cse333/25sp/lectures/
// 21-network-dns-code/dnsresolve.cc.html
// - IPV4 vs IPV6 ipstrings

#include <stdio.h>       // for snprintf()
#include <unistd.h>      // for close(), fcntl()
#include <sys/types.h>   // for socket(), getaddrinfo(), etc.
#include <sys/socket.h>  // for socket(), getaddrinfo(), etc.
#include <arpa/inet.h>   // for inet_ntop()
#include <netdb.h>       // for getaddrinfo()
#include <errno.h>       // for errno, used by strerror()
#include <string.h>      // for memset, strerror()
#include <iostream>      // for std::cerr, etc.

#include "./ServerSocket.h"

extern "C" {
  #include "libhw1/CSE333.h"
}

using std::string;

namespace hw4 {

// Gets list of addrinfo structs in results (output param)
// based on ai_family and port.
// Returns false if fails to get addr info, true otherwise.
static bool GetAddrInfoStructs(const int& ai_family, const uint16_t& port,
                               struct addrinfo** const results);

// Loops through results until creates and binds to socket.
// Sets listen_fd (output param) to socket's file descriptor,
// and sock_family (output param) to address family.
static void CreateSocketAndBind(const struct addrinfo* const results,
                                int* const listen_fd, int* const sock_family);

// Returns true when listen_sock_fd hears a connection which gets accepted.
// Returns false if a non-recoverable error happens on accept().
static bool WaitForClient(const int& listen_sock_fd,
                          struct sockaddr_storage* const caddr,
                          int* const client_fd);

ServerSocket::ServerSocket(uint16_t port) {
  port_ = port;
  listen_sock_fd_ = -1;
}

ServerSocket::~ServerSocket() {
  // Close the listening socket if it's not zero.  The rest of this
  // class will make sure to zero out the socket if it is closed
  // elsewhere.
  if (listen_sock_fd_ != -1)
    close(listen_sock_fd_);
  listen_sock_fd_ = -1;
}

bool ServerSocket::BindAndListen(int ai_family, int *const listen_fd) {
  // Use "getaddrinfo," "socket," "bind," and "listen" to
  // create a listening socket on port port_.  Return the
  // listening socket through the output parameter "*listen_fd"
  // and set the ServerSocket data member "listen_sock_fd_"

  // STEP 1:
  // get address structs
  struct addrinfo* results;  // output param: list of addrinfo structs
  if (!GetAddrInfoStructs(ai_family, port_, &results)) return false;

  CreateSocketAndBind(results, listen_fd, &sock_family_);
  freeaddrinfo(results);

  if (*listen_fd == -1) {
    // bind failed
    return false;
  } else {
    // set listening file (network) descriptor
    listen_sock_fd_ = *listen_fd;
  }

  // bind succeeded! tell OS we want this to be listening socket
  if (listen(*listen_fd, SOMAXCONN) != 0) {
    std::cerr << "Failed to mark socket as listening: ";
    std::cerr << strerror(errno) << std::endl;
    close(*listen_fd);
    return false;
  } else {
    return true;
  }
}

static bool GetAddrInfoStructs(const int& ai_family, const uint16_t& port,
                               struct addrinfo** const results) {
  // populate hints
  struct addrinfo hints;
  // fill sizeof bytes at &hints with 0
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = ai_family;      // param!
  hints.ai_socktype = SOCK_STREAM;  // stream
  hints.ai_flags = AI_PASSIVE;      // use wildcard "INADDR_ANY"
  hints.ai_flags |= AI_V4MAPPED;    // use v4-mapped v6 if no v6 found
  hints.ai_protocol = IPPROTO_TCP;  // tcp protocol
  hints.ai_canonname = nullptr;
  hints.ai_addr = nullptr;
  hints.ai_next = nullptr;
  // hints.ai_passive not set and null internet host:
  // network address is set to loopback interface address,
  // can communicate w peers running on same host

  // get address structs
  const char* port_str = std::to_string(port).c_str();
  if (int res = getaddrinfo(nullptr,     // char* internet host
                                         // (see above comment)
                            port_str,    // char* service
                            &hints,      // struct addrinfo*
                            results)     // struct addrinfo**
      != 0) {
    std::cerr << "getaddrinfo failed: ";
    std::cerr << gai_strerror(res) << std::endl;
    return false;
  } else {
    return true;
  }
}

static void CreateSocketAndBind(const struct addrinfo* const results,
                                int* const listen_fd, int* const sock_family) {
  // loop through addrinfo structs until create and bind to socket
  *listen_fd = -1;
  for (const struct addrinfo* rp = results; rp != nullptr; rp = rp->ai_next) {
    // create socket
    *listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (*listen_fd == -1) {
      // creating socket failed, try next result
      std::cerr << "socket() failed: " << strerror(errno) << std::endl;
      *listen_fd = -1;
      continue;
    }

    // configure socket so port becomes available on exit
    int optval = 1;
    setsockopt(*listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // bind socket to address and port num returned by getaddrinfo()
    if (bind(*listen_fd, rp->ai_addr, rp->ai_addrlen) != 0) {
      // bind failed, try next addr/port
      close(*listen_fd);
      *listen_fd = -1;
      continue;
    } else {
      // bind worked! set address family field
      *sock_family = rp->ai_family;
      break;
    }
  }  // end of loop over address structs
}

bool ServerSocket::Accept(int *const accepted_fd,
                          string *const client_addr,
                          uint16_t *const client_port,
                          string *const client_dns_name,
                          string *const server_addr,
                          string *const server_dns_name) const {
  // STEP 2:

  // wait for a client to arrive
  struct sockaddr_storage caddr;
  int client_fd;
  if (!WaitForClient(listen_sock_fd_, &caddr, &client_fd)) return false;


  // set output params!


  // set accepted_fd
  *accepted_fd = client_fd;

  // get server addr struct
  struct sockaddr_storage saddr;
  socklen_t saddr_len = sizeof(saddr);
  if (getsockname(client_fd,                                 // sockfd
                reinterpret_cast<struct sockaddr*>(&saddr),  // sockaddr* addr
                &saddr_len)
      != 0) {
    std::cerr << "getsockname() failed" << std::endl;
    close(client_fd);
    return false;
  }

  sa_family_t family = caddr.ss_family;
  struct hostent* chost;  // for client_dns_name
  struct hostent* shost;  // for server_dns_name


  if (family == AF_INET) {
    // set client port
    struct sockaddr_in* caddr_in_4 = (struct sockaddr_in*)&caddr;
    *client_port = ntohs(caddr_in_4->sin_port);

    // set client_addr
    char ip_string_4[INET_ADDRSTRLEN];
    struct in_addr* caddr_ptr_4 = &(caddr_in_4->sin_addr);
    Verify333(inet_ntop(family,       // int addr fam
                        caddr_ptr_4,  // void* network addr struct
                        ip_string_4,  // char[] dst
                        sizeof(ip_string_4))
              != nullptr);
    *client_addr = ip_string_4;

    // for client_dns_name
    chost = gethostbyaddr(caddr_ptr_4,  // void* addr
                          sizeof(*caddr_ptr_4),
                          family);

    // set server addr
    struct sockaddr_in* saddr_in_4 = (struct sockaddr_in*)&saddr;
    struct in_addr* saddr_ptr_4 = &(saddr_in_4->sin_addr);
    Verify333(inet_ntop(family,       // int addr fam
                        saddr_ptr_4,  // void* network addr struct
                        ip_string_4,  // char[] dst
                        sizeof(ip_string_4))
              != nullptr);
    *server_addr = ip_string_4;

    // for server_dns_name
    shost = gethostbyaddr(saddr_ptr_4,  // void* addr
                          sizeof(*saddr_ptr_4),
                          family);


  } else {  // family == AF_INET6
    // set client port
    struct sockaddr_in6* caddr_in_6 = (struct sockaddr_in6*)&caddr;
    *client_port = ntohs(caddr_in_6->sin6_port);

    // set client_addr
    char ip_string_6[INET6_ADDRSTRLEN];
    struct in6_addr* caddr_ptr_6 = &(caddr_in_6->sin6_addr);
    Verify333(inet_ntop(family,       // int addr fam
                        caddr_ptr_6,  // void* network addr struct
                        ip_string_6,  // char[] dst
                        sizeof(ip_string_6))
              != nullptr);
    *client_addr = ip_string_6;

    // for client_dns_name
    chost = gethostbyaddr(caddr_ptr_6,  // void* addr
                          sizeof(*caddr_ptr_6),
                          family);

    // set server addr
    struct sockaddr_in6* saddr_in_6 = (struct sockaddr_in6*)&saddr;
    struct in6_addr* saddr_ptr_6 = &(saddr_in_6->sin6_addr);
    Verify333(inet_ntop(family,       // int addr fam
                        saddr_ptr_6,  // void* network addr struct
                        ip_string_6,  // char[] dst
                        sizeof(ip_string_6))
              != nullptr);
    *server_addr = ip_string_6;

    // for server_dns_name
    shost = gethostbyaddr(saddr_ptr_6,  // void* addr
                          sizeof(*saddr_ptr_6),
                          family);
  }


  // set client_dns_name and server_dns_name
  // no valid dns name? use ip addr
  *client_dns_name = (chost != nullptr) ? chost->h_name : *client_addr;
  *server_dns_name = (shost != nullptr) ? shost->h_name : *server_addr;

  return true;
}

static bool WaitForClient(const int& listen_sock_fd,
                          struct sockaddr_storage* const caddr,
                          int* const client_fd) {
  while (1) {
    socklen_t caddr_len = sizeof(*caddr);
    *client_fd = accept(listen_sock_fd,
                       reinterpret_cast<struct sockaddr *>(caddr),
                       &caddr_len);
    // error
    if (*client_fd < 0) {
      // recoverable
      if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)) {
        continue;
      // non-recoverable
      } else {
        std::cerr << "Failure on accept: " << strerror(errno) << std::endl;
        return false;
      }
    // success
    } else {
      return true;
    }
  }  // end of loop waiting for client to arrive
}

}  // namespace hw4
