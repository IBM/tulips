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
#include <tulips/ssl/Server.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#define SERVER_VERBOSE 1

#if SERVER_VERBOSE
#define SERVER_LOG(__args) LOG("SSLSRV", __args)
#else
#define SERVER_LOG(...) ((void)0)
#endif

namespace tulips { namespace ssl {

Server::Server(interface::Server::Delegate& delegate, transport::Device& device,
               const size_t nconn, const ssl::Protocol type,
               std::string const& cert, std::string const& key)
  : m_delegate(delegate)
  , m_dev(device)
  , m_server(*this, device, nconn)
  , m_context(nullptr)
{
  int err = 0;
  SERVER_LOG("protocol: " << ssl::toString(type));
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
  m_context = SSL_CTX_new(ssl::getMethod(type, true, flags));
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
  SERVER_LOG("using certificate: " << cert);
  /*
   * Indicate the key file to be used.
   */
  err = SSL_CTX_use_PrivateKey_file(AS_SSL(m_context), key.c_str(),
                                    SSL_FILETYPE_PEM);
  if (err != 1) {
    throw std::runtime_error("SSL_CTX_use_PrivateKey_file failed");
  }
  SERVER_LOG("using key: " << key);
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

Server::~Server()
{
  SSL_CTX_free(AS_SSL(m_context));
}

Status
Server::close(const ID id)
{
  /*
   * Grab the context.
   */
  void* cookie = m_server.cookie(id);
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
      SERVER_LOG("SSL shutdown sent");
      flush(id, cookie);
      return Status::OperationInProgress;
    }
    case 1: {
      SERVER_LOG("shutdown completed");
      return m_server.close(id);
    }
    default: {
      SERVER_LOG("SSL_shutdown error: " << ssl::errorToString(c.ssl, e));
      return Status::ProtocolError;
    }
  }
}

bool
Server::isClosed(const ID id) const
{
  return m_server.isClosed(id);
}

Status
Server::send(const ID id, const uint32_t len, const uint8_t* const data,
             uint32_t& off)
{
  /*
   * Grab the context.
   */
  void* cookie = m_server.cookie(id);
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

void*
Server::onConnected(ID const& id, void* const cookie, uint8_t& opts)
{
  void* user = m_delegate.onConnected(id, cookie, opts);
  auto* c = new Context(AS_SSL(m_context), m_dev.mss(), user);
  c->state = Context::State::Accept;
  return c;
}

Action
Server::onAcked(ID const& id, void* const cookie)
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
Server::onAcked(ID const& id, void* const cookie, const uint32_t alen,
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
Server::onNewData(ID const& id, void* const cookie, const uint8_t* const data,
                  const uint32_t len)
{
  /*
   * Grab the context.
   */
  Context& c = *reinterpret_cast<Context*>(cookie);
  /*
   * Process the incoming data.
   */
  return c.onNewData(id, m_delegate, data, len);
}

Action
Server::onNewData(ID const& id, void* const cookie, const uint8_t* const data,
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
Server::onClosed(ID const& id, void* const cookie)
{
  auto* c = reinterpret_cast<Context*>(cookie);
  m_delegate.onClosed(id, c->cookie);
  delete c;
}

Status
Server::flush(const ID id, void* const cookie)
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
  Status res = m_server.send(id, len, ssl::bio::readAt(c.bout), rem);
  if (res != Status::Ok) {
    c.blocked = res == Status::OperationInProgress;
    return res;
  }
  ssl::bio::skip(c.bout, rem);
  return Status::Ok;
}

}}
