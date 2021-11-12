#include <string>
#include <boost/crc.hpp>
#include <boost/format.hpp>

#include "misc.hpp"

namespace filehasher {

std::string hash_crc16(const void *data, size_t size) {
    boost::crc_16_type crc;
    crc.process_bytes(data, size);
    return (boost::format("%04X") % crc.checksum()).str();
}

} //namespace filehasher
