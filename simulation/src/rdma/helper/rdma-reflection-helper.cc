#include "ns3/rdma-reflection-helper.h"
#include "ns3/object.h"
#include "ns3/boolean.h"
#include "ns3/integer.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"

namespace ns3 {

void PopulateAttributes(ObjectBase& obj, const rfl::Object<rfl::Generic>& config)
{
  const TypeId& type{obj.GetInstanceTypeId()};

  for (const auto& [attr_name, attr_generic] : config) {
    TypeId::AttributeInformation attr_info;
    if(!type.LookupAttributeByName(attr_name, &attr_info)) {
      NS_ABORT_MSG("Cannot find attribute " << attr_name << " in " << type.GetName());
    }

    attr_info.accessor->Set(&obj, *ConvertJsonToAttribute(attr_generic, attr_info));
  }
}

void PopulateAttributes(ObjectFactory& factory, const rfl::Object<rfl::Generic>& config)
{
  const TypeId& type{factory.GetTypeId()};

  for (const auto& [attr_name, attr_generic] : config) {
    TypeId::AttributeInformation attr_info;
    if(!type.LookupAttributeByName(attr_name, &attr_info)) {
      NS_ABORT_MSG("Cannot find attribute " << attr_name << " in " << type.GetName());
    }

    factory.Set(attr_name, *ConvertJsonToAttribute(attr_generic, attr_info));
  }
}

bool IsExactInteger(double value)
{
  [[maybe_unused]] double int_part;
  return {modf(value, &int_part) == 0.0};
}

Ptr<AttributeValue> ConvertJsonToAttribute(const rfl::Generic& obj, const TypeId::AttributeInformation& info)
{
  std::string type;
  if(info.checker) {
    type = info.checker->GetValueTypeName();
  }

  return std::visit([&](const auto& value) -> Ptr<AttributeValue> {

    using JsonFieldType = std::decay_t<decltype(value)>;

    // At most one of the `is_*` is true
    constexpr bool is_bool{std::is_same_v<JsonFieldType, bool>};
    constexpr bool is_int{std::is_integral_v<JsonFieldType> && !is_bool};
    constexpr bool is_real{std::is_floating_point_v<JsonFieldType>};
    constexpr bool is_str{std::is_same_v<JsonFieldType, std::string>};
    
    // Special cases, conversions

    if(type == "ns3::TimeValue") {
      if constexpr(is_int || is_real) {
        // Allows to store seconds as numeric
        return Create<TimeValue>(Seconds(value));
      }
    }
    else if(type == "ns3::UintegerValue") {
      // The type checking is fickle,
      // IntegerValue is not compatible with UintegerValue.
      if constexpr(is_int || is_real) {
        NS_ABORT_MSG_IF(value < 0, "Value should be positive");
        // NS_ABORT_MSG_IF(!IsExactInteger(value), "Value should be an integer");
        return Create<UintegerValue>(value);
      }
    }
    else if(type == "ns3::IntegerValue") {
      if constexpr(is_int || is_real) {
        // NS_ABORT_MSG_IF(!IsExactInteger(value), "Value should be an integer");
        return Create<IntegerValue>(value);
      }
    }
    else if(type == "ns3::DoubleValue") {
      if constexpr(is_int) {
        return Create<DoubleValue>(value);
      }
    }

    // Normal cases, fallback, no conversion

    if constexpr(is_bool) {
      return Create<BooleanValue>(value); // before integer because `is_integral_v` is valid for `bool`
    }
    if constexpr(is_int) {
      return Create<IntegerValue>(value);
    }
    else if constexpr(is_real) {
      return Create<DoubleValue>(value);
    }
    else if constexpr(is_str) {
      return Create<StringValue>(value);
    }
    else {
      NS_ABORT_MSG("Unsupported attribute field type");
      return Create<EmptyAttributeValue>();
    }
  }, obj.get());
}

TypeId::AttributeInformation FindConfigAttribute(const std::string& fullName)
{
  // Mainly copied from `Config::SetDefaultFailSafe()`
  
  std::string::size_type pos = fullName.rfind ("::");
  NS_ABORT_MSG_IF(pos == std::string::npos, "fullName should contain '::'");

  std::string tidName = fullName.substr (0, pos);
  std::string paramName = fullName.substr (pos + 2, fullName.size () - (pos + 2));
  TypeId tid = TypeId::LookupByName(tidName);
  
  TypeId::AttributeInformation info;
  tid.LookupAttributeByName(paramName, &info);
  for (uint32_t j = 0; j < tid.GetAttributeN (); j++) {
    TypeId::AttributeInformation tmp = tid.GetAttribute (j);
    if (tmp.name == paramName)
    {
      return tmp;
    }
  }
  NS_ABORT_MSG("Cannot find field " << fullName);
}
  
} // namespace ns3