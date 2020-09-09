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

#include <tulips/system/Utils.h>
#include <uspace/client/Connection.h>
#include <uspace/client/State.h>
#include <utils/Basic.h>
#include <linenoise/linenoise.h>
#include <cstring>
#include <map>
#include <string>
#include <tclap/CmdLine.h>

using namespace tulips;
using namespace tools;
using namespace uspace;
using namespace utils;

/*
 * Hint handling.
 */

char*
hints(const char* buf, int* color, int* bold, void* cookie)
{
  std::string e(buf);
  auto* state = reinterpret_cast<utils::State*>(cookie);
  if (state->commands.count(e) != 0) {
    return state->commands[e]->hint(*state, color, bold);
  }
  return nullptr;
}

/*
 * Completion handling.
 */

void
completion(const char* buf, linenoiseCompletions* lc, void* cookie)
{
  std::string e(buf);
  auto* state = reinterpret_cast<utils::State*>(cookie);
  Commands::const_iterator it = state->commands.lower_bound(e);
  while (it != state->commands.end() && it->first.length() >= e.length() &&
         it->first.substr(0, e.length()) == e) {
    linenoiseAddCompletion(lc, it->first.c_str());
    it++;
  }
}

/*
 * Execution control.
 */

void
execute(utils::State& s, std::string const& line)
{
  std::vector<std::string> args;
  tulips::system::utils::split(line, ' ', args);
  if (args.empty()) {
    return;
  }
  if (s.commands.count(args[0]) == 0) {
    std::cout << "Invalid command: " << args[0] << "." << std::endl;
    return;
  }
  s.commands[args[0]]->execute(s, args);
}

/*
 * General main.
 */

int
main(int argc, char** argv)
{
  TCLAP::CmdLine cmdL("TULIPS connector", ' ', "1.0");
  TCLAP::ValueArg<std::string> iffA("I", "interface", "Network interface",
                                    false, "", "INTERFACE", cmdL);
  TCLAP::SwitchArg pcpA("P", "pcap", "Capture packets", cmdL);
  cmdL.parse(argc, argv);
  /*
   * Linenoise.
   */
  linenoiseSetCompletionCallback(completion);
  linenoiseSetHintsCallback(hints);
  linenoiseHistorySetMaxLen(1000);
  /*
   * Commands.
   */
  client::State state(pcpA.isSet());
  basic::populate(state.commands);
  client::connection::populate(state.commands);
  /*
   * Main loop.
   */
  char* line;
  while (state.keep_running && (line = linenoise("> ", &state)) != nullptr) {
    if (strlen(line) > 0) {
      linenoiseHistoryAdd(line);
    }
    execute(state, line);
    free(line);
  }
  /*
   * Clean-up.
   */
  return 0;
}
