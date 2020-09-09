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
#include <openssl/err.h>

namespace tulips { namespace ssl {

/*
 * Utilities
 */

const SSL_METHOD*
getMethod(const Protocol type, const bool server, long& flags)
{
  const SSL_METHOD* method = nullptr;
  /*
   * Check requested type.
   */
  switch (type) {
    case Protocol::SSLv3: {
      method = server ? SSLv23_server_method() : SSLv23_client_method();
      flags = SSL_OP_NO_SSLv2 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 |
              SSL_OP_NO_TLSv1_2;
      break;
    }
    case Protocol::TLSv1: {
      method = server ? TLSv1_server_method() : TLSv1_client_method();
      flags = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1_1 |
              SSL_OP_NO_TLSv1_2;
      break;
    }
    case Protocol::TLSv1_1: {
      method = server ? TLSv1_1_server_method() : TLSv1_1_client_method();
      flags = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 |
              SSL_OP_NO_TLSv1_2;
      break;
    }
    case Protocol::TLSv1_2: {
      method = server ? TLSv1_2_server_method() : TLSv1_2_client_method();
      flags = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 |
              SSL_OP_NO_TLSv1_1;
      break;
    }
  }
  /*
   * Return the SSL method.
   */
  return method;
}

std::string
errorToString(SSL* ssl, const int err)
{
  switch (SSL_get_error(ssl, err)) {
    case SSL_ERROR_NONE:
      return "SSL_ERROR_NONE";
    case SSL_ERROR_ZERO_RETURN:
      return "SSL_ERROR_ZERO_RETURN";
    case SSL_ERROR_WANT_READ:
      return "SSL_ERROR_WANT_READ";
    case SSL_ERROR_WANT_WRITE:
      return "SSL_ERROR_WANT_WRITE";
    case SSL_ERROR_WANT_CONNECT:
      return "SSL_ERROR_WANT_CONNECT";
    case SSL_ERROR_WANT_ACCEPT:
      return "SSL_ERROR_WANT_ACCEPT";
    case SSL_ERROR_WANT_X509_LOOKUP:
      return "SSL_ERROR_WANT_X509_LOOKUP";
    case SSL_ERROR_SYSCALL:
      return "SSL_ERROR_SYSCALL";
    case SSL_ERROR_SSL: {
      char buffer[1024];
      ERR_error_string_n(ERR_peek_error(), buffer, 1024);
      return std::string(buffer);
    }
  }
  return "no error";
}

/*
 * SSL context.
 */

Context::Context(SSL_CTX* ctx, const size_t buflen, void* cookie)
  : bin(bio::allocate(buflen))
  , bout(bio::allocate(buflen))
  , ssl(SSL_new(ctx))
  , state(State::Closed)
  , cookie(cookie)
  , blocked(false)
{
  SSL_set_bio(ssl, bin, bout);
}

Context::~Context()
{
  /*
   * No need to free the BIOs, SSL_free does that for us.
   */
  SSL_free(ssl);
}

Action
Context::abortOrClose(const Action r, const uint32_t alen, uint8_t* const sdata,
                      uint32_t& slen)
{
  /*
   * Process an abort request.
   */
  if (r == Action::Abort) {
    SSL_LOG("aborting connection");
    return Action::Abort;
  }
  /*
   * Process a close request.
   */
  if (r == Action::Close) {
    SSL_LOG("closing connection");
    /*
     * Call SSL_shutdown, repeat if necessary.
     */
    int e = SSL_shutdown(ssl);
    if (e == 0) {
      e = SSL_shutdown(ssl);
    }
    /*
     * Check that the SSL connection expect an answer from the other peer.
     */
    if (e < 0) {
      if (SSL_get_error(ssl, e) != SSL_ERROR_WANT_READ) {
        SSL_LOG("SSL_shutdown error: " << ssl::errorToString(ssl, e));
        return Action::Abort;
      }
      /*
       * Flush the shutdown signal.
       */
      state = State::Shutdown;
      return flush(alen, sdata, slen);
    }
    SSL_LOG("SSL_shutdown error, aborting connection");
    return Action::Abort;
  }
  /*
   * Default return.
   */
  return Action::Continue;
}

Action
Context::flush(uint32_t alen, uint8_t* const sdata, uint32_t& slen)
{
  /*
   * Check and send any data in the BIO buffer.
   */
  size_t len = pending();
  if (len == 0) {
    return Action::Continue;
  }
  /*
   * Send the response.
   */
  size_t rlen = len > alen ? alen : len;
  BIO_read(bout, sdata, rlen);
  slen = rlen;
  return Action::Continue;
}

}}
