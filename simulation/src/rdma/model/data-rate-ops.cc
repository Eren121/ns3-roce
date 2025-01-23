#include "data-rate-ops.h"

namespace ns3 {

DataRate operator*(const double& c, const DataRate& d)
{
	return DataRate(d.GetBitRate()*c);
};

DataRate operator*(const DataRate& d, const double& c)
{
	return DataRate(d.GetBitRate()*c);
};

DataRate operator/(const DataRate& d, const double& c)
{
	return DataRate(d.GetBitRate()/c);
};

double operator/(const DataRate& lhs, const DataRate& rhs)
{
	return double(lhs.GetBitRate())/rhs.GetBitRate();
};

DataRate operator+(const DataRate& lhs, const DataRate& rhs)
{
	return DataRate(lhs.GetBitRate()+rhs.GetBitRate());
};

DataRate& operator+=(DataRate& lhs, const DataRate& rhs)
{
	lhs = lhs + rhs;
	return lhs;
}

}