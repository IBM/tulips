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
#include <string>

#ifdef __OpenBSD__
#include <pcap.h>
#else
#include <pcap/pcap.h>
#endif

namespace tulips { namespace transport { namespace pcap {

class Device
  : public transport::Device
  , public Processor
{
public:
  Device(transport::Device& device, std::string const& fn);
  ~Device() override;

  std::string const& name() const override { return m_device.name(); }

  stack::ethernet::Address const& address() const override
  {
    return m_device.address();
  }

  stack::ipv4::Address const& ip() const override { return m_device.ip(); }

  stack::ipv4::Address const& gateway() const override
  {
    return m_device.gateway();
  }

  stack::ipv4::Address const& netmask() const override
  {
    return m_device.netmask();
  }

  Status listen(const uint16_t port) override { return m_device.listen(port); }

  void unlisten(const uint16_t port) override { m_device.unlisten(port); }

  Status poll(Processor& proc) override;
  Status wait(Processor& proc, const uint64_t ns) override;

  uint32_t mtu() const override { return m_device.mtu(); }

  uint32_t mss() const override { return m_device.mss(); }

  uint8_t receiveBufferLengthLog2() const override
  {
    return m_device.receiveBufferLengthLog2();
  }

  uint16_t receiveBuffersAvailable() const override
  {
    return m_device.receiveBuffersAvailable();
  }

  Status prepare(uint8_t*& buf) override;
  Status commit(const uint32_t len, uint8_t* const buf,
                const uint16_t mss = 0) override;

private:
  Status run() override { return Status::Ok; }
  Status process(const uint16_t len, const uint8_t* const data) override;

  transport::Device& m_device;
  pcap_t* m_pcap;
  pcap_dumper_t* m_pcap_dumper;
  Processor* m_proc;
};

}}}
