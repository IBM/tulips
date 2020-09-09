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

#include <tulips/api/Status.h>
#include <cstdint>

namespace tulips { namespace transport {

class Producer
{
public:
  virtual ~Producer() = default;

  /**
   * @return the producer's segment size.
   */
  virtual uint32_t mss() const = 0;

  /*
   * Prepare an asynchronous send buffer to use in a future commit. The buffer
   * is at least of the size of mss().
   *
   * @param buf a reference to an uint8_t pointer to hold the new buffer.
   *
   * @return the status of the operation.
   */
  virtual Status prepare(uint8_t*& buf) = 0;

  /*
   * Commit a prepared buffer.
   *
   * @param len the length of the contained data.
   * @param buf the previously prepared buffer.
   * @param mss the mss to use in case of segmentation offload.
   *
   * @return the status of the operation.
   */
  virtual Status commit(const uint32_t len, uint8_t* const buf,
                        const uint16_t mss = 0) = 0;
};

}}
