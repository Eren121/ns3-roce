#pragma once

#include "ns3/nstime.h"
#include "ns3/type-id.h"
#include "ns3/object-factory.h"
#include <rfl/json.hpp>
#include <rfl.hpp>
#include <type_traits>

namespace ns3
{

enum class RflType {
  Bool, Integer, Double, String, Unknown
};

template<typename V, typename T = std::decay_t<V>>
constexpr RflType rflTypeOf = 
    std::is_same_v<T, bool>                    ? RflType::Bool
  : std::is_integral_v<T>                      ? RflType::Integer
  : std::is_floating_point_v<T>                ? RflType::Double
  : std::is_same_v<T, std::string>             ? RflType::String
  : RflType::Unknown;
}

namespace rfl
{

/**
 * Make `ns3::Time` usable with `rfl` reflection.
 * Allow `ns3::Time` to be stored as numeric (seconds) or string.
 * Serialization stores as numeric.
 */
template <>
struct Reflector<ns3::Time> {
  using ReflType = rfl::Variant<int64_t, double, std::string>;

  static ns3::Time to(const ReflType& v) noexcept {
    return v.visit([](const auto& underlying) {
      constexpr ns3::RflType type = ns3::rflTypeOf<decltype(underlying)>; 
      
      if constexpr(type == ns3::RflType::Integer
                || type == ns3::RflType::Double) {
        return ns3::Seconds(underlying);
      }
      else if constexpr(type == ns3::RflType::String) {
        return ns3::Time{underlying};
      }

      NS_ABORT_MSG("Cannot convert object to ns3::Time");
    });
  }

  static ReflType from(const ns3::Time& v) {
    return v.GetSeconds();
  }
};

} // namespace rfl

namespace ns3
{

/**
 * @return true If the real value can be exactly represented as an integer.
 */
bool IsExactInteger(double value);

void PopulateAttributes(ObjectBase& obj, const rfl::Object<rfl::Generic>& config);
void PopulateAttributes(ObjectFactory& factory, const rfl::Object<rfl::Generic>& config);

/**
 * From any structure `T` copy all the fields to the ns3 object.
 */
template<typename T>
void PopulateAttributes(ObjectBase& obj, const T& config)
{
  PopulateAttributes(obj, rfl::to_generic(config).to_object().value());
}

template<typename T>
void PopulateAttributes(ObjectFactory& factory, const T& config)
{
  PopulateAttributes(factory, rfl::to_generic(config).to_object().value());
}

/**
 * @param info Hint to interpret the Json value.
 */
Ptr<AttributeValue> ConvertJsonToAttribute(const rfl::Generic& obj, const TypeId::AttributeInformation& info = {});

/**
 * @return Attribute information from the full name.
 */
TypeId::AttributeInformation FindConfigAttribute(const std::string& fullName);

} // namespace ns3