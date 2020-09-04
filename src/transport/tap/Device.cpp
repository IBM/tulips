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

#include <tulips/transport/tap/Device.h>
#include <tulips/transport/Utils.h>
#include <tulips/system/Utils.h>
#include <cerrno>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <net/if_tun.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <sys/types.h>
#include <sys/stat.h>

#define TAP_VERBOSE 1
#define TAP_HEXDUMP 0

#if TAP_VERBOSE
#define TAP_LOG(__args) LOG("TAP", __args)
#else
#define TAP_LOG(...) ((void)0)
#endif

namespace tulips { namespace transport { namespace tap {

Device::Device(std::string const& devname, stack::ipv4::Address const& ip,
               stack::ipv4::Address const& nm, stack::ipv4::Address const& dr)
  : transport::Device(devname)
  , m_address()
  , m_ip(ip)
  , m_dr(dr)
  , m_nm(nm)
  , m_fd(-1)
  , m_mtu(0)
  , m_buffers()
{
  int ret = 0;
  /*
   * Open the TUN device.
   */
  m_fd = open(("/dev/" + devname).c_str(), O_RDWR);
  if (m_fd < 0) {
    throw std::runtime_error("Cannot open " + devname +
                             " device: " + strerror(errno));
  }
  /*
   * Get the TUN device info.
   */
  struct tuninfo info;
  ret = ioctl(m_fd, TUNGIFINFO, &info);
  if (ret < 0) {
    ::close(m_fd);
    throw std::runtime_error(devname + " is not a TUN/TAP device");
  }
  /*
   * Check if it is a TAP device.
   */
  if (info.type != IFT_ETHER) {
    ::close(m_fd);
    throw std::runtime_error(devname + " is not a TAP device");
  }
  /*
   * Get the device information.
   */
  if (!utils::getInterfaceInformation(devname, m_address, m_mtu)) {
    ::close(m_fd);
    throw std::runtime_error("Cannot get TAP device information");
  }
  /*
   * Set the file descriptor as non-blocking.
   */
  if (fcntl(m_fd, F_SETFL, O_NONBLOCK) == -1) {
    ::close(m_fd);
    throw std::runtime_error("Cannot set the device in non-blocking mode");
  }
  /*
   * Get the device information.
   */
  TAP_LOG("MAC address: " << m_address.toString());
  TAP_LOG("IP address: " << m_ip.toString());
  TAP_LOG("IP gateway: " << m_dr.toString());
  TAP_LOG("IP netmask: " << m_nm.toString());
  TAP_LOG("MTU: " << m_mtu);
  /*
   * Create the buffers.
   */
  for (int i = 0; i < 64; i += 1) {
    m_buffers.push_back(new uint8_t[m_mtu + stack::ethernet::HEADER_LEN]);
  }
}

Device::~Device()
{
  ::close(m_fd);
  std::list<uint8_t*>::iterator it;
  for (it = m_buffers.begin(); it != m_buffers.end(); it++) {
    delete[] * it;
  }
  m_buffers.clear();
}

Status
Device::poll(Processor& proc)
{
  ssize_t ret = 0;
  uint8_t buffer[m_mtu];
  /*
   * Read the available data.
   */
  ret = read(m_fd, buffer, m_mtu);
  if (ret <= 0) {
    if (errno == EAGAIN) {
      return Status::NoDataAvailable;
    } else {
      TAP_LOG(strerror(errno));
      return Status::HardwareError;
    }
  }
  /*
   * Call on the processor.
   */
  TAP_LOG("processing " << ret << "B");
#if TAP_VERBOSE && TAP_HEXDUMP
  stack::utils::hexdump(buffer, ret, std::cout);
#endif
  return proc.process(ret, buffer);
}

Status
Device::wait(Processor& proc, const uint64_t ns)
{
  fd_set fdset;
  struct timeval tv;
  uint64_t us = ns / 1000;
  /*
   * Prepare the select call.
   */
  tv.tv_sec = 0;
  tv.tv_usec = us == 0 ? 1 : us;
  FD_ZERO(&fdset);
  FD_SET(m_fd, &fdset);
  /*
   * Call select on the file descriptor.
   */
  switch (select(m_fd + 1, &fdset, nullptr, nullptr, &tv)) {
    case 0: {
      return Status::NoDataAvailable;
    }
    case 1: {
      return poll(proc);
    }
    default: {
      TAP_LOG(strerror(errno));
      return Status::HardwareError;
    }
  }
}

Status
Device::prepare(uint8_t*& buf)
{
  /*
   * Check if there is any buffer left.
   */
  if (m_buffers.empty()) {
    return Status::NoMoreResources;
  }
  /*
   * Return a buffer.
   */
  buf = m_buffers.front();
  m_buffers.pop_front();
  return Status::Ok;
}

Status
Device::commit(const uint32_t len, uint8_t* const buf, const uint16_t mss)
{
  TAP_LOG("sending " << len << "B");
#if TAP_VERBOSE && TAP_HEXDUMP
  stack::utils::hexdump(buf, len, std::cout);
#endif
  /*
   * Write the payload.
   */
  ssize_t res = write(m_fd, buf, len);
  if (res == -1) {
    TAP_LOG(strerror(errno));
    return Status::HardwareError;
  }
  m_buffers.push_back(buf);
  return Status::Ok;
}

}}}
