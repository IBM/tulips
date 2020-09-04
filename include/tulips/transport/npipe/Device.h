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
#include <string>

namespace tulips { namespace transport { namespace npipe {

class Device : public transport::Device
{
public:
  Device(stack::ethernet::Address const& address,
         stack::ipv4::Address const& ip, stack::ipv4::Address const& nm,
         stack::ipv4::Address const& dr);

  stack::ethernet::Address const& address() const override { return m_address; }

  stack::ipv4::Address const& ip() const override { return m_ip; }

  stack::ipv4::Address const& gateway() const override { return m_dr; }

  stack::ipv4::Address const& netmask() const override { return m_nm; }

  Status listen(const uint16_t UNUSED port) override { return Status::Ok; }

  void unlisten(const uint16_t UNUSED port) override {}

  Status prepare(uint8_t*& buf) override;
  Status commit(const uint32_t len, uint8_t* const buf,
                const uint16_t mss = 0) override;

  Status poll(Processor& proc) override;
  Status wait(Processor& proc, const uint64_t ns) override;

  uint32_t mtu() const override { return DEFAULT_MTU; }

  uint32_t mss() const override
  {
    return DEFAULT_MTU + stack::ethernet::HEADER_LEN;
  }

  uint8_t receiveBufferLengthLog2() const override { return 11; }

  uint16_t receiveBuffersAvailable() const override { return 32; }

protected:
  static constexpr uint32_t BUFLEN = DEFAULT_MTU + stack::ethernet::HEADER_LEN;

  inline bool write(const uint32_t len, uint8_t* const buf)
  {
    int ret = 0;
    for (uint32_t s = 0; s < len; s += ret) {
      ret = ::write(write_fd, buf + s, len - s);
      if (ret < 0) {
        return false;
      }
    }
    return true;
  }

  int waitForInput(const uint64_t ns);

  stack::ethernet::Address m_address;
  stack::ipv4::Address m_ip;
  stack::ipv4::Address m_dr;
  stack::ipv4::Address m_nm;
  uint8_t m_read_buffer[BUFLEN];
  uint8_t m_write_buffer[BUFLEN];
  int read_fd;
  int write_fd;
};

class ClientDevice : public Device
{
public:
  ClientDevice(stack::ethernet::Address const& address,
               stack::ipv4::Address const& ip, stack::ipv4::Address const& nm,
               stack::ipv4::Address const& dr, std::string const& rf,
               std::string const& wf);
};

class ServerDevice : public Device
{
public:
  ServerDevice(stack::ethernet::Address const& address,
               stack::ipv4::Address const& ip, stack::ipv4::Address const& nm,
               stack::ipv4::Address const& dr, std::string const& rf,
               std::string const& wf);

  ~ServerDevice() override;

private:
  std::string m_rf;
  std::string m_wf;
};

}}}
