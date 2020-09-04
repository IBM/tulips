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

#include <tulips/api/Server.h>
#include <tulips/system/Utils.h>
#include <arpa/inet.h>

#define SERVER_VERBOSE 1

#if SERVER_VERBOSE
#define SERVER_LOG(__args) LOG("SERVER", __args)
#else
#define SERVER_LOG(...) ((void)0)
#endif

namespace tulips {

using namespace stack;

Server::Server(Delegate& delegate, transport::Device& device,
               const size_t nconn)
  : m_delegate(delegate)
  , m_ethto(device, device.address())
  , m_ip4to(m_ethto, device.ip())
#ifdef TULIPS_ENABLE_ARP
  , m_arp(m_ethto, m_ip4to)
#endif
  , m_ethfrom(device.address())
  , m_ip4from(device.ip())
#ifdef TULIPS_ENABLE_ICMP
  , m_icmpv4from(m_ethto, m_ip4to)
#endif
#ifdef TULIPS_ENABLE_RAW
  , m_raw()
#endif
  , m_tcp(device, m_ethto, m_ip4to, *this, nconn)
{
  /*
   * Hint the device about checksum.
   */
#ifdef TULIPS_DISABLE_CHECKSUM_CHECK
  device.hint(transport::Device::VALIDATE_IP_CSUM);
  device.hint(transport::Device::VALIDATE_TCP_CSUM);
#endif
  /*
   * Connect the stack.
   */
  m_tcp.setEthernetProcessor(m_ethfrom).setIPv4Processor(m_ip4from);
#ifdef TULIPS_ENABLE_ICMP
  m_icmpv4from.setEthernetProcessor(m_ethfrom).setIPv4Processor(m_ip4from);
#endif
  m_ip4to.setDefaultRouterAddress(device.gateway())
    .setNetMask(ipv4::Address(device.netmask()));
  m_ip4from
    .setEthernetProcessor(m_ethfrom)
#ifdef TULIPS_ENABLE_RAW
    .setRawProcessor(m_raw)
#endif
#ifdef TULIPS_ENABLE_ICMP
    .setICMPv4Processor(m_icmpv4from)
#endif
    .setTCPv4Processor(m_tcp);
  m_ethfrom
#ifdef TULIPS_ENABLE_RAW
    .setRawProcessor(m_raw)
#endif
#ifdef TULIPS_ENABLE_ARP
    .setARPProcessor(m_arp)
#endif
    .setIPv4Processor(m_ip4from);
}

void
Server::listen(const stack::tcpv4::Port port, void* cookie)
{
  m_tcp.listen(port);
  m_cookies[htons(port)] = cookie;
}

void
Server::unlisten(const stack::tcpv4::Port port)
{
  m_tcp.unlisten(port);
  m_cookies.erase(htons(port));
}

Status
Server::close(const ID id)
{
  Status res = m_tcp.close(id);
#if SERVER_VERBOSE
  if (res == Status::Ok) {
    SERVER_LOG("closing connection " << id);
  }
#endif
  return res;
}

bool
Server::isClosed(const ID id) const
{
  return m_tcp.isClosed(id);
}

Status
Server::send(const ID id, const uint32_t len, const uint8_t* const data,
             uint32_t& off)
{
  return m_tcp.send(id, len, data, off);
}

void*
Server::cookie(const ID id) const
{
  return m_tcp.cookie(id);
}

void
Server::onConnected(tcpv4::Connection& c)
{
  uint8_t opts = 0;
  void* srvdata = m_cookies[c.localPort()];
  void* appdata = m_delegate.onConnected(c.id(), srvdata, opts);
  SERVER_LOG("connection " << c.id() << " connected");
  c.setCookie(appdata);
  c.setOptions(opts);
}

void
Server::onAborted(tcpv4::Connection& c)
{
  m_delegate.onClosed(c.id(), c.cookie());
  c.setCookie(nullptr);
}

void
Server::onTimedOut(tcpv4::Connection& c)
{
  m_delegate.onClosed(c.id(), c.cookie());
  c.setCookie(nullptr);
}

void
Server::onClosed(tcpv4::Connection& c)
{
  m_delegate.onClosed(c.id(), c.cookie());
  SERVER_LOG("connection " << c.id() << " closed");
  c.setCookie(nullptr);
}

Action
Server::onAcked(stack::tcpv4::Connection& c)
{
  return m_delegate.onAcked(c.id(), c.cookie());
}

Action
Server::onAcked(stack::tcpv4::Connection& c, const uint32_t alen,
                uint8_t* const sdata, uint32_t& slen)
{
  return m_delegate.onAcked(c.id(), c.cookie(), alen, sdata, slen);
}

Action
Server::onNewData(stack::tcpv4::Connection& c, const uint8_t* const data,
                  const uint32_t len)
{
  return m_delegate.onNewData(c.id(), c.cookie(), data, len);
}

Action
Server::onNewData(stack::tcpv4::Connection& c, const uint8_t* const data,
                  const uint32_t len, const uint32_t alen, uint8_t* const sdata,
                  uint32_t& slen)
{
  return m_delegate.onNewData(c.id(), c.cookie(), data, len, alen, sdata, slen);
}

}
