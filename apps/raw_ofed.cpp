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

#include <tulips/transport/ofed/Device.h>
#include <tulips/transport/pcap/Device.h>
#include <tulips/stack/ethernet/Producer.h>
#include <tulips/stack/ethernet/Processor.h>
#include <tulips/system/Affinity.h>
#include <tulips/system/Clock.h>
#include <csignal>
#include <tclap/CmdLine.h>
#include <pthread.h>

using namespace tulips;
using namespace stack;
using namespace transport;

static bool show_latency = false;
static bool keep_running = true;
static size_t alarm_delay = 0;
static size_t counter = 0;

/*
 * Raw processor.
 */

class RawProcessor : public Processor
{
public:
  RawProcessor()
    : m_ethto(nullptr)
    , m_ethfrom(nullptr)
    , m_last(system::Clock::read())
    , m_lat(0)
    , m_count(0)
  {}

  Status run() override { return Status::Ok; }

  Status process(UNUSED const uint16_t len, const uint8_t* const data) override
  {
    uint64_t value = *(uint64_t*)data;
    /*
     * Get the timing data.
     */
    if (m_last > 0) {
      m_lat += system::Clock::read() - m_last;
    }
    /*
     * Process the response.
     */
    value += 1;
    return send(sizeof(value), (uint8_t*)&value, true);
  }

  Status send(const uint16_t len, const uint8_t* const data,
              const bool swap = false)
  {
    m_ethto->setType(len);
    memcpy(m_buffer, data, len);
    m_last = system::Clock::read();
    Status ret = m_ethto->commit(len, m_buffer);
    if (ret != Status::Ok) {
      return ret;
    }
    m_count += 1;
    if (swap) {
      m_ethto->setDestinationAddress(m_ethfrom->sourceAddress());
    }
    return m_ethto->prepare(m_buffer);
  }

  RawProcessor& setEthernetProducer(ethernet::Producer& eth)
  {
    m_ethto = &eth;
    m_ethto->prepare(m_buffer);
    return *this;
  }

  RawProcessor& setEthernetProcessor(ethernet::Processor& eth)
  {
    m_ethfrom = &eth;
    return *this;
  }

  system::Clock::Value averageLatency()
  {
    uint64_t res = 0;
    if (m_count > 0) {
      res = system::Clock::nanosecondsOf(m_lat / m_count);
    }
    m_lat = 0;
    m_count = 0;
    return res;
  }

private:
  ethernet::Producer* m_ethto;
  ethernet::Processor* m_ethfrom;
  system::Clock::Value m_last;
  system::Clock::Value m_lat;
  size_t m_count;
  uint8_t* m_buffer;
};

int
main_raw(const bool sender, const size_t ival, const bool pcap, const bool wait,
         std::string const& ifn, std::string const& dst, const int usdly,
         const int cpuid)
{
  /*
   * Create an OFED device
   */
  ofed::Device ofed_device(ifn, 32);
  transport::pcap::Device* pcap_device = nullptr;
  Device* device = &ofed_device;
  /*
   * Open the pcap device
   */
  if (pcap) {
    if (sender) {
      pcap_device = new transport::pcap::Device(ofed_device, "client.pcap");
    } else {
      pcap_device = new transport::pcap::Device(ofed_device, "server.pcap");
    }
    device = pcap_device;
  }
  /*
   * Process the CPU ID.
   */
  if (cpuid >= 0 && !system::setCurrentThreadAffinity(cpuid)) {
    throw std::runtime_error("Cannot set CPU ID: " + std::to_string(cpuid));
  }
  /*
   * Processor
   */
  RawProcessor proc;
  ethernet::Producer eth_prod(*device, device->address());
  ethernet::Processor eth_proc(device->address());
  eth_prod.setType(sizeof(counter)).setDestinationAddress(dst);
  eth_proc.setRawProcessor(proc);
  proc.setEthernetProducer(eth_prod).setEthernetProcessor(eth_proc);
  /*
   * Run as sender.
   */
  if (sender) {
    /**
     * Set the alarm
     */
    alarm_delay = ival;
    alarm(ival);
    /*
     * Send the first message.
     */
    proc.send(sizeof(counter), (uint8_t*)&counter);
    /*
     * Run loop.
     */
    while (keep_running) {
      if (show_latency) {
        show_latency = false;
        std::cout << "Latency = " << proc.averageLatency() << "ns" << std::endl;
      }
      wait ? device->wait(eth_proc, 1000000) : device->poll(eth_proc);
      if (usdly != 0) {
        usleep(usdly);
      }
    }
  }
  /*
   * Run as receiver.
   */
  else {
    /**
     * Set the alarm
     */
    alarm_delay = ival;
    alarm(ival);
    /*
     * Run loop.
     */
    while (keep_running) {
      if (show_latency) {
        show_latency = false;
        std::cout << "Latency = " << proc.averageLatency() << "ns" << std::endl;
      }
      wait ? device->wait(eth_proc, 1000000) : device->poll(eth_proc);
      if (usdly > 0) {
        usleep(usdly);
      }
    }
  }
  /*
   * Delete the PCAP device.
   */
  if (pcap) {
    delete pcap_device;
  }
  return 0;
}

/*
 * Execution control.
 */

void
signal_handler(UNUSED int signal)
{
  keep_running = false;
}

void
alarm_handler(UNUSED int signal)
{
  show_latency = true;
  alarm(alarm_delay);
}

/*
 * General main.
 */

struct Options
{
  Options(TCLAP::CmdLine& cmd)
    : usd("u", "us", "uS delay between sends", false, 1000, "DELAY", cmd)
    , snd("s", "sender", "Sender mode", cmd)
    , hwa("M", "mac", "Remote ethernet address", false, "", "MAC", cmd)
    , pcp("P", "pcap", "Dump packets", cmd)
    , dly("i", "interval", "Statistics interval", false, 10, "INTERVAL", cmd)
    , iff("I", "interface", "Network interface", true, "", "INTERFACE", cmd)
    , wai("w", "wait", "Wait instead of poll", cmd)
    , cpu("", "cpu", "CPU affinity", false, -1, "CPUID")

  {}

  TCLAP::ValueArg<int> usd;
  TCLAP::SwitchArg snd;
  TCLAP::ValueArg<std::string> hwa;
  TCLAP::SwitchArg pcp;
  TCLAP::ValueArg<size_t> dly;
  TCLAP::ValueArg<std::string> iff;
  TCLAP::SwitchArg wai;
  TCLAP::ValueArg<int> cpu;
};

int
main(int argc, char** argv)
{
  TCLAP::CmdLine cmd("TULIPS OFED RAW TEST", ' ', "1.0");
  Options opts(cmd);
  cmd.parse(argc, argv);
  /**
   * Signal handler
   */
  signal(SIGINT, signal_handler);
  signal(SIGALRM, alarm_handler);
  /*
   * Run the proper mode of operation.
   */
  if (!opts.hwa.isSet()) {
    std::cerr << "Remote ethernet address must be set in RAW mode" << std::endl;
    return __LINE__;
  }
  /*
   * Run the main loop.
   */
  return main_raw(opts.snd.isSet(), opts.dly.getValue(), opts.pcp.isSet(),
                  opts.wai.isSet(), opts.iff.getValue(), opts.hwa.getValue(),
                  opts.usd.getValue(), opts.cpu.getValue());
}
