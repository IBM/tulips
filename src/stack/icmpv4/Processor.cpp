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

#include <tulips/stack/icmpv4/Processor.h>
#include <tulips/system/Compiler.h>
#include <arpa/inet.h>

#define INICMP ((const Header*)data)
#define OUTICMP ((Header*)outdata)

namespace tulips { namespace stack { namespace icmpv4 {

Processor::Processor(ethernet::Producer& eth, ipv4::Producer& ip4)
  : m_ethout(eth)
  , m_ip4out(ip4)
  , m_ethin(nullptr)
  , m_ip4in(nullptr)
  , m_arp(nullptr)
  , m_stats()
  , m_reqs()
  , m_ids(0)
{
  memset(&m_stats, 0, sizeof(m_stats));
}

Request&
Processor::attach(ethernet::Producer& eth, ipv4::Producer& ip4)
{
  Request::ID nid = m_ids + 1;
  auto* req = new Request(eth, ip4, *m_arp, nid);
  m_reqs[nid] = req;
  return *req;
}

void
Processor::detach(Request& req)
{
  m_reqs.erase(req.m_id);
  delete &req;
}

Status
Processor::process(const uint16_t UNUSED len, const uint8_t* const data)
{
  /**
   * Process the ICMP packet.
   */
  ++m_stats.recv;
  /**
   * Check if the message is valid.
   */
  if (INICMP->type != ECHO && INICMP->type != ECHO_REPLY) {
    return Status::ProtocolError;
  }
  /*
   * If it's a reply, mark the reply flag.
   */
  if (INICMP->type == ECHO_REPLY) {
    /*
     * Get request ID and check if the request is known.
     */
    Request::ID rid = INICMP->id;
    if (m_reqs.count(rid) == 0) {
      return Status::ProtocolError;
    }
    /*
     * Update the request state.
     */
    Request* req = m_reqs[rid];
    if (req->m_state != Request::REQUEST) {
      return Status::ProtocolError;
    }
    req->m_state = Request::RESPONSE;
    return Status::Ok;
  }
  /*
   * Update the IP and Ethernet attributes.
   */
  m_ip4out.setProtocol(ipv4::PROTO_ICMP);
  m_ip4out.setDestinationAddress(m_ip4in->sourceAddress());
  m_ethout.setDestinationAddress(m_ethin->sourceAddress());
  /*
   * Prepare a send buffer
   */
  uint8_t* outdata;
  Status ret = m_ip4out.prepare(outdata);
  if (ret != Status::Ok) {
    return ret;
  }
  /*
   * ICMP echo (i.e., ping) processing. This is simple, we only change the
   * ICMP type from ECHO to ECHO_REPLY and adjust the ICMP checksum before we
   * return the packet.
   */
  *OUTICMP = *INICMP;
  OUTICMP->type = ECHO_REPLY;
  if (INICMP->icmpchksum >= htons(0xffff - (ECHO << 8))) {
    OUTICMP->icmpchksum += htons(ECHO << 8) + 1;
  } else {
    OUTICMP->icmpchksum += htons(ECHO << 8);
  }
  /**
   * Send the packet
   */
  ++m_stats.sent;
  return m_ip4out.commit(HEADER_LEN, outdata);
}

}}}
