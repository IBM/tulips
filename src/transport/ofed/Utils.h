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

#include <tulips/transport/ofed/Device.h>
#include <string>

#define PRINT_EXP_CAP(__flags, __cap)                                          \
  LOG("OFED",                                                                  \
      #__cap << " = " << std::boolalpha                                        \
             << (bool)(((__flags).exp_device_cap_flags & (__cap)) == (__cap)))

#define PRINT_WC_FLAG(__wc, __flag)                                            \
  LOG("OFED", #__flag << " = " << std::boolalpha                               \
                      << (bool)(((__wc).exp_wc_flags & (__flag)) == (__flag)))

#define HAS_TSO(__caps)                                                        \
  ((__caps).max_tso > 0 && ((__caps).supported_qpts | IBV_EXP_QPT_RAW_PACKET))

bool getInterfaceDeviceAndPortIds(std::string const& ifn, std::string& name,
                                  int& portid);
bool isSupportedDevice(std::string const& ifn);

bool findSupportedInterface(std::string& ifn);

void setup(ibv_context* context, ibv_pd* pd, const uint8_t port,
           const uint16_t nbuf, const size_t sndlen, const size_t rcvlen,
           ibv_comp_channel*& comp, ibv_cq*& sendcq, ibv_cq*& recvcq,
           ibv_qp*& qp, uint8_t*& sendbuf, uint8_t*& recvbuf, ibv_mr*& sendmr,
           ibv_mr*& recvmr);
