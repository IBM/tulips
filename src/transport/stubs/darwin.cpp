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
#include <tulips/system/Compiler.h>
#include <tulips/system/Utils.h>
#include <tulips/transport/Utils.h>
#include <cerrno>
#include <string>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/types.h>

#define TRANS_VERBOSE 1

#if TRANS_VERBOSE
#define TRANS_LOG(__args) LOG("TRANS", __args)
#else
#define TRANS_LOG(...) ((void)0)
#endif

namespace {

bool
getDefaultRoute(tulips::stack::ipv4::Address const& UNUSED ip,
                tulips::stack::ipv4::Address& UNUSED dr)
{
  return false;
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
  struct ifreq ifreq = {};
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
getInterfaceInformation(std::string const& ifn,
                        stack::ethernet::Address& UNUSED hwaddr,
                        uint32_t& UNUSED mtu, stack::ipv4::Address& ipaddr,
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
  struct ifreq ifreq = {};
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
