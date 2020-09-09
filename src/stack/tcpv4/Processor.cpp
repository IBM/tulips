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

#include "Debug.h"
#include <tulips/stack/tcpv4/Options.h>
#include <tulips/stack/tcpv4/Processor.h>
#include <tulips/stack/Utils.h>
#include <tulips/system/Compiler.h>
#include <tulips/system/Utils.h>
#include <cstring>

#ifdef __linux__
#include <arpa/inet.h>
#endif

#define INTCP ((const Header*)data)

namespace tulips { namespace stack { namespace tcpv4 {

Processor::Processor(transport::Device& device, ethernet::Producer& eth,
                     ipv4::Producer& ip4, EventHandler& h, const size_t nconn)
  : m_device(device)
  , m_ethto(eth)
  , m_ipv4to(ip4)
  , m_handler(h)
  , m_nconn(nconn)
  , m_ethfrom(nullptr)
  , m_ipv4from(nullptr)
  , m_iss(0)
  , m_mss(m_ipv4to.mss() - HEADER_LEN)
  , m_listenports()
  , m_conns()
  , m_stats()
  , m_timer()
{
  m_timer.set(CLOCK_SECOND);
  m_conns.resize(nconn);
  /*
   * Set the connection IDs and create the buffers.
   */
  for (uint16_t id = 0; id < nconn; id += 1) {
    m_conns[id].m_id = id;
  }
}

void
Processor::listen(const Port port)
{
  if (m_device.listen(port) == Status::Ok) {
    m_listenports.insert(htons(port));
  }
}

void
Processor::unlisten(const Port port)
{
  m_device.unlisten(port);
  m_listenports.erase(htons(port));
}

Status
Processor::run()
{
  /*
   * Check if the TCP timer has expired. This timer facility is not exact and
   * only guarantees that at least the period amount has been exhausted.
   */
  if (!m_timer.expired()) {
    return Status::Ok;
  }
  /*
   * Reset the timer, increase the initial sequence number.
   */
  m_timer.reset();
  m_iss += 1;
  /*
   * Scan the connections
   */
  for (auto& e : m_conns) {
    /*
     * Ignore closed connections
     */
    if (e.m_state == Connection::CLOSED) {
      continue;
    }
    /*
     * Check if the connection is in a state in which we simply wait for the
     * connection to time out.
     */
    if (e.m_state == Connection::TIME_WAIT ||
        e.m_state == Connection::FIN_WAIT_2) {
      /*
       * Increase the connection's timer.
       */
      e.m_timer += 1;
      /*
       * If it timed out, close the connection.
       */
      if (e.m_timer == TIME_WAIT_TIMEOUT) {
        TCP_LOG("connection closed");
        m_device.unlisten(e.m_lport);
        e.m_state = Connection::CLOSED;
        continue;
      }
    }
    /*
     * If the connection does not have any outstanding data, skip it.
     */
    if (!e.hasOutstandingSegments()) {
      continue;
    }
    /*
     * Check if the connection needs a retransmission.
     */
    e.m_timer -= 1;
    if (e.m_timer > 0) {
      continue;
    }
    /*
     * Retransmission timeout, reset the connection.
     */
    if (e.m_nrtx == MAXRTX || ((e.m_state == Connection::SYN_SENT ||
                                e.m_state == Connection::SYN_RCVD) &&
                               e.m_nrtx == MAXSYNRTX)) {
      TCP_LOG("aborting the connection");
      m_handler.onTimedOut(e);
      return sendAbort(e);
    }
    /*
     * Exponential backoff.
     */
    e.m_timer = RTO << (e.m_nrtx > 4 ? 4 : e.m_nrtx);
    e.m_nrtx += 1;
    /*
     * Ok, so we need to retransmit.
     */
    TCP_LOG("automatic repeat request (" << e.m_nrtx << "/" << MAXRTX << ")");
    TCP_LOG("segments available? " << std::boolalpha
                                   << e.hasAvailableSegments());
    TCP_LOG("segments outstanding? " << std::boolalpha
                                     << e.hasOutstandingSegments());
    return rexmit(e);
  }
  return Status::Ok;
}

Status
Processor::process(const uint16_t len, const uint8_t* const data)
{
  Connections::iterator e;
  m_stats.recv += 1;
  /*
   * Compute and check the TCP checksum.
   */
#ifndef TULIPS_DISABLE_CHECKSUM_CHECK
  uint16_t csum = checksum(m_ipv4from->sourceAddress(),
                           m_ipv4from->destinationAddress(), len, data);
  if (csum != 0xffff) {
    m_stats.drop += 1;
    m_stats.chkerr += 1;
    LOG("TCP", "invalid checksum ("
                 << m_ipv4from->sourceAddress().toString() << ", "
                 << m_ipv4from->destinationAddress().toString() << ", " << len
                 << ", 0x" << std::hex << csum << std::dec << ")");
    return Status::CorruptedData;
  }
#endif
  /*
   * Demultiplex this segment. First check any active connections.
   */
  for (auto& e : m_conns) {
    /*
     * If it matches, process it.
     */
    if (e.m_state != Connection::CLOSED && INTCP->destport == e.m_lport &&
        INTCP->srcport == e.m_rport &&
        m_ipv4from->sourceAddress() == e.m_ripaddr) {
      return process(e, len, data);
    }
  }
  /*
   * If we didn't find and active connection that expected the packet, either
   * this packet is an old duplicate, or this is a SYN packet destined for a
   * connection in LISTEN. If the SYN flag isn't set, it is an old packet and we
   * send a RST.
   */
  if ((INTCP->flags & TCP_CTL) != TCP_SYN) {
    TCP_LOG("no connection waiting for a SYN/ACK");
    return reset(len, data);
  }
  uint16_t tmp16 = INTCP->destport;
  /*
   * No matching connection found, so we send a RST packet.
   */
  if (m_listenports.count(tmp16) == 0) {
    m_stats.synrst += 1;
    return reset(len, data);
  }
  /*
   * Handle the new connection. First we check if there are any connections
   * available. Unused connections are kept in the same table as used
   * connections, but unused ones have the tcpstate set to CLOSED. Also,
   * connections in TIME_WAIT are kept track of and we'll use the oldest one if
   * no CLOSED connections are found. Thanks to Eddie C. Dost for a very nice
   * algorithm for the TIME_WAIT search.
   */
  for (e = m_conns.begin(); e != m_conns.end(); e++) {
    if (e->m_state == Connection::CLOSED) {
      break;
    }
  }
  /*
   * If no connection is available, take the oldest waiting connection
   */
  if (e == m_conns.end()) {
    for (auto c = m_conns.begin(); c != m_conns.end(); c++) {
      if (c->m_state == Connection::TIME_WAIT) {
        if (e == m_conns.end() || c->m_timer > e->m_timer) {
          e = c;
        }
      }
    }
  }
  /*
   * All connections are used already, we drop packet and hope that the remote
   * end will retransmit the packet at a time when we have more spare
   * connections.
   */
  if (e == m_conns.end()) {
    m_stats.syndrop += 1;
    return Status::Ok;
  }
  /*
   * Update IP and Ethernet attributes
   */
  m_ipv4to.setProtocol(ipv4::PROTO_TCP);
  m_ipv4to.setDestinationAddress(m_ipv4from->sourceAddress());
  m_ethto.setDestinationAddress(m_ethfrom->sourceAddress());
  /*
   * Allocate a send buffer
   */
  uint8_t* sdat;
  Status ret = m_ipv4to.prepare(sdat);
  if (ret != Status::Ok) {
    return ret;
  }
  /*
   * Prepare the connection.
   */
  e->m_rethaddr = m_ethfrom->sourceAddress();
  e->m_ripaddr = m_ipv4from->sourceAddress();
  e->m_lport = INTCP->destport;
  e->m_rport = INTCP->srcport;
  e->m_rcv_nxt = ntohl(INTCP->seqno) + 1;
  e->m_snd_nxt = m_iss;
  e->m_state = Connection::SYN_RCVD;
  e->m_opts = 0;
  e->m_ackdata = false;
  e->m_newdata = false;
  e->m_pshdata = false;
  e->m_wndscl = 0;
  e->m_window = ntohs(INTCP->wnd);
  e->m_segidx = 0;
  e->m_nrtx = 0; // Initial SYN send
  e->m_slen = 0;
  e->m_sdat = nullptr;
  e->m_initialmss = m_device.mtu() - HEADER_OVERHEAD;
  e->m_mss = e->m_initialmss;
  e->m_sa = 0;
  e->m_sv = 4; // Initial value of the RTT variance
  e->m_rto = RTO;
  e->m_timer = RTO;
  /*
   * Prepare the connection segment.
   */
  Segment& seg = e->nextAvailableSegment();
  seg.set(1, e->m_snd_nxt, sdat); // TCP length of the SYN is one
  /*
   * Parse the TCP MSS option, if present.
   */
  if (INTCP->offset > 5) {
    uint16_t nbytes = (INTCP->offset - 5) << 2;
    Options::parse(*e, nbytes, data);
  }
  /*
   * Send the SYN/ACK
   */
  return sendSynAck(*e, seg);
}

#if !(defined(TULIPS_HAS_HW_CHECKSUM) && defined(TULIPS_DISABLE_CHECKSUM_CHECK))
uint16_t
Processor::checksum(ipv4::Address const& src, ipv4::Address const& dst,
                    const uint16_t len, const uint8_t* const data)
{
  uint16_t sum;
  /*
   * IP protocol and length fields. This addition cannot carry.
   */
  sum = len + ipv4::PROTO_TCP;
  /*
   * Sum IP source and destination addresses.
   */
  sum = utils::checksum(sum, (uint8_t*)&src, sizeof(src));
  sum = utils::checksum(sum, (uint8_t*)&dst, sizeof(dst));
  /*
   * Sum TCP header and data.
   */
  sum = utils::checksum(sum, data, len);
  return sum == 0 ? 0xffff : htons(sum);
}
#endif

Status
Processor::process(Connection& e, const uint16_t len, const uint8_t* const data)
{
  uint16_t plen;
  uint16_t window = ntohs(INTCP->wnd);
  const uint32_t seqno = ntohl(INTCP->seqno);
  const uint32_t ackno = ntohl(INTCP->ackno);
  /*
   * Reset connection data state
   */
  e.m_ackdata = false;
  e.m_newdata = false;
  e.m_pshdata = false;
  /*
   * We do a very naive form of TCP reset processing; we just accept any RST and
   * kill our connection. We should in fact check if the sequence number of this
   * reset is wihtin our advertised window before we accept the reset.
   */
  if (INTCP->flags & TCP_RST) {
    TCP_LOG("connection aborted");
    m_device.unlisten(e.m_lport);
    e.m_state = Connection::CLOSED;
    m_handler.onAborted(e);
    return Status::Ok;
  }
  /*
   * Calculated the length of the data, if the application has sent any data to
   * us. plen will contain the length of the actual TCP data.
   */
  uint16_t tcpHdrLen = HEADER_LEN_WITH_OPTS(INTCP);
  plen = len - tcpHdrLen;
  /*
   * Print the flow information if requested.
   */
  TCP_FLOW("-> " << getFlags(*INTCP) << " len:" << plen << " seq:" << seqno
                 << " ack:" << ackno << " seg:" << e.m_segidx);
  /*
   * Check if the sequence number of the incoming packet is what we're
   * expecting next, except if we are expecting a SYN/ACK.
   */
  if (!(e.m_state == Connection::SYN_SENT &&
        (INTCP->flags & TCP_CTL) == (TCP_SYN | TCP_ACK))) {
    /*
     * If there is incoming data or is SYN or FIN is set.
     */
    if (plen > 0 || (INTCP->flags & (TCP_SYN | TCP_FIN)) != 0) {
      /*
       * Send an ACK with the proper seqno if the received seqno is wrong.
       */
      if (seqno != e.m_rcv_nxt) {
        TCP_LOG("sequence ACK: in=" << seqno << " exp=" << e.m_rcv_nxt);
        return sendAck(e);
      }
    }
  }
  /*
   * Check if the incoming segment acknowledges any outstanding data. If so, we
   * update the sequence number, reset the length of the outstanding data,
   * calculate RTT estimations, and reset the retransmission timer.
   */
  if ((INTCP->flags & TCP_ACK) && e.hasOutstandingSegments()) {
    /*
     * Scan the segments.
     */
    for (size_t i = 0; i < Connection::SEGMENT_COUNT; i += 1) {
      /*
       * Get the oldest pending segment.
       */
      Segment& seg = e.segment();
      /*
       * Compute the expected ackno.
       */
      uint64_t explm = (uint64_t)seg.m_seq + seg.m_len;
      uint64_t acklm = ackno;
      /*
       * Linearly adjust the received ACK number to handle overflow cases.
       */
      if (ackno < seg.m_seq) {
        acklm += 1ULL << 32;
      }
      /*
       * Check if the peers is letting us know about something that went amok.
       * It can be either an OoO packet or a window size change.
       */
      if (ackno == seg.m_seq) {
        /*
         * In the case of a window size change.
         */
        if (e.window() != e.window(window)) {
          e.m_window = window;
          TCP_LOG("peer window updated to wnd: " << e.window()
                                                 << " on seq:" << ackno);
          /*
           * Do RTT estimation, unless we have done retransmissions. There is no
           * reason to have a REXMIT at this point.
           */
          if (e.m_nrtx == 0) {
            e.updateRttEstimation();
            e.m_timer = e.m_rto;
          }
        }
        /*
         * In the case of an OoO packet, let the ARQ do its job.
         */
        TCP_LOG("peer rexmit request on seq:" << ackno);
        return rexmit(e);
      }
      /*
       * Check if it's partial ACK (common with TSO). The first check covers the
       * normal linear case. The second checks covers wrap-around situations.
       */
      else if (acklm < explm) {
        m_stats.ackerr += 1;
        break;
      }
      /*
       * Housekeeping in case we have not processed an ACK yet.
       */
      if (!e.m_ackdata) {
        /*
         * Do RTT estimation, unless we have done retransmissions.
         */
        if (e.m_nrtx == 0) {
          e.updateRttEstimation();
        }
        /*
         * Clear the retransmission counter.
         */
        e.m_nrtx = 0;
        /*
         * Set the acknowledged flag.
         */
        e.m_ackdata = true;
        /*
         * Reset the retransmission timer.
         */
        e.m_timer = e.m_rto;
      }
      /*
       * Reset length and buffer of outstanding data and go to the next
       * segment.  The compiler will generate the wrap-around appropriate for
       * the bit length of the index.
       */
      seg.clear();
      e.m_segidx += 1;
      /*
       * Stop processing the segments if the ACK number is the one expected.
       */
      if (acklm == explm) {
        break;
      }
    }
  }
  /*
   * Do different things depending on in what state the connection is. CLOSED
   * and LISTEN are not handled here. CLOSE_WAIT is not implemented, since we
   * force the application to close when the peer sends a FIN (hence the
   * application goes directly from ESTABLISHED to LAST_ACK).
   */
  switch (e.m_state) {
    /*
     * In SYN_RCVD we have sent out a SYNACK in response to a SYN, and we are
     * waiting for an ACK that acknowledges the data we sent out the last time.
     * If so, we enter the ESTABLISHED state.
     */
    case Connection::SYN_RCVD: {
      /*
       * Process the ACK data if any.
       */
      if (e.m_ackdata) {
        TCP_LOG("connection established");
        /*
         * Send the connection event.
         */
        e.m_state = Connection::ESTABLISHED;
        m_handler.onConnected(e);
        /*
         * Send the newdata event. Pass the packet data directly. At this stage,
         * no data has been buffered.
         */
        if (plen > 0) {
          e.m_rcv_nxt += plen;
          e.m_newdata = true;
          e.m_pshdata = (INTCP->flags & TCP_PSH) == TCP_PSH;
          m_handler.onNewData(e, data + tcpHdrLen, plen);
          return sendAck(e);
        }
      }
      break;
    }
    /*
     * In SYN_SENT, we wait for a SYNACK that is sent in response to our SYN.
     * The rcv_nxt is set to sequence number in the SYNACK plus one, and we send
     * an ACK. We move into the ESTABLISHED state.
     */
    case Connection::SYN_SENT: {
      /*
       * Update the connection when established.
       */
      if (e.m_ackdata && (INTCP->flags & TCP_CTL) == (TCP_SYN | TCP_ACK)) {
        TCP_LOG("connection established");
        /*
         * Update the connection info
         */
        e.m_state = Connection::ESTABLISHED;
        e.m_rcv_nxt = seqno + 1;
        e.m_window = window;
        /*
         * Parse the options.
         */
        if (INTCP->offset > 5) {
          uint16_t nbytes = (INTCP->offset - 5) << 2;
          Options::parse(e, nbytes, data);
        }
        /*
         * Send the connected event.
         */
        m_handler.onConnected(e);
        /*
         * Send the newdata event. Pass the packet data directly. At this stage,
         * no data has been buffered.
         */
        if (plen > 0) {
          e.m_newdata = true;
          e.m_pshdata = (INTCP->flags & TCP_PSH) == TCP_PSH;
          m_handler.onNewData(e, data + tcpHdrLen, plen);
        }
        return sendAck(e);
      }
      /*
       * Inform the application that the connection failed.
       */
      m_handler.onAborted(e);
      /*
       * The connection is closed after we send the RST.
       */
      return sendAbort(e);
    }
    /*
     * In the ESTABLISHED state, we call upon the application to feed data
     * into the m_buf. If the ACKDATA flag is set, the application
     * should put new data into the buffer, otherwise we are retransmitting
     * an old segment, and the application should put that data into the
     * buffer. If the incoming packet is a FIN, we should close the
     * connection on this side as well, and we send out a FIN and enter the
     * LAST_ACK state. We require that there is no outstanding data;
     * otherwise the sequence numbers will be screwed up.
     */
    case Connection::ESTABLISHED: {
      /*
       * Check if we received a FIN request and process it.
       */
      if (INTCP->flags & TCP_FIN && e.m_state != Connection::STOPPED) {
        /*
         * If some of our data is still in flight, ignore the FIN.
         */
        if (e.hasOutstandingSegments()) {
          TCP_LOG("FIN received but outstanding data");
          return Status::Ok;
        }
        /*
         * Increase the expected next pointer.
         */
        e.m_rcv_nxt += plen + 1;
        /*
         * Process the embedded data.
         */
        if (plen > 0) {
          m_handler.onNewData(e, data + tcpHdrLen, plen);
        }
        /*
         * Acknowledge the FIN. If we are here there is no more outstanding
         * segment, so one must be available.
         */
        TCP_LOG("connection last ACK");
        e.m_state = Connection::LAST_ACK;
        Segment& seg = e.nextAvailableSegment();
        seg.set(1, e.m_snd_nxt, e.m_sdat);
        e.resetSendBuffer();
        return sendFinAck(e, seg);
      }
      /*
       * Check the URG flag. If this is set, the segment carries urgent data
       * that we must pass to the application. NOTE: skip it for now.
       */
      uint16_t urglen = 0;
      if ((INTCP->flags & TCP_URG) != 0) {
        urglen = ntohs(INTCP->urgp);
        plen -= urglen;
      }
      /*
       * If plen > 0 we have TCP data in the packet, and we flag this by setting
       * the NEWDATA flag and update the sequence number we acknowledge. If
       * the application has stopped the dataflow using stop(), we must not
       * accept any data packets from the remote host.
       */
      if (plen > 0 && e.m_state != Connection::STOPPED) {
        e.m_newdata = true;
        e.m_pshdata = (INTCP->flags & TCP_PSH) == TCP_PSH;
        e.m_rcv_nxt += plen;
      }
      /*
       * Update the peer window value.
       */
      e.m_window = window;
      /*
       * Check if the available buffer space advertised by the other end is
       * smaller than the initial MSS for this connection. If so, we set the
       * current MSS to the window size to ensure that the application does not
       * send more data than the other end can handle.
       */
      if (e.window() <= e.m_initialmss && e.window() > 0) {
        e.m_mss = e.window();
      }
      /*
       * If the remote host advertises a zero window or a window larger than the
       * initial negotiated window, we set the MSS to the initial MSS so that
       * the application will send an entire MSS of data. This data will not be
       * acknowledged by the receiver, and the application will retransmit it.
       * This is called the "persistent timer" and uses the retransmission
       * mechanim.
       */
      else {
        e.m_mss = e.m_initialmss;
      }
      /*
       * If this packet constitutes an ACK for outstanding data (flagged by the
       * ACKDATA flag, we should call the application since it might want to
       * send more data. If the incoming packet had data from the peer (as
       * flagged by the NEWDATA flag), the application must also be
       * notified.
       */
      if (e.m_ackdata || e.m_newdata) {
        /*
         * Check if the application can send.
         */
        bool can_send = e.hasAvailableSegments() && e.window() > e.m_slen;
        /*
         * Notify the application on an ACK.
         */
        if (e.m_ackdata) {
          /*
           * Check if we can send data as a result of the ACK. This is useful to
           * handle partial send without resorting to software TSO.
           */
          if (likely(can_send)) {
            uint32_t rlen = 0;
            uint32_t bound = e.window() < m_mss ? e.window() : m_mss;
            uint32_t alen = bound - e.m_slen;
            /*
             * The application can send back some data.
             */
            switch (m_handler.onAcked(e, alen, e.m_sdat + HEADER_LEN + e.m_slen,
                                      rlen)) {
              case Action::Abort:
                return sendAbort(e);
              case Action::Close:
                return sendClose(e);
              default:
                break;
            }
            /*
             * Truncate to available length if necessary
             */
            if (rlen > alen) {
              rlen = alen;
            }
            e.m_slen += rlen;
            /*
             * Update the send state.
             */
            can_send = e.hasAvailableSegments() && e.window() > e.m_slen;
          }
          /*
           * If we cannot send anything, just notify the application.
           */
          else {
            switch (m_handler.onAcked(e)) {
              case Action::Abort:
                return sendAbort(e);
              case Action::Close:
                return sendClose(e);
              default:
                break;
            }
          }
        }
        /*
         * Collect the connection's buffer state.
         */
        const uint8_t* dataptr = data + tcpHdrLen + urglen;
        const uint32_t datalen = plen;
        /*
         * Notify the application on new data.
         */
        if (e.m_newdata) {
          /*
           * Check if the connection supports DELAYED_ACK.
           */
          if (!HAS_DELAYED_ACK(e)) {
            /*
             * Send an ACK immediately. This is preferrable when sending data
             * back is not needed as the ACK latency is not subject to the
             * onNewData() callback latency anymore.
             */
            Status res = sendAck(e);
            if (res != Status::Ok) {
              return res;
            }
          }
          /*
           * Notify the application and allow it to send a response.
           */
          if (likely(can_send)) {
            uint32_t rlen = 0;
            uint32_t bound = e.window() < m_mss ? e.window() : m_mss;
            uint32_t alen = bound - e.m_slen;
            /*
             * The application can send back some data.
             */
            switch (m_handler.onNewData(e, dataptr, datalen, alen,
                                        e.m_sdat + HEADER_LEN + e.m_slen,
                                        rlen)) {
              case Action::Abort:
                return sendAbort(e);
              case Action::Close:
                return sendClose(e);
              default:
                break;
            }
            /*
             * Truncate to available length if necessary
             */
            if (rlen > alen) {
              rlen = alen;
            }
            e.m_slen += rlen;
          }
          /*
           * Notify the application and ACK if necessary. Applies when there is
           * already data in flight or if the remote window is smaller that the
           * outstanding data in the send buffer.
           */
          else {
            switch (m_handler.onNewData(e, dataptr, datalen)) {
              case Action::Abort:
                return sendAbort(e);
              case Action::Close:
                return sendClose(e);
              default:
                break;
            }
          }
        }
        /*
         * If there is any buffered send data, send it. Make sure that there is
         * an available segment before allocating one.
         */
        if (e.hasPendingSendData() && can_send) {
          return sendNoDelay(e, TCP_PSH);
        }
        /*
         * If the connection supports DELAYED_ACK and could/dit not send
         * anything, ACK if necessary.
         */
        if (HAS_DELAYED_ACK(e) && e.m_newdata) {
          return sendAck(e);
        }
        /*
         * Otherwise do nothing
         */
        return Status::Ok;
      }
      break;
    }
    /*
     * We can close this connection if the peer has acknowledged our FIN. This
     * is indicated by the ACKDATA flag.
     */
    case Connection::LAST_ACK: {
      if (e.m_ackdata) {
        TCP_LOG("connection closed");
        m_device.unlisten(e.m_lport);
        e.m_state = Connection::CLOSED;
        m_handler.onClosed(e);
      }
      break;
    }
    /*
     * The application has closed the connection, but the remote host hasn't
     * closed its end yet. Thus we do nothing but wait for a FIN from the other
     * side.
     */
    case Connection::FIN_WAIT_1: {
      if (plen > 0) {
        e.m_rcv_nxt += plen;
      }
      /*
       * If we get a FIN, change the connection to TIME_WAIT or CLOSING.
       */
      if (INTCP->flags & TCP_FIN) {
        if (e.m_ackdata) {
          TCP_LOG("connection time-wait");
          e.m_state = Connection::TIME_WAIT;
          e.m_timer = 0;
        } else {
          TCP_LOG("connection closing");
          e.m_state = Connection::CLOSING;
        }
        e.m_rcv_nxt += 1;
        m_handler.onClosed(e);
        return sendAck(e);
      }
      /*
       * Otherwise, if we received an ACK, moved to FIN_WAIT_2.
       */
      else if (e.m_ackdata) {
        TCP_LOG("Connection FIN wait #2");
        e.m_state = Connection::FIN_WAIT_2;
        return Status::Ok;
      }
      /*
       * ACK any received data.
       */
      if (plen > 0) {
        return sendAck(e);
      }
      return Status::Ok;
    }
    case Connection::FIN_WAIT_2: {
      if (plen > 0) {
        e.m_rcv_nxt += plen;
      }
      /*
       * If we get a FIN, moved to TIME_WAIT.
       */
      if (INTCP->flags & TCP_FIN) {
        TCP_LOG("connection time-wait");
        e.m_state = Connection::TIME_WAIT;
        e.m_rcv_nxt += 1;
        e.m_timer = 0;
        m_handler.onClosed(e);
        return sendAck(e);
      }
      /*
       * ACK any received data.
       */
      if (plen > 0) {
        return sendAck(e);
      }
      return Status::Ok;
    }
    case Connection::TIME_WAIT: {
      return sendAck(e);
    }
    /*
     * The user requested the connection to be closed.
     */
    case Connection::CLOSE: {
      /*
       * Check if there is still data in flight. In that case, keep waiting.
       */
      if (e.hasOutstandingSegments()) {
        break;
      }
      /*
       * Switch to FIN_WAIT_1. There is no more outstanding segment here so we
       * can grab one.
       */
      TCP_LOG("connection FIN wait #1");
      e.m_state = Connection::FIN_WAIT_1;
      Segment& seg = e.nextAvailableSegment();
      seg.set(1, e.m_snd_nxt, e.m_sdat);
      e.resetSendBuffer();
      /*
       * If there is some new data, ignore it.
       */
      if (plen > 0) {
        e.m_newdata = true;
        e.m_pshdata = (INTCP->flags & TCP_PSH) == TCP_PSH;
        e.m_rcv_nxt += plen;
      }
      /*
       * Send a FIN/ACK message. TCP does not require to send an ACK with FIN,
       * but Linux seems pretty bent on wanting one. So we play nice.
       */
      return sendFinAck(e, seg);
    }
    case Connection::CLOSING: {
      if (e.m_ackdata) {
        TCP_LOG("connection time-wait");
        e.m_state = Connection::TIME_WAIT;
        e.m_timer = 0;
      }
      break;
    }
    /*
     * Unhandled cases.
     */
    case Connection::STOPPED:
    case Connection::CLOSED:
    default: {
      break;
    }
  }
  /*
   * Return status
   */
  return Status::Ok;
}

Status
Processor::reset(const uint16_t UNUSED len, const uint8_t* const data)
{
  /*
   * Update IP and Ethernet attributes
   */
  m_ipv4to.setProtocol(ipv4::PROTO_TCP);
  m_ipv4to.setDestinationAddress(m_ipv4from->sourceAddress());
  m_ethto.setDestinationAddress(m_ethfrom->sourceAddress());
  /*
   * Allocate the send buffer
   */
  uint8_t* outdata;
  Status ret = m_ipv4to.prepare(outdata);
  if (ret != Status::Ok) {
    return ret;
  }
  /*
   * We do not send resets in response to resets.
   */
  if (INTCP->flags & TCP_RST) {
    return Status::Ok;
  }
  m_stats.rst += 1;
  OUTTCP->flags = TCP_RST;
  OUTTCP->offset = 5;
  /*
   * Flip the seqno and ackno fields in the TCP header. We also have to
   * increase the sequence number we are acknowledging.
   */
  uint32_t c = ntohl(INTCP->seqno);
  OUTTCP->seqno = INTCP->ackno;
  OUTTCP->ackno = htonl(c + 1);
  /*
   * Swap port numbers.
   */
  uint16_t tmp16 = INTCP->srcport;
  OUTTCP->srcport = INTCP->destport;
  OUTTCP->destport = tmp16;
  /*
   * And send out the RST packet!
   */
  uint16_t mss = m_device.mtu() - HEADER_OVERHEAD;
  return send(m_ipv4from->sourceAddress(), HEADER_LEN, mss, outdata);
}

}}}
