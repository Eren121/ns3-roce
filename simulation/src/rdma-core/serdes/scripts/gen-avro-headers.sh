#!/bin/bash

# Exit on any error
set -e

# Jump to current directory.
cd "$(dirname "$0")"

cpp_dir='../generated'
avsc_dir='../schemas'

# Generate a `.h` containing the corresponding serializable datatype for each `.avsc` in this directory.
for schema_path in $avsc_dir/*.avsc; do
  schema_filename=$(basename "$schema_path")

  # Filename to generate.
  header_path="$cpp_dir/${schema_filename%.avsc}.h"

  # C++ class name of the serializable datatype.
  cpp_class=$(python3 -c "import json; print(json.load(open('$schema_path'))['name'])")

  echo "--"
  echo "Schema: $(realpath "$schema_path")"
  echo "Generated header: $(realpath "$header_path")"
  echo "Class name: $cpp_class"

  # Generate the Avro C++ bindings in the `ns3` namespace.
  avrogencpp -i "$schema_path" -o "$header_path" -n ns3

  # Embed the JSON schema in the C++ source by modifying the generated C++ header.
  # We simply insert before `#endif` a function that takes a dummy parameter to disambiguiate the type
  # and return the JSON schema as a C string.
  # We don't use template for simplicity, which would necessitate to include a header for the non-specialized function.

  # Delete the `#endif` (assumes there is only one!).
  sed -i "/^#endif$/d" "$header_path"

  # Insert the utility function and the `#endif` back.
  cat >> $header_path <<EOF
namespace ns3 {
  constexpr const char* GetAvroSchema(const $cpp_class* header) {
    return R"JSON($(cat $schema_path))JSON";
  }
} // namespace ns3
#endif
EOF

done

echo "--"

