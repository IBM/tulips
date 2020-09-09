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

#include <tulips/apps/Options.h>
#include <cstdint>

namespace tulips { namespace apps {

Options::Options(TCLAP::CmdLine& cmd)
  : usd("u", "us", "uS delay between sends", false, 1000, "DELAY", cmd)
  , nag("N", "nodelay", "Disable Nagle's algorithm", cmd)
  , snd("s", "sender", "Sender mode", cmd)
  , lla("L", "lladdr", "Link address", true, "", "LLADDR", cmd)
  , src("S", "source", "Local IPv4 address", true, "", "IPv4", cmd)
  , rte("R", "route", "Default route", true, "", "IPv4", cmd)
  , msk("M", "netmask", "Local netmask", false, "255.255.255.0", "IPv4", cmd)
  , dst("D", "destination", "Remote IPv4 address", false, "", "IPv4", cmd)
  , pcp("P", "pcap", "Dump packets", cmd)
  , dly("i", "interval", "Statistics interval", false, 10, "INTERVAL", cmd)
  , iff("I", "interface", "Network interface", false, "", "INTERFACE", cmd)
  , prt("p", "port", "Port to listen/connect to", false, "PORT", cmd)
  , con("n", "nconn", "Server connections", false, 16, "NCONNS", cmd)
  , wai("w", "wait", "Wait instead of poll", cmd)
  , len("l", "length", "Payload length", false, 8, "LEN", cmd)
  , cnt("c", "count", "Send count", false, 0, "COUNT", cmd)
  , ssl("", "ssl", "Use OpenSSL", cmd)
  , crt("", "cert", "SSL certificate", false, "", "PEM", cmd)
  , key("", "key", "SSL private key", false, "", "PEM", cmd)
  , cpu("", "cpu", "CPU affinity", false, -1, "CPUID", cmd)
{}

bool
Options::isSane() const
{
  if (snd.isSet() && !dst.isSet()) {
    std::cerr << "Remote IPv4 address must be set" << std::endl;
    return false;
  }
  if (prt.getValue().empty()) {
    std::cerr << "Port list cannot be empty" << std::endl;
    return false;
  }
  return true;
}

}}
