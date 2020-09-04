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
#include <tulips/fifo/fifo.h>
#include <cstdint>
#include <map>
#include <string>
#include <infiniband/verbs_exp.h>

namespace tulips { namespace transport { namespace ofed {

class Device : public transport::Device
{
public:
  static constexpr size_t EVENT_CLEANUP_THRESHOLD = 16;
  static constexpr size_t INLINE_DATA_THRESHOLD = 256;

  static constexpr int POST_RECV_THRESHOLD = 32;
  static constexpr uint32_t RECV_BUFLEN = 2 * 1024;

  Device(const uint16_t nbuf);
  Device(std::string const& ifn, const uint16_t nbuf);
  ~Device();

  stack::ethernet::Address const& address() const { return m_address; }

  stack::ipv4::Address const& ip() const { return m_ip; }

  stack::ipv4::Address const& gateway() const { return m_dr; }

  stack::ipv4::Address const& netmask() const { return m_nm; }

  uint32_t mtu() const { return m_mtu; }

  uint8_t receiveBufferLengthLog2() const { return 11; }

  uint16_t receiveBuffersAvailable() const { return m_nbuf - m_pending; }

  Status listen(const uint16_t port);
  void unlisten(const uint16_t port);

  Status poll(Processor& proc);
  Status wait(Processor& proc, const uint64_t ns);

  uint32_t mss() const { return m_buflen; }

  Status prepare(uint8_t*& buf);
  Status commit(const uint32_t len, uint8_t* const buf, const uint16_t mss = 0);

private:
  using Filters = std::map<uint16_t, ibv_exp_flow*>;

  void construct(std::string const& ifn, const uint16_t nbuf);
  Status postReceive(const int id);

  uint16_t m_nbuf;
  uint16_t m_pending;
  int m_port;
  ibv_context* m_context;
  stack::ethernet::Address m_address;
  stack::ipv4::Address m_ip;
  stack::ipv4::Address m_dr;
  stack::ipv4::Address m_nm;
  uint32_t m_hwmtu;
  uint32_t m_mtu;
  size_t m_buflen;
  ibv_pd* m_pd;
  ibv_comp_channel* m_comp;
  size_t m_events;
  ibv_cq* m_sendcq;
  ibv_cq* m_recvcq;
  ibv_qp* m_qp;
  uint8_t* m_sendbuf;
  uint8_t* m_recvbuf;
  ibv_mr* m_sendmr;
  ibv_mr* m_recvmr;
  tulips_fifo_t m_fifo;
  ibv_flow* m_bcast;
  ibv_flow* m_flow;
  Filters m_filters;
};

}}}
