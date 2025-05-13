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
#include <iostream>
#include <stdio.h>
#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "ns3/drop-tail-queue.h"
#include "broadcom-egress-queue.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("BEgressQueue");

NS_OBJECT_ENSURE_REGISTERED(BEgressQueue);

	TypeId BEgressQueue::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::BEgressQueue")
			.SetParent<Queue>()
			.AddConstructor<BEgressQueue>()
			.AddTraceSource ("BeqEnqueue", "Enqueue a packet in the BEgressQueue. Multiple queue",
					MakeTraceSourceAccessor (&BEgressQueue::m_traceBeqEnqueue),
					"ns3::BEgressQueue::TraceBeqEnqueueCallback")
			.AddTraceSource ("BeqDequeue", "Dequeue a packet in the BEgressQueue. Multiple queue",
					MakeTraceSourceAccessor (&BEgressQueue::m_traceBeqDequeue),
					"ns3::BEgressQueue::TraceBeqDequeueCallback")
			;

		return tid;
	}

	BEgressQueue::BEgressQueue() :
		Queue(),
		NS_LOG_TEMPLATE_DEFINE("BEgressQueue")
	{
		NS_LOG_FUNCTION(this);
		m_bytesInQueueTotal = 0;
		m_rrlast = 0;
		for (uint32_t i = 0; i < qCnt; i++)
		{
			m_bytesInQueue[i] = 0;

			auto queue = CreateObject<DropTailQueue<Packet>>();
			m_queues.push_back(queue);

			// Override DropTailQueue default value of 100 packets
			// Infinite queue
			queue->SetMaxSize(QueueSize(QueueSizeUnit::PACKETS, std::numeric_limits<uint32_t>::max()));
		}
	}

	BEgressQueue::~BEgressQueue()
	{
		NS_LOG_FUNCTION(this);
	}

	bool
		BEgressQueue::DoEnqueue(Ptr<Packet> p, uint32_t qIndex)
	{
		NS_LOG_FUNCTION(this << p);
		NS_ASSERT(m_maxSize.GetUnit() == BYTES);

		if (m_bytesInQueueTotal + p->GetSize() < m_maxSize.GetValue())  //infinite queue
		{
			// Admission should never fail here
			NS_ABORT_IF(!m_queues[qIndex]->Enqueue(p));

			m_bytesInQueueTotal += p->GetSize();
			m_bytesInQueue[qIndex] += p->GetSize();
		}
		else
		{
			return false;
		}
		return true;
	}

	Ptr<Packet>
		BEgressQueue::DoDequeueRR(bool paused[]) //this is for switch only
	{
		NS_LOG_FUNCTION(this);

		if (m_bytesInQueueTotal == 0)
		{
			NS_LOG_LOGIC("Queue empty");
			return 0;
		}
		bool found = false;
		uint32_t qIndex;

		if (m_queues[0]->GetNPackets() > 0) //0 is the highest priority
		{
			found = true;
			qIndex = 0;
		}
		else
		{
			if (!found)
			{
				for (qIndex = 1; qIndex <= qCnt; qIndex++)
				{
					if (!paused[(qIndex + m_rrlast) % qCnt] && m_queues[(qIndex + m_rrlast) % qCnt]->GetNPackets() > 0)  //round robin
					{
						found = true;
						break;
					}
				}
				qIndex = (qIndex + m_rrlast) % qCnt;
			}
		}
		if (found)
		{
			Ptr<Packet> p = m_queues[qIndex]->Dequeue();
			m_traceBeqDequeue(p, qIndex);
			m_bytesInQueueTotal -= p->GetSize();
			m_bytesInQueue[qIndex] -= p->GetSize();
			if (qIndex != 0)
			{
				m_rrlast = qIndex;
			}
			m_qlast = qIndex;
			NS_LOG_LOGIC("Popped " << p);
			NS_LOG_LOGIC("Number bytes " << m_bytesInQueueTotal);
			return p;
		}
		NS_LOG_LOGIC("Nothing can be sent");
		return 0;
	}

	bool
		BEgressQueue::Enqueue(Ptr<Packet> p, uint32_t qIndex)
	{
		NS_LOG_FUNCTION(this << p);
		//
		// If DoEnqueue fails, Queue::Drop is called by the subclass
		//
		bool retval = DoEnqueue(p, qIndex);
		if (retval)
		{
			NS_LOG_LOGIC("m_traceEnqueue (p)");
			m_traceEnqueue(p);
			m_traceBeqEnqueue(p, qIndex);

			uint32_t size = p->GetSize();
			m_nBytes += size;
			m_nTotalReceivedBytes += size;

			m_nPackets++;
			m_nTotalReceivedPackets++;
		}
		return retval;
	}

	Ptr<Packet>
		BEgressQueue::DequeueRR(bool paused[])
	{
		NS_LOG_FUNCTION(this);
		Ptr<Packet> packet = DoDequeueRR(paused);
		if (packet != 0)
		{
			NS_ASSERT(m_nBytes >= packet->GetSize());
			NS_ASSERT(m_nPackets > 0u);
			m_nBytes -= packet->GetSize();
			m_nPackets--;
			NS_LOG_LOGIC("m_traceDequeue (packet)");
			m_traceDequeue(packet);
		}
		return packet;
	}

	bool
		BEgressQueue::Enqueue(Ptr<Packet> p)	//for compatiability
	{
		NS_ABORT_MSG("BEgressQueue::Enqueue not implemented");
		return false;
	}


	Ptr<Packet>
		BEgressQueue::Dequeue(void)
	{
		NS_ABORT_MSG("BEgressQueue::DoDequeue not implemented");
		return 0;
	}

	Ptr<Packet>
		BEgressQueue::Remove(void)
	{
		NS_ABORT_MSG("BEgressQueue::DoRemove not implemented");
		return 0;
	}

	Ptr<const Packet>
		BEgressQueue::Peek(void) const	//DoPeek doesn't work for multiple queues!!
	{
		NS_ABORT_MSG("BEgressQueue::Peek not implemented");

		std::cout << "Warning: Call Broadcom queues without priority\n";
		NS_LOG_FUNCTION(this);
		if (m_bytesInQueueTotal == 0)
		{
			NS_LOG_LOGIC("Queue empty");
			return 0;
		}
		NS_LOG_LOGIC("Number bytes " << m_bytesInQueue);
		return m_queues[0]->Peek();
	}

	uint32_t
		BEgressQueue::GetNBytes(uint32_t qIndex) const
	{
		return m_bytesInQueue[qIndex];
	}


	uint32_t
		BEgressQueue::GetNBytesTotal() const
	{
		return m_bytesInQueueTotal;
	}

	uint32_t
		BEgressQueue::GetLastQueue()
	{
		return m_qlast;
	}

}
