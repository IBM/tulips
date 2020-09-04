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

#include <tulips/transport/check/Device.h>
#include <cstdlib>
#include <stdexcept>

namespace tulips { namespace transport { namespace check {

Device::Device(transport::Device& device)
  : transport::Device("check")
  , m_device(device)
  , m_proc(nullptr)
  , m_buffer(nullptr)
{}

Status
Device::poll(Processor& proc)
{
  m_proc = &proc;
  return m_device.poll(*this);
}

Status
Device::wait(Processor& proc, const uint64_t ns)
{
  m_proc = &proc;
  return m_device.wait(*this, ns);
}

Status
Device::process(const uint16_t len, const uint8_t* const data)
{
  if (!check(data, len)) {
    throw std::runtime_error("Empty packet has been received !");
  }
  return m_proc->process(len, data);
}

Status
Device::prepare(uint8_t*& buf)
{
  Status ret = m_device.prepare(buf);
  m_buffer = buf;
  return ret;
}

Status
Device::commit(const uint32_t len, uint8_t* const buf, const uint16_t mss)
{
  if (!check(m_buffer, len)) {
    throw std::runtime_error("Empty packet has been received !");
  }
  return m_device.commit(len, buf, mss);
}

bool
Device::check(const uint8_t* const data, const size_t len)
{
  for (size_t i = 0; i < len; i += 1) {
    if (data[i] != 0) {
      return true;
    }
  }
  return false;
}

}}}
