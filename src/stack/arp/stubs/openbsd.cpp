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
#include <tulips/system/Utils.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>

#define ARP_VERBOSE 1

#if ARP_VERBOSE
#define ARP_LOG(__args) LOG("ARP", __args)
#else
#define ARP_LOG(...)
#endif

namespace tulips { namespace stack { namespace arp { namespace stub {

static bool
getinetaddr(std::string const& host, struct in_addr* inap)
{
  struct hostent* hp;
  /*
   * Loop-up the host address.
   */
  if (inet_aton(host.c_str(), inap) == 1) {
    ARP_LOG("inet_aton for " << host << " succeeded");
    return true;
  }
  if ((hp = gethostbyname(host.c_str())) == nullptr) {
    ARP_LOG("gethostbyname for " << host << " failed");
    return false;
  }
  /*
   * Copy the result.
   */
  memcpy(inap, hp->h_addr, sizeof(*inap));
  return true;
}

static bool
search(in_addr_t const& addr, ethernet::Address& lladdr)
{
  int mib[7];
  size_t needed;
  char *lim, *buf = nullptr, *next;
  struct rt_msghdr* rtm;
  struct sockaddr_inarp* sin;
  struct sockaddr_dl* sdl;
  bool result = false;
  /*
   * Setup MIB parameters.
   */
  mib[0] = CTL_NET;
  mib[1] = PF_ROUTE;
  mib[2] = 0;
  mib[3] = AF_INET;
  mib[4] = NET_RT_FLAGS;
  mib[5] = RTF_LLINFO;
  mib[6] = getrtable();
  /*
   * Fetch the table entries.
   */
  while (1) {
    if (sysctl(mib, 7, nullptr, &needed, nullptr, 0) == -1) {
      LOG("ARP", "route-sysctl-estimate");
    }
    if (needed == 0) {
      ARP_LOG("sysctl failed");
      return false;
    }
    if ((buf = (char*)realloc(buf, needed)) == nullptr) {
      LOG("ARP", "malloc");
    }
    if (sysctl(mib, 7, buf, &needed, nullptr, 0) == -1) {
      ARP_LOG(strerror(errno));
      if (errno == ENOMEM) {
        continue;
      }
      LOG("ARP", "actual retrieval of routing table");
    }
    lim = buf + needed;
    break;
  }
  ARP_LOG("found: " << needed);
  /*
   * Search for a match.
   */
  for (next = buf; next < lim; next += rtm->rtm_msglen) {
    rtm = (struct rt_msghdr*)next;
    if (rtm->rtm_version != RTM_VERSION) {
      continue;
    }
    sin = (struct sockaddr_inarp*)(next + rtm->rtm_hdrlen);
    sdl = (struct sockaddr_dl*)(sin + 1);
    if (addr != sin->sin_addr.s_addr) {
      ARP_LOG("skipping " << inet_ntoa(sin->sin_addr));
      continue;
    }
    u_char* cp = (u_char*)LLADDR(sdl);
    lladdr = ethernet::Address(cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
    result = true;
    break;
  }
  /*
   * Clean-up.
   */
  free(buf);
  return result;
}

static bool
get(std::string const& host, ethernet::Address& addr)
{
  struct sockaddr_inarp sin = { sizeof(sin), AF_INET, 0, { 0 }, { 0 }, 0, 0 };
  return getinetaddr(host, &sin.sin_addr) && search(sin.sin_addr.s_addr, addr);
}

bool
lookup(std::string const& eth, ipv4::Address const& ip, ethernet::Address& hw)
{
  return get(ip.toString(), hw);
}

}}}}
