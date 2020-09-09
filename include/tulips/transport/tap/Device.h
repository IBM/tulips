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

#include <tulips/transport/Device.h>
#include <tulips/stack/Ethernet.h>
#include <tulips/stack/IPv4.h>
#include <pthread.h>
#include <list>
#include <memory>
#include <string>

namespace tulips { namespace transport { namespace tap {

class Device : public transport::Device
{
public:
  Device(std::string const& devname, stack::ipv4::Address const& ip,
         stack::ipv4::Address const& nm, stack::ipv4::Address const& dr);
  ~Device();

  stack::ethernet::Address const& address() const { return m_address; }

  stack::ipv4::Address const& ip() const { return m_ip; }

  stack::ipv4::Address const& gateway() const { return m_dr; }

  stack::ipv4::Address const& netmask() const { return m_nm; }

  Status listen(const uint16_t port) { return Status::Ok; }

  void unlisten(const uint16_t port) {}

  Status prepare(uint8_t*& buf);
  Status commit(const uint32_t len, uint8_t* const buf, const uint16_t mss = 0);

  Status poll(Processor& proc);
  Status wait(Processor& proc, const uint64_t ns);

  uint32_t mtu() const { return m_mtu; }

  uint32_t mss() const { return m_mtu + stack::ethernet::HEADER_LEN; }

  uint8_t receiveBufferLengthLog2() const { return 11; }

  uint16_t receiveBuffersAvailable() const { return 32; }

protected:
  stack::ethernet::Address m_address;
  stack::ipv4::Address m_ip;
  stack::ipv4::Address m_dr;
  stack::ipv4::Address m_nm;
  int m_fd;
  uint32_t m_mtu;
  std::list<uint8_t*> m_buffers;
};

}}}
