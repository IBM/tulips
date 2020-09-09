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

#include <tulips/fifo/fifo.h>
#ifdef __linux__
#include <malloc.h>
#endif
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

tulips_fifo_error_t
tulips_fifo_create(const size_t depth, const size_t dlen,
                   tulips_fifo_t* const res)
{
  if (depth == 0) {
    return TULIPS_FIFO_INVALID_DEPTH;
  }
  if (dlen == 0) {
    return TULIPS_FIFO_INVALID_DATA_LEN;
  }
  if (*res != NULL) {
    return TULIPS_FIFO_ALREADY_ALLOCATED;
  }
  size_t payload = depth * dlen + sizeof(struct __tulips_fifo);
  void* data = malloc(payload);
  if (data == NULL) {
    return TULIPS_FIFO_MALLOC_FAILED;
  }
  memset(data, 0, payload);
  *res = (tulips_fifo_t)data;
  (*res)->depth = depth;
  (*res)->data_len = dlen;
  return TULIPS_FIFO_OK;
}
