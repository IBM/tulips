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

#include <tulips/stack/ARP.h>
#include <tulips/stack/ethernet/Producer.h>
#include <tulips/stack/ipv4/Producer.h>
#include <tulips/transport/Processor.h>
#include <tulips/system/Timer.h>
#include <vector>
#include <unistd.h>

namespace tulips { namespace stack { namespace arp {

class Processor : public transport::Processor
{
public:
  Processor(ethernet::Producer& eth, ipv4::Producer& ip4);

  Status run() override;
  Status process(const uint16_t len, const uint8_t* const data) override;

  bool has(ipv4::Address const& destipaddr);
  Status discover(ipv4::Address const& destipaddr);

  bool query(ipv4::Address const& destipaddr, ethernet::Address& ethaddr);
  void update(ipv4::Address const& ipaddr, ethernet::Address const& ethaddr);

private:
  struct Entry
  {
    Entry();

    ipv4::Address ipaddr;
    ethernet::Address ethaddr;
    uint8_t time;
  } __attribute__((packed));

  using Table = std::vector<Entry>;

  ipv4::Address const& hopAddress(ipv4::Address const& addr) const;

  ethernet::Producer& m_eth;
  ipv4::Producer& m_ipv4;
  Table m_table;
  uint8_t m_time;
  system::Timer m_timer;
};

}}}
