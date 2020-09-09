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

#include <tulips/api/Status.h>
#include <tulips/transport/Processor.h>
#include <tulips/transport/Producer.h>
#include <string>
#include <unistd.h>

namespace tulips {

namespace stack { namespace ethernet {
class Address;
}}
namespace stack { namespace ipv4 {
class Address;
}}

namespace transport {

class Device : public Producer
{
public:
  enum Hint
  {
    VALIDATE_IP_CSUM = 0x1,
    VALIDATE_TCP_CSUM = 0x2,
  };

  /*
   * The DEFAULT_MTU takes the value of the maximum size for the payload of an
   * Ethernet frame. It does NOT include the ethernet header.
   */
  static constexpr uint32_t DEFAULT_MTU = 1500;

  Device() : m_name(), m_hints(0) {}
  Device(std::string const& name) : m_name(name), m_hints(0) {}
  ~Device() override = default;

  /**
   * @return the device's name.
   */
  virtual std::string const& name() const { return m_name; }

  /**
   * @return the device's hardware address.
   */
  virtual stack::ethernet::Address const& address() const = 0;

  /**
   * @return the device's IP address.
   */
  virtual stack::ipv4::Address const& ip() const = 0;

  /**
   * @return the device's default gateway.
   */
  virtual stack::ipv4::Address const& gateway() const = 0;

  /**
   * @return the device's netmask.
   */
  virtual stack::ipv4::Address const& netmask() const = 0;

  /**
   * @return the device's MTU.
   */
  virtual uint32_t mtu() const = 0;

  /**
   * Ask the device to listen to a particular TCP port.
   *
   * @param port the TCP port.
   *
   * @return the status of the operation.
   */
  virtual Status listen(const uint16_t port) = 0;

  /**
   * Ask the device to stop listening to a TCP port.
   *
   * @param port the TCP port.
   */
  virtual void unlisten(const uint16_t port) = 0;

  /**
   * Poll the device input queues for activity. Call upon the rcv processor
   * when data is available. This operations is non-blocking.
   *
   * @param rcv the processor to handle the incoming data.
   *
   * @return the status of the operation.
   */
  virtual Status poll(Processor& rcv) = 0;

  /**
   * Wait on the device input queues for new data with a ns timeout. Call upon
   * the rcv processor when data is available. This operation is blocking
   *
   * @param rcv the processor to handle the incoming data.
   * @param ns the timeout of the operation.
   *
   * @return the status of the operation.
   */
  virtual Status wait(Processor& rcv, const uint64_t ns) = 0;

  /**
   * @return the size of a receive buffer as a power of 2.
   * @note maps directly to TCP's window scale.
   */
  virtual uint8_t receiveBufferLengthLog2() const = 0;

  /*
   * @return the number of receive buffers available.
   * @note maps directly to TCP's window size.
   */
  virtual uint16_t receiveBuffersAvailable() const = 0;

  /**
   * Give a hint to the device.
   *
   * @param h the hint to give.
   */
  void hint(const Hint h) { m_hints |= h; }

protected:
  std::string m_name;
  uint16_t m_hints;
};

}
}
