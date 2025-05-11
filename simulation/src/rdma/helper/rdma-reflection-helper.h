#pragma once

#include "ns3/nstime.h"
#include "ns3/type-id.h"
#include "ns3/object-factory.h"
#include <rfl/json.hpp>
#include <rfl.hpp>
#include <type_traits>

/**
 * @file This file contains a bunch of utility functions for conversions between ns3 type system, JSON and C++ types.
 *
 * We use the RFL library to automatically map C++ types to JSON.
 */

namespace ns3
{

enum class PrimitiveTypeCategory {
  Bool, Integer, Double, String, Unknown
};

/*
 * Permits to categorize types:
 * - All integers (all sizes, regardless of signedness) are `Integer`.
 * - All floating-point types are `Double`.
 * - Only `std::string` is `String`.
 * - All other types are `Unknown`.
 */
template<typename V, typename T = std::decay_t<V>>
constexpr PrimitiveTypeCategory CategoryOf = 
    std::is_same_v<T, bool>                    ? PrimitiveTypeCategory::Bool
  : std::is_integral_v<T>                      ? PrimitiveTypeCategory::Integer
  : std::is_floating_point_v<T>                ? PrimitiveTypeCategory::Double
  : std::is_same_v<T, std::string>             ? PrimitiveTypeCategory::String
  : PrimitiveTypeCategory::Unknown;

using SerializedType = rfl::Variant<int64_t, double, std::string>;

} // namespace ns3

namespace rfl
{

/**
 * Specialization of RFL library to make `ns3::Time` serializable.
 *
 * Time can be stored as numeric (in seconds) or as a string.
 * Serialization always serializes as numeric.
 */
template <>
struct Reflector<ns3::Time> {

  //! Deserializes the given time.
  static ns3::Time to(const SerializedType& v) noexcept {

    // Support storing the Time in multiple different types.
    return v.visit([](const auto& underlying) {
      // Here `underlying` is the basic C++ type (int, string...) were is stored the time.
      constexpr ns3::PrimitiveTypeCategory type = ns3::CategoryOf<decltype(underlying)>; 
      constexpr bool is_numeric = (type == ns3::PrimitiveTypeCategory::Integer || type == ns3::PrimitiveTypeCategory::Double);

      // Floats and integers are converted to seconds.
      if constexpr(is_numeric) {
        return ns3::Seconds(underlying);
      }
      // String is parsed by `Time` constructor.
      else if constexpr(type == ns3::PrimitiveTypeCategory::String) {
        return ns3::Time{underlying};
      }
      // Otherwise crash.
      NS_ABORT_MSG("Cannot convert object to ns3::Time");
    });
  }

  //! Serializes the given time.
  static SerializedType from(const ns3::Time& v) {
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
 * From any structure `T` copy all the fields of `config` to the ns3 object.
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