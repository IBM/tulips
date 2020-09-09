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

#include <tulips/stack/tcpv4/Connection.h>

namespace tulips { namespace stack { namespace tcpv4 {

Connection::Connection()
  : m_id(-1)
  , m_rethaddr()
  , m_ripaddr()
  , m_lport(0)
  , m_rport(0)
  , m_rcv_nxt(0)
  , m_snd_nxt(0)
  , m_state(CLOSED)
  , m_ackdata(false)
  , m_newdata(false)
  , m_pshdata(false)
  , m_wndscl(0)
  , m_window(0)
  , m_segidx(0)
  , m_nrtx(0)
  , m_slen(0)
  , m_sdat(nullptr)
  , m_initialmss(0)
  , m_mss(0)
  , m_sa(0)
  , m_sv(0)
  , m_rto(0)
  , m_timer(0)
  , m_opts(0)
  , m_cookie(nullptr)
  , m_segments()
{}

}}}
