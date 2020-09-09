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
#include <tulips/fifo/fifo.h>
#include <tulips/system/Compiler.h>
#include <tulips/transport/pcap/Device.h>
#include <tulips/transport/list/Device.h>
#include <tulips/transport/Processor.h>
#include <gtest/gtest.h>

using namespace tulips;
using namespace stack;

namespace {

class ServerDelegate : public defaults::ServerDelegate
{
public:
  ServerDelegate() : m_action(Action::Continue) {}

  tulips::Action onNewData(UNUSED Client::ID const& id,
                           UNUSED void* const cookie, const uint8_t* const data,
                           UNUSED const uint32_t len,
                           UNUSED const uint32_t alen,
                           UNUSED uint8_t* const sdata,
                           UNUSED uint32_t& slen) override
  {
    uint64_t& value = *(uint64_t*)data;
    return value != 0xdeadbeef ? Action::Abort : m_action;
  }

  void abortOnReceive() { m_action = Action::Abort; }

  void closeOnReceive() { m_action = Action::Close; }

private:
  Action m_action;
};

}

class SSL_OneClient : public ::testing::Test
{
public:
  SSL_OneClient()
    : m_client_adr(0x10, 0x0, 0x0, 0x0, 0x10, 0x10)
    , m_server_adr(0x10, 0x0, 0x0, 0x0, 0x20, 0x20)
    , m_client_ip4(10, 1, 0, 1)
    , m_server_ip4(10, 1, 0, 2)
    , m_client_list()
    , m_server_list()
    , m_client_ldev(nullptr)
    , m_server_ldev(nullptr)
    , m_client_pcap(nullptr)
    , m_server_pcap(nullptr)
    , m_client_delegate()
    , m_client(nullptr)
    , m_server_delegate()
    , m_server(nullptr)
  {}

  void connectClient(ipv4::Address const& dst_ip, const uint16_t port,
                     Client::ID& id)
  {
    /*
     * Client opens a connection.
     */
    ASSERT_EQ(Status::Ok, m_client->open(id));
    /*
     * Client tries to connect, go through ARP.
     */
    ASSERT_EQ(Status::OperationInProgress, m_client->connect(id, dst_ip, port));
    ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
    ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
    /*
     * Client tries to connect, establish a connection.
     */
    ASSERT_EQ(Status::OperationInProgress, m_client->connect(id, dst_ip, port));
    ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
    ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
    ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
    /*
     * Client tries to connect, go through SSL handshake.
     */
    ASSERT_EQ(Status::OperationInProgress, m_client->connect(id, dst_ip, port));
    ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
    ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
    ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
    ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
    ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
    ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
    ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
    ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
    /*
     * Client is connected, no more data exchanged.
     */
    ASSERT_EQ(Status::Ok, m_client->connect(id, dst_ip, port));
    ASSERT_EQ(Status::NoDataAvailable, m_client_pcap->poll(*m_client));
    ASSERT_EQ(Status::NoDataAvailable, m_server_pcap->poll(*m_server));
  }

  void expireTimeWait()
  {
    for (int i = 0; i <= tcpv4::TIME_WAIT_TIMEOUT; i += 1) {
      system::Clock::get().offsetBy(CLOCK_SECOND);
      ASSERT_EQ(Status::Ok, m_client->run());
      ASSERT_EQ(Status::Ok, m_server->run());
    }
  }

protected:
  void SetUp() override
  {
    ipv4::Address bcast(10, 1, 0, 254);
    ipv4::Address nmask(255, 255, 255, 0);
    std::string tname(
      ::testing::UnitTest::GetInstance()->current_test_info()->name());
    /*
     * Build the devices.
     */
    m_client_ldev = new transport::list::Device(m_client_adr, m_client_ip4,
                                                bcast, nmask, 9014,
                                                m_server_list, m_client_list);
    m_server_ldev = new transport::list::Device(m_server_adr, m_server_ip4,
                                                bcast, nmask, 9014,
                                                m_client_list, m_server_list);
    /*
     * Build the pcap device
     */
    std::string pcap_client = "api_secure.client." + tname + ".pcap";
    std::string pcap_server = "api_secure.server." + tname + ".pcap";
    m_client_pcap = new transport::pcap::Device(*m_client_ldev, pcap_client);
    m_server_pcap = new transport::pcap::Device(*m_server_ldev, pcap_server);
    /*
     * Define the source root and the security files.
     */
    std::string sourceRoot(TULIPS_SOURCE_ROOT);
    std::string certFile(sourceRoot + "/support/transport.cert");
    std::string keyFile(sourceRoot + "/support/transport.key");
    /*
     * Create the client.
     */
    m_client = new ssl::Client(m_client_delegate, *m_client_pcap, 1,
                               tulips::ssl::Protocol::TLSv1_2, certFile,
                               keyFile);
    /*
     * Create the server.
     */
    m_server = new ssl::Server(m_server_delegate, *m_server_pcap, 1,
                               tulips::ssl::Protocol::TLSv1_2, certFile,
                               keyFile);
  }

  void TearDown() override
  {
    /*
     * Delete client, server.
     */
    delete m_server;
    delete m_client;
    /*
     * Delete the pcap wrappers;
     */
    delete m_client_pcap;
    delete m_server_pcap;
    /*
     * Delete client and server.
     */
    delete m_client_ldev;
    delete m_server_ldev;
  }

  ethernet::Address m_client_adr;
  ethernet::Address m_server_adr;
  ipv4::Address m_client_ip4;
  ipv4::Address m_server_ip4;
  transport::list::Device::List m_client_list;
  transport::list::Device::List m_server_list;
  transport::list::Device* m_client_ldev;
  transport::list::Device* m_server_ldev;
  transport::pcap::Device* m_client_pcap;
  transport::pcap::Device* m_server_pcap;
  defaults::ClientDelegate m_client_delegate;
  ssl::Client* m_client;
  ServerDelegate m_server_delegate;
  ssl::Server* m_server;
};

TEST_F(SSL_OneClient, ListenConnectAndAbort)
{
  Client::ID id = Client::DEFAULT_ID;
  ipv4::Address dst_ip(10, 1, 0, 2);
  /*
   * Server listens
   */
  m_server->listen(12345, nullptr);
  /*
   * Client connects.
   */
  connectClient(dst_ip, 12345, id);
  /*
   * Client aborts the connection.
   */
  ASSERT_EQ(Status::Ok, m_client->abort(id));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  /*
   * Client is closed.
   */
  ASSERT_TRUE(m_client->isClosed(id));
  ASSERT_EQ(Status::NoDataAvailable, m_client_pcap->poll(*m_client));
  ASSERT_EQ(Status::NoDataAvailable, m_server_pcap->poll(*m_server));
}

TEST_F(SSL_OneClient, ListenConnectAndClose)
{
  Client::ID id = Client::DEFAULT_ID;
  ipv4::Address dst_ip(10, 1, 0, 2);
  /*
   * Server listens
   */
  m_server->listen(12345, nullptr);
  /*
   * Client connects.
   */
  connectClient(dst_ip, 12345, id);
  /*
   * Client closes the connection, go through SSL shutdown.
   */
  ASSERT_EQ(Status::OperationInProgress, m_client->close(id));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  /*
   * Advance the timers because of TIME WAIT.
   */
  expireTimeWait();
  /*
   * Client is closed.
   */
  ASSERT_TRUE(m_client->isClosed(id));
  ASSERT_TRUE(m_server->isClosed(0));
  ASSERT_EQ(Status::NoDataAvailable, m_client_pcap->poll(*m_client));
  ASSERT_EQ(Status::NoDataAvailable, m_server_pcap->poll(*m_server));
}

TEST_F(SSL_OneClient, ListenConnectAndCloseFromServer)
{
  Client::ID id = Client::DEFAULT_ID;
  ipv4::Address dst_ip(10, 1, 0, 2);
  /*
   * Server listens
   */
  m_server->listen(12345, nullptr);
  /*
   * Client connects.
   */
  connectClient(dst_ip, 12345, id);
  /*
   * Server closes the connection, go through SSL shutdown.
   */
  ASSERT_EQ(Status::OperationInProgress, m_server->close(0));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
  /*
   * Advance the timers because of TIME WAIT.
   */
  expireTimeWait();
  /*
   * Client is closed.
   */
  ASSERT_TRUE(m_client->isClosed(id));
  ASSERT_TRUE(m_server->isClosed(0));
  ASSERT_EQ(Status::NoDataAvailable, m_client_pcap->poll(*m_client));
  ASSERT_EQ(Status::NoDataAvailable, m_server_pcap->poll(*m_server));
}

TEST_F(SSL_OneClient, ListenConnectSendAndAbortFromServer)
{
  Client::ID id = Client::DEFAULT_ID;
  ipv4::Address dst_ip(10, 1, 0, 2);
  /*
   * Server listens
   */
  m_server_delegate.abortOnReceive();
  m_server->listen(12345, nullptr);
  /*
   * Client connects.
   */
  connectClient(dst_ip, 12345, id);
  /*
   * Client sends a piece of data, and the server aborts the connection.
   */
  uint32_t rem = 0;
  uint64_t data = 0xdeadbeef;
  ASSERT_EQ(Status::Ok,
            m_client->send(id, sizeof(data), (const uint8_t*)&data, rem));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
  /*
   * Client is closed.
   */
  ASSERT_TRUE(m_client->isClosed(id));
  ASSERT_EQ(Status::NoDataAvailable, m_client_pcap->poll(*m_client));
  ASSERT_EQ(Status::NoDataAvailable, m_server_pcap->poll(*m_server));
}

TEST_F(SSL_OneClient, ListenConnectSendAndCloseFromServer)
{
  Client::ID id = Client::DEFAULT_ID;
  ipv4::Address dst_ip(10, 1, 0, 2);
  /*
   * Server listens
   */
  m_server_delegate.closeOnReceive();
  m_server->listen(12345, nullptr);
  /*
   * Client connects.
   */
  connectClient(dst_ip, 12345, id);
  /*
   * Client sends a piece of data, and the server closes the connection.
   */
  uint32_t rem = 0;
  uint64_t data = 0xdeadbeef;
  ASSERT_EQ(Status::Ok,
            m_client->send(id, sizeof(data), (const uint8_t*)&data, rem));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client));
  /*
   * Client is closed.
   */
  ASSERT_TRUE(m_client->isClosed(id));
  ASSERT_EQ(Status::NoDataAvailable, m_client_pcap->poll(*m_client));
  ASSERT_EQ(Status::NoDataAvailable, m_server_pcap->poll(*m_server));
}
