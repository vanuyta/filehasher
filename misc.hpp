#ifndef FILEHASHER_MISC_HPP
#define FILEHASHER_MISC_HPP

#include <functional>
#include <string>
#include <stdexcept>

namespace filehasher {

struct error : public std::logic_error {
    error(const std::string& what) : std::logic_error(what) {}
};

using hash_function_t = std::function<std::string(const void *data, size_t size)>;

std::string hash_crc16(const void *data, size_t size);

} //namespce filehasher

#endif//FILEHASHER_MISC_HPP