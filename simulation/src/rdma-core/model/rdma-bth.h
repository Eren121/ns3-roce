#ifndef RDMA_BTH_H
#define RDMA_BTH_H

#include <ns3/object.h>
#include <ns3/tag.h>

namespace ns3 {

// Minimal Base Transport Header (BTH).
// Stores necessary informations for the BTH.
// We don't add a field to `CustomHeader` to avoid modify the size of the packet.
class RdmaBTH : public Tag
{
public:
	static TypeId GetTypeId();
  	TypeId GetInstanceTypeId() const override;
  	void Print(std::ostream &os) const override;
  	uint32_t GetSerializedSize() const override;
  	void Serialize(TagBuffer start) const override;
  	void Deserialize(TagBuffer start) override;

	bool GetReliable() const;
	void SetReliable(bool reliable);
	bool GetAckReq() const;
	void SetAckReq(bool ack_req);
	bool GetMulticast() const;
	void SetMulticast(bool multicast);
	bool GetNotif() const;
	void SetNotif(bool notif);
	void SetImm(uint32_t imm);
	uint32_t GetImm() const;
	uint16_t GetDestQpKey() const { return m_key; }
	void SetDestQpKey(uint16_t key) { m_key = key; }

private:
	// true: RC, false: UD.
	bool m_reliable{true};
	bool m_ack_req{false}; //<! When `true`, the sender requests explicitly an ACK.
	bool m_multicast{false}; //<! When `true`, the destination is a multicast group.
	bool m_notif{false}; //<! When `true`, a notification event is generated in the RX side.
	uint32_t m_imm{0};
	uint16_t m_key{0}; // Destination QP key.
};

}

#endif /* RDMA_BTH_H */