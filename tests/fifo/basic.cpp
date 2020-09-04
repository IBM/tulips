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

#include <cassert>
#include <tulips/fifo/fifo.h>
#include <gtest/gtest.h>

namespace {

constexpr size_t ITERATIONS = 1000;

void*
reader_thread(void* arg)
{
  auto fifo = reinterpret_cast<tulips_fifo_t>(arg);
  uint64_t data = 1;
  void* result = nullptr;
  tulips_fifo_error_t error;
  for (size_t i = 0; i < ITERATIONS; i += 1) {
    do {
      error = tulips_fifo_front(fifo, &result);
    } while (error != TULIPS_FIFO_OK);
    assert(data == *(uint64_t*)result);
    error = tulips_fifo_pop(fifo);
    assert(TULIPS_FIFO_OK == error);
    data += 1;
  }
  return nullptr;
}

void*
writer_thread(void* arg)
{
  auto fifo = reinterpret_cast<tulips_fifo_t>(arg);
  uint64_t data = 1;
  tulips_fifo_error_t error;
  for (size_t i = 0; i < ITERATIONS; i += 1) {
    do {
      error = tulips_fifo_push(fifo, &data);
    } while (error != TULIPS_FIFO_OK);
    data += 1;
  }
  return nullptr;
}

} // namespace

TEST(FIFO_Basic, CreateAndDestroy)
{
  tulips_fifo_t fifo = TULIPS_FIFO_DEFAULT_VALUE;
  tulips_fifo_error_t error;
  /**
   * Invalid depth
   */
  error = tulips_fifo_create(0, 0, &fifo);
  ASSERT_EQ(TULIPS_FIFO_INVALID_DEPTH, error);
  /**
   * Invalid data length
   */
  error = tulips_fifo_create(16, 0, &fifo);
  ASSERT_EQ(TULIPS_FIFO_INVALID_DATA_LEN, error);
  /**
   * Create success
   */
  error = tulips_fifo_create(16, 16, &fifo);
  ASSERT_EQ(TULIPS_FIFO_OK, error);
  /**
   * State
   */
  error = tulips_fifo_empty(fifo);
  ASSERT_EQ(TULIPS_FIFO_YES, error);
  error = tulips_fifo_full(fifo);
  ASSERT_EQ(TULIPS_FIFO_NO, error);
  /**
   * Already allocated
   */
  error = tulips_fifo_create(16, 16, &fifo);
  ASSERT_EQ(TULIPS_FIFO_ALREADY_ALLOCATED, error);
  /**
   * Destroy success
   */
  error = tulips_fifo_destroy(&fifo);
  ASSERT_EQ(TULIPS_FIFO_OK, error);
  /**
   * Already deallocated
   */
  error = tulips_fifo_destroy(&fifo);
  ASSERT_EQ(TULIPS_FIFO_IS_NULL, error);
}

TEST(FIFO_Basic, ReadWrite)
{
  tulips_fifo_t fifo = TULIPS_FIFO_DEFAULT_VALUE;
  tulips_fifo_error_t error;
  /**
   * Create success
   */
  error = tulips_fifo_create(16, 16, &fifo);
  ASSERT_EQ(TULIPS_FIFO_OK, error);
  /**
   * Front failure
   */
  void* result = nullptr;
  error = tulips_fifo_front(fifo, &result);
  ASSERT_EQ(TULIPS_FIFO_EMPTY, error);
  /**
   * Pop failure
   */
  error = tulips_fifo_pop(fifo);
  ASSERT_EQ(TULIPS_FIFO_EMPTY, error);
  /**
   * Push success
   */
  const char* data = "hi to the world!";
  error = tulips_fifo_push(fifo, data);
  ASSERT_EQ(TULIPS_FIFO_OK, error);
  /*
   * Empty error
   */
  error = tulips_fifo_empty(fifo);
  ASSERT_EQ(TULIPS_FIFO_NO, error);
  /*
   * Full error
   */
  error = tulips_fifo_full(fifo);
  ASSERT_EQ(TULIPS_FIFO_NO, error);
  /**
   * Front success
   */
  error = tulips_fifo_front(fifo, &result);
  ASSERT_EQ(TULIPS_FIFO_OK, error);
  ASSERT_EQ(0, memcmp(result, data, 16));
  /**
   * Pop success
   */
  error = tulips_fifo_pop(fifo);
  ASSERT_EQ(TULIPS_FIFO_OK, error);
  /*
   * Empty success
   */
  error = tulips_fifo_empty(fifo);
  ASSERT_EQ(TULIPS_FIFO_OK, error);
  /**
   * Destroy success
   */
  error = tulips_fifo_destroy(&fifo);
  ASSERT_EQ(TULIPS_FIFO_OK, error);
}

TEST(FIFO_Basic, FullEmpty)
{
  tulips_fifo_t fifo = TULIPS_FIFO_DEFAULT_VALUE;
  tulips_fifo_error_t error;
  /**
   * Create success
   */
  error = tulips_fifo_create(16, 16, &fifo);
  ASSERT_EQ(TULIPS_FIFO_OK, error);
  /**
   * Push success
   */
  const char* data = "hi to the world!";
  for (int i = 0; i < 16; i += 1) {
    error = tulips_fifo_push(fifo, data);
    ASSERT_EQ(TULIPS_FIFO_OK, error);
  }
  /**
   * Push failure
   */
  error = tulips_fifo_push(fifo, data);
  ASSERT_EQ(TULIPS_FIFO_FULL, error);
  /**
   * Front and pop success
   */
  void* result = nullptr;
  for (int i = 0; i < 16; i += 1) {
    error = tulips_fifo_front(fifo, &result);
    ASSERT_EQ(TULIPS_FIFO_OK, error);
    ASSERT_EQ(0, memcmp(result, data, 16));
    error = tulips_fifo_pop(fifo);
    ASSERT_EQ(TULIPS_FIFO_OK, error);
  }
  /**
   * Front and pop error
   */
  error = tulips_fifo_front(fifo, &result);
  ASSERT_EQ(TULIPS_FIFO_EMPTY, error);
  ASSERT_EQ(0, memcmp(result, data, 16));
  error = tulips_fifo_pop(fifo);
  ASSERT_EQ(TULIPS_FIFO_EMPTY, error);
  /*
   * Empty success
   */
  error = tulips_fifo_empty(fifo);
  ASSERT_EQ(TULIPS_FIFO_OK, error);
  /**
   * Destroy success
   */
  error = tulips_fifo_destroy(&fifo);
  ASSERT_EQ(TULIPS_FIFO_OK, error);
}

TEST(FIFO_Basic, MultiThread)
{
  tulips_fifo_t fifo = TULIPS_FIFO_DEFAULT_VALUE;
  tulips_fifo_error_t error;
  /**
   * Create success
   */
  error = tulips_fifo_create(16, sizeof(uint64_t), &fifo);
  ASSERT_EQ(TULIPS_FIFO_OK, error);
  /*
   * Start the threads
   */
  pthread_t t0, t1;
  pthread_create(&t0, nullptr, reader_thread, fifo);
  pthread_create(&t1, nullptr, writer_thread, fifo);
  /*
   * Join the threads
   */
  pthread_join(t0, nullptr);
  pthread_join(t1, nullptr);
  /*
   * Empty success
   */
  error = tulips_fifo_empty(fifo);
  ASSERT_EQ(TULIPS_FIFO_OK, error);
  /**
   * Destroy success
   */
  error = tulips_fifo_destroy(&fifo);
  ASSERT_EQ(TULIPS_FIFO_OK, error);
}
