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

#include "BIO.h"
#include <tulips/system/CircularBuffer.h>
#include <tulips/system/Compiler.h>

namespace tulips { namespace ssl { namespace bio {

static int
s_write(BIO* h, const char* buf, int size)
{
  auto* b = reinterpret_cast<system::CircularBuffer*>(h->ptr);
  BIO_clear_retry_flags(h);
  if (b->full()) {
    BIO_set_retry_read(h);
    return -1;
  }
  size_t res = b->write((const uint8_t*)buf, size);
  return res;
}

static int
s_read(BIO* h, char* buf, int size)
{
  auto* b = reinterpret_cast<system::CircularBuffer*>(h->ptr);
  BIO_clear_retry_flags(h);
  if (b->empty()) {
    BIO_set_retry_read(h);
    return -1;
  }
  size_t res = b->read((uint8_t*)buf, size);
  return res;
}

static long
s_ctrl(BIO* h, int cmd, long num, UNUSED void* ptr)
{
  long ret = 1;
  /*
   * Grab the buffer.
   */
  auto* b = reinterpret_cast<system::CircularBuffer*>(h->ptr);
  if (b == nullptr) {
    return 0;
  }
  /*
   * Check the command.
   */
  switch (cmd) {
    case BIO_CTRL_RESET:
      b->reset();
      break;
    case BIO_CTRL_EOF:
      ret = (long)b->empty();
      break;
    case BIO_C_SET_BUF_MEM_EOF_RETURN:
      h->num = (int)num;
      break;
    case BIO_CTRL_INFO:
    case BIO_C_SET_BUF_MEM:
    case BIO_C_GET_BUF_MEM_PTR:
      ret = 0;
      break;
    case BIO_CTRL_GET_CLOSE:
      ret = (long)h->shutdown;
      break;
    case BIO_CTRL_SET_CLOSE:
      h->shutdown = (int)num;
      break;
    case BIO_CTRL_WPENDING:
      ret = 0L;
      break;
    case BIO_CTRL_PENDING:
      ret = (long)b->available();
      break;
    case BIO_CTRL_DUP:
    case BIO_CTRL_FLUSH:
      ret = 1;
      break;
    case BIO_CTRL_PUSH:
    case BIO_CTRL_POP:
    default:
      ret = 0;
      break;
  }
  return ret;
}

static int
s_new(BIO* h)
{
  /*
   * Prepare the BIO handler.
   */
  h->shutdown = 0;
  h->init = 1;
  h->num = -1;
  h->ptr = nullptr;
  return 1;
}

static int
s_free(BIO* h)
{
  auto* b = reinterpret_cast<system::CircularBuffer*>(h->ptr);
  if (b == nullptr) {
    return 0;
  }
  if (h->shutdown && h->init) {
    delete b;
    h->ptr = nullptr;
  }
  return 1;
}

static BIO_METHOD s_method = {
  BIO_TYPE_MEM, "circular memory buffer",
  s_write,      s_read,
  nullptr, /* puts */
  nullptr, /* gets */
  s_ctrl,       s_new,
  s_free,       nullptr,
};

BIO_METHOD*
method()
{
  return &s_method;
}

BIO*
allocate(const size_t size)
{
  /*
   * Allocate a BIO.
   */
  BIO* ret = nullptr;
  if (!(ret = BIO_new(method()))) {
    return nullptr;
  }
  /*
   * Allocate a circular buffer.
   */
  auto* b = new system::CircularBuffer(size);
  ret->ptr = (void*)b;
  ret->flags = 0;
  return ret;
}

const uint8_t*
readAt(BIO* h)
{
  auto* b = reinterpret_cast<system::CircularBuffer*>(h->ptr);
  return b->readAt();
}

void
skip(BIO* h, const size_t len)
{
  auto* b = reinterpret_cast<system::CircularBuffer*>(h->ptr);
  return b->skip(len);
}

}}}
