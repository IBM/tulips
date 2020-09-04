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

#include <tulips/stack/ipv4/Processor.h>
#ifdef TULIPS_ENABLE_ICMP
#include <tulips/stack/icmpv4/Processor.h>
#endif
#include <tulips/stack/tcpv4/Processor.h>
#include <tulips/system/Compiler.h>
#include <arpa/inet.h>

#define IP_VERBOSE 0

#if IP_VERBOSE
#define IP_LOG(__args) LOG("IP", __args)
#else
#define IP_LOG(...) ((void)0)
#endif

#define INIP ((const Header*)data)

namespace tulips { namespace stack { namespace ipv4 {

Processor::Processor(Address const& ha)
  : m_hostAddress(ha)
  , m_srceAddress()
  , m_destAddress()
  , m_proto(0)
  , m_stats()
  , m_eth(nullptr)
#ifdef TULIPS_ENABLE_RAW
  , m_raw(nullptr)
#endif
#ifdef TULIPS_ENABLE_ICMP
  , m_icmp(nullptr)
#endif
  , m_tcp(nullptr)
{
  memset(&m_stats, 0, sizeof(m_stats));
}

Status
Processor::run()
{
  Status ret = Status::Ok;

  if (m_tcp) {
    ret = m_tcp->run();
  }
#ifdef TULIPS_ENABLE_ICMP
  if (m_icmp && ret == Status::Ok) {
    ret = m_icmp->run();
  }
#endif
#ifdef TULIPS_ENABLE_RAW
  if (m_raw && ret == Status::Ok) {
    ret = m_raw->run();
  }
#endif
  return ret;
}

Status
Processor::process(const uint16_t UNUSED len, const uint8_t* const data)
{
  Status ret;
  /*
   * Update stats
   */
  m_stats.recv += 1;
  /*
   * Check the version field.
   */
  if (INIP->vhl != 0x45) {
    ++m_stats.drop;
    ++m_stats.vhlerr;
    return Status::ProtocolError;
  }
  /*
   * Check the fragment flag.
   */
  if ((INIP->ipoffset[0] & 0x3f) != 0 || INIP->ipoffset[1] != 0) {
    ++m_stats.drop;
    ++m_stats.frgerr;
    return Status::ProtocolError;
  }
  /*
   * Check if the packet is destined for our IP address.
   */
  if (INIP->destipaddr != m_hostAddress) {
    ++m_stats.drop;
    return Status::ProtocolError;
  }
  /*
   * Compute and check the IP header checksum.
   */
#ifndef TULIPS_DISABLE_CHECKSUM_CHECK
  uint16_t sum = checksum(data);
  if (sum != 0xffff) {
    ++m_stats.drop;
    ++m_stats.chkerr;
    IP_LOG("data length: " << len);
    IP_LOG("invalid checksum: 0x" << std::hex << sum << std::dec);
#if IP_VERBOSE
    utils::hexdump(data, len, std::cout);
#endif
    return Status::CorruptedData;
  }
#endif
  /*
   * Extract the information
   */
  uint16_t iplen = ntohs(INIP->len) - HEADER_LEN;
  m_srceAddress = INIP->srcipaddr;
  m_destAddress = INIP->destipaddr;
  m_proto = INIP->proto;
  /*
   * Call the processors
   */
  switch (m_proto) {
    case PROTO_TCP: {
#ifdef TULIPS_STACK_RUNTIME_CHECK
      if (!m_tcp) {
        ret = Status::UnsupportedProtocol;
        break;
      }
#endif
      ret = m_tcp->process(iplen, data + HEADER_LEN);
      break;
    }
#ifdef TULIPS_ENABLE_ICMP
    case PROTO_ICMP: {
#ifdef TULIPS_STACK_RUNTIME_CHECK
      if (!m_icmp) {
        ret = Status::UnsupportedProtocol;
        break;
      }
#endif
      ret = m_icmp->process(iplen, data + HEADER_LEN);
      break;
    }
#endif
#ifdef TULIPS_ENABLE_RAW
    case PROTO_TEST: {
#ifdef TULIPS_STACK_RUNTIME_CHECK
      if (!m_raw) {
        ret = Status::UnsupportedProtocol;
        break;
      }
#endif
      ret = m_raw->process(iplen, data + HEADER_LEN);
      break;
    }
#endif
    default: {
      ret = Status::UnsupportedProtocol;
      break;
    }
  }
  /*
   * Process the output
   */
  return ret;
}

}}}
