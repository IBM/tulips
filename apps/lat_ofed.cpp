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
#include <tulips/apps/TCPLatency.h>
#include <tulips/transport/ofed/Device.h>
#include <tclap/CmdLine.h>

using namespace tulips;
using namespace apps::tcplatency;
using namespace transport;

int
main(int argc, char** argv)
{
  TCLAP::CmdLine cmd("TULIPS OFED Test", ' ', "1.0");
  apps::Options opts(cmd);
  cmd.parse(argc, argv);
  /*
   * Make sure the options are sane.
   */
  if (!opts.isSane()) {
    return __LINE__;
  }
  /*
   * Create an OFED device.
   */
  ofed::Device* device = nullptr;
  if (opts.hasInterface()) {
    device = new ofed::Device(opts.interface(), 1024);
  } else {
    device = new ofed::Device(1024);
  }
  /*
   * Call the main function.
   */
  int res = opts.isSender() ? Client::run(opts, *device)
                            : Server::run(opts, *device);
  /*
   * Clean-up.
   */
  delete device;
  return res;
}
