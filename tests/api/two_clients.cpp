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
#include <tulips/system/Compiler.h>
#include <tulips/transport/pcap/Device.h>
#include <tulips/transport/list/Device.h>
#include <tulips/transport/Processor.h>
#include <gtest/gtest.h>

using namespace tulips;
using namespace stack;

namespace {

class ClientDelegate : public defaults::ClientDelegate
{
public:
  ClientDelegate() : m_data_received(false) {}

  tulips::Action onNewData(UNUSED Client::ID const& id,
                           UNUSED void* const cookie,
                           UNUSED const uint8_t* const data,
                           UNUSED const uint32_t len) override
  {
    m_data_received = true;
    return tulips::Action::Continue;
  }

  tulips::Action onNewData(UNUSED Client::ID const& id,
                           UNUSED void* const cookie,
                           UNUSED const uint8_t* const data,
                           UNUSED const uint32_t len,
                           UNUSED const uint32_t alen,
                           UNUSED uint8_t* const sdata,
                           UNUSED uint32_t& slen) override
  {
    m_data_received = true;
    return tulips::Action::Continue;
  }

  bool dataReceived() const { return m_data_received; }

private:
  bool m_data_received;
};

class ServerDelegate : public defaults::ServerDelegate
{
public:
  using Connections = std::list<tulips::Server::ID>;

  ServerDelegate() : m_connections(), m_send_back(false) {}

  void* onConnected(Server::ID const& id, UNUSED void* const cookie,
                    UNUSED uint8_t& opts) override
  {
    m_connections.push_back(id);
    return nullptr;
  }

  Action onNewData(UNUSED Server::ID const& id, UNUSED void* const cookie,
                   const uint8_t* const data, const uint32_t len,
                   const uint32_t alen, uint8_t* const sdata,
                   uint32_t& slen) override
  {
    if (m_send_back && alen >= len) {
      memcpy(sdata, data, len);
      slen = len;
    }
    return Action::Continue;
  }

  void onClosed(tulips::Server::ID const& id,
                UNUSED void* const cookie) override
  {
    m_connections.remove(id);
  }

  Connections const& connections() const { return m_connections; }

  void doSendBack(const bool v) { m_send_back = v; }

private:
  Connections m_connections;
  bool m_send_back;
};

}

class API_TwoClients : public ::testing::Test
{
public:
  API_TwoClients()
    : m_client_list()
    , m_server_list()
    , m_client_adr(0x10, 0x0, 0x0, 0x0, 0x10, 0x10)
    , m_server_adr(0x10, 0x0, 0x0, 0x0, 0x20, 0x20)
    , m_client_ip4(10, 1, 0, 1)
    , m_server_ip4(10, 1, 0, 2)
    , m_client_ldev(nullptr)
    , m_server_ldev(nullptr)
    , m_client_pcap(nullptr)
    , m_server_pcap(nullptr)
    , m_client_delegate1()
    , m_client_delegate2()
    , m_client1(nullptr)
    , m_client2(nullptr)
    , m_server_delegate()
    , m_server(nullptr)
  {}

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
                                                bcast, nmask, 1514,
                                                m_server_list, m_client_list);
    m_server_ldev = new transport::list::Device(m_server_adr, m_server_ip4,
                                                bcast, nmask, 1514,
                                                m_client_list, m_server_list);
    /*
     * Build the pcap device
     */
    std::string pcap_client = "api_2clients.client." + tname + ".pcap";
    std::string pcap_server = "api_2clients.server." + tname + ".pcap";
    m_client_pcap = new transport::pcap::Device(*m_client_ldev, pcap_client);
    m_server_pcap = new transport::pcap::Device(*m_server_ldev, pcap_server);
    /*
     * Create the clients.
     */
    m_client1 = new Client(m_client_delegate1, *m_client_pcap, 1);
    m_client2 = new Client(m_client_delegate2, *m_client_pcap, 1);
    /*
     * Create the server.
     */
    m_server = new Server(m_server_delegate, *m_server_pcap, 2);
    /*
     * Server listens.
     */
    m_server->listen(12345, nullptr);
  }

  void TearDown() override
  {
    /*
     * Delete client, server.
     */
    delete m_server;
    delete m_client2;
    delete m_client1;
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

  transport::list::Device::List m_client_list;
  transport::list::Device::List m_server_list;
  ethernet::Address m_client_adr;
  ethernet::Address m_server_adr;
  ipv4::Address m_client_ip4;
  ipv4::Address m_server_ip4;
  transport::list::Device* m_client_ldev;
  transport::list::Device* m_server_ldev;
  transport::pcap::Device* m_client_pcap;
  transport::pcap::Device* m_server_pcap;
  ClientDelegate m_client_delegate1;
  ClientDelegate m_client_delegate2;
  Client* m_client1;
  Client* m_client2;
  ServerDelegate m_server_delegate;
  Server* m_server;
};

TEST_F(API_TwoClients, ConnectTwo)
{
  Client::ID id1 = Client::DEFAULT_ID, id2 = Client::DEFAULT_ID;
  ipv4::Address dst_ip(10, 1, 0, 2);
  /*
   * Connection client 1.
   */
  ASSERT_EQ(Status::Ok, m_client1->open(id1));
  ASSERT_EQ(Status::OperationInProgress,
            m_client1->connect(id1, dst_ip, 12345));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::OperationInProgress,
            m_client1->connect(id1, dst_ip, 12345));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client1->connect(id1, dst_ip, 12345));
  ASSERT_EQ(Status::NoDataAvailable, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::NoDataAvailable, m_server_pcap->poll(*m_server));

  /*
   * Connection client 2.
   */
  ASSERT_EQ(Status::Ok, m_client2->open(id2));
  ASSERT_EQ(Status::OperationInProgress,
            m_client2->connect(id2, dst_ip, 12345));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client2));
  ASSERT_EQ(Status::OperationInProgress,
            m_client2->connect(id2, dst_ip, 12345));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client2));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client2->connect(id2, dst_ip, 12345));
  ASSERT_EQ(Status::NoDataAvailable, m_client_pcap->poll(*m_client2));
  ASSERT_EQ(Status::NoDataAvailable, m_server_pcap->poll(*m_server));
}

TEST_F(API_TwoClients, ConnectTwoAndDisconnectFromServer)
{
  Client::ID id1 = Client::DEFAULT_ID, id2 = Client::DEFAULT_ID;
  ipv4::Address dst_ip(10, 1, 0, 2);
  /*
   * Connection client 1.
   */
  ASSERT_EQ(Status::Ok, m_client1->open(id1));
  ASSERT_EQ(Status::OperationInProgress,
            m_client1->connect(id1, dst_ip, 12345));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::OperationInProgress,
            m_client1->connect(id1, dst_ip, 12345));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client1->connect(id1, dst_ip, 12345));
  ASSERT_EQ(Status::NoDataAvailable, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::NoDataAvailable, m_server_pcap->poll(*m_server));
  /*
   * Connection client 2.
   */
  ASSERT_EQ(Status::Ok, m_client2->open(id2));
  ASSERT_EQ(Status::OperationInProgress,
            m_client2->connect(id2, dst_ip, 12345));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client2));
  ASSERT_EQ(Status::OperationInProgress,
            m_client2->connect(id2, dst_ip, 12345));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client2));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client2->connect(id2, dst_ip, 12345));
  ASSERT_EQ(Status::NoDataAvailable, m_client_pcap->poll(*m_client2));
  ASSERT_EQ(Status::NoDataAvailable, m_server_pcap->poll(*m_server));
  /*
   * Disconnect the first connection.
   */
  ASSERT_EQ(2, m_server_delegate.connections().size());
  tulips::Server::ID c0 = m_server_delegate.connections().front();
  ASSERT_EQ(Status::Ok, m_server->close(c0));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::NoDataAvailable, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::NoDataAvailable, m_client_pcap->poll(*m_client1));
  /*
   * Disconnect the second connection.
   */
  ASSERT_EQ(1, m_server_delegate.connections().size());
  tulips::Server::ID c1 = m_server_delegate.connections().front();
  ASSERT_EQ(Status::Ok, m_server->close(c1));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client2));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client2));
  ASSERT_EQ(Status::NoDataAvailable, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::NoDataAvailable, m_client_pcap->poll(*m_client2));
  /*
   * Final checks.
   */
  ASSERT_EQ(0, m_server_delegate.connections().size());
}

TEST_F(API_TwoClients, ConnectSend)
{
  Client::ID id = Client::DEFAULT_ID;
  ipv4::Address dst_ip(10, 1, 0, 2);
  /*
   * Connect client.
   */
  ASSERT_EQ(Status::Ok, m_client1->open(id));
  ASSERT_EQ(Status::OperationInProgress,
            m_client1->connect(id, dst_ip, 12345)); /* ARP REQ */
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::OperationInProgress,
            m_client1->connect(id, dst_ip, 12345)); /* SYN REQ */
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client1->connect(id, dst_ip, 12345));
  ASSERT_EQ(Status::NoDataAvailable, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::NoDataAvailable, m_server_pcap->poll(*m_server));
  /*
   * Client sends.
   */
  uint32_t rem = 0;
  uint64_t data = 0xdeadbeef;
  ASSERT_EQ(Status::Ok,
            m_client1->send(id, sizeof(data), (uint8_t*)&data, rem));
  ASSERT_EQ(sizeof(data), rem);
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::NoDataAvailable, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::NoDataAvailable, m_client_pcap->poll(*m_client1));
}

TEST_F(API_TwoClients, ConnectSendReceive)
{
  Client::ID id = Client::DEFAULT_ID;
  ipv4::Address dst_ip(10, 1, 0, 2);
  /*
   * Ask the server to send the data back.
   */
  m_server_delegate.doSendBack(true);
  /*
   * Connect client.
   */
  ASSERT_EQ(Status::Ok, m_client1->open(id));
  ASSERT_EQ(Status::OperationInProgress, m_client1->connect(id, dst_ip, 12345));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::OperationInProgress, m_client1->connect(id, dst_ip, 12345));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client1->connect(id, dst_ip, 12345));
  ASSERT_EQ(Status::NoDataAvailable, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::NoDataAvailable, m_server_pcap->poll(*m_server));
  /*
   * Client sends, server sends back and client receives.
   */
  uint32_t rem = 0;
  uint64_t data = 0xdeadbeef;
  ASSERT_EQ(Status::Ok,
            m_client1->send(id, sizeof(data), (uint8_t*)&data, rem));
  ASSERT_EQ(sizeof(data), rem);
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server));
  ASSERT_EQ(Status::NoDataAvailable, m_client_pcap->poll(*m_client1));
  ASSERT_EQ(Status::NoDataAvailable, m_server_pcap->poll(*m_server));
  ASSERT_TRUE(m_client_delegate1.dataReceived());
}
