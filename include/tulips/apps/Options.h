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

#include <tclap/CmdLine.h>
#include <cstdint>

namespace tulips { namespace apps {

class Options
{
public:
  Options(TCLAP::CmdLine& cmd);

  bool isSane() const;

  int usDelay() const { return usd.getValue(); }
  bool noDelay() const { return nag.isSet(); }
  bool isSender() const { return snd.isSet(); }
  std::string linkAddress() const { return lla.getValue(); }
  std::string source() const { return src.getValue(); }
  std::string route() const { return rte.getValue(); }
  std::string mask() const { return msk.getValue(); }
  std::string destination() const { return dst.getValue(); }
  bool dumpPackets() const { return pcp.isSet(); }
  size_t interval() const { return dly.getValue(); }
  bool hasInterface() const { return iff.isSet(); }
  std::string interface() const { return iff.getValue(); }
  uint16_t port() const { return prt.getValue()[0]; }
  std::vector<uint16_t> ports() const { return prt.getValue(); }
  size_t connections() const { return con.getValue(); }
  bool wait() const { return wai.isSet(); }
  size_t length() const { return len.getValue(); }
  size_t count() const { return cnt.getValue(); }
  bool withSSL() const { return ssl.isSet(); }
  std::string sslCert() const { return crt.getValue(); }
  std::string sslKey() const { return key.getValue(); }
  long cpuId() const { return cpu.getValue(); }

private:
  TCLAP::ValueArg<int> usd;
  TCLAP::SwitchArg nag;
  TCLAP::SwitchArg snd;
  TCLAP::ValueArg<std::string> lla;
  TCLAP::ValueArg<std::string> src;
  TCLAP::ValueArg<std::string> rte;
  TCLAP::ValueArg<std::string> msk;
  TCLAP::ValueArg<std::string> dst;
  TCLAP::SwitchArg pcp;
  TCLAP::ValueArg<size_t> dly;
  TCLAP::ValueArg<std::string> iff;
  TCLAP::MultiArg<uint16_t> prt;
  TCLAP::ValueArg<size_t> con;
  TCLAP::SwitchArg wai;
  TCLAP::ValueArg<size_t> len;
  TCLAP::ValueArg<size_t> cnt;
  TCLAP::SwitchArg ssl;
  TCLAP::ValueArg<std::string> crt;
  TCLAP::ValueArg<std::string> key;
  TCLAP::ValueArg<long> cpu;
};

}}
