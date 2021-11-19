#ifndef FILEHASHER_COMMONDEFS_HPP
#define FILEHASHER_COMMONDEFS_HPP

namespace filehasher {

struct error : public std::logic_error {
    error(const std::string& what) : std::logic_error(what) {}
};

// Dummy limit for result set...
// In some cases (when we process 10GB file by 2B chunks for example) - several million hashes can be produced.
// To provide ordered results - we need to store all results in memmory
// This cam be fixed with "external" sorting implementation. But it will involeves more I/O operations
/// TODO: Implement external sorting (or find one);
inline const size_t results_limit       = 100000;

// Dummy limit for queue of chanks to be processed...
// It will unlikely affect perfomance - optimal number of parralel computations = H/W threaded supported.
// This limit overlaps this value - so workers will not spend too many time waiting for job
// But in also help to avoid rnning memmory out.
inline const size_t queue_limit         = 1000;

// This limit will be used to adjust queue size when large blocks will be requested.
// Programm will try to determine queue size to fit all blocks waiting to be processed in to the limit.
// If requested block is large then this limit - synchronous sequetila processing will be done.
// So, one block will be calculated synchronously using chanks equal to this size.
// When using "mapping" aproach - this limit is ignored.
inline const size_t soft_memmory_limit  = 1024 * 1024 * 1024; // 1GB

// Buffer size that will be used if hash calculation process will fallback to synchronous mode.
// This can happens when requested block size is  greater then soft_memmory_limit / 2 .
// Or when only one block should be calculated.
inline const size_t sync_buffer_size  = 1024 * 1024 * 10; // 10MB

}//namespace filehasher

#endif//FILEHASHER_COMMONDEFS_HPP