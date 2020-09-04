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

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace tulips { namespace stack { namespace tcpv4 {

class Segment
{
public:
  Segment();

private:
  inline void set(const uint32_t len, const uint32_t seq, uint8_t* const dat)
  {
    m_len = len;
    m_seq = seq;
    m_dat = dat;
  }

  inline void mark(const uint32_t seq) { m_seq = seq; }

  inline void clear()
  {
    m_len = 0;
    m_seq = 0;
    m_dat = nullptr;
  }

  inline void swap(uint8_t* const to)
  {
    memcpy(to, m_dat, m_len);
    m_dat = to;
  }

  /*
   * The len field is used to check if the segment was fully acknowledged. It
   * is also used to check if the segment is valid (=0).
   */
  uint32_t m_len; // 4 - Length of the data that was sent
  uint32_t m_seq; // 4 - Sequence number of the segment
  uint8_t* m_dat; // 8 - Data that was sent

  friend class Connection;
  friend class Processor;
} __attribute__((aligned(16)));

static_assert(sizeof(Segment) == 16, "Invalid size for tcpv4::Segment");

}}}
