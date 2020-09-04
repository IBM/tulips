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

#include <tulips/transport/list/Device.h>
#include <tulips/system/Compiler.h>
#include <cstdlib>
#include <ctime>

#define LIST_VERBOSE 0

#if LIST_VERBOSE
#define LIST_LOG(__args) LOG("LIST", __args)
#else
#define LIST_LOG(...) ((void)0)
#endif

namespace tulips { namespace transport { namespace list {

Device::Device(stack::ethernet::Address const& address,
               stack::ipv4::Address const& ip, stack::ipv4::Address const& dr,
               stack::ipv4::Address const& nm, const uint32_t mtu, List& rf,
               List& wf)
  : transport::Device("shm")
  , m_address(address)
  , m_ip(ip)
  , m_dr(dr)
  , m_nm(nm)
  , m_mtu(mtu)
  , m_read(rf)
  , m_write(wf)
  , m_mutex()
  , m_cond()
{
  pthread_mutex_init(&m_mutex, nullptr);
  pthread_cond_init(&m_cond, nullptr);
}

Device::~Device()
{
  pthread_cond_destroy(&m_cond);
  pthread_mutex_destroy(&m_mutex);
}

Status
Device::poll(Processor& proc)
{
  /*
   * If there is no data, return.
   */
  if (m_read.empty()) {
    return Status::NoDataAvailable;
  }
  /*
   * Process the data.
   */
  Packet* packet = m_read.front();
  LIST_LOG("processing packet: " << packet->len << "B, " << packet);
  Status ret = proc.process(packet->len, packet->data);
  m_read.pop_front();
  delete packet;
  return ret;
}

Status
Device::wait(Processor& proc, const uint64_t ns)
{
  /*
   * If there is no data, wait if requested otherwise return
   */
  if (m_read.empty() && waitForInput(ns)) {
    return Status::NoDataAvailable;
  }
  /*
   * Process the data
   */
  Packet* packet = m_read.front();
  LIST_LOG("processing packet: " << packet->len << "B, " << packet);
  Status ret = proc.process(packet->len, packet->data);
  m_read.pop_front();
  Packet::release(packet);
  return ret;
}

Status
Device::prepare(uint8_t*& buf)
{
  Packet* packet = Packet::allocate(m_mtu);
  LIST_LOG("preparing packet: " << mss() << "B, " << packet);
  buf = packet->data;
  return Status::Ok;
}

Status
Device::commit(const uint32_t len, uint8_t* const buf,
               const uint16_t UNUSED mss)
{
  auto* packet = (Packet*)(buf - sizeof(uint32_t));
  LIST_LOG("committing packet: " << len << "B, " << packet);
  packet->len = len;
  m_write.push_back(packet);
  pthread_cond_signal(&m_cond);
  return Status::Ok;
}

Status
Device::drop()
{
  if (m_read.empty()) {
    return Status::NoDataAvailable;
  }
  Packet* packet = m_read.front();
  m_read.pop_front();
  delete packet;
  return Status::Ok;
}

/*
 * This implementation is very expensive...
 */
bool
Device::waitForInput(const uint64_t ns)
{
  uint32_t us = ns / 1000;
  struct timespec ts = { .tv_sec = 0, .tv_nsec = us == 0 ? 1000 : us };
  pthread_cond_timedwait(&m_cond, &m_mutex, &ts);
  return m_read.empty();
}

}}}
