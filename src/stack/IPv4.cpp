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

#include <tulips/stack/IPv4.h>
#ifdef TULIPS_ENABLE_ICMP
#include <tulips/stack/ICMPv4.h>
#endif
#include <tulips/stack/Utils.h>
#include <tulips/system/Utils.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <arpa/inet.h>

namespace tulips { namespace stack { namespace ipv4 {

const Address Address::BROADCAST(0xff, 0xff, 0xff, 0xff);

/*
 * Address class definition
 */

Address::Address() : m_data(0) {}

Address::Address(Address const& o) : m_data()
{
  m_data = o.m_data;
}

Address::Address(const uint8_t a0, const uint8_t a1, const uint8_t a2,
                 const uint8_t a3)
  : m_data()
{
  Alias alias = { .mbr = { a0, a1, a2, a3 } };
  m_data = alias.raw;
}

Address::Address(std::string const& dst) : m_data()
{
  std::vector<std::string> parts;
  system::utils::split(dst, '.', parts);
  if (parts.size() != 4) {
    throw std::invalid_argument("String is not a valid IPv4 address");
  }
  Alias alias = { .raw = 0 };
  for (int i = 0; i < 4; i += 1) {
    int value;
    std::istringstream(parts[i]) >> value;
    alias.mbr[i] = value;
  }
  m_data = alias.raw;
}

std::string
Address::toString() const
{
  Alias alias = { .raw = m_data };
  std::ostringstream oss;
  oss << (unsigned int)alias.mbr[0] << "." << (unsigned int)alias.mbr[1] << "."
      << (unsigned int)alias.mbr[2] << "." << (unsigned int)alias.mbr[3];
  return oss.str();
}

/*
 * IPv4 checksum.
 */

#if !(defined(TULIPS_HAS_HW_CHECKSUM) && defined(TULIPS_DISABLE_CHECKSUM_CHECK))
uint16_t
checksum(const uint8_t* const data)
{
  uint16_t sum = utils::checksum(0, data, sizeof(Header));
  return (sum == 0) ? 0xffff : htons(sum);
}
#endif

}}}
