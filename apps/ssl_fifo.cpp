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
#include <tulips/ssl/Client.h>
#include <tulips/ssl/Server.h>
#include <tulips/system/Compiler.h>
#include <tulips/transport/pcap/Device.h>
#include <tulips/transport/shm/Device.h>
#include <iostream>
#include <tclap/CmdLine.h>

using namespace tulips;
using namespace stack;
using namespace transport;

enum class ClientState
{
  Connect,
  Run,
  Close
};

class ServerDelegate : public defaults::ServerDelegate
{
public:
  Action onNewData(UNUSED Server::ID const& id, UNUSED void* const cookie,
                   const uint8_t* const data, const uint32_t len) override
  {
    std::string res((const char*)data, len);
    std::cout << res << std::endl;
    return Action::Continue;
  }

  Action onNewData(UNUSED Server::ID const& id, UNUSED void* const cookie,
                   const uint8_t* const data, const uint32_t len,
                   UNUSED const uint32_t alen, UNUSED uint8_t* const sdata,
                   UNUSED uint32_t& slen) override
  {
    std::string res((const char*)data, len);
    std::cout << res << std::endl;
    return Action::Continue;
  }
};

struct Options
{
  Options(TCLAP::CmdLine& cmd)
    : crt("c", "certificate", "SSL certificate", true, "", "CERT", cmd)
    , key("k", "key", "SSL key", true, "", "KEY", cmd)
  {}

  TCLAP::ValueArg<std::string> crt;
  TCLAP::ValueArg<std::string> key;
};

int
main(int argc, char** argv)
{
  TCLAP::CmdLine cmd("TULIPS SSL Tool", ' ', "1.0");
  Options opts(cmd);
  cmd.parse(argc, argv);
  /*
   * Create the transport FIFOs
   */
  tulips_fifo_t cfifo = TULIPS_FIFO_DEFAULT_VALUE;
  tulips_fifo_t sfifo = TULIPS_FIFO_DEFAULT_VALUE;
  /*
   * Build the FIFOs
   */
  tulips_fifo_create(32, 1038, &cfifo);
  tulips_fifo_create(32, 1038, &sfifo);
  /*
   * Build the devices
   */
  ethernet::Address cadr(0x10, 0x0, 0x0, 0x0, 0x10, 0x10);
  ethernet::Address sadr(0x10, 0x0, 0x0, 0x0, 0x20, 0x20);
  ipv4::Address cip4(10, 1, 0, 1);
  ipv4::Address sip4(10, 1, 0, 2);
  ipv4::Address bcast(10, 1, 0, 254);
  ipv4::Address nmask(255, 255, 255, 0);
  shm::Device cshm(cadr, cip4, bcast, nmask, sfifo, cfifo);
  shm::Device sshm(sadr, sip4, bcast, nmask, cfifo, sfifo);
  /*
   * Create PCAP devices.
   */
  transport::pcap::Device cdev(cshm, "ssl_client.pcap");
  transport::pcap::Device sdev(sshm, "ssl_server.pcap");
  /*
   * Initialize the client.
   */
  defaults::ClientDelegate client_delegate;
  ssl::Client client(client_delegate, cdev, 1, ssl::Protocol::TLSv1_2,
                     opts.crt.getValue(), opts.key.getValue());
  /*
   * Open a connection.
   */
  Client::ID id;
  client.open(id);
  /*
   * Initialize the server
   */
  ServerDelegate server_delegate;
  ssl::Server server(server_delegate, sdev, 1, ssl::Protocol::TLSv1_2,
                     opts.crt.getValue(), opts.key.getValue());
  server.listen(1234, nullptr);
  /*
   * Run loop
   */
  size_t counter = 0;
  ClientState state = ClientState::Connect;
  bool keep_running = true;
  while (keep_running) {
    /*
     * Process the client stack
     */
    if (cdev.poll(client) == Status::NoDataAvailable) {
      client.run();
    }
    /*
     * Process the server stack
     */
    if (sdev.poll(server) == Status::NoDataAvailable) {
      server.run();
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
        uint32_t off = 0;
        const char* const data = "la vie est belle avec OpenSSL!!";
        client.send(id, strlen(data), (const uint8_t*)data, off);
        if (++counter == 1) {
          state = ClientState::Close;
        }
        break;
      }
      case ClientState::Close: {
        client.close(id);
        keep_running = !client.isClosed(id);
        break;
      }
    }
  }
  /*
   * Destroy the FIFOs
   */
  tulips_fifo_destroy(&cfifo);
  tulips_fifo_destroy(&sfifo);
  return 0;
}
