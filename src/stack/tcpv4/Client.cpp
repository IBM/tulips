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

#include "Debug.h"
#include <tulips/stack/tcpv4/Options.h>
#include <tulips/stack/tcpv4/Processor.h>
#include <tulips/stack/Utils.h>
#include <tulips/system/Compiler.h>
#include <tulips/system/Utils.h>
#include <cstring>

#ifdef __linux__
#include <arpa/inet.h>
#endif

namespace tulips { namespace stack { namespace tcpv4 {

Status
Processor::connect(ethernet::Address const& rhwaddr,
                   ipv4::Address const& ripaddr, const Port rport,
                   Connection::ID& id)
{
  Port lport = 0;
  Connections::iterator e;
  /*
   * Update IP and Ethernet attributes
   */
  m_ipv4to.setProtocol(ipv4::PROTO_TCP);
  m_ipv4to.setDestinationAddress(ripaddr);
  m_ethto.setDestinationAddress(rhwaddr);
  /*
   * Allocate a send buffer
   */
  uint8_t* outdata;
  Status ret = m_ipv4to.prepare(outdata);
  if (ret != Status::Ok) {
    return ret;
  }
  /*
   * Find an unused local port
   */
  do {
    e = m_conns.end();
    /*
     * Compute a proper local port value
     */
    do {
      lport = system::Clock::read() & 0xFFFF;
    } while (lport < 4096);
    /*
     * Check if this port is already in use
     */
    for (auto it = m_conns.begin(); it != m_conns.end(); it++) {
      if (it->m_state != Connection::CLOSED && it->m_lport == htons(lport)) {
        e = it;
      }
    }
  } while (e != m_conns.end());
  /*
   * Allocate a new connection
   */
  for (auto it = m_conns.begin(); it != m_conns.end(); it++) {
    if (it->m_state == Connection::CLOSED) {
      e = it;
      break;
    }
    if (it->m_state == Connection::TIME_WAIT) {
      if (e == m_conns.end() || it->m_timer > e->m_timer) {
        e = it;
      }
    }
  }
  if (e == m_conns.end()) {
    return Status::NoMoreResources;
  }
  /*
   * Add the filter to the device.
   */
  ret = m_device.listen(lport);
  if (ret != Status::Ok) {
    TCP_LOG("registering client-side filter failed");
    return ret;
  }
  /*
   * Prepare the connection.
   */
  e->m_rethaddr = rhwaddr;
  e->m_ripaddr = ripaddr;
  e->m_lport = htons(lport);
  e->m_rport = htons(rport);
  e->m_rcv_nxt = 0;
  e->m_snd_nxt = m_iss;
  e->m_state = Connection::SYN_SENT;
  e->m_opts = 0;
  e->m_ackdata = false;
  e->m_newdata = false;
  e->m_pshdata = false;
  e->m_wndscl = 0;
  e->m_window = 0;
  e->m_segidx = 0;
  e->m_nrtx = 1;
  e->m_slen = 0;
  e->m_sdat = nullptr;
  e->m_initialmss = m_device.mtu() - HEADER_OVERHEAD;
  e->m_mss = e->m_initialmss;
  e->m_sa = 0;
  e->m_sv = 16; // Initial value of the RTT variance
  e->m_rto = RTO;
  e->m_timer = RTO;
  e->m_cookie = nullptr;
  /*
   * Prepare the connection segment.
   */
  Segment& seg = e->nextAvailableSegment();
  seg.set(1, e->m_snd_nxt, outdata); // TCP length of the SYN is one
  /*
   * Send SYN
   */
  OUTTCP->flags = 0;
  if (sendSyn(*e, seg) != Status::Ok) {
    m_device.unlisten(lport);
    e->m_state = Connection::CLOSED;
    return ret;
  }
  id = e - m_conns.begin();
  return Status::Ok;
}

Status
Processor::abort(Connection::ID const& id)
{
  /*
   * Check if the connection is valid
   */
  if (id >= m_nconn) {
    return Status::InvalidConnection;
  }
  Connection& c = m_conns[id];
  /*
   * Abort the connection
   */
  m_device.unlisten(c.m_lport);
  c.m_state = Connection::CLOSED;
  m_handler.onAborted(c);
  /*
   * Send the RST message.
   */
  uint8_t* outdata = c.m_sdat;
  OUTTCP->flags = 0;
  return sendAbort(c);
}

Status
Processor::close(Connection::ID const& id)
{
  /*
   * Check if the connection is valid
   */
  if (id >= m_nconn) {
    return Status::InvalidConnection;
  }
  Connection& c = m_conns[id];
  if (c.m_state != Connection::ESTABLISHED) {
    return Status::NotConnected;
  }
  /*
   * If we are already busy, return OK.
   */
  if (c.hasOutstandingSegments()) {
    TCP_LOG("connection close");
    c.m_state = Connection::CLOSE;
    return Status::Ok;
  }
  /*
   * Close the connection.
   */
  uint8_t* outdata = c.m_sdat;
  OUTTCP->flags = 0;
  return sendClose(c);
}

bool
Processor::isClosed(Connection::ID const& id) const
{
  /*
   * Check if the connection is valid
   */
  if (id >= m_nconn) {
    return true;
  }
  Connection const& c = m_conns[id];
  return c.m_state == Connection::CLOSED;
}

Status
Processor::send(Connection::ID const& id, const uint32_t len,
                const uint8_t* const data, uint32_t& off)
{
  /*
   * Check if the connection is valid.
   */
  if (id >= m_nconn) {
    return Status::InvalidConnection;
  }
  Connection& c = m_conns[id];
  if (c.m_state != Connection::ESTABLISHED) {
    return Status::NotConnected;
  }
  if (HAS_NODELAY(c) && !c.hasAvailableSegments()) {
    return Status::OperationInProgress;
  }
  if (len == 0 || data == nullptr) {
    return Status::InvalidArgument;
  }
  if (off >= len) {
    return Status::InvalidArgument;
  }
  /*
   * Transmit the data. The off parameter is used to store how much data has
   * been written. It is also used as an offset in case the previous write was
   * partial. It is up to the application to reset that offset once the full
   * payload has been transfered.
   */
  uint32_t bound = c.window() < m_mss ? c.window() : m_mss;
  uint32_t slen = len - off;
  /*
   * Check the various corner cases: the remote window can suddenly become
   * smaller than what we want to send or it is just too small to send anything
   * of value.
   */
  if (bound < c.m_slen) {
    return Status::OperationInProgress;
  }
  if (c.m_slen + slen > bound) {
    slen = bound - c.m_slen;
  }
  /*
   * Copy the payload if there is any.
   */
  if (slen != 0) {
    memcpy(c.m_sdat + HEADER_LEN + c.m_slen, data + off, slen);
    /*
     * Remember how much data we send out now so that we know when everything
     * has been acknowledged.
     */
    off += slen;
    c.m_slen = c.m_slen + slen;
  }
  /*
   * Check if we can send the current segment.
   */
  if (!c.hasAvailableSegments()) {
    return slen == 0 ? Status::OperationInProgress : Status::Ok;
  }
  /*
   * Send the segment.
   */
  if (HAS_NODELAY(c)) {
    return sendNoDelay(c, off == len ? TCP_PSH : 0);
  }
  return sendNagle(c, bound);
}

Status
Processor::get(Connection::ID const& id, ipv4::Address& ripaddr, Port& lport,
               Port& rport)
{
  /*
   * Check if the connection is valid.
   */
  if (id >= m_nconn) {
    return Status::InvalidConnection;
  }
  Connection& c = m_conns[id];
  if (c.m_state != Connection::ESTABLISHED) {
    return Status::NotConnected;
  }
  /*
   * Get the connection info.
   */
  ripaddr = c.m_ripaddr;
  lport = ntohs(c.m_lport);
  rport = ntohs(c.m_rport);
  return Status::Ok;
}

void*
Processor::cookie(Connection::ID const& id) const
{
  /*
   * Check if the connection is valid.
   */
  if (id >= m_nconn) {
    return nullptr;
  }
  Connection const& c = m_conns[id];
  return c.cookie();
}

}}}
