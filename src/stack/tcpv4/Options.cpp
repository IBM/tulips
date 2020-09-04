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

#include <tulips/stack/tcpv4/Options.h>
#include <tulips/stack/tcpv4/Connection.h>
#include <tulips/system/Utils.h>
#include <cstdint>
#include <ostream>
#include <arpa/inet.h>

#define OPT_VERBOSE 1

#if OPT_VERBOSE
#define OPT_LOG(__args) LOG("TCP", __args)
#else
#define OPT_LOG(...) ((void)0)
#endif

namespace tulips { namespace stack { namespace tcpv4 { namespace Options {

void
parse(Connection& e, const uint16_t len, const uint8_t* const data)
{
  /*
   * Get the number of options bytes
   */
  const uint8_t* options = &data[HEADER_LEN];
  /*
   * Parse the options
   */
  for (int c = 0; c < len;) {
    uint8_t opt = options[c];
    /*
     * End of options.
     */
    if (opt == END) {
      break;
    }
    /*
     * NOP option.
     */
    else if (opt == NOOP) {
      c += 1;
    }
    /*
     * An MSS option with the right option length.
     */
    else if (opt == MSS && options[c + 1] == MSS_LEN) {
      uint16_t omss = ntohs(*(uint16_t*)&options[c + 2]);
      c += MSS_LEN;
      /*
       * An MSS option with the right option length.
       */
      uint16_t nmss = omss > e.m_initialmss ? e.m_initialmss : omss;
      OPT_LOG("initial MSS update: " << e.m_initialmss << " -> " << nmss);
      e.m_initialmss = nmss;
    }
    /*
     * A WSC option with the right option length.
     */
    else if (opt == WSC && options[c + 1] == WSC_LEN) {
      uint8_t wsc = options[c + 2];
      c += WSC_LEN;
      /*
       * RFC1323 limits window scaling to 14. SYN and SYN/ACK do not contain a
       * scaled version of the window size. So we just shift it here.
       */
      e.m_wndscl = wsc > 14 ? 14 : wsc;
      e.m_window >>= e.m_wndscl;
    }
    /*
     * All other options have a length field, so that we easily can
     * skip past them.
     */
    else {
      /*
       * All other options have a length field, so that we easily can skip past
       * them.
       */
      if (options[c + 1] == 0) {
        /*
         * If the length field is zero, the options are malformed and we don't
         * process them further.
         */
        break;
      }
      /*
       * Add the option length and check for overrun
       */
      c += options[c + 1];
      if (c >= 40) {
        /*
         * The option length is invalid, we stop processing the options.
         */
        break;
      }
    }
  }
}

}}}}
