/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Copyright (c) 2006 Georgia Tech Research Corporation, INRIA
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation;
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef BROADCOM_EGRESS_H
#define BROADCOM_EGRESS_H

#include <queue>
#include <ns3/packet.h>
#include <ns3/queue.h>
#include <ns3/drop-tail-queue.h>
#include <ns3/point-to-point-net-device.h>
#include <ns3/event-id.h>

namespace ns3 {

	class TraceContainer;

	/**
	 * @brief Merge multiple queues into one (multi-queue).
	 */
	class BEgressQueue : public Queue<Packet> {
	public:
		static TypeId GetTypeId(void);
		static const unsigned qCnt = 8; //max number of queues, 8 for switches
		BEgressQueue();
		~BEgressQueue() override;
		bool Enqueue(Ptr<Packet> p, uint32_t qIndex);
		Ptr<Packet> DequeueRR(bool paused[]);
		uint32_t GetNBytes(uint32_t qIndex) const;
		uint32_t GetNBytesTotal() const;
		uint32_t GetLastQueue();

		TracedCallback<Ptr<const Packet>, uint32_t> m_traceBeqEnqueue;
		TracedCallback<Ptr<const Packet>, uint32_t> m_traceBeqDequeue;

	private:
		bool DoEnqueue(Ptr<Packet> p, uint32_t qIndex);

		/**
		 * @brief Dequeue a packet from one of the queues.
		 * @param paused `paused[i]` says whether the i-th queue is paused.
		 * 
		 * It is round-robin, that means if the last dequeued packet was from queue `i`,
		 * it will check first queue `i+1`, then `i+2`, ..., and the first queue to not be empty will be dequeued.
		 * The exception is the queue zero, which has highest priority over all the other queues.
		 */
		Ptr<Packet> DoDequeueRR(bool paused[]);
		//for compatibility
		bool Enqueue(Ptr<Packet> p) override;
		Ptr<Packet> Dequeue(void) override;
		Ptr<const Packet> Peek(void) const override;
		Ptr<Packet> Remove (void) override;
		uint32_t m_bytesInQueue[qCnt];
		uint32_t m_bytesInQueueTotal;
		uint32_t m_rrlast; //!< Like `m_qlast`, but is not updated when the popped index is zero.
		uint32_t m_qlast;  //!< Last popped queue index.
		std::vector<Ptr<Queue> > m_queues; // uc queues
	};

} // namespace ns3

#endif /* DROPTAIL_H */
