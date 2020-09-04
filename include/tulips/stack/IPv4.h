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

namespace tulips { namespace stack { namespace ipv4 {

/*
 * The IPv4 address.
 */
class Address
{
public:
  using Data = uint32_t;

  static const Address BROADCAST;

  Address();
  Address(Address const& o);
  Address(const uint8_t a0, const uint8_t a1, const uint8_t a2,
          const uint8_t a3);
  Address(std::string const& dst);

  inline Address& operator=(Address const& o)
  {
    if (this != &o) {
      m_data = o.m_data;
    }
    return *this;
  }

  inline bool operator==(Address const& o) const { return m_data == o.m_data; }

  inline bool operator!=(Address const& o) const { return m_data != o.m_data; }

  inline bool empty() const { return m_data == 0; }

  inline Data* data() { return &m_data; }

  inline const Data* data() const { return &m_data; }

  std::string toString() const;

private:
  union Alias
  {
    Data raw;
    uint8_t mbr[4];
  };

  Data m_data;

  friend class Producer;
  friend class Processor;
} __attribute__((packed));

/*
 * The IPv4 header.
 */
struct Header
{
  uint8_t vhl;
  uint8_t tos;
  uint16_t len;
  uint16_t ipid;
  uint8_t ipoffset[2];
  uint8_t ttl;
  uint8_t proto;
  uint16_t ipchksum;
  ipv4::Address srcipaddr;
  ipv4::Address destipaddr;
} __attribute__((packed));

static constexpr size_t USED HEADER_LEN = sizeof(Header);

static constexpr uint8_t USED PROTO_ICMP = 1;
static constexpr uint8_t USED PROTO_TCP = 6;
static constexpr uint8_t USED PROTO_TEST = 254;

/*
 * The IPv4 checksum.
 */
#if !(defined(TULIPS_HAS_HW_CHECKSUM) && defined(TULIPS_DISABLE_CHECKSUM_CHECK))
uint16_t checksum(const uint8_t* const data);
#endif

}}}
