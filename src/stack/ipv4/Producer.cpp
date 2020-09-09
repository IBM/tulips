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

#include <tulips/stack/ipv4/Producer.h>
#include <cstring>
#include <arpa/inet.h>

#define OUTIP ((Header*)outdata)
#define TTL 64

namespace tulips { namespace stack { namespace ipv4 {

Producer::Producer(ethernet::Producer& prod, Address const& ha)
  : m_eth(prod)
  , m_hostAddress(ha)
  , m_destAddress()
  , m_defaultRouterAddress()
  , m_netMask(Address::BROADCAST)
  , m_proto(0)
  , m_ipid(0)
  , m_stats()
{
  memset(&m_stats, 0, sizeof(m_stats));
}

Status
Producer::prepare(uint8_t*& buf)
{
  /*
   * Set ethernet attributes (ethernet destination address must be set!!)
   */
  m_eth.setType(ethernet::ETHTYPE_IP);
  /*
   * Grab a buffer
   */
  uint8_t* outdata;
  Status ret = m_eth.prepare(outdata);
  if (ret != Status::Ok) {
    return ret;
  }
  /*
   * Update state.
   */
  m_ipid += 1;
  /*
   * Prepare the content of the header
   */
  OUTIP->vhl = 0x45;
  OUTIP->tos = 0;
  OUTIP->len = HEADER_LEN;
  OUTIP->ipid = htons(m_ipid);
  OUTIP->ipoffset[0] = 0;
  OUTIP->ipoffset[1] = 0;
  OUTIP->ttl = TTL;
  OUTIP->proto = m_proto;
  OUTIP->ipchksum = 0;
  OUTIP->srcipaddr = m_hostAddress;
  OUTIP->destipaddr = m_destAddress;
  /*
   * Advance the buffer
   */
  buf = outdata + HEADER_LEN;
  return ret;
}

Status
Producer::commit(const uint32_t len, uint8_t* const buf, const uint16_t mss)
{
  uint8_t* outdata = buf - HEADER_LEN;
  uint32_t outlen = len + HEADER_LEN;
  /*
   * Fill in the remaining header fields.
   */
  OUTIP->len = htons(outlen);
  /*
   * Compute the checksum
   */
#ifndef TULIPS_HAS_HW_CHECKSUM
  OUTIP->ipchksum = ~checksum(outdata);
#endif
  /*
   * Commit the buffer.
   */
  m_stats.sent += 1;
  return m_eth.commit(outlen, outdata, mss);
}

}}}
