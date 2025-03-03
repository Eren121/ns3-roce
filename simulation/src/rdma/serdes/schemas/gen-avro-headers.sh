#!/bin/bash

cd "$(dirname "$0")"

for schema in *.avsc; do
  header="../${schema%.avsc}.h"
  name=$(python3 -c "import json; print(json.load(open('$schema'))['name'])")

  avrogencpp -i "$schema" -o "$header" -n ns3

  # Embed the schema
  sed -i "/^#endif$/d" "$header"

  cat >> $header <<EOF
namespace ns3 {
  constexpr const char* GetAvroSchema(const $name* header) {
    return R"JSON($(cat $schema))JSON";
  }
} // namespace ns3
#endif
EOF

done

