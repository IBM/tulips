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

#include "Context.h"
#include <tulips/ssl/Client.h>
#include <tulips/stack/Utils.h>
#include <stdexcept>
#include <openssl/err.h>
#include <openssl/ssl.h>

#define CLIENT_VERBOSE 1

#if CLIENT_VERBOSE
#define CLIENT_LOG(__args) LOG("SSLCLI", __args)
#else
#define CLIENT_LOG(...) ((void)0)
#endif

namespace tulips { namespace ssl {

Client::Client(interface::Client::Delegate& delegate, transport::Device& device,
               const size_t nconn, const Protocol type, std::string const& cert,
               std::string const& key)
  : m_delegate(delegate)
  , m_dev(device)
  , m_client(*this, device, nconn)
  , m_context(nullptr)
{
  int err = 0;
  CLIENT_LOG("protocol: " << ssl::toString(type));
  /*
   * Initialize the SSL library.
   */
  SSL_library_init();
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  ERR_load_BIO_strings();
  ERR_load_crypto_strings();
  /*
   * Create the SSL context.
   */
  long flags = 0;
  m_context = SSL_CTX_new(ssl::getMethod(type, false, flags));
  if (m_context == nullptr) {
    throw std::runtime_error("SSL_CTX_new failed");
  }
  SSL_CTX_set_options(AS_SSL(m_context), flags);
  /*
   * Load certificate and private key files, and check consistency.
   */
  err = SSL_CTX_use_certificate_file(AS_SSL(m_context), cert.c_str(),
                                     SSL_FILETYPE_PEM);
  if (err != 1) {
    throw std::runtime_error("SSL_CTX_use_certificate_file failed");
  }
  CLIENT_LOG("using certificate: " << cert);
  /*
   * Indicate the key file to be used.
   */
  err = SSL_CTX_use_PrivateKey_file(AS_SSL(m_context), key.c_str(),
                                    SSL_FILETYPE_PEM);
  if (err != 1) {
    throw std::runtime_error("SSL_CTX_use_PrivateKey_file failed");
  }
  CLIENT_LOG("using key: " << key);
  /*
   * Make sure the key and certificate file match.
   */
  if (SSL_CTX_check_private_key(AS_SSL(m_context)) != 1) {
    throw std::runtime_error("SSL_CTX_check_private_key failed");
  }
  /*
   * Use AES ciphers.
   */
  const char* const PREFERRED_CIPHERS = "HIGH:!aNULL:!PSK:!SRP:!MD5:!RC4:!3DES";
  int res = SSL_CTX_set_cipher_list(AS_SSL(m_context), PREFERRED_CIPHERS);
  if (res != 1) {
    throw std::runtime_error("SSL_CTX_set_cipher_list failed");
  }
}

Client::~Client()
{
  SSL_CTX_free(AS_SSL(m_context));
}

Status
Client::open(ID& id)
{
  Status res = m_client.open(id);
  if (res != Status::Ok) {
    return res;
  }
  return Status::Ok;
}

Status
Client::connect(const ID id, stack::ipv4::Address const& ripaddr,
                const stack::tcpv4::Port rport)
{

  void* cookie = m_client.cookie(id);
  /*
   * If the cookie is nullptr, we are not connected yet.
   */
  if (cookie == nullptr) {
    Status res = m_client.connect(id, ripaddr, rport);
    if (res != Status::Ok) {
      return res;
    }
    cookie = m_client.cookie(id);
  }
  /*
   * Perform the handshake.
   */
  Context& c = *reinterpret_cast<Context*>(cookie);
  switch (c.state) {
    /*
     * Perform the client SSL handshake.
     */
    case Context::State::Connect: {
      int e = SSL_connect(c.ssl);
      switch (e) {
        case 0: {
          CLIENT_LOG("connect error");
          return Status::ProtocolError;
        }
        case 1: {
          CLIENT_LOG("SSL_connect successful");
          c.state = Context::State::Ready;
          return Status::Ok;
        }
        default: {
          if (SSL_get_error(c.ssl, e) != SSL_ERROR_WANT_READ) {
            CLIENT_LOG("connect error: " << ssl::errorToString(c.ssl, e));
            return Status::ProtocolError;
          }
          Status res = flush(id, cookie);
          if (res != Status::Ok) {
            return res;
          }
          return Status::OperationInProgress;
        }
      }
    }
    /*
     * Connection is ready.
     */
    case Context::State::Ready: {
      return Status::Ok;
    }
    /*
     * Connection is being shut down.
     */
    case Context::State::Shutdown: {
      return Status::InvalidArgument;
    }
    /*
     * Default.
     */
    default: {
      return Status::ProtocolError;
    }
  }
}

Status
Client::abort(const ID id)
{
  /*
   * Grab the context.
   */
  void* cookie = m_client.cookie(id);
  if (cookie == nullptr) {
    return Status::InvalidArgument;
  }
  Context& c = *reinterpret_cast<Context*>(cookie);
  /*
   * Check if the connection is in the right state.
   */
  if (c.state != Context::State::Ready && c.state != Context::State::Shutdown) {
    return Status::NotConnected;
  }
  /*
   * Abort the connection.
   */
  return m_client.abort(id);
}

Status
Client::close(const ID id)
{
  /*
   * Grab the context.
   */
  void* cookie = m_client.cookie(id);
  if (cookie == nullptr) {
    return Status::NotConnected;
  }
  Context& c = *reinterpret_cast<Context*>(cookie);
  /*
   * Check if the connection is in the right state.
   */
  if (c.state != Context::State::Ready && c.state != Context::State::Shutdown) {
    return Status::NotConnected;
  }
  if (c.state == Context::State::Shutdown) {
    return Status::OperationInProgress;
  }
  /*
   * Mark the state as shut down.
   */
  c.state = Context::State::Shutdown;
  /*
   * Call SSL_shutdown, repeat if necessary.
   */
  int e = SSL_shutdown(c.ssl);
  /*
   * Go through the shutdown state machine.
   */
  switch (e) {
    case 0: {
      CLIENT_LOG("SSL shutdown sent");
      flush(id, cookie);
      return Status::OperationInProgress;
    }
    case 1: {
      CLIENT_LOG("shutdown completed");
      return m_client.close(id);
    }
    default: {
      CLIENT_LOG("SSL_shutdown error: " << ssl::errorToString(c.ssl, e));
      return Status::ProtocolError;
    }
  }
}

bool
Client::isClosed(const ID id) const
{
  return m_client.isClosed(id);
}

Status
Client::send(const ID id, const uint32_t len, const uint8_t* const data,
             uint32_t& off)
{
  /*
   * Grab the context.
   */
  void* cookie = m_client.cookie(id);
  if (cookie == nullptr) {
    return Status::InvalidArgument;
  }
  Context& c = *reinterpret_cast<Context*>(cookie);
  /*
   * Check if the connection is in the right state.
   */
  if (c.state != Context::State::Ready) {
    return Status::InvalidConnection;
  }
  /*
   * Check if we can write anything.
   */
  if (c.blocked) {
    return Status::OperationInProgress;
  }
  /*
   * Write the data. With BIO mem, the write will always succeed.
   */
  off = 0;
  off += SSL_write(c.ssl, data, len);
  /*
   * Flush the data.
   */
  return flush(id, cookie);
}

system::Clock::Value
Client::averageLatency(const ID id)
{
  return m_client.averageLatency(id);
}

void*
Client::onConnected(ID const& id, void* const cookie, uint8_t& opts)
{
  void* user = m_delegate.onConnected(id, cookie, opts);
  auto* c = new Context(AS_SSL(m_context), m_dev.mss(), user);
  c->state = Context::State::Connect;
  return c;
}

Action
Client::onAcked(ID const& id, void* const cookie)
{
  /*
   * Grab the context.
   */
  Context& c = *reinterpret_cast<Context*>(cookie);
  /*
   * Return if the handshake was not done.
   */
  if (c.state != Context::State::Ready) {
    return Action::Continue;
  }
  /*
   * Notify the delegate.
   */
  return m_delegate.onAcked(id, c.cookie);
}

Action
Client::onAcked(ID const& id, void* const cookie, const uint32_t alen,
                uint8_t* const sdata, uint32_t& slen)
{
  /*
   * Grab the context.
   */
  Context& c = *reinterpret_cast<Context*>(cookie);
  /*
   * If the BIO has data pending, flush it.
   */
  return c.onAcked(id, m_delegate, alen, sdata, slen);
}

Action
Client::onNewData(ID const& id, void* const cookie, const uint8_t* const data,
                  const uint32_t len)
{
  /*
   * Grab the context.
   */
  Context& c = *reinterpret_cast<Context*>(cookie);
  /*
   * Decrypt the incoming data.
   */
  return c.onNewData(id, m_delegate, data, len);
}

Action
Client::onNewData(ID const& id, void* const cookie, const uint8_t* const data,
                  const uint32_t len, const uint32_t alen, uint8_t* const sdata,
                  uint32_t& slen)
{
  /*
   * Grab the context.
   */
  Context& c = *reinterpret_cast<Context*>(cookie);
  /*
   * Write the data in the input BIO.
   */
  return c.onNewData(id, m_delegate, data, len, alen, sdata, slen);
}

void
Client::onClosed(ID const& id, void* const cookie)
{
  auto* c = reinterpret_cast<Context*>(cookie);
  if (c != nullptr) {
    m_delegate.onClosed(id, c->cookie);
    delete c;
  }
}

Status
Client::flush(const ID id, void* const cookie)
{
  /*
   * Grab the context.
   */
  Context& c = *reinterpret_cast<Context*>(cookie);
  /*
   * Send the pending data.
   */
  size_t len = c.pending();
  if (len == 0) {
    return Status::Ok;
  }
  uint32_t rem = 0;
  Status res = m_client.send(id, len, ssl::bio::readAt(c.bout), rem);
  if (res != Status::Ok) {
    c.blocked = res == Status::OperationInProgress;
    return res;
  }
  ssl::bio::skip(c.bout, rem);
  return Status::Ok;
}

}}
