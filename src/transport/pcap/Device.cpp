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

#include <tulips/transport/pcap/Device.h>
#include <tulips/stack/Ethernet.h>
#include <tulips/system/Clock.h>

#define PCAP_VERBOSE 0

#if PCAP_VERBOSE
#define PCAP_LOG(__args) LOG("PCAP", __args)
#else
#define PCAP_LOG(...) ((void)0)
#endif

namespace tulips { namespace transport { namespace pcap {

static void
writePacket(pcap_dumper_t* const dumper, const void* const data,
            const size_t len)
{
  static system::Clock::Value cps = system::Clock::get().cyclesPerSecond();
  static system::Clock::Value first = 0;
  struct pcap_pkthdr hdr;
  if (first == 0) {
    first = system::Clock::read();
    hdr.ts.tv_sec = 0;
    hdr.ts.tv_usec = 0;
  } else {
    system::Clock::Value current = system::Clock::read();
    system::Clock::Value delta = current - first;
    system::Clock::Value secs = delta / cps;
#ifdef __OpenBSD__
    system::Clock::Value nscs = delta - secs * cps;
    nscs = nscs * 1000000ULL / cps;
    hdr.ts.tv_sec = secs;
    hdr.ts.tv_usec = nscs;
#else
    system::Clock::Value nscs = delta - secs * cps;
    nscs = nscs * 1000000000ULL / cps;
    hdr.ts.tv_sec = secs;
    hdr.ts.tv_usec = nscs;
#endif
  }
  hdr.caplen = len;
  hdr.len = len;
  pcap_dump((u_char*)dumper, &hdr, (const u_char*)data);
}

Device::Device(transport::Device& device, std::string const& fn)
  : transport::Device("pcap")
  , m_device(device)
  , m_pcap(nullptr)
  , m_pcap_dumper(nullptr)
  , m_proc(nullptr)
{
  /*
   * We adapt the snapshot length to the lower link MSS. With TSO enabled, the
   * length of the IP packet is wrong if the payload is larger than 64K. It will
   * lead to invalid packets in the resulting PCAP.
   */
  uint32_t snaplen = m_device.mss() + stack::ethernet::HEADER_LEN;
  PCAP_LOG("snaplen is " << snaplen);
#ifdef __OpenBSD__
  m_pcap = pcap_open_dead(DLT_EN10MB, snaplen);
#else
  m_pcap = pcap_open_dead_with_tstamp_precision(DLT_EN10MB, snaplen,
                                                PCAP_TSTAMP_PRECISION_NANO);
#endif
  m_pcap_dumper = pcap_dump_open(m_pcap, fn.c_str());
}

Device::~Device()
{
  pcap_dump_flush(m_pcap_dumper);
  pcap_dump_close(m_pcap_dumper);
  pcap_close(m_pcap);
}

Status
Device::poll(Processor& proc)
{
  m_proc = &proc;
  return m_device.poll(*this);
}

Status
Device::wait(Processor& proc, const uint64_t ns)
{
  m_proc = &proc;
  return m_device.wait(*this, ns);
}

Status
Device::prepare(uint8_t*& buf)
{
  return m_device.prepare(buf);
}

Status
Device::commit(const uint32_t len, uint8_t* const buf, const uint16_t mss)
{
  Status ret = m_device.commit(len, buf, mss);
  if (ret == Status::Ok) {
    writePacket(m_pcap_dumper, buf, len);
  }
  return ret;
}

Status
Device::process(const uint16_t len, const uint8_t* const data)
{
  if (len > 0) {
    writePacket(m_pcap_dumper, data, len);
  }
  return m_proc->process(len, data);
}

}}}
