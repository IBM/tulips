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

#include <uspace/client/Poller.h>

namespace tulips { namespace tools { namespace uspace { namespace client {

Poller::Poller(const bool pcap)
  : m_capture(pcap)
  , m_ofed(128)
  , m_pcap(pcap ? new transport::pcap::Device(m_ofed, "packets.pcap") : nullptr)
  , m_device(pcap ? (transport::Device*)m_pcap : (transport::Device*)&m_ofed)
  , m_delegate()
  , m_client(m_delegate, *m_device, 32)
  , m_run(true)
  , m_thread()
  , m_mutex()
  , m_cond()
  , m_action(Action::None)
  , m_ripaddr()
  , m_lport()
  , m_rport()
  , m_id()
  , m_status()
{
  pthread_create(&m_thread, nullptr, &Poller::entrypoint, this);
  pthread_mutex_init(&m_mutex, nullptr);
  pthread_cond_init(&m_cond, nullptr);
}

Poller::Poller(std::string const& dev, const bool pcap)
  : m_capture(pcap)
  , m_ofed(dev, 128)
  , m_pcap(pcap ? new transport::pcap::Device(m_ofed, "packets.pcap") : nullptr)
  , m_device(pcap ? (transport::Device*)m_pcap : (transport::Device*)&m_ofed)
  , m_delegate()
  , m_client(m_delegate, *m_device, 32)
  , m_run(true)
  , m_thread()
  , m_mutex()
  , m_cond()
  , m_action(Action::None)
  , m_ripaddr()
  , m_lport()
  , m_rport()
  , m_id()
  , m_status()
{
  pthread_create(&m_thread, nullptr, &Poller::entrypoint, this);
  pthread_mutex_init(&m_mutex, nullptr);
  pthread_cond_init(&m_cond, nullptr);
}

Poller::~Poller()
{
  /*
   * Clean-up runtime variables.
   */
  m_run = false;
  pthread_join(m_thread, nullptr);
  pthread_cond_destroy(&m_cond);
  pthread_mutex_destroy(&m_mutex);
  /*
   * Clean-up devices.
   */
  if (m_capture) {
    delete m_pcap;
  }
}

Status
Poller::connect(stack::ipv4::Address const& ripaddr,
                const stack::tcpv4::Port rport, Client::ID& id)
{
  Status result;
  pthread_mutex_lock(&m_mutex);
  /*
   * Get a client ID.
   */
  result = m_client.open(id);
  /*
   * Connect the client.
   */
  if (result == Status::Ok) {
    m_ripaddr = ripaddr;
    m_rport = rport;
    m_id = id;
    do {
      m_action = Action::Connect;
      pthread_cond_wait(&m_cond, &m_mutex);
    } while (m_status == Status::OperationInProgress);
    result = m_status;
  }
  /*
   * Return the result.
   */
  pthread_mutex_unlock(&m_mutex);
  return result;
}

Status
Poller::close(const Client::ID id)
{
  Status result;
  pthread_mutex_lock(&m_mutex);
  /*
   * Ask the poller to close the connection.
   */
  m_action = Action::Close;
  m_id = id;
  pthread_cond_wait(&m_cond, &m_mutex);
  result = m_status;
  /*
   * Return the result.
   */
  pthread_mutex_unlock(&m_mutex);
  return result;
}

Status
Poller::get(const Client::ID id, stack::ipv4::Address& ripaddr,
            stack::tcpv4::Port& lport, stack::tcpv4::Port& rport)
{
  Status result;
  pthread_mutex_lock(&m_mutex);
  /*
   * Ask the poller to grab the connection info.
   */
  m_action = Action::Info;
  m_id = id;
  pthread_cond_wait(&m_cond, &m_mutex);
  result = m_status;
  ripaddr = m_ripaddr;
  lport = m_lport;
  rport = m_rport;
  /*
   * Return the result.
   */
  pthread_mutex_unlock(&m_mutex);
  return result;
}

Status
Poller::write(const Client::ID id, std::string const& data)
{
  Status result;
  pthread_mutex_lock(&m_mutex);
  /*
   * Ask the poller to close the connection.
   */
  m_action = Action::Write;
  m_id = id;
  m_data = data;
  pthread_cond_wait(&m_cond, &m_mutex);
  result = m_status;
  /*
   * Return the result.
   */
  pthread_mutex_unlock(&m_mutex);
  return result;
}

void
Poller::run()
{
  bool closing = false;
  uint32_t off = 0;
  /*
   * Thread run loop.
   */
  while (m_run) {
    /*
     * Poll the device.
     */
    if (m_device->wait(m_client, 100000000ULL) == Status::NoDataAvailable) {
      m_client.run();
    }
    /*
     * Check any incoming commands from the user.
     */
    pthread_mutex_lock(&m_mutex);
    switch (m_action) {
      case Action::Connect: {
        m_status = m_client.connect(m_id, m_ripaddr, m_rport);
        m_action = Action::None;
        pthread_cond_signal(&m_cond);
        break;
      }
      case Action::Close: {
        /*
         * Check if the connection is closing.
         */
        if (closing) {
          if (m_client.isClosed(m_id)) {
            closing = false;
            m_action = Action::None;
            pthread_cond_signal(&m_cond);
          }
        }
        /*
         * Try to close the connection.
         */
        else {
          m_status = m_client.close(m_id);
          if (m_status != Status::Ok) {
            m_action = Action::None;
            pthread_cond_signal(&m_cond);
          } else {
            closing = true;
          }
        }
        break;
      }
      case Action::Info: {
        m_status = m_client.get(m_id, m_ripaddr, m_lport, m_rport);
        m_action = Action::None;
        pthread_cond_signal(&m_cond);
        break;
      }
      case Action::Write: {
        m_status = m_client.send(m_id, m_data.length(),
                                 (const uint8_t*)m_data.c_str(), off);
        switch (m_status) {
          case Status::Ok: {
            if (off == m_data.length()) {
              off = 0;
              m_action = Action::None;
              pthread_cond_signal(&m_cond);
            }
            break;
          }
          case Status::OperationInProgress: {
            break;
          }
          default: {
            off = 0;
            m_action = Action::None;
            pthread_cond_signal(&m_cond);
            break;
          }
        }
        break;
      }
      case Action::None: {
        break;
      }
    }
    pthread_mutex_unlock(&m_mutex);
  }
}

}}}}
