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

#include <tulips/system/Compiler.h>
#include <cstdint>
#include <string>
#include <unistd.h>

namespace tulips { namespace stack { namespace ethernet {

/*
 * The ethernet address class.
 */
class Address
{
public:
  using Data = uint8_t[6];

  static const Address BROADCAST;

  Address();
  Address(Address const& o);

  Address(const uint8_t a0, const uint8_t a1, const uint8_t a2,
          const uint8_t a3, const uint8_t a4, const uint8_t a5);

  Address(std::string const& dst);

  inline Address& operator=(Address const& o)
  {
    if (this != &o) {
      m_data[0] = o.m_data[0];
      m_data[1] = o.m_data[1];
      m_data[2] = o.m_data[2];
      m_data[3] = o.m_data[3];
      m_data[4] = o.m_data[4];
      m_data[5] = o.m_data[5];
    }
    return *this;
  }

  inline bool operator==(Address const& o) const
  {
    return m_data[0] == o.m_data[0] && m_data[1] == o.m_data[1] &&
           m_data[2] == o.m_data[2] && m_data[3] == o.m_data[3] &&
           m_data[4] == o.m_data[4] && m_data[5] == o.m_data[5];
  }

  inline bool operator!=(Address const& o) const
  {
    return m_data[0] != o.m_data[0] || m_data[1] != o.m_data[1] ||
           m_data[2] != o.m_data[2] || m_data[3] != o.m_data[3] ||
           m_data[4] != o.m_data[4] || m_data[5] != o.m_data[5];
  }

  Data& data() { return m_data; }

  Data const& data() const { return m_data; }

  std::string toString() const;

private:
  Data m_data;
} __attribute__((packed));

/*
 * The Ethernet header.
 */
struct Header
{
  Address dest;
  Address src;
  uint16_t type;
} __attribute__((packed));

static constexpr size_t USED HEADER_LEN = sizeof(Header);

static constexpr uint16_t USED ETHTYPE_ARP = 0x0806;
static constexpr uint16_t USED ETHTYPE_IP = 0x0800;

}}}
