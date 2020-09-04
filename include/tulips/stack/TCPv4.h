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
#include <tulips/system/Compiler.h>
#include <cstdint>
#include <limits>
#include <unistd.h>

namespace tulips { namespace stack { namespace tcpv4 {

/*
 * The TCPv4 port type.
 */
using Port = uint16_t;

/*
 * Sequence number limits.
 */
using SeqLimits = std::numeric_limits<uint32_t>;

/*
 * The TCPv4 header.
 */
struct Header
{
  Port srcport;
  Port destport;
  uint32_t seqno;
  uint32_t ackno;
  struct
  {
    uint8_t reserved : 4;
    uint8_t offset : 4;
  };
  uint8_t flags;
  uint16_t wnd;
  uint16_t chksum;
  uint16_t urgp;
  uint8_t opts[];
} __attribute__((packed));

#define HEADER_LEN_WITH_OPTS(__HDR)                                            \
  (((tulips::stack::tcpv4::Header*)__HDR)->offset << 2)

static constexpr size_t USED HEADER_LEN = sizeof(Header);
static constexpr uint8_t USED RTO = 3;
static constexpr uint16_t USED HEADER_OVERHEAD = ipv4::HEADER_LEN + HEADER_LEN;

}}}
