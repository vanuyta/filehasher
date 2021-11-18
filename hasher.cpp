#include <string>
#include <boost/crc.hpp>
#include <boost/format.hpp>

#include "hasher.hpp"

namespace filehasher {

// Base struct for hasher implementaions.
// To add new algo: derive from this struct and select implementation dureing 'hasher' construction
struct hasher::hasher_impl
{
    friend struct hasher_crc16;
    virtual ~hasher_impl() {}

    virtual void process_bytes(const void *bytes, size_t size) = 0;
    virtual std::string result() = 0;

    //used to support copy/assign operations with main "hasher" class.
    virtual std::unique_ptr<hasher_impl> clone() const = 0;
};

// CRC16 based on Boost.CRC implementation
struct hasher_crc16 : public hasher::hasher_impl
{
    boost::crc_16_type crc;

    void process_bytes(const void *bytes, size_t size) override {
        crc.process_bytes(bytes, size);
    }
    virtual std::string result() override {
        auto res = (boost::format("%04X") % crc.checksum()).str();
        crc.reset();
        return std::move(res);
    }
    virtual std::unique_ptr<hasher_impl> clone() const override {
        return std::make_unique<hasher_crc16>(*this);
    };
};

// Only crc16 is currently implemented
hasher::hasher(hash_types) : imp(std::make_unique<hasher_crc16>())
{}

void hasher::process_bytes(const void *bytes, size_t size) {
    imp->process_bytes(bytes, size);
}

std::string hasher::result(){
    return imp->result();
}

hasher::hasher(const hasher& rhs) : imp(rhs.imp->clone())
{}

hasher& hasher::operator=(const hasher& rhs) {
    if(!(&rhs == this))
        imp = rhs.imp->clone();
    return *this;
}

hasher::hasher(hasher&& lhs) = default;
hasher& hasher::operator=(hasher&& lhs) = default;
hasher::~hasher() = default;

}//namespace filehasher
