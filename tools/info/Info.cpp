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

#include <tulips/stack/Ethernet.h>
#include <tulips/stack/IPv4.h>
#include <tulips/stack/TCPv4.h>
#include <tulips/system/Compiler.h>
#include <iostream>

using namespace std;
using namespace tulips;
using namespace stack;

int
main(int UNUSED argc, UNUSED char** argv)
{
  /*
   * Stack information.
   */
  cout << "ethernet header size is: " << ethernet::HEADER_LEN << "B" << endl;
  cout << "ipv4 header size is: " << ipv4::HEADER_LEN << "B" << endl;
  cout << "tcpv4 header size is: " << tcpv4::HEADER_LEN << "B" << endl;
  cout << "header overhead is: " << tcpv4::HEADER_OVERHEAD << "B" << endl;
  /*
   * MTU references.
   */
  size_t max_1500 = 1500 - ipv4::HEADER_LEN - tcpv4::HEADER_LEN;
  size_t max_9000 = 9000 - ipv4::HEADER_LEN - tcpv4::HEADER_LEN;
  cout << "1500B MTU max payload is: " << max_1500 << "B" << endl;
  cout << "9000B MTU max payload is: " << max_9000 << "B" << endl;
  return 0;
}
