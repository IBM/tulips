/*
 * Copyright (c) 2020, International Business Machines
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <socket/client/Connection.h>
#include <socket/client/State.h>
#include <utils/State.h>
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <linenoise/linenoise.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <tulips/stack/IPv4.h>
#include <tulips/stack/Utils.h>
#include <tulips/system/Compiler.h>
#include <tulips/system/Utils.h>
#include <unistd.h>

namespace tulips { namespace tools { namespace socket { namespace client {
namespace connection {

/*
 * Close.
 */

class Close : public utils::Command
{
public:
  Close() : Command("close a connection"), m_hint(" <socket>") {}

  void help(UNUSED utils::Arguments const& args) override
  {
    std::cout << "Usage: close SOCKET" << std::endl;
  }

  void execute(utils::State& us, utils::Arguments const& args) override
  {
    auto& s = dynamic_cast<State&>(us);
    /*
     * Check arity.
     */
    if (args.size() != 2) {
      help(args);
      return;
    }
    /*
     * Parse the port socket.
     */
    int sock;
    std::istringstream(args[1]) >> sock;
    /*
     * Check if the connection exists.
     */
    if (s.connections.count(sock) == 0) {
      std::cout << "No such socket." << std::endl;
      return;
    }
    /*
     * Grab and close the connection.
     */
    Connection c = s.connections[sock];
    close(sock);
    s.connections.erase(sock);
    std::cout << "Connection " << c.first.toString() << ":" << c.second
              << " closed." << std::endl;
  }

  char* hint(UNUSED utils::State& s, int* color, UNUSED int* bold) override
  {
    *color = LN_GREEN;
    return (char*)m_hint.c_str();
  }

private:
  std::string m_hint;
};

/*
 * Clear.
 */

class Clear : public utils::Command
{
public:
  Clear() : Command("clear all connections") {}

  void help(UNUSED utils::Arguments const& args) override
  {
    std::cout << "Clear all connections" << std::endl;
  }

  void execute(utils::State& us, UNUSED utils::Arguments const& args) override
  {
    auto& s = dynamic_cast<State&>(us);
    /*
     * Grab and close the connection.
     */
    Connections::iterator it;
    for (it = s.connections.begin(); it != s.connections.end(); it++) {
      close(it->first);
    }
    s.connections.clear();
    std::cout << "Connections cleared." << std::endl;
  }

  char* hint(UNUSED utils::State& s, int* color, UNUSED int* bold) override
  {
    *color = LN_GREEN;
    return (char*)m_hint.c_str();
  }

private:
  std::string m_hint;
};

/*
 * Connect.
 */

class Connect : public utils::Command
{
public:
  Connect() : Command("connect to a remote TCP server"), m_hint(" <ip> <port>")
  {}

  void help(UNUSED utils::Arguments const& args) override
  {
    std::cout << "Usage: connect IP PORT" << std::endl;
  }

  void execute(utils::State& us, utils::Arguments const& args) override
  try {
    auto& s = dynamic_cast<State&>(us);
    /*
     * Check arity.
     */
    if (args.size() != 3) {
      help(args);
      return;
    }
    /*
     * Parse the IP address.
     */
    stack::ipv4::Address ip(args[1]);
    /*
     * Parse the port number.
     */
    uint16_t port;
    std::istringstream(args[2]) >> port;
    /*
     * Create socket.
     */
    int sk = ::socket(PF_INET, SOCK_STREAM, 0);
    if (sk < 0) {
      std::cout << strerror(errno) << std::endl;
      return;
    }
    /*
     * Create the address definition.
     */
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    memcpy(&saddr.sin_addr.s_addr, ip.data(), 4);
    saddr.sin_port = htons(port);
    /*
     * Connect.
     */
    if (connect(sk, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
      std::cout << strerror(errno) << std::endl;
      ::close(sk);
      return;
    }
    /*
     * Register the socket in the list.
     */
    s.connections[sk] = std::make_pair(ip, port);
    std::cout << "OK - " << sk << std::endl;
  } catch (...) {
    help(args);
  }

  char* hint(UNUSED utils::State& s, int* color, UNUSED int* bold) override
  {
    *color = LN_GREEN;
    return (char*)m_hint.c_str();
  }

private:
  std::string m_hint;
};

/*
 * List.
 */

class List : public utils::Command
{
public:
  List() : Command("list active connections") {}

  void help(UNUSED utils::Arguments const& args) override
  {
    std::cout << "List active connections." << std::endl;
  }

  void execute(utils::State& us, UNUSED utils::Arguments const& args) override
  {
    auto& s = dynamic_cast<State&>(us);
    /*
     * Check connections.
     */
    if (s.connections.empty()) {
      std::cout << "No active connections." << std::endl;
    } else {
      /*
       * Print the header.
       */
      std::cout << std::setw(7) << std::left << "Socket " << std::setw(16)
                << std::left << "IP " << std::setw(5) << std::left << "Port"
                << std::right << std::endl;
      /*
       * Print the connections.
       */
      Connections::iterator it;
      for (it = s.connections.begin(); it != s.connections.end(); it++) {
        std::cout << std::setw(7) << std::left << it->first << std::setw(16)
                  << std::left << it->second.first.toString() << std::setw(5)
                  << std::left << it->second.second << std::right << std::endl;
      }
    }
  }
};

/*
 * Read.
 */

class Read : public utils::Command
{
public:
  Read() : Command("read data from an active connection"), m_hint(" <socket>")
  {}

  void help(UNUSED utils::Arguments const& args) override
  {
    std::cout << "Usage: read SOCKET" << std::endl;
  }

  void execute(utils::State& us, utils::Arguments const& args) override
  {
    auto& s = dynamic_cast<State&>(us);
    /*
     * Check arity.
     */
    if (args.size() != 2) {
      help(args);
      return;
    }
    /*
     * Parse the port socket.
     */
    int sock;
    std::istringstream(args[1]) >> sock;
    /*
     * Check if the connection exists.
     */
    if (s.connections.count(sock) == 0) {
      std::cout << "No such socket." << std::endl;
      return;
    }
    /*
     * Get the available bytes.
     */
    int len = 0;
    ioctl(sock, FIONREAD, &len);
    if (len == 0) {
      std::cout << "No data available." << std::endl;
      return;
    }
    /*
     * Read data.
     */
    uint8_t buffer[len];
    int res = read(sock, buffer, len);
    if (res <= 0) {
      std::cout << strerror(errno) << std::endl;
    }
    stack::utils::hexdump(buffer, len, std::cout);
  }

  char* hint(UNUSED utils::State& s, int* color, UNUSED int* bold) override
  {
    *color = LN_GREEN;
    return (char*)m_hint.c_str();
  }

private:
  std::string m_hint;
};

/*
 * Write.
 */

class Write : public utils::Command
{
public:
  Write()
    : Command("write data to an active connection")
    , m_hint(" <socket> <data> ...")
  {}

  void help(UNUSED utils::Arguments const& args) override
  {
    std::cout << "Usage: write SOCKET DATA [DATA ...]" << std::endl;
  }

  void execute(utils::State& us, utils::Arguments const& args) override
  {
    auto& s = dynamic_cast<State&>(us);
    /*
     * Check arity.
     */
    if (args.size() < 3) {
      help(args);
      return;
    }
    /*
     * Parse the port socket.
     */
    int sock;
    std::istringstream(args[1]) >> sock;
    /*
     * Check if the connection exists.
     */
    if (s.connections.count(sock) == 0) {
      std::cout << "No such socket." << std::endl;
      return;
    }
    /*
     * Write data.
     */
    std::string data;
    std::vector<std::string> rest(args.begin() + 2, args.end());
    system::utils::join(rest, ' ', data);
    int res = write(sock, data.c_str(), data.length());
    if (res <= 0) {
      std::cout << strerror(errno) << std::endl;
    }
    std::cout << "OK - " << res << std::endl;
  }

  char* hint(UNUSED utils::State& s, int* color, UNUSED int* bold) override
  {
    *color = LN_GREEN;
    return (char*)m_hint.c_str();
  }

private:
  std::string m_hint;
};

/*
 * Helpers.
 */

void
populate(utils::Commands& cmds)
{
  cmds["close"] = new Close;
  cmds["clear"] = new Clear;
  cmds["connect"] = new Connect;
  cmds["list"] = new List;
  cmds["read"] = new Read;
  cmds["write"] = new Write;
}

}}}}}
