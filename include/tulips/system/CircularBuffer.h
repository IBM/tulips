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

namespace tulips { namespace system {

class CircularBuffer
{
public:
  CircularBuffer(const size_t size);
  ~CircularBuffer();

  inline bool empty() const { return m_read == m_write; }

  inline bool full() const { return m_write - m_read == m_size; }

  inline size_t read(uint8_t* const buffer, const size_t len)
  {
    const size_t delta = available();
    size_t n = len > delta ? delta : len;
    memcpy(buffer, readAt(), n);
    m_read += n;
    return n;
  }

  inline size_t write(const uint8_t* const buffer, const size_t len)
  {
    const size_t delta = left();
    size_t n = len > delta ? delta : len;
    memcpy(writeAt(), buffer, n);
    m_write += n;
    return n;
  }

  inline size_t available() const { return m_write - m_read; }

  inline size_t left() const { return m_size - available(); }

  inline void reset()
  {
    m_read = 0;
    m_write = 0;
  }

  inline const uint8_t* readAt() const { return &m_data[m_read & m_mask]; }

  inline uint8_t* writeAt() const { return &m_data[m_write & m_mask]; }

  inline void skip(const size_t len)
  {
    const size_t delta = available();
    size_t n = len > delta ? delta : len;
    m_read += n;
  }

private:
  static size_t fit(const size_t size);

  const size_t m_size;
  const size_t m_mask;
  uint8_t* m_data;
  size_t m_read;
  size_t m_write;
};

}}
