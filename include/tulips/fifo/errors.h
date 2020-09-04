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

#ifndef TULIPS_ERRORS_H_
#define TULIPS_ERRORS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum __tulips_fifo_error
{
  TULIPS_FIFO_NO = 0xFF,
  TULIPS_FIFO_YES = 0x00,
  TULIPS_FIFO_OK = 0x00,
  TULIPS_FIFO_IS_NULL = 0x01,
  TULIPS_FIFO_ALREADY_ALLOCATED = 0x02,
  TULIPS_FIFO_INVALID_DEPTH = 0x03,
  TULIPS_FIFO_INVALID_DATA_LEN = 0x04,
  TULIPS_FIFO_MALLOC_FAILED = 0x05,
  TULIPS_FIFO_EMPTY = 0x06,
  TULIPS_FIFO_FULL = 0x07,
  TULIPS_FIFO_NO_SPACE_LEFT = 0x08,
  TULIPS_FIFO_NO_PENDING_PUSH = 0x09
} tulips_fifo_error_t;

#ifdef __cplusplus
}
#endif

#endif // TULIPS_ERRORS_H_
