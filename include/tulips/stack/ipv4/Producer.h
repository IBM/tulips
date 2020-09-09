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
#include <tulips/stack/ethernet/Producer.h>
#include <tulips/transport/Producer.h>
#include <cstdint>
#include <string>

namespace tulips { namespace stack { namespace ipv4 {

class Producer : public transport::Producer
{
public:
  struct Statistics
  {
    size_t sent; // Number of sent packets at the IP layer.
  };

  Producer(ethernet::Producer& prod, Address const& ha);

  uint32_t mss() const override { return m_eth.mss() - HEADER_LEN; }

  Status prepare(uint8_t*& buf) override;
  Status commit(const uint32_t len, uint8_t* const buf,
                const uint16_t mss = 0) override;

  Address const& hostAddress() const { return m_hostAddress; }

  Producer& setDestinationAddress(Address const& addr)
  {
    m_destAddress = addr;
    return *this;
  }

  Address const& defaultRouterAddress() const { return m_defaultRouterAddress; }

  Producer& setDefaultRouterAddress(Address const& addr)
  {
    m_defaultRouterAddress = addr;
    return *this;
  }

  Address const& netMask() const { return m_netMask; }

  Producer& setNetMask(Address const& addr)
  {
    m_netMask = addr;
    return *this;
  }

  void setProtocol(const uint8_t proto) { m_proto = proto; }

  bool isLocal(Address const& addr) const
  {
    return (addr.m_data & m_netMask.m_data) ==
           (m_hostAddress.m_data & m_netMask.m_data);
  }

private:
  ethernet::Producer& m_eth;
  Address m_hostAddress;
  Address m_destAddress;
  Address m_defaultRouterAddress;
  Address m_netMask;
  uint8_t m_proto;
  uint16_t m_ipid;
  Statistics m_stats;
};

}}}
