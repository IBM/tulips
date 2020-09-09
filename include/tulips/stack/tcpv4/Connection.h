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

#include <tulips/stack/TCPv4.h>
#include <tulips/stack/ethernet/Producer.h>
#include <tulips/stack/ipv4/Producer.h>
#include <tulips/stack/tcpv4/Options.h>
#include <tulips/stack/tcpv4/Segment.h>
#include <tulips/system/SpinLock.h>
#include <cstdint>
#include <stdexcept>

namespace tulips { namespace stack { namespace tcpv4 {

/*
 * We rely on the compiler to wrap around the value of the next segment. We
 * need at least 3 bits for NRTX to SEGM_B cannot be more that 5.
 */
#define SEGM_B 4
#define NRTX_B 9 - SEGM_B

#define HAS_NODELAY(__e) (__e.m_opts & Connection::NO_DELAY)
#define HAS_DELAYED_ACK(__e) (__e.m_opts & Connection::DELAYED_ACK)

class Connection
{
public:
  using ID = uint16_t;

  enum State
  {
    CLOSE = 0x1,
    CLOSED = 0x2,
    CLOSING = 0x3,
    ESTABLISHED = 0x4,
    FIN_WAIT_1 = 0x5,
    FIN_WAIT_2 = 0x6,
    LAST_ACK = 0x7,
    STOPPED = 0x8,
    SYN_RCVD = 0x9,
    SYN_SENT = 0xA,
    TIME_WAIT = 0xB,
  };

  enum Option
  {
    NO_DELAY = 0x1,
    DELAYED_ACK = 0x2
  };

  Connection();

  inline ID id() const { return m_id; }

  inline uint16_t localPort() const { return m_lport; }

  inline uint16_t remotePort() const { return m_rport; }

  inline void* cookie() const { return m_cookie; }

  inline void setCookie(void* const cookie) { m_cookie = cookie; }

  inline void setOptions(const uint8_t opts) { m_opts |= opts; }

  inline void clearOptions(const uint8_t opts) { m_opts &= ~opts; }

  inline bool isNewDataPushed() const { return m_pshdata; }

private:
  static constexpr size_t SEGMENT_COUNT = 1 << SEGM_B;
  static constexpr size_t SEGMENT_BMASK = SEGMENT_COUNT - 1;

  inline bool isActive() const { return m_state != CLOSED; }

  inline bool hasAvailableSegments() const
  {
    for (size_t i = 0; i < SEGMENT_COUNT; i += 1) {
      if (m_segments[i].m_len == 0) {
        return true;
      }
    }
    return false;
  }

  inline bool hasOutstandingSegments() const
  {
    for (size_t i = 0; i < SEGMENT_COUNT; i += 1) {
      if (m_segments[i].m_len != 0) {
        return true;
      }
    }
    return false;
  }

  inline bool hasPendingSendData() const { return m_slen != 0; }

  uint32_t window() const { return (uint32_t)m_window << m_wndscl; }

  uint32_t window(const uint16_t wnd) const
  {
    return (uint32_t)wnd << m_wndscl;
  }

  inline size_t id(Segment const& s) const { return &s - m_segments; }

  inline Segment& segment() { return m_segments[m_segidx]; }

  inline Segment& nextAvailableSegment()
  {
    size_t idx = 0;
    for (size_t i = m_segidx; i < m_segidx + SEGMENT_COUNT; i += 1) {
      idx = i & SEGMENT_BMASK;
      if (m_segments[idx].m_len == 0) {
        return m_segments[idx];
      }
    }
    throw std::runtime_error("have you called hasAvailableSegments()?");
  }

  inline size_t level() const
  {
    size_t level = 0;
    for (size_t i = 0; i < SEGMENT_COUNT; i += 1) {
      if (m_segments[i].m_len == 0) {
        level += 1;
      }
    }
    return level;
  }

  inline void updateRttEstimation()
  {
    int8_t m = m_rto - m_timer;
    /*
     * This is taken directly from VJs original code in his paper
     */
    m = m - (m_sa >> 3);
    m_sa += m;
    m = m < 0 ? -m : m;
    m = m - (m_sv >> 2);
    m_sv += m;
    m_rto = (m_sa >> 3) + m_sv;
  }

  inline void resetSendBuffer()
  {
    m_slen = 0;
    m_sdat = nullptr;
  }

  /*
   * Member variables, all in one cache line.
   */

  ID m_id; // 2 - Connection ID

  ethernet::Address m_rethaddr; // 6 - Ethernet address of the remote host
  ipv4::Address m_ripaddr;      // 4 - IP address of the remote host

  Port m_lport; // 2 - Local TCP port, in network byte order
  Port m_rport; // 2 - Local remote TCP port, in network byte order

  uint32_t m_rcv_nxt; // 4 - Sequence number that we expect to receive next
  uint32_t m_snd_nxt; // 4 - Sequence number that was last sent by us

  struct
  {
    uint64_t m_state : 4;       // . - Connection state
    uint64_t m_ackdata : 1;     // . - Connection has been acked
    uint64_t m_newdata : 1;     // . - Connection has new data
    uint64_t m_pshdata : 1;     // . - Connection data is being pushed
    uint64_t m_wndscl : 8;      // . - Remote peer window scale (max is 14)
    uint64_t m_window : 16;     // . - Remote peer window
    uint64_t m_segidx : SEGM_B; // 8 - Free segment index
    uint64_t m_nrtx : NRTX_B;   // . - Number of retransmissions (3 bit minimum)
    uint64_t m_slen : 24;       // . - Length of the send buffer
  };

  uint8_t* m_sdat; // 8 - Send buffer

  uint16_t m_initialmss; // 2 - Initial maximum segment size for the connection
  uint16_t m_mss;        // 2 - Current maximum segment size for the connection

  uint8_t m_sa;    // 1 - Retransmission time-out calculation state
  uint8_t m_sv;    // 1 - Retransmission time-out calculation state
  uint8_t m_rto;   // 1 - Retransmission time-out
  uint8_t m_timer; // 1 - Retransmission timer

  uint64_t m_opts; // 8 - Connection options (NO_DELAY, etc..)
  void* m_cookie;  // 8 - Application state

  /*
   * Segments. Size is 16B per segment, 4 segments per cache line, for a maximum
   * of 16 segments (segment index is 4 bits).
   */

  Segment m_segments[SEGMENT_COUNT];

  /*
   * Friendship declaration.
   */

  friend class Processor;
  friend void Options::parse(Connection&, const uint16_t, const uint8_t* const);

} __attribute__((aligned(64)));

static_assert(sizeof(Connection) == (1 << SEGM_B) * sizeof(Segment) + 64,
              "Size of tcpv4::Connection is invalid");

}}}
