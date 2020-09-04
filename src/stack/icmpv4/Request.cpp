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

#include <tulips/stack/icmpv4/Request.h>

#define OUTICMP ((Header*)outdata)

namespace tulips { namespace stack { namespace icmpv4 {

Request::Request(ethernet::Producer& eth, ipv4::Producer& ip4,
                 arp::Processor& arp, const ID id)
  : m_eth(eth), m_ip4(ip4), m_arp(arp), m_id(id), m_state(IDLE), m_seq(1)
{}

Status
Request::operator()(ipv4::Address const& dst)
{
  ethernet::Address deth;
  /*
   * Check our current status.
   */
  if (m_state == REQUEST) {
    return Status::OperationInProgress;
  }
  if (m_state == RESPONSE) {
    m_state = IDLE;
    m_seq += 1;
    return Status::OperationCompleted;
  }
  /*
   * Get a HW translation of the destination address
   */
  if (!m_arp.query(dst, deth)) {
    return Status::HardwareTranslationMissing;
  }
  /*
   * Setup the IP protocol
   */
  m_ip4.setDestinationAddress(dst);
  m_ip4.setProtocol(ipv4::PROTO_ICMP);
  /*
   * Grab a send buffer
   */
  uint8_t* data;
  Status ret = m_ip4.prepare(data);
  if (ret != Status::Ok) {
    return ret;
  }
  /*
   * Setup ICMP message
   */
  auto* hdr = reinterpret_cast<Header*>(data);
  hdr->type = stack::icmpv4::ECHO;
  hdr->icode = 0;
  hdr->id = m_id;
  hdr->seqno = m_seq;
  hdr->icmpchksum = 0;
  hdr->icmpchksum = ~stack::icmpv4::checksum(data);
  /*
   * Commit the buffer.
   */
  m_state = REQUEST;
  return m_ip4.commit(HEADER_LEN, data);
}

}}}
