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

#include "Utils.h"
#include <tulips/fifo/errors.h>
#include <tulips/transport/Utils.h>
#include <tulips/stack/Ethernet.h>
#include <tulips/system/Compiler.h>
#include <tulips/system/Utils.h>
#include <csignal>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <fcntl.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/mman.h>

#define OFED_VERBOSE 0
#define OFED_HEXDUMP 0

/*
 * Enable OFED_CAPTSOB to limit the TSO segment to 64KB. This is required to
 * debug large segment with the PCAP transport enabled as IP packets cannot be
 * larger than 64KB.
 */
#define OFED_CAPTSOB 0

#if OFED_VERBOSE
#define OFED_LOG(__args) LOG("OFED", __args)
#else
#define OFED_LOG(...) ((void)0)
#endif

namespace tulips { namespace transport { namespace ofed {

Device::Device(const uint16_t nbuf)
  : transport::Device()
  , m_nbuf(nbuf)
  , m_pending(0)
  , m_port(0)
  , m_context(nullptr)
  , m_address()
  , m_ip()
  , m_dr()
  , m_nm()
  , m_hwmtu(0)
  , m_mtu(0)
  , m_buflen(0)
  , m_pd(nullptr)
  , m_comp(nullptr)
  , m_events(0)
  , m_sendcq(nullptr)
  , m_recvcq(nullptr)
  , m_qp(nullptr)
  , m_sendbuf(nullptr)
  , m_recvbuf(nullptr)
  , m_sendmr(nullptr)
  , m_recvmr(nullptr)
  , m_fifo(TULIPS_FIFO_DEFAULT_VALUE)
  , m_bcast(nullptr)
  , m_flow(nullptr)
  , m_filters()
{
  std::string ifn;
  /*
   * Find a valid interface
   */
  if (!findSupportedInterface(ifn)) {
    throw std::runtime_error("No supported interface found");
  }
  /*
   * Construct the device.
   */
  this->m_name = ifn;
  construct(ifn, nbuf);
}

Device::Device(std::string const& ifn, const uint16_t nbuf)
  : transport::Device(ifn)
  , m_nbuf(nbuf)
  , m_pending(0)
  , m_port(0)
  , m_context(nullptr)
  , m_address()
  , m_ip()
  , m_dr()
  , m_nm()
  , m_hwmtu(0)
  , m_mtu(0)
  , m_buflen(0)
  , m_pd(nullptr)
  , m_comp(nullptr)
  , m_events(0)
  , m_sendcq(nullptr)
  , m_recvcq(nullptr)
  , m_qp(nullptr)
  , m_sendbuf(nullptr)
  , m_recvbuf(nullptr)
  , m_sendmr(nullptr)
  , m_recvmr(nullptr)
  , m_fifo(TULIPS_FIFO_DEFAULT_VALUE)
  , m_bcast(nullptr)
  , m_flow(nullptr)
  , m_filters()
{
  /*
   * Check if the interface driver is mlx?_core.
   */
  if (!isSupportedDevice(ifn)) {
    throw std::runtime_error("Unsupported interface: " + ifn);
  }
  /*
   * Construct the device.
   */
  this->m_name = ifn;
  construct(ifn, nbuf);
}

void
Device::construct(std::string const& ifn, const uint16_t UNUSED nbuf)
{
  int res, ndev = 0;
  ibv_device** devlist = ibv_get_device_list(&ndev);
  /*
   * Check if there are any device available.
   */
  OFED_LOG("using network interface: " << ifn);
  if (ndev == 0) {
    ibv_free_device_list(devlist);
    throw std::runtime_error("No OFED-compatible device found");
  }
  /*
   * Get the device id and the port id.
   */
  std::string devname;
  if (!getInterfaceDeviceAndPortIds(ifn, devname, m_port)) {
    throw std::runtime_error("Cannot get device and port IDs");
  }
  /*
   * Take the one that matches our the provided name.
   */
  ibv_device* device = nullptr;
  for (int i = 0; i < ndev; i += 1) {
    if (devname == devlist[i]->name) {
      device = devlist[i];
      break;
    }
  }
  if (device == nullptr) {
    ibv_free_device_list(devlist);
    throw std::runtime_error("Requested device not found");
  }
  OFED_LOG("name: " << device->name);
  OFED_LOG("dev_name: " << device->dev_name);
  OFED_LOG("dev_path: " << device->dev_path);
  OFED_LOG("ibdev_path: " << device->ibdev_path);
  /*
   * Get lladdr and MTU.
   */
  if (!utils::getInterfaceInformation(ifn, m_address, m_hwmtu)) {
    throw std::runtime_error("Cannot read device's lladdr and mtu");
  }
  OFED_LOG("hardware address: " << m_address.toString());
  OFED_LOG("MTU: " << m_hwmtu);
  m_mtu = m_hwmtu;
  m_buflen = m_hwmtu + stack::ethernet::HEADER_LEN;
  OFED_LOG("send buffer length: " << m_buflen);
  /*
   * Get L3 addresses.
   */
  if (!utils::getInterfaceInformation(ifn, m_ip, m_nm, m_dr)) {
    throw std::runtime_error("Cannot read device's L3 addresses");
  }
  OFED_LOG("ip address: " << m_ip.toString());
  OFED_LOG("netmask: " << m_nm.toString());
  OFED_LOG("router address: " << m_dr.toString());
  /*
   * Open the device.
   */
  m_context = ibv_open_device(device);
  if (m_context == nullptr) {
    throw std::runtime_error("Cannot open device");
  }
  /*
   * Query the device for its MAC.
   */
  struct ibv_exp_device_attr deva;
  deva.comp_mask = IBV_EXP_DEVICE_ATTR_EXP_CAP_FLAGS |
                   IBV_EXP_DEVICE_ATTR_TSO_CAPS;
  res = ibv_exp_query_device(m_context, &deva);
  if (res != 0) {
    std::string error(strerror(res));
    throw std::runtime_error("Cannot query device: " + error);
  }
  /*
   * Find out the capabilities.
   */
  PRINT_EXP_CAP(deva, IBV_EXP_DEVICE_MANAGED_FLOW_STEERING);
  PRINT_EXP_CAP(deva, IBV_EXP_DEVICE_VXLAN_SUPPORT);
  PRINT_EXP_CAP(deva, IBV_EXP_DEVICE_RX_CSUM_IP_PKT);
  PRINT_EXP_CAP(deva, IBV_EXP_DEVICE_RX_CSUM_TCP_UDP_PKT);
  /*
   * In case we are compiled with HW checksum support, make sure it is
   * supported.
   */
#ifdef TULIPS_HAS_HW_CHECKSUM
  if (!(deva.exp_device_cap_flags & IBV_EXP_DEVICE_RX_CSUM_IP_PKT)) {
#ifdef TULIPS_IGNORE_INCOMPATIBLE_HW
    std::cerr << OFED_HEADER
              << "IP checksum offload not supported by device, ignored"
              << std::endl;
#else
    throw std::runtime_error("Device does not support IP checksum offload");
#endif
  }
  if (!(deva.exp_device_cap_flags & IBV_EXP_DEVICE_RX_CSUM_TCP_UDP_PKT)) {
#ifdef TULIPS_IGNORE_INCOMPATIBLE_HW
    std::cerr << OFED_HEADER
              << "TCP/UDP checksum offload not supported by device"
              << std::endl;
#else
    throw std::runtime_error(
      "Device does not support TCP/UDP checksum offload");
#endif
  }
#endif
  /*
   * In case we are compiled with HW TSO, makes sure it is supported.
   */
#ifdef TULIPS_HAS_HW_TSO
  if (HAS_TSO(deva.tso_caps)) {
    OFED_LOG("max TSO length: " << deva.tso_caps.max_tso);
#if OFED_CAPTSOB
    m_buflen = 65536;
    OFED_LOG("updated limit send buffer length: " << m_buflen);
#else
    m_buflen = deva.tso_caps.max_tso;
    OFED_LOG("updated send buffer length: " << m_buflen);
#endif
  } else {
#ifdef TULIPS_IGNORE_INCOMPATIBLE_HW
    std::cerr << OFED_HEADER << "TSO not supported by device, ignored"
              << std::endl;
#else
    throw std::runtime_error("Device does not support TSO");
#endif
  }
#endif
  /*
   * Query the device port.
   */
  struct ibv_port_attr pattr;
  res = ibv_query_port(m_context, m_port + 1, &pattr);
  if (res != 0) {
    std::string error(strerror(res));
    throw std::runtime_error("Cannot query port: " + error);
  }
  /*
   * Allocate a protection domain.
   */
  m_pd = ibv_alloc_pd(m_context);
  if (m_pd == nullptr) {
    throw std::runtime_error("Cannot allocate protection domain");
  }
  /*
   * Setup the CQ, QP, etc...
   */
  setup(m_context, m_pd, m_port, m_nbuf, m_buflen, RECV_BUFLEN, m_comp,
        m_sendcq, m_recvcq, m_qp, m_sendbuf, m_recvbuf, m_sendmr, m_recvmr);
  /*
   * Prepare the receive buffers.
   */
  for (int i = 0; i < m_nbuf; i += 1) {
    if (postReceive(i) != Status::Ok) {
      throw std::runtime_error("Cannot post receive buffer");
    }
  }
  /*
   * Create the send FIFO.
   */
  tulips_fifo_create(m_nbuf, sizeof(uint8_t*), &m_fifo);
  /*
   * Fill the send FIFO.
   */
  for (int i = 0; i < m_nbuf; i += 1) {
    uint8_t* address = m_sendbuf + i * m_buflen;
    tulips_fifo_push(m_fifo, &address);
  }
  /*
   * Define the raw flow attribute structure.
   */
#if defined(TULIPS_ENABLE_ARP) || defined(TULIPS_ENABLE_RAW)
  struct raw_eth_flow_attr
  {
    struct ibv_flow_attr attr;
    struct ibv_flow_spec_eth spec_eth;
  } __attribute__((packed));
  /*
   * Fill in the attributes.
   */
  struct raw_eth_flow_attr flow;
  memset(&flow, 0, sizeof(flow));
  //
  flow.attr.type = IBV_FLOW_ATTR_NORMAL;
  flow.attr.size = sizeof(flow);
  flow.attr.num_of_specs = 1;
  flow.attr.port = m_port + 1;
  //
  flow.spec_eth.type = IBV_FLOW_SPEC_ETH;
  flow.spec_eth.size = sizeof(struct ibv_flow_spec_eth);
  /*
   * Create the broadcast flow.
   */
#ifdef TULIPS_ENABLE_ARP
  memset(flow.spec_eth.val.dst_mac, 0xFF, 6);
  memset(flow.spec_eth.mask.dst_mac, 0xFF, 6);
  m_bcast = ibv_create_flow(m_qp, (ibv_flow_attr*)&flow);
  if (m_bcast == nullptr) {
    throw std::runtime_error("Cannot create broadcast flow");
  }
#endif
  /*
   * Setup the local MAC flow.
   */
  memcpy(flow.spec_eth.val.dst_mac, m_address.data(), 6);
  memset(flow.spec_eth.mask.dst_mac, 0xFF, 6);
  m_flow = ibv_create_flow(m_qp, (ibv_flow_attr*)&flow);
  if (m_flow == nullptr) {
    throw std::runtime_error("Cannot create unicast flow");
  }
#endif
}

Status
Device::postReceive(const int id)
{
  struct ibv_sge sge;
  struct ibv_recv_wr wr;
  const uint8_t* addr = m_recvbuf + id * RECV_BUFLEN;
  /*
   * SGE entry.
   */
  sge.addr = (uint64_t)addr;
  sge.length = RECV_BUFLEN;
  sge.lkey = m_recvmr->lkey;
  /*
   * Work request.
   */
  wr.wr_id = id;
  wr.next = nullptr;
  wr.sg_list = &sge;
  wr.num_sge = 1;
  /*
   * Post the received the buffer list.
   */
  struct ibv_recv_wr* bad_wr;
  if (ibv_post_recv(m_qp, &wr, &bad_wr) != 0) {
    LOG("OFDED", "post receive of buffer id=" << id << "failed");
    return Status::HardwareError;
  }
  return Status::Ok;
}

Device::~Device()
{
  /*
   * Destroy dynamic flows.
   */
  for (auto& m_filter : m_filters) {
    ibv_exp_destroy_flow(m_filter.second);
  }
  m_filters.clear();
  /*
   * Destroy static flows.
   */
  if (m_flow) {
    ibv_destroy_flow(m_flow);
  }
  if (m_bcast) {
    ibv_destroy_flow(m_bcast);
  }
  /*
   * Clean-up all CQ events.
   */
  ibv_cq* cq = nullptr;
  void* context = nullptr;
  if (ibv_get_cq_event(m_comp, &cq, &context) == 0) {
    m_events += 1;
  }
  ibv_ack_cq_events(m_recvcq, m_events);
  /*
   * Destroy memory regions
   */
  tulips_fifo_destroy(&m_fifo);
  if (m_sendmr) {
    ibv_dereg_mr(m_sendmr);
  }
  if (m_sendbuf) {
    munmap(m_sendbuf, m_nbuf * m_buflen);
  }
  if (m_recvmr) {
    ibv_dereg_mr(m_recvmr);
  }
  if (m_recvbuf) {
    munmap(m_recvbuf, m_nbuf * RECV_BUFLEN);
  }
  /*
   * Destroy queue pair
   */
  if (m_qp) {
    ibv_destroy_qp(m_qp);
  }
  if (m_sendcq) {
    ibv_destroy_cq(m_sendcq);
  }
  if (m_recvcq) {
    ibv_destroy_cq(m_recvcq);
  }
  if (m_comp) {
    ibv_destroy_comp_channel(m_comp);
  }
  if (m_pd) {
    ibv_dealloc_pd(m_pd);
  }
  if (m_context) {
    ibv_close_device(m_context);
  }
}

Status
Device::listen(const uint16_t port)
{
  /*
   * Define the TCP flow attribute structure.
   */
  struct tcp_flow_attr
  {
    struct ibv_exp_flow_attr atr;
    struct ibv_exp_flow_spec_eth eth;
    struct ibv_exp_flow_spec_ipv4 ip4;
    struct ibv_exp_flow_spec_tcp_udp tcp;
  } __attribute__((packed));
  /*
   * Fill in the attributes.
   */
  struct tcp_flow_attr flow;
  memset(&flow, 0, sizeof(flow));
  //
  flow.atr.type = IBV_EXP_FLOW_ATTR_NORMAL;
  flow.atr.size = sizeof(flow);
  flow.atr.num_of_specs = 3;
  flow.atr.port = m_port + 1;
  //
  flow.eth.type = IBV_EXP_FLOW_SPEC_ETH;
  flow.eth.size = sizeof(struct ibv_exp_flow_spec_eth);
  memcpy(flow.eth.val.dst_mac, m_address.data(), 6);
  memset(flow.eth.mask.dst_mac, 0xFF, 6);
  //
  flow.ip4.type = IBV_EXP_FLOW_SPEC_IPV4;
  flow.ip4.size = sizeof(struct ibv_exp_flow_spec_ipv4);
  memcpy(&flow.ip4.val.dst_ip, m_ip.data(), 4);
  memset(&flow.ip4.mask.dst_ip, 0xFF, 4);
  //
  flow.tcp.type = IBV_EXP_FLOW_SPEC_TCP;
  flow.tcp.size = sizeof(struct ibv_exp_flow_spec_tcp_udp);
  flow.tcp.val.dst_port = htons(port);
  flow.tcp.mask.dst_port = 0xFFFF;
  /*
   * Setup the TCP flow.
   */
  ibv_exp_flow* f = ibv_exp_create_flow(m_qp, (ibv_exp_flow_attr*)&flow);
  if (f == nullptr) {
    OFED_LOG("cannot create TCP/UDP FLOW");
    return Status::HardwareError;
  }
  /*
   * Register the flow.
   */
  OFED_LOG("TCP/UDP flow for port " << port << " created");
  m_filters[port] = f;
  return Status::Ok;
}

void
Device::unlisten(const uint16_t port)
{
  if (m_filters.count(port) > 0) {
    ibv_exp_destroy_flow(m_filters[port]);
    m_filters.erase(port);
  }
}

/*
 * NOTE It is tempting to use a do/while loop here, but the idea is flawed. The
 * parent event loop needs some breathing room to do other things in case of
 * bursts.
 */
Status
Device::poll(Processor& proc)
{
  int cqn = 0;
  struct ibv_exp_wc wc[m_nbuf];
  /*
   * Process the incoming recv buffers.
   */
  cqn = ibv_exp_poll_cq(m_recvcq, m_nbuf, wc, sizeof(struct ibv_exp_wc));
  if (cqn < 0) {
    OFED_LOG("polling recv completion queue failed");
    return Status::HardwareError;
  }
  if (cqn == 0) {
    return Status::NoDataAvailable;
  }
  OFED_LOG(cqn << " buffers available");
  /*
   * Process the buffers.
   */
  int pre = 0;
  m_pending = cqn;
  for (int i = 0; i < cqn; i += 1) {
    int id = wc[i].wr_id;
    size_t len = wc[i].byte_len;
    const uint8_t* addr = m_recvbuf + id * RECV_BUFLEN;
    OFED_LOG("processing id=" << id << " addr=" << (void*)addr
                              << " len=" << len);
#if OFED_VERBOSE && OFED_HEXDUMP
    stack::utils::hexdump(addr, wc[i].byte_len, std::cout);
#endif
    /*
     * Validate the IP checksums
     */
#ifdef TULIPS_HAS_HW_CHECKSUM
    if (wc[i].exp_wc_flags & IBV_EXP_WC_RX_IPV4_PACKET) {
      if (m_hints & Device::VALIDATE_IP_CSUM) {
        if (!(wc[i].exp_wc_flags & IBV_EXP_WC_RX_IP_CSUM_OK)) {
          OFED_LOG("invalid IP checksum, dropping packet");
          continue;
        }
      }
      if (m_hints & Device::VALIDATE_TCP_CSUM) {
        if (!(wc[i].exp_wc_flags & IBV_EXP_WC_RX_TCP_UDP_CSUM_OK)) {
          OFED_LOG("invalid TCP/UDP checksum, dropping packet");
          continue;
        }
      }
    }
#endif
    /*
     * Process the packet.
     */
    proc.process(len, addr);
    /*
     * Re-post the buffers every 10 WCs.
     */
    if (i > 0 && i % POST_RECV_THRESHOLD == 0) {
      for (int j = pre; j < pre + POST_RECV_THRESHOLD; j += 1) {
        const int lid = wc[j].wr_id;
        Status res = postReceive(lid);
        if (res != Status::Ok) {
          LOG("OFDED", "re-post receive of buffer id=" << lid << "failed");
          return Status::HardwareError;
        }
      }
      pre += POST_RECV_THRESHOLD;
      m_pending -= POST_RECV_THRESHOLD;
    }
  }
  /*
   * Prepare the received buffers for repost.
   */
  for (int i = pre; i < cqn; i += 1) {
    const int id = wc[i].wr_id;
    Status res = postReceive(id);
    if (res != Status::Ok) {
      LOG("OFDED", "re-post receive of buffer id=" << id << "failed");
      return Status::HardwareError;
    }
  }
  m_pending = 0;
  return Status::Ok;
}

Status
Device::wait(Processor& proc, const uint64_t ns)
{
  sigset_t sigset;
  struct pollfd pfd = { m_comp->fd, POLLIN, 0 };
  struct timespec tsp = { 0, long(ns) };
  /*
   * Prepare the signal set.
   */
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGALRM);
  sigaddset(&sigset, SIGINT);
  /*
   * Check if we need to clean the events.
   */
  if (m_events == EVENT_CLEANUP_THRESHOLD) {
    ibv_ack_cq_events(m_recvcq, m_events);
    m_events = 0;
  }
  /*
   * Request a notification.
   */
  if (ibv_req_notify_cq(m_recvcq, 0) != 0) {
    OFED_LOG("requesting notification failed");
    return Status::HardwareError;
  }
  /*
   * Wait for the notification.
   */
  int rc = ::ppoll(&pfd, 1, &tsp, &sigset);
  /*
   * In case of an error (and not an interruption).
   */
  if (rc < 0 && errno != EINTR) {
    return Status::HardwareError;
  }
  /*
   * In case of a timeout.
   */
  else if (rc == 0) {
    return Status::NoDataAvailable;
  }
  /*
   * Get the notification if there is any pending event.
   */
  else if (rc > 0) {
    ibv_cq* cq = nullptr;
    void* context = nullptr;
    if (ibv_get_cq_event(m_comp, &cq, &context) != 0) {
      OFED_LOG("getting notification failed");
      return Status::HardwareError;
    }
    m_events += 1;
  }
  /*
   * Poll the CQ if there is an event on the CQ or we have been interrupted.
   */
  return poll(proc);
}

Status
Device::prepare(uint8_t*& buf)
{
  int cqn;
  struct ibv_exp_wc wc[m_nbuf];
  /*
   * Process the successfully sent buffers.
   */
  cqn = ibv_exp_poll_cq(m_sendcq, m_nbuf, wc, sizeof(struct ibv_exp_wc));
  if (cqn < 0) {
    OFED_LOG("polling send completion queue failed");
    return Status::HardwareError;
  }
  /*
   * Queue the sent buffers.
   */
  for (int i = 0; i < cqn; i += 1) {
    auto* addr = (uint8_t*)wc[i].wr_id;
    tulips_fifo_push(m_fifo, &addr);
  }
  /*
   * Look for an available buffer.
   */
  if (tulips_fifo_empty(m_fifo) == TULIPS_FIFO_YES) {
    buf = nullptr;
    return Status::NoMoreResources;
  }
  uint8_t** buffer = nullptr;
  if (tulips_fifo_front(m_fifo, (void**)&buffer) != TULIPS_FIFO_OK) {
    return Status::HardwareError;
  }
  buf = *buffer;
  OFED_LOG("prepare buffer " << (void*)buf);
  tulips_fifo_pop(m_fifo);
  return Status::Ok;
}

Status
Device::commit(const uint32_t len, uint8_t* const buf,
               const uint16_t UNUSED mss)
{
  /*
   * Get the header length.
   */
#ifdef TULIPS_HAS_HW_TSO
  uint32_t header_len;
  if (!stack::utils::headerLength(buf, len, header_len)) {
    OFED_LOG("cannot get packet header length");
    return Status::IncompleteData;
  }
  /*
   * Reject the request if the MSS provided is too small for the job.
   */
  if (len > (m_hwmtu + stack::ethernet::HEADER_LEN) && mss <= header_len) {
    LOG("OFED",
        "mss=" << mss << " for hwmtu=" << m_hwmtu << " and len=" << len);
    return Status::InvalidArgument;
  }
  /*
   * Adjust the MSS if the MSS provided is 0 or too big for the device.
   */
  uint16_t lmss = mss;
  if (mss == 0 || mss > m_hwmtu - header_len) {
    lmss = m_hwmtu + stack::ethernet::HEADER_LEN - header_len;
    OFED_LOG("adjusting request MSS from " << mss << " to " << lmss);
  }
#endif
  /*
   * Prepare the SGE.
   */
  struct ibv_sge sge;
#ifdef TULIPS_HAS_HW_TSO
  if (len > header_len) {
    sge.addr = (uint64_t)buf + header_len;
    sge.length = len - header_len;
    sge.lkey = m_sendmr->lkey;
  } else {
    sge.addr = (uint64_t)buf;
    sge.length = len;
    sge.lkey = m_sendmr->lkey;
  }
#else
  sge.addr = (uint64_t)buf;
  sge.length = len;
  sge.lkey = m_sendmr->lkey;
#endif
  /*
   * Prepare the WR.
   */
  struct ibv_exp_send_wr wr;
  memset(&wr, 0, sizeof(wr));
  wr.wr_id = (uint64_t)buf;
  wr.sg_list = &sge;
  wr.num_sge = 1;
#ifdef TULIPS_HAS_HW_TSO
  if (len > header_len) {
    wr.tso.mss = lmss;
    wr.tso.hdr = buf;
    wr.tso.hdr_sz = header_len;
    wr.exp_opcode = IBV_EXP_WR_TSO;
  } else {
    wr.exp_opcode = IBV_EXP_WR_SEND;
  }
#else
  wr.exp_opcode = IBV_EXP_WR_SEND;
#endif
  wr.exp_send_flags = IBV_EXP_SEND_SIGNALED | IBV_EXP_SEND_IP_CSUM;
  /*
   * Mark the transaction inline.
   */
  wr.exp_send_flags |= len <= INLINE_DATA_THRESHOLD ? IBV_SEND_INLINE : 0;
  /*
   * Post the work request.
   */
  struct ibv_exp_send_wr* bad_wr;
  if (ibv_exp_post_send(m_qp, &wr, &bad_wr) != 0) {
    LOG("OFED",
        "post send of buffer len=" << len << " failed, " << strerror(errno));
    return Status::HardwareError;
  }
  OFED_LOG("commit buffer " << (void*)buf << " len " << len);
#if OFED_VERBOSE && OFED_HEXDUMP
  stack::utils::hexdump(buf, len, std::cout);
#endif
  return Status::Ok;
}

}}}
