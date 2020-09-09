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

#include <tulips/stack/tcpv4/Processor.h>
#include <tulips/stack/ipv4/Producer.h>
#include <tulips/stack/ipv4/Processor.h>
#include <tulips/stack/ethernet/Producer.h>
#include <tulips/stack/ethernet/Processor.h>
#include <tulips/system/Compiler.h>
#include <tulips/transport/pcap/Device.h>
#include <tulips/transport/shm/Device.h>
#include <tulips/transport/Processor.h>
#include <gtest/gtest.h>
#include <fstream>

using namespace tulips;
using namespace stack;

namespace {

class Client : public tcpv4::EventHandler
{
public:
  Client(std::string const& fn) : m_out(), m_connected(false)
  {
    m_out.open(fn.c_str());
  }

  ~Client() override { m_out.close(); }

  void onConnected(UNUSED tcpv4::Connection& c) override
  {
    m_out << "onConnected:" << std::endl;
    m_connected = true;
  }

  void onAborted(UNUSED tcpv4::Connection& c) override
  {
    m_out << "onAborted:" << std::endl;
    m_connected = false;
  }

  void onTimedOut(UNUSED tcpv4::Connection& c) override
  {
    m_out << "onTimedOut:" << std::endl;
  }

  void onSent(UNUSED tcpv4::Connection& c) override
  {
    m_out << "onSent:" << std::endl;
  }

  Action onAcked(UNUSED tcpv4::Connection& c) override
  {
    m_out << "onAcked:" << std::endl;
    return Action::Continue;
  }

  Action onAcked(UNUSED tcpv4::Connection& c, UNUSED const uint32_t alen,
                 UNUSED uint8_t* const sdata, UNUSED uint32_t& slen) override
  {
    m_out << "onAcked:" << std::endl;
    return Action::Continue;
  }

  Action onNewData(UNUSED tcpv4::Connection& c,
                   UNUSED const uint8_t* const data,
                   const uint32_t len) override
  {
    m_out << "onNewData: " << len << "B" << std::endl;
    return Action::Continue;
  }

  Action onNewData(UNUSED tcpv4::Connection& c,
                   UNUSED const uint8_t* const data, UNUSED const uint32_t len,
                   UNUSED const uint32_t alen, UNUSED uint8_t* const sdata,
                   UNUSED uint32_t& slen) override
  {
    m_out << "onNewData:" << std::endl;
    return Action::Continue;
  }

  void onClosed(UNUSED tcpv4::Connection& c) override
  {
    m_out << "onClosed:" << std::endl;
    m_connected = false;
  }

  bool isConnected() const { return m_connected; }

private:
  std::ofstream m_out;
  bool m_connected;
};

class Server : public tcpv4::EventHandler
{
public:
  Server(std::string const& fn)
    : m_out(), m_connected(false), m_cid(-1), m_rlen(0)
  {
    m_out.open(fn.c_str());
  }

  ~Server() override { m_out.close(); }

  void onConnected(tcpv4::Connection& c) override
  {
    m_out << "onConnected:" << std::endl;
    m_connected = true;
    m_cid = c.id();
  }

  void onAborted(UNUSED tcpv4::Connection& c) override
  {
    m_out << "onAborted:" << std::endl;
    m_connected = false;
    m_cid = -1;
  }

  void onTimedOut(UNUSED tcpv4::Connection& c) override
  {
    m_out << "onTimedOut:" << std::endl;
  }

  void onSent(UNUSED tcpv4::Connection& c) override
  {
    m_out << "onSent:" << std::endl;
  }

  Action onAcked(UNUSED tcpv4::Connection& c) override
  {
    m_out << "onAcked:" << std::endl;
    return Action::Continue;
  }

  Action onAcked(UNUSED tcpv4::Connection& c, UNUSED const uint32_t alen,
                 UNUSED uint8_t* const sdata, UNUSED uint32_t& slen) override
  {
    m_out << "onAcked:" << std::endl;
    return Action::Continue;
  }

  Action onNewData(UNUSED tcpv4::Connection& c,
                   UNUSED const uint8_t* const data,
                   const uint32_t len) override
  {
    m_out << "onNewData:" << std::endl;
    m_rlen = len;
    return Action::Continue;
  }

  Action onNewData(UNUSED tcpv4::Connection& c,
                   UNUSED const uint8_t* const data, const uint32_t len,
                   UNUSED const uint32_t alen, UNUSED uint8_t* const sdata,
                   UNUSED uint32_t& slen) override
  {
    m_out << "onNewData:" << std::endl;
    m_rlen = len;
    return Action::Continue;
  }

  void onClosed(UNUSED tcpv4::Connection& c) override
  {
    m_out << "onClosed:" << std::endl;
    m_connected = false;
    m_cid = -1;
  }

  bool isConnected() const { return m_connected; }

  tcpv4::Connection::ID connectionID() const { return m_cid; }

  uint32_t receivedLength() const { return m_rlen; }

private:
  std::ofstream m_out;
  bool m_connected;
  tcpv4::Connection::ID m_cid;
  uint32_t m_rlen;
};

} // namespace

class TCP_Nagle : public ::testing::Test
{
public:
  TCP_Nagle()
    : m_client_fifo(nullptr)
    , m_server_fifo(nullptr)
    , m_client_adr(0x10, 0x0, 0x0, 0x0, 0x10, 0x10)
    , m_server_adr(0x10, 0x0, 0x0, 0x0, 0x20, 0x20)
    , m_bcast(10, 1, 0, 254)
    , m_nmask(255, 255, 255, 0)
    , m_client_ip4(10, 1, 0, 1)
    , m_server_ip4(10, 1, 0, 2)
    , m_client(nullptr)
    , m_server(nullptr)
    , m_client_pcap(nullptr)
    , m_server_pcap(nullptr)
    , m_client_evt(nullptr)
    , m_client_ip4_prod(nullptr)
    , m_client_ip4_proc(nullptr)
    , m_client_tcp(nullptr)
    , m_client_eth_prod(nullptr)
    , m_client_eth_proc(nullptr)
    , m_server_evt(nullptr)
    , m_server_ip4_prod(nullptr)
    , m_server_ip4_proc(nullptr)
    , m_server_tcp(nullptr)
    , m_server_eth_prod(nullptr)
    , m_server_eth_proc(nullptr)
  {}

protected:
  void SetUp() override
  {
    std::string tname(
      ::testing::UnitTest::GetInstance()->current_test_info()->name());
    /*
     * Create the transport FIFOs.
     */
    m_client_fifo = TULIPS_FIFO_DEFAULT_VALUE;
    m_server_fifo = TULIPS_FIFO_DEFAULT_VALUE;
    /*
     * Build the FIFOs
     */
    tulips_fifo_create(32, 128, &m_client_fifo);
    tulips_fifo_create(32, 128, &m_server_fifo);
    /*
     * Build the devices.
     */
    m_client = new transport::shm::Device(m_client_adr, m_client_ip4, m_bcast,
                                          m_nmask, m_server_fifo,
                                          m_client_fifo);
    m_server = new transport::shm::Device(m_server_adr, m_server_ip4, m_bcast,
                                          m_nmask, m_client_fifo,
                                          m_server_fifo);
    /*
     * Build the pcap device
     */
    std::string client_n = "tcp_nagle.client." + tname;
    std::string server_n = "tcp_nagle.server." + tname;
    m_client_pcap = new transport::pcap::Device(*m_client, client_n + ".pcap");
    m_server_pcap = new transport::pcap::Device(*m_server, server_n + ".pcap");
    /*
     * Client stack
     */
    m_client_evt = new Client(client_n + ".log");
    m_client_eth_prod =
      new ethernet::Producer(*m_client_pcap, m_client_pcap->address());
    m_client_ip4_prod = new ipv4::Producer(*m_client_eth_prod, m_client_ip4);
    m_client_eth_proc = new ethernet::Processor(m_client_pcap->address());
    m_client_ip4_proc = new ipv4::Processor(m_client_ip4);
    m_client_tcp = new tcpv4::Processor(*m_client_pcap, *m_client_eth_prod,
                                        *m_client_ip4_prod, *m_client_evt, 1);
    /*
     * Client processor binding
     */
    (*m_client_tcp)
      .setEthernetProcessor(*m_client_eth_proc)
      .setIPv4Processor(*m_client_ip4_proc);
    (*m_client_ip4_prod).setDefaultRouterAddress(m_bcast).setNetMask(m_nmask);
    (*m_client_ip4_proc)
      .setEthernetProcessor(*m_client_eth_proc)
      .setTCPv4Processor(*m_client_tcp);
    (*m_client_eth_proc).setIPv4Processor(*m_client_ip4_proc);
    /*
     * Server stack
     */
    m_server_evt = new Server(server_n + ".log");
    m_server_eth_prod =
      new ethernet::Producer(*m_server_pcap, m_server_pcap->address());
    m_server_ip4_prod = new ipv4::Producer(*m_server_eth_prod, m_server_ip4);
    m_server_eth_proc = new ethernet::Processor(m_server_pcap->address());
    m_server_ip4_proc = new ipv4::Processor(m_server_ip4);
    m_server_tcp = new tcpv4::Processor(*m_server_pcap, *m_server_eth_prod,
                                        *m_server_ip4_prod, *m_server_evt, 1);
    /*
     * Server processor binding
     */
    (*m_server_tcp)
      .setEthernetProcessor(*m_server_eth_proc)
      .setIPv4Processor(*m_server_ip4_proc);
    (*m_server_ip4_prod).setDefaultRouterAddress(m_bcast).setNetMask(m_nmask);
    (*m_server_ip4_proc)
      .setEthernetProcessor(*m_server_eth_proc)
      .setTCPv4Processor(*m_server_tcp);
    (*m_server_eth_proc).setIPv4Processor(*m_server_ip4_proc);
    /*
     * TCP server listens
     */
    m_server_tcp->listen(1234);
  }

  void TearDown() override
  {
    /*
     * Delete client stack.
     */
    delete m_client_evt;
    delete m_client_ip4_proc;
    delete m_client_ip4_prod;
    delete m_client_tcp;
    delete m_client_eth_proc;
    delete m_client_eth_prod;
    /*
     * Delete server stack.
     */
    delete m_server_evt;
    delete m_server_ip4_proc;
    delete m_server_ip4_prod;
    delete m_server_tcp;
    delete m_server_eth_proc;
    delete m_server_eth_prod;
    /*
     * Delete the pcap wrappers;
     */
    delete m_client_pcap;
    delete m_server_pcap;
    /*
     * Delete client and server.
     */
    delete m_client;
    delete m_server;
    /*
     * Delete the FIFOs.
     */
    tulips_fifo_destroy(&m_client_fifo);
    tulips_fifo_destroy(&m_server_fifo);
  }

  tulips_fifo_t m_client_fifo;
  tulips_fifo_t m_server_fifo;
  ethernet::Address m_client_adr;
  ethernet::Address m_server_adr;
  ipv4::Address m_bcast;
  ipv4::Address m_nmask;
  ipv4::Address m_client_ip4;
  ipv4::Address m_server_ip4;
  transport::shm::Device* m_client;
  transport::shm::Device* m_server;
  transport::pcap::Device* m_client_pcap;
  transport::pcap::Device* m_server_pcap;
  Client* m_client_evt;
  ipv4::Producer* m_client_ip4_prod;
  ipv4::Processor* m_client_ip4_proc;
  tcpv4::Processor* m_client_tcp;
  ethernet::Producer* m_client_eth_prod;
  ethernet::Processor* m_client_eth_proc;
  Server* m_server_evt;
  ipv4::Producer* m_server_ip4_prod;
  ipv4::Processor* m_server_ip4_proc;
  tcpv4::Processor* m_server_tcp;
  ethernet::Producer* m_server_eth_prod;
  ethernet::Processor* m_server_eth_proc;
};

TEST_F(TCP_Nagle, ConnectSendNagle)
{
  tcpv4::Connection::ID c;
  /*
   * Server listens, client connects
   */
  ASSERT_EQ(Status::Ok,
            m_client_tcp->connect(m_server_adr, m_server_ip4, 1234, c));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server_eth_proc));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client_eth_proc));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server_eth_proc));
  ASSERT_TRUE(m_client_evt->isConnected());
  ASSERT_TRUE(m_server_evt->isConnected());
  /*
   * The client sends some data, #1
   */
  uint32_t res = 0;
  uint64_t pld = 0xdeadbeefULL;
  ASSERT_EQ(Status::Ok, m_client_tcp->send(c, 8, (uint8_t*)&pld, res));
  ASSERT_EQ(8, res);
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server_eth_proc));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client_eth_proc));
  ASSERT_EQ(8, m_server_evt->receivedLength());
  /*
   * The client sends some data, #2
   */
  res = 0;
  ASSERT_EQ(Status::Ok, m_client_tcp->send(c, 8, (uint8_t*)&pld, res));
  ASSERT_EQ(8, res);
  res = 0;
  ASSERT_EQ(Status::Ok, m_client_tcp->send(c, 8, (uint8_t*)&pld, res));
  ASSERT_EQ(8, res);
  res = 0;
  ASSERT_EQ(Status::Ok, m_client_tcp->send(c, 8, (uint8_t*)&pld, res));
  ASSERT_EQ(8, res);
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server_eth_proc));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client_eth_proc));
  ASSERT_EQ(8, m_server_evt->receivedLength());
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server_eth_proc));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client_eth_proc));
  ASSERT_EQ(16, m_server_evt->receivedLength());
}

TEST_F(TCP_Nagle, ConnectSendConsecutiveNagle)
{
  tcpv4::Connection::ID c;
  const size_t PKTLEN = 35;
  /*
   * Server listens, client connects
   */
  ASSERT_EQ(Status::Ok,
            m_client_tcp->connect(m_server_adr, m_server_ip4, 1234, c));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server_eth_proc));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client_eth_proc));
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server_eth_proc));
  ASSERT_TRUE(m_client_evt->isConnected());
  ASSERT_TRUE(m_server_evt->isConnected());
  /*
   * The client sends some data, #1
   */
  uint32_t res = 0;
  uint8_t pld[PKTLEN] = { 0xA };
  ASSERT_EQ(Status::Ok, m_client_tcp->send(c, PKTLEN, pld, res));
  ASSERT_EQ(PKTLEN, res);
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server_eth_proc));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client_eth_proc));
  ASSERT_EQ(PKTLEN, m_server_evt->receivedLength());
  /*
   * The client sends some more data. This segment will be in-flight, activating
   * Nagle's algorigthm.
   */
  res = 0;
  ASSERT_EQ(Status::Ok, m_client_tcp->send(c, PKTLEN, (uint8_t*)&pld, res));
  ASSERT_EQ(PKTLEN, res);
  /*
   * Fill all segments.
   */
  for (int i = 0; i < 1 << SEGM_B; i += 1) {
    res = 0;
    ASSERT_EQ(Status::Ok, m_client_tcp->send(c, PKTLEN, (uint8_t*)&pld, res));
    ASSERT_EQ(PKTLEN, res);
    res = 0;
    ASSERT_EQ(Status::Ok, m_client_tcp->send(c, PKTLEN, (uint8_t*)&pld, res));
    ASSERT_EQ(PKTLEN, res);
  }
  /*
   * No more segment to allow sending data.
   */
  res = 0;
  ASSERT_EQ(m_client_tcp->send(c, PKTLEN, (uint8_t*)&pld, res),
            Status::OperationInProgress);
  /*
   * Receive the first payload.
   */
  ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server_eth_proc));
  ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client_eth_proc));
  ASSERT_EQ(PKTLEN, m_server_evt->receivedLength());
  /*
   * Receive the combined payloads.
   */
  for (int i = 0; i < 1 << SEGM_B; i += 1) {
    ASSERT_EQ(Status::Ok, m_server_pcap->poll(*m_server_eth_proc));
    ASSERT_EQ(Status::Ok, m_client_pcap->poll(*m_client_eth_proc));
    ASSERT_EQ(2 * PKTLEN, m_server_evt->receivedLength());
  }
}
