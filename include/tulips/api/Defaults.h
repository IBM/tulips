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
#include <tulips/api/Server.h>

namespace tulips { namespace defaults {

class ClientDelegate : public Client::Delegate
{
public:
  void* onConnected(Client::ID const& id, void* const cookie,
                    uint8_t& opts) override;

  Action onAcked(Client::ID const& id, void* const cookie) override;

  Action onAcked(Client::ID const& id, void* const cookie, const uint32_t alen,
                 uint8_t* const sdata, uint32_t& slen) override;

  Action onNewData(Client::ID const& id, void* const cookie,
                   const uint8_t* const data, const uint32_t len) override;

  Action onNewData(Client::ID const& id, void* const cookie,
                   const uint8_t* const data, const uint32_t len,
                   const uint32_t alen, uint8_t* const sdata,
                   uint32_t& slen) override;

  void onClosed(Client::ID const& id, void* const cookie) override;
};

class ServerDelegate : public Server::Delegate
{
public:
  void* onConnected(Server::ID const& id, void* const cookie,
                    uint8_t& opts) override;

  Action onAcked(Server::ID const& id, void* const cookie) override;

  Action onAcked(Server::ID const& id, void* const cookie, const uint32_t alen,
                 uint8_t* const sdata, uint32_t& slen) override;

  Action onNewData(Server::ID const& id, void* const cookie,
                   const uint8_t* const data, const uint32_t len) override;

  Action onNewData(Server::ID const& id, void* const cookie,
                   const uint8_t* const data, const uint32_t len,
                   const uint32_t alen, uint8_t* const sdata,
                   uint32_t& slen) override;

  void onClosed(Server::ID const& id, void* const cookie) override;
};

}}
