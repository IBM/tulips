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

#include <tulips/stack/Ethernet.h>
#include <tulips/system/Utils.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace tulips { namespace stack { namespace ethernet {

const Address Address::BROADCAST(0xff, 0xff, 0xff, 0xff, 0xff, 0xff);

/**
 * Address class definition
 */

Address::Address() : m_data()
{
  m_data[0] = 0;
  m_data[1] = 0;
  m_data[2] = 0;
  m_data[3] = 0;
  m_data[4] = 0;
  m_data[5] = 0;
}

Address::Address(Address const& o) : m_data()
{
  m_data[0] = o.m_data[0];
  m_data[1] = o.m_data[1];
  m_data[2] = o.m_data[2];
  m_data[3] = o.m_data[3];
  m_data[4] = o.m_data[4];
  m_data[5] = o.m_data[5];
}

Address::Address(const uint8_t a0, const uint8_t a1, const uint8_t a2,
                 const uint8_t a3, const uint8_t a4, const uint8_t a5)
  : m_data()
{
  m_data[0] = a0;
  m_data[1] = a1;
  m_data[2] = a2;
  m_data[3] = a3;
  m_data[4] = a4;
  m_data[5] = a5;
}

Address::Address(std::string const& dst) : m_data()
{
  std::vector<std::string> parts;
  system::utils::split(dst, ':', parts);
  if (parts.size() != 6) {
    throw std::invalid_argument("String is not a valid ethernet address");
  }
  for (int i = 0; i < 6; i += 1) {
    int value;
    std::istringstream(parts[i]) >> std::hex >> value;
    m_data[i] = value;
  }
}

std::string
Address::toString() const
{
  std::ostringstream oss;
  oss << std::hex << std::setw(2) << std::setfill('0')
      << (unsigned int)m_data[0] << ":" << std::setw(2) << std::setfill('0')
      << (unsigned int)m_data[1] << ":" << std::setw(2) << std::setfill('0')
      << (unsigned int)m_data[2] << ":" << std::setw(2) << std::setfill('0')
      << (unsigned int)m_data[3] << ":" << std::setw(2) << std::setfill('0')
      << (unsigned int)m_data[4] << ":" << std::setw(2) << std::setfill('0')
      << (unsigned int)m_data[5] << std::dec;
  return oss.str();
}

}}}
