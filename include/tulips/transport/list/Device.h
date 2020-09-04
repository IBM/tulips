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
#include <tulips/system/Compiler.h>
#include <cstdlib>
#include <limits>
#include <list>
#include <new>
#include <string>
#include <pthread.h>

namespace tulips { namespace transport { namespace list {

class Device : public transport::Device
{
public:
  struct Packet
  {
    static Packet* allocate(const uint32_t mtu)
    {
      void* data = malloc(sizeof(Packet) + mtu);
      return new (data) Packet;
    }

    static void release(Packet* p) { free(p); }

    uint32_t len;
    uint8_t data[];
  } __attribute__((packed));

  using List = std::list<Packet*>;

  Device(stack::ethernet::Address const& address,
         stack::ipv4::Address const& ip, stack::ipv4::Address const& dr,
         stack::ipv4::Address const& nm, const uint32_t mtu, List& rf,
         List& wf);
  ~Device() override;

  stack::ethernet::Address const& address() const override { return m_address; }

  stack::ipv4::Address const& ip() const override { return m_ip; }

  stack::ipv4::Address const& gateway() const override { return m_dr; }

  stack::ipv4::Address const& netmask() const override { return m_nm; }

  Status listen(const uint16_t UNUSED port) override { return Status::Ok; }

  void unlisten(const uint16_t UNUSED port) override {}

  Status poll(Processor& proc) override;
  Status wait(Processor& proc, const uint64_t ns) override;

  Status prepare(uint8_t*& buf) override;
  Status commit(const uint32_t len, uint8_t* const buf,
                const uint16_t mss) override;

  uint32_t mtu() const override { return m_mtu - stack::ethernet::HEADER_LEN; }

  uint32_t mss() const override { return m_mtu; }

  uint8_t receiveBufferLengthLog2() const override { return 10; }

  uint16_t receiveBuffersAvailable() const override
  {
    return std::numeric_limits<uint16_t>::max();
  }

  Status drop();

protected:
  bool waitForInput(const uint64_t ns);

  stack::ethernet::Address m_address;
  stack::ipv4::Address m_ip;
  stack::ipv4::Address m_dr;
  stack::ipv4::Address m_nm;
  uint32_t m_mtu;
  List& m_read;
  List& m_write;
  pthread_mutex_t m_mutex;
  pthread_cond_t m_cond;
};

}}}
