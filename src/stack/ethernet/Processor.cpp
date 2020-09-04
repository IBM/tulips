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

#include <tulips/stack/ethernet/Processor.h>
#ifdef TULIPS_ENABLE_ARP
#include <tulips/stack/arp/Processor.h>
#endif
#include <tulips/stack/ipv4/Processor.h>
#include <arpa/inet.h>

#define ETH_VERBOSE 0

#if ETH_VERBOSE
#define ETH_LOG(__args) LOG("ETH", __args)
#else
#define ETH_LOG(...) ((void)0)
#endif

namespace tulips { namespace stack { namespace ethernet {

Processor::Processor(Address const& ha)
  : m_hostAddress(ha)
  , m_srceAddress()
  , m_destAddress()
  , m_type(0)
#ifdef TULIPS_ENABLE_RAW
  , m_raw(nullptr)
#endif
#ifdef TULIPS_ENABLE_ARP
  , m_arp(nullptr)
#endif
  , m_ipv4(nullptr)
{}

Status
Processor::run()
{
  Status ret = Status::Ok;
  /**
   * Reset the state
   */
  m_srceAddress = Address();
  m_destAddress = Address();
  m_type = 0;
  /**
   * Run the processors
   */
#ifdef TULIPS_ENABLE_RAW
  if (m_raw) {
    ret = m_raw->run();
  }
#endif
#ifdef TULIPS_ENABLE_ARP
  if (m_arp && ret == Status::Ok) {
    ret = m_arp->run();
  }
#endif
  if (m_ipv4 && ret == Status::Ok) {
    ret = m_ipv4->run();
  }
  return ret;
}

Status
Processor::process(const uint16_t len, const uint8_t* const data)
{
  ETH_LOG("processing frame: " << len << "B");
  /**
   * Grab the incoming information
   */
  const auto* hdr = reinterpret_cast<const Header*>(data);
  m_srceAddress = hdr->src;
  m_destAddress = hdr->dest;
  m_type = ntohs(hdr->type);
  /**
   * Process the remaing buffer
   */
  Status ret;
  switch (m_type) {
#ifdef TULIPS_ENABLE_ARP
    case ETHTYPE_ARP: {
#ifdef TULIPS_STACK_RUNTIME_CHECK
      if (!m_arp) {
        ret = Status::UnsupportedProtocol;
        break;
      }
#endif
      ret = m_arp->process(len - HEADER_LEN, data + HEADER_LEN);
      break;
    }
#endif
    case ETHTYPE_IP: {
#ifdef TULIPS_STACK_RUNTIME_CHECK
      if (!m_ipv4) {
        ret = Status::UnsupportedProtocol;
        break;
      }
#endif
      ret = m_ipv4->process(len - HEADER_LEN, data + HEADER_LEN);
      break;
    }
    default: {
#ifdef TULIPS_ENABLE_RAW
      if (m_type <= 1500) {
#ifdef TULIPS_STACK_RUNTIME_CHECK
        if (!m_raw) {
          ret = Status::UnsupportedProtocol;
          break;
        }
#endif
        ret = m_raw->process(m_type, data + HEADER_LEN);
        break;
      }
#endif
      ret = Status::UnsupportedProtocol;
      break;
    }
  }
  /**
   * Process outputs
   */
  return ret;
}

}}}
