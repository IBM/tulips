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

#include <socket/server/Listener.h>
#include <socket/server/State.h>
#include <utils/State.h>
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <linenoise/linenoise.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <tulips/system/Compiler.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/timerfd.h>
#endif

namespace tulips { namespace tools { namespace socket { namespace server {
namespace listener {

/*
 * Listener class.
 */

constexpr size_t BUFFER_SIZE = (1024 * 1024);

class Listener
{
public:
  struct Client
  {
    Client() : fd(-1), counter(0), last(0), delta(0) {}
    Client(int fd) : fd(fd), counter(0), last(0), delta(0) {}

    ~Client()
    {
      if (fd >= 0) {
        ::close(fd);
      }
    }

    int fd;
    size_t counter;
    size_t last;
    size_t delta;
  };

  using Clients = std::map<int, Client>;

  Listener(const uint16_t port)
    : m_fd(0)
#ifdef __linux__
    , m_timerfd(-1)
#endif
    , m_thread()
    , m_lock()
    , m_cond()
    , m_run(true)
    , m_prey(-1)
    , m_buffer()
  {
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    /*
     * Open the socket.
     */
    m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_fd < 0) {
      throw std::runtime_error(strerror(errno));
    }
    /*
     * Reuse address and port.
     */
    int optval = 1;
    setsockopt(m_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    /*
     * Bind the socket.
     */
    if (bind(m_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
      throw std::runtime_error(strerror(errno));
    }
    /*
     * Listen for the clients.
     */
    listen(m_fd, 10);
    /*
     * Open a timer FD.
     */
#ifdef __linux__
    m_timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (m_timerfd < 0) {
      throw std::runtime_error(strerror(errno));
    }
#endif
    /*
     * Start the thread.
     */
    pthread_mutex_init(&m_lock, nullptr);
    pthread_cond_init(&m_cond, nullptr);
    pthread_create(&m_thread, nullptr, &Listener::entrypoint, this);
  }

  ~Listener()
  {
    m_run = false;
    ::pthread_join(m_thread, nullptr);
    pthread_mutex_destroy(&m_lock);
    pthread_cond_destroy(&m_cond);
#ifdef __linux__
    close(m_timerfd);
#endif
    close(m_fd);
  }

  Clients const& clients() const { return m_clients; }

  bool kill(const int client)
  {
    bool result;
    pthread_mutex_lock(&m_lock);
    m_prey = client;
    pthread_cond_wait(&m_cond, &m_lock);
    result = m_prey == -1;
    m_prey = -1;
    pthread_mutex_unlock(&m_lock);
    return result;
  }

private:
  static void* entrypoint(void* arg)
  {
    auto* l = reinterpret_cast<Listener*>(arg);
    l->run();
    return nullptr;
  }

  void dokill()
  {
    pthread_mutex_lock(&m_lock);
    if (m_prey != -1) {
      if (m_clients.count(m_prey) != 0) {
        ::close(m_prey);
        m_clients.erase(m_prey);
        m_prey = -1;
      }
      pthread_cond_signal(&m_cond);
    }
    pthread_mutex_unlock(&m_lock);
  }

  void run()
  {
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    /*
     * Prepare the timer.
     */
#ifdef __linux__
    struct timespec ts = { 1, 0 };
    struct itimerspec its = { ts, ts };
    timerfd_settime(m_timerfd, 0, &its, &its);
#endif
    /*
     * Runner loop.
     */
    while (m_run) {
      /*
       * Prepare the poll descriptors.
       */
      struct pollfd fds[CLIENT_FD_IDX + m_clients.size()];
      memset(fds, 0, sizeof(fds));
      fds[0].fd = m_fd;
      fds[0].events = POLLIN;
#ifdef __linux__
      fds[1].fd = m_timerfd;
      fds[1].events = POLLIN;
#endif
      size_t i = CLIENT_FD_IDX;
      Clients::iterator it;
      for (it = m_clients.begin(); it != m_clients.end(); it++) {
        fds[i].fd = it->second.fd;
        fds[i].events = POLLIN;
        i += 1;
      }
      /*
       * Wait for a connection.
       */
      int res = poll(fds, CLIENT_FD_IDX + m_clients.size(), 100);
      if (res <= 0) {
        dokill();
        continue;
      }
      /*
       * Process the client sockets.
       */
      for (i = CLIENT_FD_IDX; i < CLIENT_FD_IDX + m_clients.size(); i += 1) {
        if (fds[i].revents & POLLIN) {
          res = read(fds[i].fd, m_buffer, BUFFER_SIZE);
          if (res <= 0) {
            ::close(fds[i].fd);
            m_clients.erase(fds[i].fd);
          } else {
            m_clients[fds[i].fd].counter += res;
          }
        } else if (fds[i].revents & POLLHUP) {
          ::close(fds[i].fd);
          m_clients.erase(fds[i].fd);
        }
      }
      /*
       * Process the server socket.
       */
      if (fds[0].revents & POLLIN) {
        int fd = accept(m_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (fd >= 0) {
          fcntl(fd, O_NONBLOCK);
          m_clients[fd].fd = fd;
        }
      }
      /*
       * Process the timer socket.
       */
#ifdef __linux__
      if (fds[1].revents & POLLIN) {
        /*
         * Read the timer.
         */
        uint64_t value;
        ssize_t ret = read(m_timerfd, &value, sizeof(value));
        if (ret <= 0) {
          continue;
        }
        /*
         * Update the client info.
         */
        for (auto& m_client : m_clients) {
          m_client.second.delta =
            (m_client.second.counter - m_client.second.last);
          m_client.second.last = m_client.second.counter;
        }
      }
#endif
      /*
       * Check if there is any client to kill.
       */
      dokill();
    }
  }

#ifdef __linux__
  static constexpr int CLIENT_FD_IDX = 2;
#else
  static constexpr int CLIENT_FD_IDX = 1;
#endif

  int m_fd;
#ifdef __linux__
  int m_timerfd;
#endif
  pthread_t m_thread;
  pthread_mutex_t m_lock;
  pthread_cond_t m_cond;
  volatile bool m_run;
  Clients m_clients;
  int m_prey;
  uint8_t m_buffer[BUFFER_SIZE];
};

/*
 * Our listeners.
 */

using Listeners = std::map<uint16_t, Listener*>;
static Listeners s_listeners;

/*
 * Open.
 */

class Open : public utils::Command
{
public:
  Open() : Command("open a port"), m_hint(" <port>") {}

  void help(UNUSED utils::Arguments const& args) override
  {
    std::cout << "Usage: open PORT" << std::endl;
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
    int port;
    std::istringstream(args[1]) >> port;
    /*
     * Check if the port exists.
     */
    if (s.ports.count(port) != 0) {
      std::cout << "Port is already open." << std::endl;
      return;
    }
    /*
     * Listen to the port.
     */
    try {
      s_listeners[port] = new Listener(port);
      s.ports.insert(port);
      std::cout << "Opening port " << port << "." << std::endl;
    } catch (std::runtime_error& e) {
      std::cout << e.what() << "." << std::endl;
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
 * Close.
 */

class Close : public utils::Command
{
public:
  Close() : Command("close a port"), m_hint(" <port>") {}

  void help(UNUSED utils::Arguments const& args) override
  {
    std::cout << "Usage: close PORT" << std::endl;
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
    int port;
    std::istringstream(args[1]) >> port;
    /*
     * Check if the port exists.
     */
    if (s.ports.count(port) == 0) {
      std::cout << "Port is not open." << std::endl;
      return;
    }
    /*
     * Close the port.
     */
    delete s_listeners[port];
    s_listeners.erase(port);
    s.ports.erase(port);
    std::cout << "Port " << port << " closed." << std::endl;
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
 * Kill.
 */

class Kill : public utils::Command
{
public:
  Kill() : Command("kill a port's client"), m_hint(" <port> <client>") {}

  void help(UNUSED utils::Arguments const& args) override
  {
    std::cout << "Usage: kill PORT CLIENT" << std::endl;
  }

  void execute(utils::State& us, utils::Arguments const& args) override
  {
    auto& s = dynamic_cast<State&>(us);
    /*
     * Check arity.
     */
    if (args.size() != 3) {
      help(args);
      return;
    }
    /*
     * Parse the port socket.
     */
    int port;
    std::istringstream(args[1]) >> port;
    /*
     * Check if the port exists.
     */
    if (s.ports.count(port) == 0) {
      std::cout << "Port is not open." << std::endl;
      return;
    }
    /*
     * Parse the client socket.
     */
    int client;
    std::istringstream(args[2]) >> client;
    /*
     * Kill the port's client.
     */
    if (s_listeners[port]->kill(client)) {
      std::cout << "Client " << client << " killed." << std::endl;
    } else {
      std::cout << "Client does not exist." << std::endl;
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
 * List ports.
 */

class ListPorts : public utils::Command
{
public:
  ListPorts() : Command("list active ports") {}

  void help(UNUSED utils::Arguments const& args) override
  {
    std::cout << "List active ports." << std::endl;
  }

  void execute(utils::State& us, UNUSED utils::Arguments const& args) override
  {
    auto& s = dynamic_cast<State&>(us);
    /*
     * Check ports.
     */
    if (s.ports.empty()) {
      std::cout << "No active ports." << std::endl;
    } else {
      /*
       * Print the header.
       */
      std::cout << std::setw(7) << std::left << "Port" << std::setw(5)
                << std::left << "Clients" << std::right << std::endl;
      /*
       * Print the ports.
       */
      Ports::iterator it;
      for (it = s.ports.begin(); it != s.ports.end(); it++) {
        Listener::Clients const& clients = s_listeners[*it]->clients();
        std::cout << std::setw(7) << std::left << *it << std::setw(5)
                  << std::left << clients.size() << std::right << std::endl;
      }
    }
  }
};

/*
 * List client.
 */

class ListClients : public utils::Command
{
public:
  ListClients() : Command("list active clients"), m_hint(" <port>") {}

  void help(UNUSED utils::Arguments const& args) override
  {
    std::cout << "Usage: lsclients [PORT]" << std::endl;
  }

  void execute(utils::State& us, utils::Arguments const& args) override
  {
    auto& s = dynamic_cast<State&>(us);
    /*
     * Check arity.
     */
    if (args.size() != 1 && args.size() != 2) {
      help(args);
      return;
    }
    /*
     * If no arguments, display all the clients for all the ports.
     */
    if (args.size() == 1) {
      /*
       * Check if there is any port.
       */
      if (s.ports.empty()) {
        std::cout << "No open port." << std::endl;
        return;
      }
      /*
       * Check if there is any client.
       */
      size_t count = 0;
      Listeners::const_iterator it;
      for (it = s_listeners.begin(); it != s_listeners.end(); it++) {
        count += it->second->clients().size();
      }
      if (count == 0) {
        std::cout << "No active client." << std::endl;
        return;
      }
      /*
       * Print the header.
       */
      std::cout << std::setw(7) << std::left << "Port" << std::setw(7)
                << std::left << "Client" << std::setw(12) << std::left
                << "Bytes" << std::setw(12) << std::left << "Throughput"
                << std::endl;
      /*
       * Iterate over the ports.
       */
      for (it = s_listeners.begin(); it != s_listeners.end(); it++) {
        Listener::Clients const& clients = it->second->clients();
        size_t i = 0, size = clients.size();
        if (size > 0) {
          Listener::Clients::const_iterator jt;
          for (jt = clients.begin(); jt != clients.end(); jt++) {
            /*
             * Print the port the first time around.
             */
            if (i == 0) {
              std::cout << std::setw(7) << it->first;
            } else {
              std::cout << std::setw(7) << " ";
            }
            /*
             * Print the client info.
             */
            std::cout << std::setw(7) << std::left << jt->second.fd
                      << std::setw(12) << std::left << jt->second.counter;
            printThroughput(jt->second.delta);
            std::cout << std::endl;
            i += 1;
          }
        }
      }
    }
    /*
     * Display the clients of a specific port.
     */
    else {
      /*
       * Parse the port socket.
       */
      int port;
      std::istringstream(args[1]) >> port;
      /*
       * Check if the port exists.
       */
      if (s.ports.count(port) == 0) {
        std::cout << "Port does not exists." << std::endl;
        return;
      }
      /*
       * Check if there is any clients.
       */
      Listener::Clients const& clients = s_listeners[port]->clients();
      if (clients.empty()) {
        std::cout << "Port has no clients." << std::endl;
        return;
      }
      /*
       * Print the header.
       */
      std::cout << std::setw(7) << std::left << "Client" << std::setw(12)
                << std::left << "Bytes" << std::setw(12) << std::left
                << "Throughput" << std::endl;
      /*
       * Print the ports.
       */
      Listener::Clients::const_iterator it;
      for (it = clients.begin(); it != clients.end(); it++) {
        std::cout << std::setw(7) << std::left << it->second.fd << std::setw(12)
                  << std::left << it->second.counter;
        printThroughput(it->second.delta);
        std::cout << std::endl;
      }
    }
  }

  char* hint(UNUSED utils::State& s, int* color, UNUSED int* bold) override
  {
    *color = LN_GREEN;
    return (char*)m_hint.c_str();
  }

private:
  static void printThroughput(const uint64_t value)
  {
    double bps = value << 3;
    std::ostringstream oss;
    oss << std::setprecision(3);
    /*
     * Update unit.
     */
    if (bps > 1e9L) {
      bps /= 1e9L;
      oss << bps << " Gb/s";
    } else if (bps > 1e6L) {
      bps /= 1e6L;
      oss << bps << " Mb/s";
    } else if (bps > 1e3L) {
      bps /= 1e3L;
      oss << bps << " Kb/s";
    } else {
      oss << bps << " b/s";
    }
    std::cout << std::setw(12) << std::left << oss.str();
  }

  std::string m_hint;
};

/*
 * Helpers.
 */

void
populate(utils::Commands& cmds)
{
  cmds["open"] = new Open;
  cmds["close"] = new Close;
  cmds["kill"] = new Kill;
  cmds["lsports"] = new ListPorts;
  cmds["lsclients"] = new ListClients;
}

}}}}}
