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

#include <tulips/stack/icmpv4/Request.h>
#include <tulips/stack/ICMPv4.h>
#include <tulips/stack/arp/Processor.h>
#include <tulips/stack/ethernet/Processor.h>
#include <tulips/stack/ipv4/Processor.h>
#include <tulips/transport/Processor.h>
#include <map>
#include <cstdint>

namespace tulips { namespace stack { namespace icmpv4 {

class Processor : public transport::Processor
{
public:
  Processor(ethernet::Producer& eth, ipv4::Producer& ip4);

  Status run() override { return Status::Ok; }
  Status process(const uint16_t len, const uint8_t* const data) override;

  Request& attach(ethernet::Producer& eth, ipv4::Producer& ip4);
  void detach(Request& req);

  Processor& setEthernetProcessor(ethernet::Processor& eth)
  {
    m_ethin = &eth;
    return *this;
  }

  Processor& setIPv4Processor(ipv4::Processor& ipv4)
  {
    m_ip4in = &ipv4;
    return *this;
  }

  Processor& setARPProcessor(arp::Processor& arp)
  {
    m_arp = &arp;
    return *this;
  }

private:
  using Requests = std::map<Request::ID, Request*>;

  ethernet::Producer& m_ethout;
  ipv4::Producer& m_ip4out;
  ethernet::Processor* m_ethin;
  ipv4::Processor* m_ip4in;
  arp::Processor* m_arp;
  Statistics m_stats;
  Requests m_reqs;
  Request::ID m_ids;
};

}}}
