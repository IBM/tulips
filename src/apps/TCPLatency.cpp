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

#include <tulips/api/Defaults.h>
#include <tulips/api/Client.h>
#include <tulips/api/Server.h>
#include <tulips/apps/TCPLatency.h>
#include <tulips/ssl/Client.h>
#include <tulips/ssl/Server.h>
#include <tulips/stack/ethernet/Producer.h>
#include <tulips/stack/Utils.h>
#include <tulips/system/Affinity.h>
#include <tulips/system/Compiler.h>
#include <tulips/transport/pcap/Device.h>
#include <csignal>
#include <iostream>
#include <sstream>

using namespace tulips;
using namespace stack;
using namespace transport;

/*
 * 100ms wait delay.
 */
constexpr size_t WAIT_DELAY = 100000000ULL;

static bool show_latency = false;
static bool keep_running = true;
static size_t alarm_delay = 0;
static size_t sends = 0;
static size_t successes = 0;
static size_t iterations = 0;

static void
signal_handler(UNUSED int signal)
{
  keep_running = false;
}

static void
alarm_handler(UNUSED int signal)
{
  show_latency = true;
  alarm(alarm_delay);
}

namespace tulips { namespace apps { namespace tcplatency {

namespace Client {

enum class State
{
  Connect,
  Run,
  Closing
};

class Delegate : public defaults::ClientDelegate
{
public:
  Delegate(const bool nodelay) : m_nodelay(nodelay) {}

  void* onConnected(UNUSED tulips::Client::ID const& id,
                    UNUSED void* const cookie, uint8_t& opts) override
  {
    opts = m_nodelay ? tcpv4::Connection::NO_DELAY : 0;
    return nullptr;
  }

private:
  bool m_nodelay;
};

int
run(Options const& options, transport::Device& base_device)
{
  /*
   * Create an PCAP device
   */
  transport::pcap::Device* pcap_device = nullptr;
  Device* device = &base_device;
  /**
   * Signal handler
   */
  signal(SIGINT, signal_handler);
  signal(SIGALRM, alarm_handler);
  /**
   * Set the alarm
   */
  alarm_delay = options.interval();
  alarm(alarm_delay);
  /*
   * Run as sender.
   */
  uint8_t data[options.length()];
  State state = State::Connect;
  /*
   * Check if we should wrap the device in a PCAP device.
   */
  if (options.dumpPackets()) {
    pcap_device = new transport::pcap::Device(base_device, "client.pcap");
    device = pcap_device;
  }
  /*
   * Define the client delegate.
   */
  Client::Delegate delegate(options.noDelay());
  /*
   * Build the client.
   */
  interface::Client* client = nullptr;
  if (options.withSSL()) {
    client = new tulips::ssl::Client(delegate, *device, 1,
                                     tulips::ssl::Protocol::TLSv1_2,
                                     options.sslCert(), options.sslKey());
  } else {
    client = new tulips::Client(delegate, *device, 1);
  }
  /*
   * Set the CPU affinity.
   */
  if (options.cpuId() >= 0) {
    if (!system::setCurrentThreadAffinity(options.cpuId())) {
      throw std::runtime_error("Cannot set CPU ID");
    }
  }
  /*
   * Open a connection.
   */
  tulips::Client::ID id;
  client->open(id);
  /*
   * Latency timer.
   */
  system::Timer timer;
  if (options.usDelay() != 0) {
    timer.set((CLOCK_SECOND * options.usDelay()) / 1000000ULL);
  }
  /*
   * Run loop.
   */
  bool keep_running_local = keep_running;
  uint32_t res = 0;
  size_t last = 0, iter = 0;
  while (keep_running_local) {
    /*
     * Process the stack
     */
    if (options.wait()) {
      switch (device->wait(*client, WAIT_DELAY)) {
        case Status::Ok: {
          break;
        }
        case Status::NoDataAvailable: {
          client->run();
        }
        default: {
          std::cout << "Unknown error, aborting" << std::endl;
          keep_running_local = false;
          continue;
        }
      }
    } else {
      switch (device->poll(*client)) {
        case Status::Ok: {
          break;
        }
        case Status::NoDataAvailable: {
          if ((iter++ & 0x1FULL) == 0) {
            client->run();
          }
          break;
        }
        default: {
          std::cout << "Unknown error, aborting" << std::endl;
          keep_running_local = false;
          continue;
        }
      }
    }
    /*
     * Process the application
     */
    switch (state) {
      case State::Connect: {
        keep_running_local = keep_running;
        switch (client->connect(id, options.destination(), options.port())) {
          case Status::Ok: {
            state = State::Run;
            break;
          }
          case Status::OperationInProgress: {
            break;
          }
          default: {
            keep_running_local = false;
            break;
          }
        }
        break;
      }
      case State::Run: {
        /*
         * Show client latency if requested.
         */
        if (show_latency) {
          size_t cur = sends, delta = (cur - last) / alarm_delay;
          double hits = (double)successes / (double)iterations * 100.0;
          last = cur;
          successes = 0;
          iterations = 0;
          show_latency = false;
          if (delta > 0) {
            std::ostringstream oss;
            oss << std::setprecision(2) << std::fixed;
            oss << delta;
            options.noDelay() ? oss << " round-trips/s" : oss << " sends/s";
            oss << ", hits: " << hits << "%, latency: ";
            double lat = client->averageLatency(id);
            if (lat > 1e9L) {
              oss << (lat / 1e9L) << " s";
            } else if (lat > 1e6L) {
              oss << (lat / 1e6L) << " ms";
            } else if (lat > 1e3L) {
              oss << (lat / 1e3L) << " us";
            } else {
              oss << lat << " ns";
            }
            std::cout << oss.str() << std::endl;
          }
        }
        /*
         * Process the delay.
         */
        if (options.usDelay() != 0) {
          if (!timer.expired()) {
            break;
          }
          timer.reset();
        }
        /*
         * Check if we need to stop.
         */
        if (!keep_running) {
          client->close(id);
          state = State::Closing;
          break;
        }
        /*
         * Process the iteration.
         */
        iterations += 1;
        auto* payload = reinterpret_cast<uint64_t*>(data);
        *payload = sends;
        Status status = client->send(id, options.length(), data, res);
        switch (status) {
          case Status::Ok: {
            successes += 1;
            if (res == options.length()) {
              sends += 1;
              res = 0;
            }
            if (options.count() > 0 && sends == options.count()) {
              keep_running = false;
            }
            break;
          }
          case Status::OperationInProgress: {
            break;
          }
          default: {
            std::cout << "TCP send error, stopping" << std::endl;
            keep_running_local = false;
            break;
          }
        }
        break;
      }
      case State::Closing: {
        if (client->close(id) == Status::NotConnected && client->isClosed(id)) {
          keep_running_local = false;
        }
        break;
      }
    }
  }
  /*
   * Delete the PCAP device.
   */
  if (options.dumpPackets()) {
    delete pcap_device;
  }
  return 0;
}

}

namespace Server {

class Delegate : public defaults::ServerDelegate
{
public:
  Delegate() : m_next(0), m_bytes(0) {}

  Action onNewData(UNUSED tulips::Server::ID const& id,
                   UNUSED void* const cookie, const uint8_t* const data,
                   const uint32_t len) override
  {
    size_t const& header = *reinterpret_cast<const size_t*>(data);
    if (header != m_next) {
      std::cout << "header error: next=" << m_next << " cur=" << header
                << std::endl;
    }
    m_next += 1;
    m_bytes += len;
    return Action::Continue;
  }

  Action onNewData(UNUSED tulips::Server::ID const& id,
                   UNUSED void* const cookie, const uint8_t* const data,
                   const uint32_t len, UNUSED const uint32_t alen,
                   UNUSED uint8_t* const sdata, UNUSED uint32_t& slen) override
  {
    size_t const& header = *reinterpret_cast<const size_t*>(data);
    if (header != m_next) {
      std::cout << "header error: next=" << m_next << " cur=" << header
                << std::endl;
    }
    m_next += 1;
    m_bytes += len;
    return Action::Continue;
  }

  double throughput(const uint64_t sec)
  {
    static uint64_t prev = 0;
    uint64_t delta = m_bytes - prev;
    prev = m_bytes;
    return (double)(delta << 3) / sec;
  }

private:
  size_t m_next;
  uint64_t m_bytes;
};

int
run(Options const& options, transport::Device& base_device)
{
  /*
   * Create an PCAP device
   */
  transport::pcap::Device* pcap_device = nullptr;
  Device* device = &base_device;
  /**
   * Signal handler
   */
  signal(SIGINT, signal_handler);
  signal(SIGALRM, alarm_handler);
  /**
   * Set the alarm
   */
  alarm_delay = options.interval();
  alarm(alarm_delay);
  /*
   * Run as receiver.
   */
  size_t iter = 0;
  Delegate delegate;
  /*
   * Check if we should wrap the device in a PCAP device.
   */
  if (options.dumpPackets()) {
    pcap_device = new transport::pcap::Device(base_device, "server.pcap");
    device = pcap_device;
  }
  /**
   * Initialize the server
   */
  interface::Server* server = nullptr;
  if (options.withSSL()) {
    server = new tulips::ssl::Server(delegate, *device, options.connections(),
                                     tulips::ssl::Protocol::TLSv1_2,
                                     options.sslCert(), options.sslKey());
  } else {
    server = new tulips::Server(delegate, *device, options.connections());
  }
  /*
   * Listen to the local ports.
   */
  for (auto p : options.ports()) {
    server->listen(p, nullptr);
  }
  /*
   * Set the CPU affinity.
   */
  if (options.cpuId() >= 0) {
    if (!system::setCurrentThreadAffinity(options.cpuId())) {
      throw std::runtime_error("Cannot set CPU ID");
    }
  }
  /*
   * Latency timer.
   */
  system::Timer timer;
  if (options.usDelay() != 0) {
    timer.set((CLOCK_SECOND * options.usDelay()) / 1000000ULL);
  }
  /*
   * Listen to incoming data.
   */
  while (keep_running) {
    /*
     * Process the artificial delay.
     */
    if (options.usDelay() != 0) {
      if (!timer.expired()) {
        continue;
      }
      timer.reset();
    }
    /*
     * Process the stack
     */
    if (options.wait()) {
      switch (device->wait(*server, WAIT_DELAY)) {
        case Status::Ok: {
          break;
        }
        case Status::NoDataAvailable: {
          server->run();
          break;
        }
        default: {
          std::cout << "Unknown error, aborting" << std::endl;
          keep_running = false;
          continue;
        }
      }
    } else {
      switch (device->poll(*server)) {
        case Status::Ok: {
          break;
        }
        case Status::NoDataAvailable: {
          if ((iter++ & 0x1FULL) == 0) {
            server->run();
          }
          break;
        }
        default: {
          std::cout << "Unknown error, aborting" << std::endl;
          keep_running = false;
          continue;
        }
      }
    }
    /*
     * Print latency if necessary.
     */
    if (show_latency) {
      double tps = delegate.throughput(alarm_delay);
      show_latency = false;
      if (tps > 0) {
        std::ostringstream oss;
        oss << std::setprecision(2) << std::fixed;
        oss << "throughput = ";
        if (tps > 1e9L) {
          oss << (tps / 1e9L) << " Gb/s";
        } else if (tps > 1e6L) {
          oss << (tps / 1e6L) << " Mb/s";
        } else if (tps > 1e3L) {
          oss << (tps / 1e3L) << " Kb/s";
        } else {
          oss << tps << " b/s";
        }
        std::cout << oss.str() << std::endl;
      }
    }
  }
  /*
   * Clean-up.
   */
  delete server;
  if (options.dumpPackets()) {
    delete pcap_device;
  }
  return 0;
}

}

}}}
