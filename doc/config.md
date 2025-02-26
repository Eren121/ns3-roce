# Configuration

Configuration is stored in JSON files containing a top-level object.

Each file path is interpreted relatively to the parent directory of the global configuration file, which is passed as argument to the program.

A JSON object can directly store ns-3 objects.
Each key maps to the attribute with the same name. the value is forwarded to the attribute constructor with the appropriate type (eg. `StringValue`, `IntegerValue`), and standard units are assumed to interpret numeric values (bits, seconds).

Examples:
- `ns3::Time` can be stored as `10` (10 seconds), or `"10s"`.
- `ns3::DataRate` can be stored as `1e9` (1 Gbps) or `"1Gbps"`.


