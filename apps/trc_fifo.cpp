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

#include <tulips/api/Client.h>
#include <tulips/api/Server.h>
#include <tulips/api/Defaults.h>
#include <tulips/system/Compiler.h>
#include <tulips/transport/shm/Device.h>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <tclap/CmdLine.h>

using namespace tulips;
using namespace stack;

/*
 * Client state
 */

static int interval = 0;
static size_t count = 0;
static size_t sends = 0;
static size_t retries = 0;
static size_t start = 0;
static size_t cumul = 0;

/*
 * Server state
 */

class ServerDelegate : public defaults::ServerDelegate
{
public:
  Action onNewData(UNUSED Server::ID const& id, UNUSED void* const cookie,
                   UNUSED const uint8_t* const data,
                   UNUSED const uint32_t len) override
  {
    cumul += system::Clock::read() - start;
    count += 1;
    return Action::Continue;
  }

  Action onNewData(UNUSED Server::ID const& id, UNUSED void* const cookie,
                   UNUSED const uint8_t* const data, UNUSED const uint32_t len,
                   UNUSED const uint32_t alen, UNUSED uint8_t* const sdata,
                   UNUSED uint32_t& slen) override
  {
    cumul += system::Clock::read() - start;
    count += 1;
    return Action::Continue;
  }
};

/*
 * Runner threads
 */

static bool keep_running = true;

void
signal_handler(UNUSED int signal)
{
  keep_running = false;
}

void
alarm_handler(UNUSED int signal)
{
  static size_t last = 0;
  static double cpns = CLOCK_SECOND / 1e9;
  size_t cur = count, delta = cur - last;
  last = cur;
  double hits = sends / (float)retries * 100.0;
  double avgns = (double)cumul / cpns / delta;
  cumul = 0;
  alarm(interval);
  printf("%ld half round-trips per seconds, hits = %.2f, avg = %.4lf\n",
         delta / 10, hits, avgns);
}

enum class ClientState
{
  Connect,
  Run
};

/*
 * Main function
 */

struct Options
{
  Options(TCLAP::CmdLine& cmd)
    : nag("N", "nodelay", "Disable Nagle's algorithm", cmd)
    , wai("w", "wait", "Wait instead of poll", cmd)
    , dly("i", "interval", "Statistics interval", false, 10, "INTERVAL", cmd)
  {}

  TCLAP::SwitchArg nag;
  TCLAP::SwitchArg wai;
  TCLAP::ValueArg<size_t> dly;
};

int
main(int argc, char** argv)
{
  TCLAP::CmdLine cmd("TULIPS Trace Tool", ' ', "1.0");
  Options opts(cmd);
  cmd.parse(argc, argv);
  /*
   * Signal handler
   */
  signal(SIGINT, signal_handler);
  signal(SIGALRM, alarm_handler);
  /*
   * Create the transport FIFOs
   */
  tulips_fifo_t client_fifo = TULIPS_FIFO_DEFAULT_VALUE;
  tulips_fifo_t server_fifo = TULIPS_FIFO_DEFAULT_VALUE;
  /*
   * Build the FIFOs
   */
  tulips_fifo_create(64, 128, &client_fifo);
  tulips_fifo_create(64, 128, &server_fifo);
  /*
   * Build the devices
   */
  ethernet::Address client_adr(0x10, 0x0, 0x0, 0x0, 0x10, 0x10);
  ethernet::Address server_adr(0x10, 0x0, 0x0, 0x0, 0x20, 0x20);
  ipv4::Address client_ip4(10, 1, 0, 1);
  ipv4::Address server_ip4(10, 1, 0, 2);
  ipv4::Address bcast(10, 1, 0, 254);
  ipv4::Address nmask(255, 255, 255, 0);
  transport::shm::Device client_dev(client_adr, client_ip4, bcast, nmask,
                                    server_fifo, client_fifo);
  transport::shm::Device server_dev(server_adr, server_ip4, bcast, nmask,
                                    client_fifo, server_fifo);
  /*
   * Initialize the client.
   */
  defaults::ClientDelegate client_delegate;
  Client client(client_delegate, client_dev, 1);
  /*
   * Open a connection.
   */
  Client::ID id;
  client.open(id);
  /*
   * Initialize the server
   */
  defaults::ServerDelegate server_delegate;
  Server server(server_delegate, server_dev, 1);
  server.listen(1234, nullptr);
  /*
   * Set the alarm
   */
  interval = opts.dly.getValue();
  alarm(interval);
  /*
   * Run loop
   */
  Status s;
  ClientState state = ClientState::Connect;
  while (keep_running) {
    /*
     * Process the client stack
     */
    s = opts.wai.isSet() ? client_dev.wait(client, 1000000)
                         : client_dev.poll(client);
    if (s == Status::NoDataAvailable) {
      client.run();
    }
    /*
     * Process the application
     */
    switch (state) {
      case ClientState::Connect: {
        if (client.connect(id, ipv4::Address(10, 1, 0, 2), 1234) ==
            Status::Ok) {
          state = ClientState::Run;
        }
        break;
      }
      case ClientState::Run: {
        uint32_t res = 0;
        size_t lcount = count + 1;
        start = system::Clock::read();
        if (client.send(id, sizeof(lcount), (uint8_t*)&lcount, res) ==
            Status::Ok) {
          sends += 1;
        }
        retries += 1;
        break;
      }
    }
    /*
     * Process the server stack
     */
    s = opts.wai.isSet() ? server_dev.wait(server, 1000000)
                         : server_dev.poll(server);
    if (s == Status::NoDataAvailable) {
      server.run();
    }
  }
  /*
   * Destroy the FIFOs
   */
  tulips_fifo_destroy(&client_fifo);
  tulips_fifo_destroy(&server_fifo);
  return 0;
}
