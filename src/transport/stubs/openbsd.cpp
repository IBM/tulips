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
#include <tulips/transport/Utils.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <stdint.h>
#include <ifaddrs.h>

#define TRANS_VERBOSE 1

#if TRANS_VERBOSE
#define TRANS_LOG(__args) LOG("TRANS", __args)
#else
#define TRANS_LOG(...) ((void)0)
#endif

namespace {

static bool
getDefaultRoute(tulips::stack::ipv4::Address const& ip,
                tulips::stack::ipv4::Address& dr)
{
  int mib[7];
  size_t needed;
  char *lim, *buf = nullptr, *next;
  struct rt_msghdr* rtm;
  struct sockaddr_in *sin, *adr;
  bool result = false;
  /*
   * Setup MIB parameters.
   */
  mib[0] = CTL_NET;
  mib[1] = PF_ROUTE;
  mib[2] = 0;
  mib[3] = AF_INET;
  mib[4] = NET_RT_FLAGS;
  mib[5] = RTF_GATEWAY;
  mib[6] = getrtable();
  /*
   * Fetch the table entries.
   */
  while (1) {
    if (sysctl(mib, 7, nullptr, &needed, nullptr, 0) == -1) {
      LOG("ARP", "route-sysctl-estimate");
    }
    if (needed == 0) {
      TRANS_LOG("sysctl failed");
      return false;
    }
    if ((buf = (char*)realloc(buf, needed)) == nullptr) {
      LOG("ARP", "malloc");
    }
    if (sysctl(mib, 7, buf, &needed, nullptr, 0) == -1) {
      TRANS_LOG(strerror(errno));
      if (errno == ENOMEM) {
        continue;
      }
    }
    lim = buf + needed;
    break;
  }
  TRANS_LOG("found: " << needed);
  /*
   * Search for a match.
   */
  for (next = buf; next < lim; next += rtm->rtm_msglen) {
    rtm = (struct rt_msghdr*)next;
    if (rtm->rtm_version != RTM_VERSION) {
      continue;
    }
    sin = (struct sockaddr_in*)(next + rtm->rtm_hdrlen);
    adr = sin + 1;
    memcpy(dr.data(), &adr->sin_addr.s_addr, 4);
    result = true;
    break;
  }
  /*
   * Clean-up.
   */
  free(buf);
  return result;
}

}

namespace tulips { namespace transport { namespace utils {

bool
getInterfaceInformation(std::string const& ifn,
                        stack::ethernet::Address& hwaddr, uint32_t& mtu)
{
  /*
   * Get the ethernet address.
   */
  struct ifaddrs *ifap, *ifaptr;
  uint8_t* ptr;
  if (getifaddrs(&ifap) == 0) {
    for (ifaptr = ifap; ifaptr != nullptr; ifaptr = (ifaptr)->ifa_next) {
      if (!strcmp((ifaptr)->ifa_name, ifn.c_str()) &&
          (((ifaptr)->ifa_addr)->sa_family == AF_LINK)) {
        ptr = (uint8_t*)LLADDR((struct sockaddr_dl*)(ifaptr)->ifa_addr);
        hwaddr = stack::ethernet::Address(ptr[0], ptr[1], ptr[2], ptr[3],
                                          ptr[4], ptr[5]);
        break;
      }
    }
    freeifaddrs(ifap);
  } else {
    return false;
  }
  /*
   * Create a dummy socket.
   */
  int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (sock < 0) {
    TRANS_LOG(strerror(errno));
    return false;
  }
  /*
   * Get the device MTU.
   */
  struct ifreq ifreq;
  memcpy(ifreq.ifr_name, ifn.c_str(), IFNAMSIZ);
  if (ioctl(sock, SIOCGIFMTU, &ifreq) < 0) {
    TRANS_LOG(strerror(errno));
    close(sock);
    return false;
  }
  mtu = ifreq.ifr_ifru.ifru_metric;
  /*
   * Clean-up.
   */
  close(sock);
  return true;
}

bool
getInterfaceInformation(std::string const& ifn, stack::ipv4::Address& ipaddr,
                        stack::ipv4::Address& draddr,
                        stack::ipv4::Address& ntmask)
{
  /*
   * Create a dummy socket.
   */
  int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (sock < 0) {
    TRANS_LOG(strerror(errno));
    return false;
  }
  /*
   * Get the IPv4 address.
   */
  struct ifreq ifreq;
  memcpy(ifreq.ifr_name, ifn.c_str(), IFNAMSIZ);
  if (ioctl(sock, SIOCGIFADDR, &ifreq) < 0) {
    TRANS_LOG(strerror(errno));
    close(sock);
    return false;
  }
  memcpy(ipaddr.data(), &ifreq.ifr_ifru.ifru_addr.sa_data[2], 4);
  /*
   * Get the IPv4 default route address.
   */
  if (!getDefaultRoute(ipaddr, draddr)) {
    return false;
  }
  /*
   * Get the IPv4 netmask.
   */
  memcpy(ifreq.ifr_name, ifn.c_str(), IFNAMSIZ);
  if (ioctl(sock, SIOCGIFNETMASK, &ifreq) < 0) {
    TRANS_LOG(strerror(errno));
    close(sock);
    return false;
  }
  memcpy(ntmask.data(), &ifreq.ifr_ifru.ifru_addr.sa_data[2], 4);
  /*
   * Clean-up.
   */
  return true;
}

}}}
