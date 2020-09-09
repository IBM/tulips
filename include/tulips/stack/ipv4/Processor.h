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

#include <tulips/stack/IPv4.h>
#include <tulips/stack/Ethernet.h>
#include <tulips/transport/Processor.h>
#include <cstdint>

namespace tulips { namespace stack {

namespace ethernet {
class Processor;
}
#ifdef TULIPS_ENABLE_ICMP
namespace icmpv4 {
class Processor;
}
#endif
namespace tcpv4 {
class Processor;
}

namespace ipv4 {

class Processor : public transport::Processor
{
public:
  struct Statistics
  {
    size_t drop;   // Number of dropped packets at the IP layer.
    size_t recv;   // Number of received packets at the IP layer.
    size_t vhlerr; // Number of packets dropped (bad IP version or header len).
    size_t lenerr; // Number of packets dropped (bad IP len).
    size_t frgerr; // Number of packets dropped since they were IP fragments.
    size_t chkerr; // Number of packets dropped due to IP checksum errors.
  };

  Processor(Address const& ha);

  Status run() override;
  Status process(const uint16_t len, const uint8_t* const data) override;

  Address const& sourceAddress() const { return m_srceAddress; }

  Address const& destinationAddress() const { return m_destAddress; }

  uint8_t protocol() const { return m_proto; }

  Processor& setEthernetProcessor(ethernet::Processor& eth)
  {
    m_eth = &eth;
    return *this;
  }

#ifdef TULIPS_ENABLE_RAW
  Processor& setRawProcessor(transport::Processor& raw)
  {
    m_raw = &raw;
    return *this;
  }
#endif

#ifdef TULIPS_ENABLE_ICMP
  Processor& setICMPv4Processor(icmpv4::Processor& icmp)
  {
    m_icmp = &icmp;
    return *this;
  }
#endif

  Processor& setTCPv4Processor(tcpv4::Processor& tcp)
  {
    m_tcp = &tcp;
    return *this;
  }

private:
  Address m_hostAddress;
  Address m_srceAddress;
  Address m_destAddress;
  uint8_t m_proto;
  Statistics m_stats;
  ethernet::Processor* m_eth;
#ifdef TULIPS_ENABLE_RAW
  transport::Processor* m_raw;
#endif
#ifdef TULIPS_ENABLE_ICMP
  icmpv4::Processor* m_icmp;
#endif
  tcpv4::Processor* m_tcp;
};

}
}}
