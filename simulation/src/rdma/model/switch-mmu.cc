#include <iostream>
#include <fstream>
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/object-vector.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"
#include "ns3/assert.h"
#include "ns3/global-value.h"
#include "ns3/boolean.h"
#include "ns3/simulator.h"
#include "ns3/rdma-random.h"
#include "switch-mmu.h"

namespace ns3 {

	NS_LOG_COMPONENT_DEFINE("SwitchMmu");
	NS_OBJECT_ENSURE_REGISTERED (SwitchMmu);

	TypeId SwitchMmu::GetTypeId(void){
		static TypeId tid = TypeId("ns3::SwitchMmu")
			.SetParent<Object>()
			.AddConstructor<SwitchMmu>()
			.AddAttribute ("BufferSize",
				"Buffer size. PFC threshold depends on it.",
				QueueSizeValue(QueueSize("12MiB")),
				MakeQueueSizeAccessor(&SwitchMmu::m_buffer_size),
				MakeQueueSizeChecker())
			;
		return tid;
	}

	SwitchMmu::SwitchMmu(void)
	{
		m_buffer_size = QueueSize(QueueSizeUnit::BYTES, 12 * 1024 * 1024);
		reserve = 4 * 1024;
		resume_offset = 3 * 1024;

		// headroom
		shared_used_bytes = 0;
		memset(hdrm_bytes, 0, sizeof(hdrm_bytes));
		memset(ingress_bytes, 0, sizeof(ingress_bytes));
		memset(paused, 0, sizeof(paused));
		memset(egress_bytes, 0, sizeof(egress_bytes));
	}
	bool SwitchMmu::CheckIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize)
	{
		NS_LOG_FUNCTION(this << port << qIndex << psize);

		if (psize + hdrm_bytes[port][qIndex] > headroom[port] && psize + GetSharedUsed(port, qIndex) > GetPfcThreshold(port)){
			printf("%lu %u Drop: queue:%u,%u: Headroom full\n", Simulator::Now().GetTimeStep(), node_id, port, qIndex);
			for (uint32_t i = 1; i < 64; i++)
				printf("(%u,%u)", hdrm_bytes[i][3], ingress_bytes[i][3]);
			printf("\n");
			return false;
		}
		return true;
	}

	void SwitchMmu::UpdateIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize)
	{
		NS_LOG_FUNCTION(this << port << qIndex << psize);

		uint32_t new_bytes = ingress_bytes[port][qIndex] + psize;
		if (new_bytes <= reserve){
			ingress_bytes[port][qIndex] += psize;
		}else {
			uint32_t thresh = GetPfcThreshold(port);
			if (new_bytes - reserve > thresh){
				hdrm_bytes[port][qIndex] += psize;
			}else {
				ingress_bytes[port][qIndex] += psize;
				shared_used_bytes += std::min(psize, new_bytes - reserve);
			}
		}
	}
	void SwitchMmu::UpdateEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize)
	{
		NS_LOG_FUNCTION(this << port << qIndex << psize);

		egress_bytes[port][qIndex] += psize;
	}

	void SwitchMmu::RemoveFromIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize)
	{
		NS_LOG_FUNCTION(this << port << qIndex << psize);

		uint32_t from_hdrm = std::min(hdrm_bytes[port][qIndex], psize);

		NS_ABORT_IF(psize < from_hdrm);

		uint32_t from_shared = std::min(psize - from_hdrm, ingress_bytes[port][qIndex] > reserve ? ingress_bytes[port][qIndex] - reserve : 0);
		
		NS_ABORT_IF(hdrm_bytes[port][qIndex] < from_hdrm);
		NS_ABORT_IF(ingress_bytes[port][qIndex] < psize - from_hdrm);
		NS_ABORT_IF(shared_used_bytes < from_shared);

		hdrm_bytes[port][qIndex] -= from_hdrm;
		ingress_bytes[port][qIndex] -= psize - from_hdrm;
		shared_used_bytes -= from_shared;
	}
	
	void SwitchMmu::RemoveFromEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize)
	{
		NS_LOG_FUNCTION(this << port << qIndex << psize);
		egress_bytes[port][qIndex] -= psize;
	}
	
	bool SwitchMmu::CheckShouldPause(uint32_t port, uint32_t qIndex)
	{
		NS_LOG_FUNCTION(this << port << qIndex);

		if(paused[port][qIndex]) {
			return false;
		}
		
		NS_LOG_DEBUG("usage: " << GetSharedUsed(port, qIndex)
													 << "/" << GetPfcThreshold(port));

		if(hdrm_bytes[port][qIndex] > 0) {
			NS_LOG_LOGIC("PFC headroom not empty");
			return true;
		}

		if(GetSharedUsed(port, qIndex) >= GetPfcThreshold(port)) {
			NS_LOG_LOGIC("PFC threshold reached (" << GetSharedUsed(port, qIndex) << "/" << GetPfcThreshold(port) << ")");
			return true;
		}

		return false;
	}
	bool SwitchMmu::CheckShouldResume(uint32_t port, uint32_t qIndex)
	{
		NS_LOG_DEBUG("usage: " << GetSharedUsed(port, qIndex)
													 << "/" << GetPfcThreshold(port));

		if (!paused[port][qIndex])
			return false;
		uint32_t shared_used = GetSharedUsed(port, qIndex);
		return hdrm_bytes[port][qIndex] == 0 && (shared_used == 0 || shared_used + resume_offset <= GetPfcThreshold(port));
	}
	void SwitchMmu::SetPause(uint32_t port, uint32_t qIndex){
		paused[port][qIndex] = true;
	}
	void SwitchMmu::SetResume(uint32_t port, uint32_t qIndex){
		paused[port][qIndex] = false;
	}

	uint32_t SwitchMmu::GetPfcThreshold(uint32_t port) {
		// Should have at least ~200KB in the buffer size from the test to be safe
		if(m_buffer_size.GetValue() < total_hdrm + total_rsrv + shared_used_bytes) {
			NS_LOG_WARN("Buffer size is smaller than reserved size");
			return 0;
		}

		return (m_buffer_size.GetValue() - total_hdrm - total_rsrv - shared_used_bytes) >> pfc_a_shift[port];
	}
	uint32_t SwitchMmu::GetSharedUsed(uint32_t port, uint32_t qIndex){
		uint32_t used = ingress_bytes[port][qIndex];
		return used > reserve ? used - reserve : 0;
	}
	bool SwitchMmu::ShouldSendCN(uint32_t ifindex, uint32_t qIndex)
	{
		NS_LOG_FUNCTION(this);

		if (qIndex == 0) {
			return false;
		}
		if (egress_bytes[ifindex][qIndex] > kmax[ifindex]) {
			NS_LOG_LOGIC("ECN should send: " << egress_bytes[ifindex][qIndex] << "/" << kmin[ifindex]);
			return true;
		}
		if (egress_bytes[ifindex][qIndex] > kmin[ifindex]) {
			double p = pmax[ifindex] * double(egress_bytes[ifindex][qIndex] - kmin[ifindex]) / (kmax[ifindex] - kmin[ifindex]);
			if (GenRandomDouble(0, 1) < p) {
				NS_LOG_LOGIC("ECN should send: " << egress_bytes[ifindex][qIndex] << "/" << kmin[ifindex] << ", p=" << p);
				return true;
			}
		}
		return false;
	}
	void SwitchMmu::ConfigEcn(uint32_t port, uint32_t _kmin, uint32_t _kmax, double _pmax){
		kmin[port] = _kmin * 1000;
		kmax[port] = _kmax * 1000;
		pmax[port] = _pmax;
	}
	void SwitchMmu::ConfigHdrm(uint32_t port, uint32_t size){
		headroom[port] = size;
	}
	void SwitchMmu::ConfigNPort(uint32_t n_port){
		total_hdrm = 0;
		total_rsrv = 0;
		for (uint32_t i = 1; i <= n_port; i++){
			total_hdrm += headroom[i];
			total_rsrv += reserve;
		}
	}
}
