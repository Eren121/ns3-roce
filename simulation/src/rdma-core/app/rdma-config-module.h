#pragma once

#include "ns3/object.h"

namespace ns3 {

class RdmaNetwork;

class RdmaConfigModule : public Object
{
public:
    static TypeId GetTypeId();

    //! Called when the module is loaded and attributes are initialized.
    virtual void OnModuleLoaded(RdmaNetwork& network) = 0;
};

} // namespace ns3