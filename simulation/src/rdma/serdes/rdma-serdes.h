#pragma once

#include "ns3/rdma-helper.h"
#include <avro/Compiler.hh>
#include <avro/DataFile.hh>
#include <avro/Decoder.hh>
#include <avro/Encoder.hh>
#include <avro/ValidSchema.hh>
#include <string>

namespace ns3 {

/**
 * Helper to write data to files for later analysis.
 */
template<typename T>
class RdmaSerializer
{
public:
  /**
   * Create a writer to an avro output file.
   * Truncate if the file already exist.
   */
  RdmaSerializer(const fs::path& output_path)
    : m_schema{LoadSchema(GetAvroSchema(static_cast<T*>(nullptr)))},
      m_writer{output_path.c_str(), m_schema}
  {
  }

  /**
   * Write a record to the file.
   */
  void write(const T& record)
  {
    m_writer.write(record);
  }

private:
  static avro::ValidSchema LoadSchema(const char *json)
  {
    std::string str{json};
    std::istringstream is(str);
    avro::ValidSchema result;
    avro::compileJsonSchema(is, result);
    return result;
  }

private:
  avro::ValidSchema m_schema;
  avro::DataFileWriter<T> m_writer;
};

} // namespace ns3