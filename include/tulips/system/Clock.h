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

#if defined(rdtsc)
#undef rdtsc
#endif

#include <cstdint>

#define CLOCK_SECOND tulips::system::Clock::get().cyclesPerSecond()

namespace tulips { namespace system {

class Clock
{
public:
  using Value = uint64_t;

  inline static Clock& get()
  {
    static Clock clock;
    return clock;
  }

  inline Value cyclesPerSecond() { return m_cps; }

#ifdef TULIPS_CLOCK_HAS_OFFSET
  inline static Value read() { return rdtsc() + get().offset(); }

  inline void offsetBy(const Value offset) { m_offset += offset; }

  inline Value offset() const { return m_offset; }
#else
  inline static Value read() { return rdtsc(); }
#endif

  inline static uint64_t nanosecondsOf(const Value v)
  {
    static Value cps = get().cyclesPerSecond();
    return v * 1000000000ULL / cps;
  }

private:
  Clock();

  inline static uint64_t rdtsc()
  {
    uint64_t a, d;
    __asm__ __volatile__("rdtsc" : "=a"(a), "=d"(d));
    return (a | (d << 32));
  }

  static uint64_t getCPS();

  Value m_cps;
#ifdef TULIPS_CLOCK_HAS_OFFSET
  Value m_offset;
#endif
};

}}
