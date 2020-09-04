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

#include <tulips/api/Status.h>

namespace tulips {

std::string
toString(const Status s)
{
  switch (s) {
    case Status::Ok:
      return "Ok";
    case Status::InvalidArgument:
      return "InvalidArgument";
    case Status::HardwareError:
      return "HardwareError";
    case Status::NoMoreResources:
      return "NoMoreResources";
    case Status::HardwareLinkLost:
      return "HardwareLinkLost";
    case Status::NoDataAvailable:
      return "NoDataAvailable";
    case Status::CorruptedData:
      return "CorruptedData";
    case Status::IncompleteData:
      return "IncompleteData";
    case Status::UnsupportedData:
      return "UnsupportedData";
    case Status::ProtocolError:
      return "ProtocolError";
    case Status::UnsupportedProtocol:
      return "UnsupportedProtocol";
    case Status::HardwareTranslationMissing:
      return "HardwareTranslationMissing";
    case Status::InvalidConnection:
      return "InvalidConnection";
    case Status::NotConnected:
      return "NotConnected";
    case Status::OperationInProgress:
      return "OperationInProgress";
    case Status::OperationCompleted:
      return "OperationCompleted";
    case Status::UnsupportedOperation:
      return "UnsupportedOperation";
  }
#if defined(__GNUC__) && defined(__GNUC_PREREQ)
#if !__GNUC_PREREQ(5, 0)
  return "";
#endif
#endif
}

std::ostream&
operator<<(std::ostream& os, const Status& status)
{
  os << toString(status);
  return os;
}

}
