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

#include <tulips/api/Client.h>
#include <tulips/api/Interface.h>
#include <tulips/ssl/Protocol.h>
#include <vector>

namespace tulips { namespace ssl {

class Client
  : public interface::Client
  , public interface::Client::Delegate
{
public:
  Client(interface::Client::Delegate& delegate, transport::Device& device,
         const size_t nconn, const Protocol type, std::string const& cert,
         std::string const& key);
  ~Client() override;

  /**
   * Device interface.
   */

  inline Status run() override { return m_client.run(); }

  inline Status process(const uint16_t len, const uint8_t* const data) override
  {
    return m_client.process(len, data);
  }

  /**
   * Client interface.
   */

  Status open(ID& id) override;

  Status connect(const ID id, stack::ipv4::Address const& ripaddr,
                 const stack::tcpv4::Port rport) override;

  Status abort(const ID id) override;

  Status close(const ID id) override;

  bool isClosed(const ID id) const override;

  Status send(const ID id, const uint32_t len, const uint8_t* const data,
              uint32_t& off) override;

  system::Clock::Value averageLatency(const ID id) override;

  /*
   * Client delegate.
   */

  void* onConnected(ID const& id, void* const cookie, uint8_t& opts) override;

  Action onAcked(ID const& id, void* const cookie) override;

  Action onAcked(ID const& id, void* const cookie, const uint32_t alen,
                 uint8_t* const sdata, uint32_t& slen) override;

  Action onNewData(ID const& id, void* const cookie, const uint8_t* const data,
                   const uint32_t len) override;

  Action onNewData(ID const& id, void* const cookie, const uint8_t* const data,
                   const uint32_t len, const uint32_t alen,
                   uint8_t* const sdata, uint32_t& slen) override;

  void onClosed(ID const& id, void* const cookie) override;

private:
  Status flush(const ID id, void* const cookie);

  interface::Client::Delegate& m_delegate;
  transport::Device& m_dev;
  tulips::Client m_client;
  void* m_context;
};

}}
