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

#include <tulips/stack/Ethernet.h>
#include <tulips/transport/Processor.h>
#include <cstdint>

namespace tulips { namespace stack {

#ifdef TULIPS_ENABLE_ARP
namespace arp {
class Processor;
}
#endif

namespace ipv4 {
class Processor;
}

namespace ethernet {

class Processor : public transport::Processor
{
public:
  Processor(Address const& ha);

  Status run() override;
  Status process(const uint16_t len, const uint8_t* const data) override;

  Address const& sourceAddress() { return m_srceAddress; }

  Address const& destinationAddress() { return m_destAddress; }

  uint16_t type() const { return m_type; }

#ifdef TULIPS_ENABLE_RAW
  Processor& setRawProcessor(transport::Processor& raw)
  {
    m_raw = &raw;
    return *this;
  }
#endif

#ifdef TULIPS_ENABLE_ARP
  Processor& setARPProcessor(arp::Processor& arp)
  {
    m_arp = &arp;
    return *this;
  }
#endif

  Processor& setIPv4Processor(ipv4::Processor& ip4)
  {
    m_ipv4 = &ip4;
    return *this;
  }

private:
  Address m_hostAddress;
  Address m_srceAddress;
  Address m_destAddress;
  uint16_t m_type;
#ifdef TULIPS_ENABLE_RAW
  transport::Processor* m_raw;
#endif
#ifdef TULIPS_ENABLE_ARP
  arp::Processor* m_arp;
#endif
  ipv4::Processor* m_ipv4;
};

}
}}
