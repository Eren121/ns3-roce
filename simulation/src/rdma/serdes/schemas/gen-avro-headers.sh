#!/bin/bash

# Jump to current directory.
cd "$(dirname "$0")"

# Generate a `.h` containing the corresponding serializable datatype for each `.avsc` in this directory.
for schema in *.avsc; do

  # Filename to generate.
  header="../${schema%.avsc}.h"

  # C++ class name of the serializable datatype.
  cpp_class=$(python3 -c "import json; print(json.load(open('$schema'))['name'])")

  # Generate the Avro C++ bindings in the `ns3` namespace.
  avrogencpp -i "$schema" -o "$header" -n ns3

  # Embed the JSON schema in the C++ source by modifying the generated C++ header.
  # We simply insert before `#endif` a function that takes a dummy parameter to disambiguiate the type
  # and return the JSON schema as a C string.
  # We don't use template for simplicity, which would necessitate to include a header for the non-specialized function.

  # Delete the `#endif` (assumes there is only one!).
  sed -i "/^#endif$/d" "$header"

  # Insert the utility function and the `#endif` back.
  cat >> $header <<EOF
namespace ns3 {
  constexpr const char* GetAvroSchema(const $cpp_class* header) {
    return R"JSON($(cat $schema))JSON";
  }
} // namespace ns3
#endif
EOF

done

