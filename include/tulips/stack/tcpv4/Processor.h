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

#pragma once

#include <tulips/system/Buffer.h>
#include <tulips/stack/tcpv4/Connection.h>
#include <tulips/stack/tcpv4/EventHandler.h>
#include <tulips/stack/TCPv4.h>
#include <tulips/stack/ethernet/Producer.h>
#include <tulips/stack/ethernet/Processor.h>
#include <tulips/stack/ipv4/Producer.h>
#include <tulips/stack/ipv4/Processor.h>
#include <tulips/system/Timer.h>
#include <tulips/transport/Device.h>
#include <tulips/transport/Processor.h>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <vector>

#define OUTTCP ((Header*)outdata)

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20
#define TCP_ECE 0x40
#define TCP_CWR 0x80
#define TCP_CTL 0x3f

namespace tulips { namespace stack { namespace tcpv4 {

/*
 * Protocol constants.
 */
static constexpr int USED MAXRTX = 5;
static constexpr int USED MAXSYNRTX = 5;
static constexpr int USED TIME_WAIT_TIMEOUT = 120;

/*
 * The TCPv4 statistics.
 */
struct Statistics
{
  uint64_t drop;    // Number of dropped TCP segments.
  uint64_t recv;    // Number of recived TCP segments.
  uint64_t sent;    // Number of sent TCP segments.
  uint64_t chkerr;  // Number of TCP segments with a bad checksum.
  uint64_t ackerr;  // Number of TCP segments with a bad ACK number.
  uint64_t rst;     // Number of recevied TCP RST (reset) segments.
  uint64_t rexmit;  // Number of retransmitted TCP segments.
  uint64_t syndrop; // Number of dropped SYNs (no connection was avaliable).
  uint64_t synrst;  // Number of SYNs for closed ports, triggering a RST.
};

/*
 * The TCPv4 processor.
 */
class Processor : public transport::Processor
{
public:
  Processor(transport::Device& device, ethernet::Producer& eth,
            ipv4::Producer& ip4, EventHandler& h, const size_t nconn);

  Status run() override;
  Status process(const uint16_t len, const uint8_t* const data) override;

  Processor& setEthernetProcessor(ethernet::Processor& eth)
  {
    m_ethfrom = &eth;
    return *this;
  }

  Processor& setIPv4Processor(ipv4::Processor& ip4)
  {
    m_ipv4from = &ip4;
    return *this;
  }

  /*
   * Server-side operations
   */

  void listen(const Port port);
  void unlisten(const Port port);

  /*
   * Client-side operations
   */

  Status connect(ethernet::Address const& rhwaddr, ipv4::Address const& ripaddr,
                 const Port rport, Connection::ID& id);

  Status abort(Connection::ID const& id);
  Status close(Connection::ID const& id);

  bool isClosed(Connection::ID const& id) const;

  /*
   * In the non-error cases, the send methods may return:
   * - OK : the payload has been written and/or pending data has been sent.
   * - OperationInProgress : no operation could have been performed.
   */

  Status send(Connection::ID const& id, const uint32_t len,
              const uint8_t* const data, uint32_t& off);

  Status get(Connection::ID const& id, ipv4::Address& ripaddr, Port& lport,
             Port& rport);

  void* cookie(Connection::ID const& id) const;

  /*
   * Some connection related methods, mostly for testing.
   */

  inline Status hasOutstandingSegments(Connection::ID const& id, bool& res)
  {
    if (id >= m_nconn) {
      return Status::InvalidConnection;
    }
    Connection& c = m_conns[id];
    res = c.hasOutstandingSegments();
    return Status::Ok;
  }

private:
  using Ports = std::set<Port>;
  using Connections = std::vector<Connection>;

#if !(defined(TULIPS_HAS_HW_CHECKSUM) && defined(TULIPS_DISABLE_CHECKSUM_CHECK))
  static uint16_t checksum(ipv4::Address const& src, ipv4::Address const& dst,
                           const uint16_t len, const uint8_t* const data);
#endif

  Status process(Connection& e, const uint16_t len, const uint8_t* const data);
  Status reset(const uint16_t len, const uint8_t* const data);

  Status sendNagle(Connection& e, const uint32_t bound);
  Status sendNoDelay(Connection& e, const uint8_t flag = 0);

  Status sendAbort(Connection& e);
  Status sendClose(Connection& e);
  Status sendSyn(Connection& e, Segment& s);
  Status sendAck(Connection& e);

  inline Status sendSynAck(Connection& e, Segment& s)
  {
    uint8_t* outdata = s.m_dat;
    OUTTCP->flags = TCP_ACK;
    return sendSyn(e, s);
  }

  inline Status sendFin(Connection& e, Segment& s)
  {
    uint8_t* outdata = s.m_dat;
    OUTTCP->flags |= TCP_FIN;
    OUTTCP->offset = 5;
    return send(e, HEADER_LEN, s);
  }

  inline Status sendFinAck(Connection& e, Segment& s)
  {
    uint8_t* outdata = s.m_dat;
    OUTTCP->flags = TCP_ACK;
    return sendFin(e, s);
  }

  Status send(Connection& e);

  inline Status send(Connection& e, Segment& s, const uint8_t flags = 0)
  {
    uint8_t* outdata = s.m_dat;
    /*
     * Send PSH/ACK message. TCP does not require to send an ACK with PSH,
     * but Linux seems pretty bent on wanting one. So we play nice. Again.
     */
    OUTTCP->flags = flags | TCP_ACK;
    OUTTCP->offset = 5;
    return send(e, s.m_len + HEADER_LEN, s);
  }

  Status send(Connection& e, const uint32_t len, Segment& s);

  Status send(ipv4::Address const& dst, const uint32_t len, const uint16_t mss,
              uint8_t* const outdata);

  Status rexmit(Connection& e);

  transport::Device& m_device;
  ethernet::Producer& m_ethto;
  ipv4::Producer& m_ipv4to;
  EventHandler& m_handler;
  const size_t m_nconn;
  ethernet::Processor* m_ethfrom;
  ipv4::Processor* m_ipv4from;
  uint32_t m_iss;
  uint32_t m_mss;
  Ports m_listenports;
  Connections m_conns;
  Statistics m_stats;
  system::Timer m_timer;
};

}}}
