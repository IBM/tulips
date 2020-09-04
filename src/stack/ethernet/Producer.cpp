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

#include <tulips/stack/ethernet/Producer.h>
#include <arpa/inet.h>

#define ETH_VERBOSE 0

#if ETH_VERBOSE
#define ETH_LOG(__args) LOG("ETH", __args)
#else
#define ETH_LOG(...) ((void)0)
#endif

namespace tulips { namespace stack { namespace ethernet {

Producer::Producer(transport::Producer& prod, Address const& ha)
  : m_prod(prod), m_hostAddress(ha), m_destAddress(), m_type(0)
{}

Status
Producer::prepare(uint8_t*& buf)
{
  /*
   * Grab a buffer
   */
  uint8_t* tmp;
  Status ret = m_prod.prepare(tmp);
  if (ret != Status::Ok) {
    return ret;
  }
  /*
   * Prepare the header
   */
  auto* hdr = reinterpret_cast<Header*>(tmp);
  hdr->src = m_hostAddress;
  hdr->dest = m_destAddress;
  hdr->type = htons(m_type);
  /*
   * Advance that buffer
   */
  buf = tmp + HEADER_LEN;
  return ret;
}

Status
Producer::commit(const uint32_t len, uint8_t* const buf, const uint16_t mss)
{
  ETH_LOG("committing frame: " << len << "B");
  return m_prod.commit(len + HEADER_LEN, buf - HEADER_LEN, mss);
}

}}}
