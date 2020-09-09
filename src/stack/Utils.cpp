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
#include <tulips/stack/IPv4.h>
#include <tulips/stack/TCPv4.h>
#include <cstdint>
#include <iomanip>
#include <ostream>

namespace tulips { namespace stack { namespace utils {

uint16_t
checksum(const uint16_t seed, const uint8_t* const data, const uint16_t len)
{
  uint16_t t, sum = seed;
  const uint8_t* dataptr = data;
  const uint8_t* last_byte = data + len - 1;
  /*
   * If at least two more bytes.
   */
  while (dataptr < last_byte) {
    t = (dataptr[0] << 8) + dataptr[1];
    sum += t;
    if (sum < t) {
      sum++;
    }
    dataptr += 2;
  }
  if (dataptr == last_byte) {
    t = (dataptr[0] << 8) + 0;
    sum += t;
    if (sum < t) {
      sum++;
    }
  }
  /*
   * Return sum in host byte order
   */
  return sum;
}

#define HEXFMT(_n) "0x" << std::hex << std::setw(_n) << std::setfill('0')
#define RSTFMT std::dec << std::setfill(' ')

void
hexdump(const uint8_t* const data, const uint16_t len, std::ostream& out)
{
  uint16_t i;
  for (i = 0; i < len; i += 1) {
    if (i % 8 == 0) {
      out << HEXFMT(3) << i << RSTFMT << ": ";
    }
    out << HEXFMT(2) << (unsigned int)data[i] << RSTFMT;
    if ((i + 1) % 8 == 0) {
      out << std::endl;
    } else {
      out << " ";
    }
  }
  if (i % 8 != 0) {
    out << std::endl;
  }
}

bool
headerLength(const uint8_t* const packet, const uint32_t plen, uint32_t& len)
{
  const uint8_t* ip;
  const uint8_t* tcp;
  /*
   * Walk up the packet structure.
   */
  len = stack::ethernet::HEADER_LEN;
  ip = packet + stack::ethernet::HEADER_LEN;
  len += stack::ipv4::HEADER_LEN;
  tcp = ip + stack::ipv4::HEADER_LEN;
  len += HEADER_LEN_WITH_OPTS(tcp);
  /*
   * Check the result.
   */
  return len <= plen;
}

}}}
