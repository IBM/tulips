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

#include <tulips/api/Defaults.h>
#include <tulips/system/Compiler.h>

namespace tulips { namespace defaults {

void*
ClientDelegate::onConnected(UNUSED Client::ID const& id,
                            UNUSED void* const cookie, UNUSED uint8_t& opts)
{
  return nullptr;
}

Action
ClientDelegate::onAcked(UNUSED Client::ID const& id, UNUSED void* const cookie)
{
  return Action::Continue;
}

Action
ClientDelegate::onAcked(UNUSED Client::ID const& id, UNUSED void* const cookie,
                        UNUSED const uint32_t alen, UNUSED uint8_t* const sdata,
                        UNUSED uint32_t& slen)
{
  return Action::Continue;
}

Action
ClientDelegate::onNewData(UNUSED Client::ID const& id,
                          UNUSED void* const cookie,
                          UNUSED const uint8_t* const data,
                          UNUSED const uint32_t len)
{
  return Action::Continue;
}

Action
ClientDelegate::onNewData(UNUSED Client::ID const& id,
                          UNUSED void* const cookie,
                          UNUSED const uint8_t* const data,
                          UNUSED const uint32_t len, UNUSED const uint32_t alen,
                          UNUSED uint8_t* const sdata, UNUSED uint32_t& slen)
{
  return Action::Continue;
}

void
ClientDelegate::onClosed(UNUSED Client::ID const& id, UNUSED void* const cookie)
{}

void*
ServerDelegate::onConnected(UNUSED Server::ID const& id,
                            UNUSED void* const cookie, UNUSED uint8_t& opts)
{
  return nullptr;
}

Action
ServerDelegate::onAcked(UNUSED Client::ID const& id, UNUSED void* const cookie)
{
  return Action::Continue;
}

Action
ServerDelegate::onAcked(UNUSED Client::ID const& id, UNUSED void* const cookie,
                        UNUSED const uint32_t alen, UNUSED uint8_t* const sdata,
                        UNUSED uint32_t& slen)
{
  return Action::Continue;
}

Action
ServerDelegate::onNewData(UNUSED Server::ID const& id,
                          UNUSED void* const cookie,
                          UNUSED const uint8_t* const data,
                          UNUSED const uint32_t len)
{
  return Action::Continue;
}

Action
ServerDelegate::onNewData(UNUSED Server::ID const& id,
                          UNUSED void* const cookie,
                          UNUSED const uint8_t* const data,
                          UNUSED const uint32_t len, UNUSED const uint32_t alen,
                          UNUSED uint8_t* const sdata, UNUSED uint32_t& slen)
{
  return Action::Continue;
}

void
ServerDelegate::onClosed(UNUSED Server::ID const& id, UNUSED void* const cookie)
{}

}}
