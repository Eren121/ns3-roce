#pragma once

#include <ns3/data-rate.h>

namespace ns3 {
    
double operator* (const DataRate& lhs, const Time& rhs);
double operator* (const Time& lhs, const DataRate& rhs);

DataRate operator*(const double& c, const DataRate& d);
DataRate operator*(const DataRate& d, const double& c);

DataRate operator/(const DataRate& d, const double& c);
double operator/(const DataRate& lhs, const DataRate& rhs);

DataRate operator+(const DataRate& lhs, const DataRate& rhs);
DataRate& operator+=(DataRate& lhs, const DataRate& rhs);

}