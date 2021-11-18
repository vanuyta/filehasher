#ifndef FILEHASHER_HASHER_HPP
#define FILEHASHER_HASHER_HPP

#include <string>
#include <memory>

namespace filehasher {

// Implements hashing algorithm
// Only CRC16 currently implemented. Implementation is used Boost.CRC library
// Implementation detailes are hided using `pimpl`
class hasher {
public:
    enum class hash_types {crc_16};

    explicit hasher(hash_types);
    void process_bytes(const void *bytes, size_t size);
    std::string result();

    hasher(const hasher& lhs);
    hasher& operator = (const hasher& lhs);
    hasher(hasher&& lhs);
    hasher& operator=(hasher&& lhs);
    ~hasher();

    struct hasher_impl;
private:
    std::unique_ptr<hasher_impl> imp;
};

}//namespce filehasher

#endif//FILEHASHER_HASHER_HPP