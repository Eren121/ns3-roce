#pragma once

#include "ns3/nstime.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/pointer.h"
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
  :                                              PrimitiveTypeCategory::Unknown;

//! Represents a serialized C++ object, but can only represent basic types (primitives and string).
using SerializedPrimitive = rfl::Variant<int64_t, double, std::string>;

//! Represents any serialized JSON object.
using SerializedJsonObject = rfl::Object<rfl::Generic>;

//! Represents any serialized JSON object, or a primitive type.
using SerializedJsonAny = rfl::Generic;

} // namespace ns3

namespace rfl
{

/**
 * Specialization of RFL library to make `ns3::Time` serializable.
 *
 * Time can be stored as numeric (in seconds) or as a string.
 * Serialization always serializes as numeric.
 *
 * @see https://rfl.getml.com/custom_parser/.
 */
template <>
struct Reflector<ns3::Time> {

  //! The specialization of `Reflector` must have `ReflType` as member.
  using ReflType = ns3::SerializedPrimitive;

  //! Deserializes the given time.
  static ns3::Time to(const ns3::SerializedPrimitive& v) noexcept {

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
  static ns3::SerializedPrimitive from(const ns3::Time& v) {
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

/**
 * @{
 * Sets all attributes from a configuration to a ns3 object.
 * @param ns3_obj The ns3 object ot modify the attributes.
 * @param attributes The key-value serialized JSON object to apply the attributes.
 *
 * Crashs if an attribute does not exist in the ns3 object.
 */
void PopulateAttributes(ObjectBase& ns3_obj, const SerializedJsonObject& attributes);

/**
 * Wrapper that intelligentely extract C++ fields of the POD `attributes` dynamically.
 */
template<typename T>
void PopulateAttributes(ObjectBase& ns3_obj, const T& attributes)
{
  PopulateAttributes(ns3_obj, rfl::to_generic(attributes).to_object().value());
}
//! @}

/**
 * @{
 * Modifies the factory so any object created with the factory will have the attributes given in `attributes`.
 */
void PopulateAttributes(ObjectFactory& ns3_factory, const SerializedJsonObject& attributes);

/**
 * Wrapper that intelligentely extract C++ fields of the POD `attributes` dynamically.
 */
template<typename T>
void PopulateAttributes(ObjectFactory& factory, const T& config)
{
  PopulateAttributes(factory, rfl::to_generic(config).to_object().value());
}
//! @}

/**
 * Converts any serialized value to an attribute value.
 * Crashs if it's impossible.
 * The resulting attribute value can be used to set the attribute `info`.
 *
 * @param info Hint that helps to interpret the serialized value.
 * For example, if `obj` is numeric but `info` is a Time,
 * this will interpret the numeric value as an amount of seconds.
 */
Ptr<AttributeValue> ConvertJsonToAttribute(const SerializedJsonAny& obj, const TypeId::AttributeInformation& info = {});

/**
 * Finds a ns3 attribute globally from the full name of the attribute.
 * Crashs if the attribute is not found.
 * @return Attribute information from the full name.
 */
TypeId::AttributeInformation FindConfigAttribute(const std::string& fullName);

/**
 * Adds a Time attribute by typing less.
 */
template<typename T, typename F>
void AddTimeAttribute(TypeId& tid,
  const char* const field_name,
  const char* const field_description,
  F T::*field)
{
  tid.AddAttribute(
    field_name,
    field_description,
    TimeValue(),
    MakeTimeAccessor(field),
    MakeTimeChecker());
}

/**
 * Adds a String attribute by typing less.
 */
template<typename T, typename F>
void AddStringAttribute(TypeId& tid,
  const char* const field_name,
  const char* const field_description,
  F T::*field)
{
  tid.AddAttribute(
    field_name,
    field_description,
    StringValue(),
    MakeStringAccessor(field),
    MakeStringChecker());
}


/**
 * Adds a Boolean attribute by typing less.
 */
template<typename T, typename F>
void AddBooleanAttribute(TypeId& tid,
  const char* const field_name,
  const char* const field_description,
  F T::*field,
  bool default_value = false)
{
  tid.AddAttribute(
    field_name,
    field_description,
    BooleanValue(default_value),
    MakeBooleanAccessor(field),
    MakeBooleanChecker());
}

/**
 * Adds an unsigned integer attribute by typing less.
 */
template<typename T, typename F>
void AddUintegerAttribute(TypeId& tid,
  const char* const field_name,
  const char* const field_description,
  F T::*field)
{
  tid.AddAttribute(
    field_name,
    field_description,
    UintegerValue(0),
    MakeUintegerAccessor(field),
    MakeUintegerChecker<F>());
}

/**
 * Adds an object attribute by typing less.
 */
template<typename T, typename F>
void AddObjectAttribute(TypeId& tid,
  const char* const field_name,
  const char* const field_description,
  F T::*field)
{
  tid.AddAttribute(
    field_name,
    field_description,
    PointerValue(),
    MakePointerAccessor<F>(field),
    MakePointerChecker<F>());
}

} // namespace ns3