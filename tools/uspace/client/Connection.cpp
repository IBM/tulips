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

#include <uspace/client/Connection.h>
#include <uspace/client/State.h>
#include <iostream>
#include <sstream>
#include <linenoise/linenoise.h>
#include <tulips/system/Compiler.h>
#include <tulips/system/Utils.h>

namespace tulips { namespace tools { namespace uspace { namespace client {
namespace connection {

/*
 * Close.
 */

class Close : public utils::Command
{
public:
  Close() : Command("close a connection"), m_hint(" <id>") {}

  void help(UNUSED utils::Arguments const& args) override
  {
    std::cout << "Usage: close ID" << std::endl;
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
    Client::ID c;
    std::istringstream(args[1]) >> c;
    /*
     * Check if the connection exists.
     */
    if (s.ids.count(c) == 0) {
      std::cout << "No such connection." << std::endl;
      return;
    }
    /*
     * Grab and close the connection.
     */
    switch (s.poller.close(c)) {
      case Status::Ok: {
        std::cout << "Connection closed." << std::endl;
        s.ids.erase(c);
        break;
      }
      case Status::NotConnected: {
        std::cout << "No such connection." << std::endl;
        break;
      }
      default: {
        std::cout << "Error." << std::endl;
        break;
      }
    }
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
     * Create a connection.
     */
    Client::ID id;
    switch (s.poller.connect(ip, port, id)) {
      case Status::Ok: {
        std::cout << "OK - " << id << std::endl;
        s.ids.insert(id);
        break;
      }
      default: {
        std::cout << "Error." << std::endl;
        break;
      }
    }
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
    if (s.ids.empty()) {
      std::cout << "No active connections." << std::endl;
    } else {
      /*
       * Print the header.
       */
      std::cout << std::setw(7) << std::left << "ID " << std::setw(16)
                << std::left << "IP " << std::setw(12) << std::left
                << "Local port" << std::setw(11) << std::left << "Remote port"
                << std::right << std::endl;
      /*
       * Print the connections.
       */
      IDs::iterator it;
      stack::ipv4::Address ip;
      stack::tcpv4::Port lport, rport;
      for (it = s.ids.begin(); it != s.ids.end(); it++) {
        s.poller.get(*it, ip, lport, rport);
        std::cout << std::setw(7) << std::left << *it << std::setw(16)
                  << std::left << ip.toString() << std::setw(12) << std::left
                  << lport << std::setw(11) << std::left << rport << std::right
                  << std::endl;
      }
    }
  }
};

class Write : public utils::Command
{
public:
  Write()
    : Command("write data to an active connection"), m_hint(" <id> <data> ...")
  {}

  void help(UNUSED utils::Arguments const& args) override
  {
    std::cout << "Usage: write ID DATA [DATA ...]" << std::endl;
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
    Client::ID id;
    std::istringstream(args[1]) >> id;
    /*
     * Check if the connection exists.
     */
    if (s.ids.count(id) == 0) {
      std::cout << "No such connection." << std::endl;
      return;
    }
    /*
     * Write data.
     */
    std::string data;
    std::vector<std::string> rest(args.begin() + 2, args.end());
    system::utils::join(rest, ' ', data);
    switch (s.poller.write(id, data)) {
      case Status::Ok: {
        std::cout << "OK - " << data.length() << "." << std::endl;
        break;
      }
      default: {
        std::cout << "Error." << std::endl;
      }
    }
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
  cmds["connect"] = new Connect;
  cmds["list"] = new List;
  cmds["write"] = new Write;
}

}}}}}
