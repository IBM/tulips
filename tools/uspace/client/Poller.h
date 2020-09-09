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

#include <tulips/api/Client.h>
#include <tulips/api/Defaults.h>
#include <tulips/transport/ofed/Device.h>
#include <tulips/transport/pcap/Device.h>
#include <string>
#include <pthread.h>

namespace tulips { namespace tools { namespace uspace { namespace client {

class Poller
{
public:
  Poller(const bool pcap);
  Poller(std::string const& dev, const bool pcap);
  ~Poller();

  Status connect(stack::ipv4::Address const& ripaddr,
                 const stack::tcpv4::Port rport, Client::ID& id);

  Status close(const Client::ID id);

  Status get(const Client::ID id, stack::ipv4::Address& ripaddr,
             stack::tcpv4::Port& lport, stack::tcpv4::Port& rport);

  Status write(const Client::ID id, std::string const& data);

private:
  enum class Action
  {
    Connect,
    Close,
    Info,
    Write,
    None
  };

  static void* entrypoint(void* data)
  {
    auto* poller = reinterpret_cast<Poller*>(data);
    poller->run();
    return nullptr;
  }

  void run();

  const bool m_capture;
  transport::ofed::Device m_ofed;
  transport::pcap::Device* m_pcap;
  transport::Device* m_device;
  defaults::ClientDelegate m_delegate;
  Client m_client;
  volatile bool m_run;
  pthread_t m_thread;
  pthread_mutex_t m_mutex;
  pthread_cond_t m_cond;
  Action m_action;
  stack::ipv4::Address m_ripaddr;
  stack::tcpv4::Port m_lport;
  stack::tcpv4::Port m_rport;
  Client::ID m_id;
  Status m_status;
  std::string m_data;
};

}}}}
