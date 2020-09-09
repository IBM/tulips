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

#include "tulips/fifo/errors.h"
#include <tulips/stack/Utils.h>
#include <tulips/system/Compiler.h>
#include <tulips/transport/shm/Device.h>
#include <cstdlib>
#include <ctime>

#define SHM_VERBOSE 0
#define SHM_HEXDUMP 0

#if SHM_VERBOSE
#define SHM_LOG(__args) LOG("SHM", __args)
#else
#define SHM_LOG(...) ((void)0)
#endif

#define RETRY_COUNT 1

namespace tulips { namespace transport { namespace shm {

Device::Device(stack::ethernet::Address const& address,
               stack::ipv4::Address const& ip, stack::ipv4::Address const& dr,
               stack::ipv4::Address const& nm, tulips_fifo_t rf,
               tulips_fifo_t wf)
  : transport::Device("shm")
  , m_address(address)
  , m_ip(ip)
  , m_dr(dr)
  , m_nm(nm)
  , read_fifo(rf)
  , write_fifo(wf)
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
  bool empty = false;
  /*
   * Check the FIFO for data
   */
  for (size_t i = 0; i < RETRY_COUNT; i += 1) {
    empty = tulips_fifo_empty(read_fifo) == TULIPS_FIFO_YES;
    if (!empty) {
      break;
    }
  }
  /*
   * If there is no data, wait if requested otherwise return
   */
  if (empty) {
    return Status::NoDataAvailable;
  }
  /*
   * Process the data
   */
  Packet* packet = nullptr;
  if (tulips_fifo_front(read_fifo, (void**)&packet) != TULIPS_FIFO_OK) {
    return Status::HardwareError;
  }
  SHM_LOG("processing packet: " << packet->len << "B, " << packet);
#if SHM_VERBOSE && SHM_HEXDUMP
  stack::utils::hexdump(packet->data, packet->len, std::cout);
#endif
  Status ret = proc.process(packet->len, packet->data);
  tulips_fifo_pop(read_fifo);
  return ret;
}

Status
Device::wait(Processor& proc, const uint64_t ns)
{
  bool empty = false;
  /*
   * Check the FIFO for data
   */
  for (size_t i = 0; i < RETRY_COUNT; i += 1) {
    empty = tulips_fifo_empty(read_fifo) == TULIPS_FIFO_YES;
    if (!empty) {
      break;
    }
  }
  /*
   * If there is no data, wait if requested otherwise return
   */
  if (empty && waitForInput(ns)) {
    return Status::NoDataAvailable;
  }
  /*
   * Process the data
   */
  Packet* packet = nullptr;
  if (tulips_fifo_front(read_fifo, (void**)&packet) != TULIPS_FIFO_OK) {
    return Status::HardwareError;
  }
  SHM_LOG("processing packet: " << packet->len << "B, " << packet);
  Status ret = proc.process(packet->len, packet->data);
  tulips_fifo_pop(read_fifo);
  return ret;
}

Status
Device::prepare(uint8_t*& buf)
{
  if (tulips_fifo_full(write_fifo) == TULIPS_FIFO_YES) {
    return Status::NoMoreResources;
  }
  Packet* packet = nullptr;
  tulips_fifo_prepare(write_fifo, (void**)&packet);
  SHM_LOG("preparing packet: " << mss() << "B, " << packet);
  buf = packet->data;
  return Status::Ok;
}

Status
Device::commit(const uint32_t len, uint8_t* const buf,
               const uint16_t UNUSED mss)
{
  auto* packet = (Packet*)(buf - sizeof(uint32_t));
  SHM_LOG("committing packet: " << len << "B, " << packet);
  packet->len = len;
#if SHM_VERBOSE && SHM_HEXDUMP
  stack::utils::hexdump(packet->data, packet->len, std::cout);
#endif
  tulips_fifo_commit(write_fifo);
  pthread_cond_signal(&m_cond);
  return Status::Ok;
}

Status
Device::drop()
{
  if (tulips_fifo_empty(read_fifo) == TULIPS_FIFO_YES) {
    return Status::NoDataAvailable;
  }
  tulips_fifo_pop(read_fifo);
  return Status::Ok;
}

/*
 * This implementation is very expensive...
 */
bool
Device::waitForInput(const uint64_t ns)
{
  uint32_t us = ns / 1000;
  struct timespec ts = { .tv_sec = 0, .tv_nsec = us == 0 ? 1 : us };
  pthread_mutex_lock(&m_mutex);
  pthread_cond_timedwait(&m_cond, &m_mutex, &ts);
  pthread_mutex_unlock(&m_mutex);
  return tulips_fifo_empty(read_fifo) == TULIPS_FIFO_YES;
}

}}}
