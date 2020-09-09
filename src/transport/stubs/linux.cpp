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

#include <tulips/stack/Utils.h>
#include <tulips/system/Utils.h>
#include <tulips/transport/Utils.h>
#include <array>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ether.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#define TRANS_VERBOSE 1

#if TRANS_VERBOSE
#define TRANS_LOG(__args) LOG("TRANS", __args)
#else
#define TRANS_LOG(...) ((void)0)
#endif

namespace {

bool
getGateway(std::string const& dev, tulips::stack::ipv4::Address& gw)
{
  std::ifstream route("/proc/net/route");
  int line_count = 0;
  /*
   * Check if the file is valid.
   */
  if (route.bad()) {
    TRANS_LOG("cannot open /proc/net/route");
    return false;
  }
  /*
   * Look for the gateway matching the interface.
   */
  while (!route.eof()) {
    /*
     * Read a line from that file.
     */
    std::string line;
    std::vector<std::string> parts;
    std::getline(route, line);
    line_count += 1;
    /*
     * Skip the headers.
     */
    if (line_count <= 2) {
      continue;
    }
    /*
     * Parse the input.
     */
    tulips::system::utils::split(line, '\t', parts);
    if (parts[0] == dev) {
      union
      {
        std::array<uint8_t, 4> b;
        uint32_t a;
      } __attribute__((packed));
      std::istringstream(parts[2]) >> std::hex >> a;
      gw = tulips::stack::ipv4::Address(b[0], b[1], b[2], b[3]);
      route.close();
      return true;
    }
  }
  route.close();
  return false;
}

}

namespace tulips { namespace transport { namespace utils {

bool
getInterfaceInformation(std::string const& ifn,
                        stack::ethernet::Address& hwaddr, uint32_t& mtu)
{
  /*
   * Create a dummy socket.
   */
  int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (sock < 0) {
    return false;
  }
  /*
   * Get the ethernet address.
   */
  struct ifreq req;
  memcpy(req.ifr_name, ifn.c_str(), IFNAMSIZ);
  if (ioctl(sock, SIOCGIFHWADDR, &req) < 0) {
    close(sock);
    return false;
  }
  memcpy(hwaddr.data(), req.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
  /*
   * Get the device MTU.
   */
  memcpy(req.ifr_name, ifn.c_str(), IFNAMSIZ);
  if (ioctl(sock, SIOCGIFMTU, &req) < 0) {
    close(sock);
    return false;
  }
  mtu = req.ifr_ifru.ifru_mtu;
  /**
   * Clean-up.
   */
  close(sock);
  return true;
}

bool
getInterfaceInformation(std::string const& ifn, stack::ipv4::Address& ipaddr,
                        stack::ipv4::Address& ntmask,
                        stack::ipv4::Address& draddr)
{
  /*
   * Create a dummy socket.
   */
  int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (sock < 0) {
    return false;
  }
  /*
   * Get the IPv4 address.
   */
  struct ifreq req;
  memcpy(req.ifr_name, ifn.c_str(), IFNAMSIZ);
  if (ioctl(sock, SIOCGIFADDR, &req) < 0) {
    close(sock);
    return false;
  }
  memcpy(ipaddr.data(), &req.ifr_addr.sa_data[2], 4);
  /*
   * Get the IPv4 netmask.
   */
  memcpy(req.ifr_name, ifn.c_str(), IFNAMSIZ);
  if (ioctl(sock, SIOCGIFNETMASK, &req) < 0) {
    close(sock);
    return false;
  }
  memcpy(ntmask.data(), &req.ifr_netmask.sa_data[2], 4);
  /*
   * Get the IPv4 gateway address.
   */
  if (!getGateway(ifn, draddr)) {
    close(sock);
    return false;
  }
  /*
   * Clean-up.
   */
  close(sock);
  return true;
}

}}}
