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

#include <tulips/system/Compiler.h>
#include <utils/Basic.h>
#include <utils/State.h>
#include <iomanip>
#include <iostream>

namespace tulips { namespace tools { namespace utils { namespace basic {

/*
 * Quit.
 */

class Help : public Command
{
public:
  Help() : Command("print this help") {}

  void help(UNUSED Arguments const& args) override
  {
    std::cout << "Print this help." << std::endl;
  }

  void execute(State& s, Arguments const& args) override
  {
    if (args.size() == 1) {
      Commands::const_iterator it;
      /*
       * Get the size of the larges item.
       */
      size_t l = 0;
      for (it = s.commands.begin(); it != s.commands.end(); it++) {
        l = it->first.length() > l ? it->first.length() : l;
      }
      /*
       * Display the commands.
       */
      for (it = s.commands.begin(); it != s.commands.end(); it++) {
        std::cout << std::setw((int)l + 1) << std::left << it->first << "-- "
                  << it->second->about() << std::endl;
      }
    } else if (s.commands.count(args[1]) == 0) {
      std::cout << "Invalid command: " << args[1] << std::endl;
    } else {
      s.commands[args[1]]->help(args);
    }
  }
};

/*
 * Quit.
 */

class Quit : public Command
{
public:
  Quit() : Command("leave the tool") {}

  void help(UNUSED Arguments const& args) override
  {
    std::cout << "Leave the client." << std::endl;
  }

  void execute(State& s, UNUSED Arguments const& args) override
  {
    s.keep_running = false;
  }
};

/*
 * Helpers.
 */

void
populate(Commands& cmds)
{
  cmds["help"] = new Help;
  cmds["quit"] = new Quit;
}

}}}}
