#include "src/stirling/cassandra/type_decoder.h"

#include <algorithm>
#include <utility>

#include "src/common/base/byte_utils.h"

namespace pl {
namespace stirling {
namespace cass {

// These Extract functions parse raw byte sequences into CQL types.
// Note that the CQL protocol is big-endian, so all extract functions
// also intrinsically convert from big-endian to host ordering.

template <typename TIntType>
StatusOr<TIntType> TypeDecoder::ExtractIntCore() {
  if (buf_.size() < sizeof(TIntType)) {
    return error::ResourceUnavailable("Insufficient number of bytes");
  }
  TIntType val = utils::BEndianBytesToInt<TIntType>(buf_);
  buf_.remove_prefix(sizeof(TIntType));
  return val;
}

template <typename TFloatType>
StatusOr<TFloatType> ExtractFloatCore(std::string_view* buf) {
  if (buf->size() < sizeof(TFloatType)) {
    return error::ResourceUnavailable("Insufficient number of bytes");
  }
  TFloatType val = utils::BEndianBytesToFloat<TFloatType>(*buf);
  buf->remove_prefix(sizeof(TFloatType));
  return val;
}

template <typename TCharType>
StatusOr<std::basic_string<TCharType>> TypeDecoder::ExtractBytesCore(int64_t len) {
  if (static_cast<ssize_t>(buf_.size()) < len) {
    return error::ResourceUnavailable("Insufficient number of bytes");
  }

  // TODO(oazizi): Optimization when input and output types match: no need for tbuf.
  auto tbuf = CreateStringView<TCharType>(buf_);
  std::basic_string<TCharType> str(tbuf.substr(0, len));
  buf_.remove_prefix(len);
  return str;
}

template <typename TCharType, size_t N>
Status TypeDecoder::ExtractBytesCore(TCharType* out) {
  if (buf_.size() < N) {
    return error::Internal("Insufficient number of bytes");
  }

  // TODO(oazizi): Optimization when input and output types match: no need for tbuf.
  auto tbuf = CreateStringView<TCharType>(buf_);
  memcpy(out, tbuf.data(), N);
  buf_.remove_prefix(N);
  return Status::OK();
}

// [int] A 4 bytes signed integer
StatusOr<int32_t> TypeDecoder::ExtractInt() { return ExtractIntCore<int32_t>(); }

// [long] A 8 bytes signed integer
StatusOr<int64_t> TypeDecoder::ExtractLong() { return ExtractIntCore<int64_t>(); }

// [short] A 2 bytes unsigned integer
StatusOr<uint16_t> TypeDecoder::ExtractShort() { return ExtractIntCore<uint16_t>(); }

// [byte] A 2 bytes unsigned integer
StatusOr<uint8_t> TypeDecoder::ExtractByte() { return ExtractIntCore<uint8_t>(); }

// [float]
StatusOr<float> ExtractFloat(std::string_view* buf) { return ExtractFloatCore<float>(buf); }

// [double]
StatusOr<double> ExtractDouble(std::string_view* buf) { return ExtractFloatCore<double>(buf); }

// [string] A [short] n, followed by n bytes representing an UTF-8 string.
StatusOr<std::string> TypeDecoder::ExtractString() {
  PL_ASSIGN_OR_RETURN(uint16_t len, ExtractShort());
  return ExtractBytesCore<char>(len);
}

// [long string] An [int] n, followed by n bytes representing an UTF-8 string.
StatusOr<std::string> TypeDecoder::ExtractLongString() {
  PL_ASSIGN_OR_RETURN(int32_t len, ExtractInt());
  len = std::max(len, 0);
  return ExtractBytesCore<char>(len);
}

// [uuid] A 16 bytes long uuid.
StatusOr<sole::uuid> TypeDecoder::ExtractUUID() {
  sole::uuid uuid;

  // Logically, we want to get the different components of the UUID, and ensure correct byte-order.
  // For example, see datastax:
  // https://github.com/datastax/cpp-driver/blob/bbbbd7bc3eaba1b10ad8ac6f53c41fa93ee718db/src/serialization.hpp
  // They do it in components, because each component is big-endian ordered.
  // The ordering of bytes for the entire UUID is effectively:
  //   input:  {15 ...........  8  7  6  5  4  3  2  1  0}
  //   output: {8 ............ 15}{6  7}{4  5}{0  1  2  3}
  //
  // Equivalent code would be:
  //   PL_ASSIGN_OR_RETURN(uint64_t time_low, ExtractInt(buf));
  //   PL_ASSIGN_OR_RETURN(uint64_t time_mid, ExtractShort(buf));
  //   PL_ASSIGN_OR_RETURN(uint64_t time_hi_version, ExtractShort(buf));
  //   PL_ASSIGN_OR_RETURN(uint64_t clock_seq_and_node, ExtractLong(buf));
  //
  // But then we constitute the components according to the following formula,
  // from uuid1() in sole.hpp:
  //
  //   uuid.ab = (time_low << 32) | (time_mid << 16) | time_hi_version;
  //   uuid.cd = clock_seq_and_node;
  //
  // But we notice that the outcome of all this is:
  //   uuid.ab = {0  1  2  3}{4  5}{6  7}
  //   uuid.cd = {8 ................. 15}
  //
  // And we realize that we can achieve this directly with the following shortcut:

  PL_ASSIGN_OR_RETURN(uuid.ab, ExtractLong());
  PL_ASSIGN_OR_RETURN(uuid.cd, ExtractLong());

  return uuid;
}

// [string list] A [short] n, followed by n [string].
StatusOr<StringList> TypeDecoder::ExtractStringList() {
  PL_ASSIGN_OR_RETURN(uint16_t n, ExtractShort());

  StringList string_list;
  for (int i = 0; i < n; ++i) {
    PL_ASSIGN_OR_RETURN(std::string s, ExtractString());
    string_list.push_back(std::move(s));
  }

  return string_list;
}

// [bytes] A [int] n, followed by n bytes if n >= 0. If n < 0,
//         no byte should follow and the value represented is `null`.
StatusOr<std::basic_string<uint8_t>> TypeDecoder::ExtractBytes() {
  PL_ASSIGN_OR_RETURN(int32_t len, ExtractInt());
  len = std::max(len, 0);
  return ExtractBytesCore<uint8_t>(len);
}

// [short bytes]  A [short] n, followed by n bytes if n >= 0.
StatusOr<std::basic_string<uint8_t>> TypeDecoder::ExtractShortBytes() {
  PL_ASSIGN_OR_RETURN(uint16_t len, ExtractShort());
  return ExtractBytesCore<uint8_t>(len);
}

// [inet] An address (ip and port) to a node. It consists of one
//        [byte] n, that represents the address size, followed by n
//        [byte] representing the IP address (in practice n can only be
//        either 4 (IPv4) or 16 (IPv6)), following by one [int]
//        representing the port.
StatusOr<SockAddr> TypeDecoder::ExtractInet() {
  PL_ASSIGN_OR_RETURN(uint8_t n, ExtractByte());

  SockAddr addr;

  switch (n) {
    case 4: {
      addr.family = SockAddrFamily::kIPv4;
      addr.addr = in_addr{};
      PL_RETURN_IF_ERROR((ExtractBytesCore<uint8_t, 4>(reinterpret_cast<uint8_t*>(&addr.addr))));
    } break;
    case 16: {
      addr.family = SockAddrFamily::kIPv6;
      addr.addr = in6_addr{};
      PL_RETURN_IF_ERROR((ExtractBytesCore<uint8_t, 16>(reinterpret_cast<uint8_t*>(&addr.addr))));
    } break;
  }

  PL_ASSIGN_OR_RETURN(addr.port, ExtractInt());

  return addr;
}

// [string map] A [short] n, followed by n pair <k><v> where <k> and <v>
//              are [string].
StatusOr<StringMap> TypeDecoder::ExtractStringMap() {
  PL_ASSIGN_OR_RETURN(uint16_t n, ExtractShort());

  StringMap string_map;
  for (int i = 0; i < n; ++i) {
    PL_ASSIGN_OR_RETURN(std::string key, ExtractString());
    PL_ASSIGN_OR_RETURN(std::string val, ExtractString());
    string_map.insert({std::move(key), std::move(val)});
  }

  return string_map;
}

// [string multimap] A [short] n, followed by n pair <k><v> where <k> is a
//                   [string] and <v> is a [string list].
StatusOr<StringMultiMap> TypeDecoder::ExtractStringMultiMap() {
  PL_ASSIGN_OR_RETURN(uint16_t n, ExtractShort());

  StringMultiMap string_multimap;
  for (int i = 0; i < n; ++i) {
    PL_ASSIGN_OR_RETURN(std::string key, ExtractString());
    PL_ASSIGN_OR_RETURN(StringList val, ExtractStringList());
    string_multimap.insert({std::move(key), std::move(val)});
  }

  return string_multimap;
}

StatusOr<Option> TypeDecoder::ExtractOption() {
  Option col_spec;
  PL_ASSIGN_OR_RETURN(uint16_t id, ExtractShort());
  col_spec.type = static_cast<DataType>(id);
  if (col_spec.type == DataType::kCustom) {
    PL_ASSIGN_OR_RETURN(col_spec.value, ExtractString());
  }
  return col_spec;
}

StatusOr<std::vector<Option>> TypeDecoder::ExtractOptionList() {
  PL_ASSIGN_OR_RETURN(uint16_t n, ExtractShort());

  std::vector<Option> options;
  for (uint32_t i = 0; i < n; ++i) {
    PL_ASSIGN_OR_RETURN(Option option, ExtractOption());
    options.push_back(std::move(option));
  }
  return options;
}

}  // namespace cass
}  // namespace stirling
}  // namespace pl
