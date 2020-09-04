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

#ifndef TULIPS_FIFO_H_
#define TULIPS_FIFO_H_

#ifdef __cplusplus
extern "C" {
#define restrict __restrict
#endif

#include <tulips/fifo/errors.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define TULIPS_FIFO_DEFAULT_VALUE NULL

typedef struct __tulips_fifo
{
  size_t depth;
  size_t data_len;
  uint64_t prepare_count;
  volatile uint64_t write_count;
  volatile uint64_t read_count;
  uint8_t data[];
} * restrict tulips_fifo_t;

/**
 * Inline methods
 */

static inline tulips_fifo_error_t
tulips_fifo_empty(tulips_fifo_t const fifo)
{
#ifdef TULIPS_FIFO_RUNTIME_CHECKS
  if (fifo == TULIPS_FIFO_DEFAULT_VALUE) {
    return TULIPS_FIFO_IS_NULL;
  }
#endif
  if (fifo->read_count == fifo->write_count) {
    return TULIPS_FIFO_YES;
  }
  return TULIPS_FIFO_NO;
}

static inline tulips_fifo_error_t
tulips_fifo_full(tulips_fifo_t const fifo)
{
#ifdef TULIPS_FIFO_RUNTIME_CHECKS
  if (fifo == TULIPS_FIFO_DEFAULT_VALUE) {
    return TULIPS_FIFO_IS_NULL;
  }
#endif
  if (fifo->write_count - fifo->read_count == fifo->depth) {
    return TULIPS_FIFO_YES;
  }
  return TULIPS_FIFO_NO;
}

static inline tulips_fifo_error_t
tulips_fifo_must_prepare(tulips_fifo_t const fifo)
{
#ifdef TULIPS_FIFO_RUNTIME_CHECKS
  if (fifo == TULIPS_FIFO_DEFAULT_VALUE) {
    return TULIPS_FIFO_IS_NULL;
  }
#endif
  if (fifo->prepare_count == fifo->write_count) {
    return TULIPS_FIFO_YES;
  }
  return TULIPS_FIFO_NO;
}

static inline tulips_fifo_error_t
tulips_fifo_must_commit(tulips_fifo_t const fifo)
{
#ifdef TULIPS_FIFO_RUNTIME_CHECKS
  if (fifo == TULIPS_FIFO_DEFAULT_VALUE) {
    return TULIPS_FIFO_IS_NULL;
  }
#endif
  if (fifo->prepare_count - fifo->read_count == fifo->depth) {
    return TULIPS_FIFO_YES;
  }
  return TULIPS_FIFO_NO;
}

static inline tulips_fifo_error_t
tulips_fifo_front(tulips_fifo_t const fifo, void** const data)
{
#ifdef TULIPS_FIFO_RUNTIME_CHECKS
  if (fifo == TULIPS_FIFO_DEFAULT_VALUE) {
    return TULIPS_FIFO_IS_NULL;
  }
#endif
  if (tulips_fifo_empty(fifo) == TULIPS_FIFO_YES) {
    return TULIPS_FIFO_EMPTY;
  }
  size_t index = fifo->read_count % fifo->depth;
  *data = fifo->data + index * fifo->data_len;
  return TULIPS_FIFO_OK;
}

static inline tulips_fifo_error_t
tulips_fifo_push(tulips_fifo_t const fifo, const void* restrict const data)
{
#ifdef TULIPS_FIFO_RUNTIME_CHECKS
  if (fifo == TULIPS_FIFO_DEFAULT_VALUE) {
    return TULIPS_FIFO_IS_NULL;
  }
#endif
  if (tulips_fifo_full(fifo) == TULIPS_FIFO_YES) {
    return TULIPS_FIFO_FULL;
  }
  size_t index = fifo->write_count % fifo->depth;
  void* result = fifo->data + index * fifo->data_len;
  memcpy(result, data, fifo->data_len);
  fifo->write_count += 1;
  return TULIPS_FIFO_OK;
}

static inline tulips_fifo_error_t
tulips_fifo_pop(tulips_fifo_t const fifo)
{
#ifdef TULIPS_FIFO_RUNTIME_CHECKS
  if (fifo == TULIPS_FIFO_DEFAULT_VALUE) {
    return TULIPS_FIFO_IS_NULL;
  }
#endif
  if (tulips_fifo_empty(fifo) == TULIPS_FIFO_YES) {
    return TULIPS_FIFO_EMPTY;
  }
  fifo->read_count += 1;
  return TULIPS_FIFO_OK;
}

static inline tulips_fifo_error_t
tulips_fifo_prepare(tulips_fifo_t const fifo, void** const data)
{
#ifdef TULIPS_FIFO_RUNTIME_CHECKS
  if (fifo == TULIPS_FIFO_DEFAULT_VALUE) {
    return TULIPS_FIFO_IS_NULL;
  }
#endif
  if (tulips_fifo_must_commit(fifo) == TULIPS_FIFO_YES) {
    return TULIPS_FIFO_NO_SPACE_LEFT;
  }
  size_t index = fifo->prepare_count % fifo->depth;
  *data = fifo->data + index * fifo->data_len;
  fifo->prepare_count += 1;
  return TULIPS_FIFO_OK;
}

static inline tulips_fifo_error_t
tulips_fifo_commit(tulips_fifo_t const fifo)
{
#ifdef TULIPS_FIFO_RUNTIME_CHECKS
  if (fifo == TULIPS_FIFO_DEFAULT_VALUE) {
    return TULIPS_FIFO_IS_NULL;
  }
#endif
  if (tulips_fifo_must_prepare(fifo) == TULIPS_FIFO_YES) {
    return TULIPS_FIFO_NO_PENDING_PUSH;
  }
  fifo->write_count += 1;
  return TULIPS_FIFO_OK;
}

/**
 * Other methods
 */

tulips_fifo_error_t tulips_fifo_create(const size_t depth, const size_t dlen,
                                       tulips_fifo_t* const res);

tulips_fifo_error_t tulips_fifo_destroy(tulips_fifo_t* const fifo);

#ifdef __cplusplus
}
#endif

#endif // TULIPS_FIFO_H_

/* vim: set ft=c */
