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

private:
	// true: RC, false: UD.
	bool m_reliable;
};

}

#endif /* RDMA_BTH_H */