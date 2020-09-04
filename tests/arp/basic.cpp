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

#include <tulips/stack/ethernet/Producer.h>
#include <tulips/stack/ethernet/Processor.h>
#include <tulips/stack/arp/Processor.h>
#include <tulips/stack/ipv4/Producer.h>
#include <tulips/stack/ipv4/Processor.h>
#include <tulips/system/Compiler.h>
#include <tulips/transport/pcap/Device.h>
#include <tulips/transport/shm/Device.h>
#include <tulips/transport/Processor.h>
#include <gtest/gtest.h>

using namespace tulips;
using namespace stack;

namespace {

class ClientProcessor : public transport::Processor
{
public:
  ClientProcessor() : m_data(0) {}

  Status run() override { return Status::Ok; }

  Status process(const uint16_t UNUSED len, const uint8_t* const data) override
  {
    m_data = *(uint64_t*)data;
    return Status::Ok;
  }

  uint64_t data() const { return m_data; }

private:
  uint64_t m_data;
};

class ServerProcessor : public transport::Processor
{
public:
  ServerProcessor() : m_ipv4to(nullptr), m_ipv4from(nullptr), m_data(0) {}

  Status run() override { return Status::Ok; }

  Status process(const uint16_t UNUSED len, const uint8_t* const data) override
  {
    m_data = *(uint64_t*)data;
    m_ipv4to->setProtocol(ipv4::PROTO_TEST);
    m_ipv4to->setDestinationAddress(m_ipv4from->sourceAddress());
    uint8_t* outdata;
    m_ipv4to->prepare(outdata);
    *(uint64_t*)outdata = 0xdeadc0deULL;
    return m_ipv4to->commit(8, outdata);
  }

  ServerProcessor& setIPv4Producer(ipv4::Producer& ip4)
  {
    m_ipv4to = &ip4;
    return *this;
  }

  ServerProcessor& setIPv4Processor(ipv4::Processor& ip4)
  {
    m_ipv4from = &ip4;
    return *this;
  }

  uint64_t data() const { return m_data; }

private:
  ipv4::Producer* m_ipv4to;
  ipv4::Processor* m_ipv4from;
  uint64_t m_data;
};

} // namespace

TEST(ARP_Basic, RequestResponse)
{
  std::string tname(
    ::testing::UnitTest::GetInstance()->current_test_info()->name());
  /*
   * Create the transport FIFOs
   */
  tulips_fifo_t client_fifo = TULIPS_FIFO_DEFAULT_VALUE;
  tulips_fifo_t server_fifo = TULIPS_FIFO_DEFAULT_VALUE;
  /*
   * Build the FIFOs
   */
  tulips_fifo_create(64, 32, &client_fifo);
  tulips_fifo_create(64, 32, &server_fifo);
  /*
   * Build the devices
   */
  ethernet::Address client_adr(0x10, 0x0, 0x0, 0x0, 0x10, 0x10);
  ethernet::Address server_adr(0x10, 0x0, 0x0, 0x0, 0x20, 0x20);
  ipv4::Address client_ip4(10, 1, 0, 1);
  ipv4::Address server_ip4(10, 1, 0, 2);
  ipv4::Address bcast(10, 1, 0, 254);
  ipv4::Address nmask(255, 255, 255, 0);
  transport::shm::Device client(client_adr, client_ip4, bcast, nmask,
                                server_fifo, client_fifo);
  transport::shm::Device server(server_adr, server_ip4, bcast, nmask,
                                client_fifo, server_fifo);
  /*
   * Build the pcap device
   */
  transport::pcap::Device client_pcap(client, "arp_client_" + tname + ".pcap");
  transport::pcap::Device server_pcap(server, "arp_server_" + tname + ".pcap");
  /*
   * Client stack
   */
  ethernet::Producer client_eth_prod(client_pcap, client.address());
  ipv4::Producer client_ip4_prod(client_eth_prod, ipv4::Address(10, 1, 0, 1));
  ipv4::Processor client_ip4_proc(ipv4::Address(10, 1, 0, 1));
  ethernet::Processor client_eth_proc(client.address());
  arp::Processor client_arp(client_eth_prod, client_ip4_prod);
  ClientProcessor client_proc;
  /*
   * Bind the stack
   */
  client_ip4_prod.setDestinationAddress(ipv4::Address(10, 1, 0, 2))
    .setNetMask(ipv4::Address(255, 255, 255, 0));
  client_ip4_proc.setEthernetProcessor(client_eth_proc)
    .setRawProcessor(client_proc);
  client_eth_proc.setARPProcessor(client_arp).setIPv4Processor(client_ip4_proc);
  /*
   * Server stack
   */
  ethernet::Producer server_eth_prod(server_pcap, server.address());
  ipv4::Producer server_ip4_prod(server_eth_prod, ipv4::Address(10, 1, 0, 2));
  ethernet::Processor server_eth_proc(server.address());
  ipv4::Processor server_ip4_proc(ipv4::Address(10, 1, 0, 2));
  arp::Processor server_arp(server_eth_prod, server_ip4_prod);
  ServerProcessor server_proc;
  /*
   * Bind the stack
   */
  server_proc.setIPv4Producer(server_ip4_prod)
    .setIPv4Processor(server_ip4_proc);
  server_ip4_prod.setDestinationAddress(ipv4::Address(10, 1, 0, 2))
    .setNetMask(ipv4::Address(255, 255, 255, 0));
  server_ip4_proc.setEthernetProcessor(server_eth_proc)
    .setRawProcessor(server_proc);
  server_eth_proc.setARPProcessor(server_arp).setIPv4Processor(server_ip4_proc);
  /*
   * Client sends the ARP discovery
   */
  client_arp.discover(ipv4::Address(10, 1, 0, 2));
  ASSERT_EQ(Status::Ok, server_pcap.poll(server_eth_proc));
  ASSERT_EQ(Status::Ok, client_pcap.poll(client_eth_proc));
  /*
   * Client sends payload to server
   */
  uint8_t* data;
  ethernet::Address dest;
  ASSERT_TRUE(client_arp.query(ipv4::Address(10, 1, 0, 2), dest));
  client_eth_prod.setDestinationAddress(dest);
  client_ip4_prod.setProtocol(ipv4::PROTO_TEST);
  ASSERT_EQ(Status::Ok, client_ip4_prod.prepare(data));
  *(uint64_t*)data = 0xdeadbeefULL;
  ASSERT_EQ(Status::Ok, client_ip4_prod.commit(8, data));
  ASSERT_EQ(Status::Ok, server_pcap.poll(server_eth_proc));
  ASSERT_EQ(0xdeadbeefULL, server_proc.data());
  ASSERT_EQ(Status::Ok, client_pcap.poll(client_eth_proc));
  ASSERT_EQ(0xdeadc0de, client_proc.data());
  /*
   * Destroy the FIFOs
   */
  tulips_fifo_destroy(&client_fifo);
  tulips_fifo_destroy(&server_fifo);
}
