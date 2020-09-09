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

#pragma once

#include "BIO.h"
#include <tulips/api/Action.h>
#include <tulips/api/Interface.h>
#include <tulips/ssl/Protocol.h>
#include <tulips/system/Utils.h>
#include <string>
#include <openssl/ssl.h>

#define SSL_VERBOSE 1

#if SSL_VERBOSE
#define SSL_LOG(__args) LOG("SSL", __args)
#else
#define SSL_LOG(...)
#endif

#define AS_SSL(__c) reinterpret_cast<SSL_CTX*>(__c)

namespace tulips { namespace ssl {

/*
 * Utilities.
 */

const SSL_METHOD* getMethod(const Protocol type, const bool server,
                            long& flags);
std::string errorToString(SSL* ssl, const int err);

/*
 * SSL context.
 */

struct Context
{
  enum class State
  {
    Closed,
    Connect,
    Accept,
    Ready,
    Shutdown
  };

  Context(SSL_CTX* ctx, const size_t buflen, void* cookie = nullptr);
  ~Context();

  /**
   * Process pending data on ACK.
   */
  template<typename ID>
  Action onAcked(ID const& id, interface::Delegate<ID>& delegate,
                 const uint32_t alen, uint8_t* const sdata, uint32_t& slen)
  {
    /*
     * Reset the blocked state.
     */
    blocked = false;
    /*
     * If the BIO has data pending, flush it.
     */
    if (pending() > 0) {
      return flush(alen, sdata, slen);
    }
    /*
     * Call the delegate and encrypt the output.
     */
    uint8_t out[alen];
    uint32_t rlen = 0;
    Action act = delegate.onAcked(id, cookie, alen, out, rlen);
    if (act != Action::Continue) {
      return abortOrClose(act, alen, sdata, slen);
    }
    if (rlen > alen) {
      rlen = alen;
    }
    SSL_write(ssl, out, rlen);
    return flush(alen, sdata, slen);
  }

  /**
   * Processing incoming data and encrypt the response.
   */
  template<typename ID>
  Action onNewData(ID const& id, interface::Delegate<ID>& delegate,
                   const uint8_t* const data, const uint32_t len)
  {
    /*
     * Write the data in the input BIO.
     */
    BIO_write(bin, data, len);
    /*
     * Only accept Ready state.
     */
    if (state != State::Ready) {
      return Action::Abort;
    }
    /*
     * Decrypt and pass the data to the delegate.
     */
    int ret = 0;
    uint8_t in[len];
    /*
     * Process the internal buffer as long as there is data available.
     */
    do {
      ret = SSL_read(ssl, in, len);
      /*
       * Handle partial data.
       */
      if (ret < 0) {
        if (SSL_get_error(ssl, ret) == SSL_ERROR_WANT_READ) {
          break;
        }
        SSL_LOG("SSL_read error: " << errorToString(ssl, ret));
        return Action::Abort;
      }
      /*
       * Handle shutdown.
       */
      if (ret == 0) {
        int err = SSL_get_error(ssl, ret);
        if (err == SSL_ERROR_ZERO_RETURN || err == SSL_ERROR_SSL) {
          if (SSL_shutdown(ssl) != 1) {
            return Action::Abort;
          }
          SSL_LOG("SSL_shutdown received");
          break;
        }
        SSL_LOG("SSL_read error: " << errorToString(ssl, ret));
        return Action::Abort;
      }
      /*
       * Notify the delegate.
       */
      if (delegate.onNewData(id, cookie, in, ret) != Action::Continue) {
        return Action::Abort;
      }
    } while (ret > 0);
    /*
     * Continue processing.
     */
    return Action::Continue;
  }

  /**
   * Processing incoming data and encrypt the response.
   */
  template<typename ID>
  Action onNewData(ID const& id, interface::Delegate<ID>& delegate,
                   const uint8_t* const data, const uint32_t len,
                   const uint32_t alen, uint8_t* const sdata, uint32_t& slen)
  {
    /*
     * Write the data in the input BIO.
     */
    BIO_write(bin, data, len);
    /*
     * Check the connection's state.
     */
    switch (state) {
      /*
       * Closed is not a valid state.
       */
      case State::Closed: {
        return Action::Abort;
      }
      /*
       * Handle the SSL handshake.
       */
      case State::Connect: {
        int e = SSL_connect(ssl);
        switch (e) {
          case 0: {
            SSL_LOG("SSL_connect error, controlled shutdown");
            return Action::Abort;
          }
          case 1: {
            SSL_LOG("SSL_connect successful");
            state = State::Ready;
            return flush(alen, sdata, slen);
          }
          default: {
            if (SSL_get_error(ssl, e) == SSL_ERROR_WANT_READ) {
              return flush(alen, sdata, slen);
            }
            SSL_LOG("SSL_connect error: " << errorToString(ssl, e));
            return Action::Abort;
          }
        }
#if defined(__GNUC__) && defined(__GNUC_PREREQ)
#if !__GNUC_PREREQ(5, 0)
        break;
#endif
#endif
      }
      /*
       * Process SSL_accept.
       */
      case State::Accept: {
        int e = SSL_accept(ssl);
        switch (e) {
          case 0: {
            SSL_LOG("SSL_accept error: " << errorToString(ssl, e));
            return Action::Abort;
          }
          case 1: {
            SSL_LOG("SSL_accept successful");
            state = State::Ready;
            return flush(alen, sdata, slen);
          }
          default: {
            if (SSL_get_error(ssl, e) == SSL_ERROR_WANT_READ) {
              return flush(alen, sdata, slen);
            }
            SSL_LOG("SSL_accept error: " << errorToString(ssl, e));
            return Action::Abort;
          }
        }
      }
      /*
       * Decrypt and pass the data to the delegate.
       */
      case State::Ready: {
        int ret = 0;
        uint32_t acc = 0;
        uint8_t in[len], out[alen];
        /*
         * Process the internal buffer as long as there is data available.
         */
        do {
          ret = SSL_read(ssl, in, len);
          /*
           * Handle partial data.
           */
          if (ret < 0) {
            if (SSL_get_error(ssl, ret) == SSL_ERROR_WANT_READ) {
              break;
            }
            SSL_LOG("SSL_read error: " << errorToString(ssl, ret));
            return Action::Abort;
          }
          /*
           * Handle shutdown.
           */
          if (ret == 0) {
            int err = SSL_get_error(ssl, ret);
            if (err == SSL_ERROR_ZERO_RETURN || err == SSL_ERROR_SSL) {
              if (SSL_shutdown(ssl) != 1) {
                return Action::Abort;
              }
              SSL_LOG("SSL_shutdown received");
              break;
            }
            SSL_LOG("SSL_read error: " << errorToString(ssl, ret));
            return Action::Abort;
          }
          /*
           * Notify the delegate.
           */
          uint32_t rlen = 0;
          Action res =
            delegate.onNewData(id, cookie, in, ret, alen - acc, out, rlen);
          if (res != Action::Continue) {
            return abortOrClose(res, alen, sdata, slen);
          }
          /*
           * Update the accumulator and encrypt the data.
           */
          if (rlen + acc > alen) {
            rlen = alen - acc;
          }
          acc += rlen;
          SSL_write(ssl, out, rlen);
        } while (ret > 0);
        /*
         * Flush the output.
         */
        return flush(alen, sdata, slen);
      }
      /*
       * Handle the last piece of shutdown.
       */
      case State::Shutdown: {
        if (SSL_shutdown(ssl) == 1) {
          return Action::Close;
        }
        return Action::Abort;
      }
    }
#if defined(__GNUC__) && defined(__GNUC_PREREQ)
#if !__GNUC_PREREQ(5, 0)
    return Action::Continue;
#endif
#endif
  }

  /**
   * Return how much data is pending on the write channel.
   */
  inline size_t pending() { return BIO_ctrl_pending(bout); }

  /**
   * Handle delegate response.
   */
  Action abortOrClose(const Action r, const uint32_t alen, uint8_t* const sdata,
                      uint32_t& slen);

  /**
   * Flush any data pending in the write channel.
   */
  Action flush(const uint32_t alen, uint8_t* const sdata, uint32_t& slen);

  BIO* bin;
  BIO* bout;
  SSL* ssl;
  State state;
  void* cookie;
  bool blocked;
};

}}
