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

#include <tulips/api/Action.h>
#include <tulips/stack/tcpv4/Connection.h>
#include <cstdint>

namespace tulips { namespace stack { namespace tcpv4 {

class EventHandler
{
public:
  virtual ~EventHandler() = default;

  /*
   * Called when the connection c is connected.
   */
  virtual void onConnected(Connection& c) = 0;

  /*
   * Called when the connection c is aborted.
   */
  virtual void onAborted(Connection& c) = 0;

  /*
   * Called when the connection c has timed out.
   */
  virtual void onTimedOut(Connection& c) = 0;

  /*
   * Called when a full frame has been sent. Only if built with
   * TULIPS_ENABLE_LATENCY_MONITOR.
   */
  virtual void onSent(Connection& c) = 0;

  /*
   * Called when data for c has been acked.
   */
  virtual Action onAcked(Connection& c) = 0;

  /*
   * Called when data for c has been acked and a reponse can be sent.
   */
  virtual Action onAcked(Connection& c, const uint32_t alen,
                         uint8_t* const sdata, uint32_t& slen) = 0;

  /*
   * Called when new data on c has been received.
   */
  virtual Action onNewData(Connection& c, const uint8_t* const data,
                           const uint32_t len) = 0;

  /*
   * Called when new data on c has been received and a response can be sent.
   */
  virtual Action onNewData(Connection& c, const uint8_t* const data,
                           const uint32_t len, const uint32_t alen,
                           uint8_t* const sdata, uint32_t& slen) = 0;

  /*
   * Called when the connection c has been closed.
   */
  virtual void onClosed(Connection& c) = 0;
};

}}}
