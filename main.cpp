#include <iostream>
#include <fstream>
#include <chrono>
#include <set>
#include <filesystem>
#include <boost/interprocess/managed_mapped_file.hpp>

#include "commondefs.hpp"
#include "options.hpp"
#include "threading.hpp"

using namespace filehasher;

// Defines hash calculation result, that contains chank number in file and its hash value.
struct result_t {
    size_t      cunk_number;
    std::string hash;
};

// As ordered result is allowed - specify 'std::less' to make it possible to store results in ordered containers.
namespace std {
    template<> struct less<result_t>
    {
       bool operator() (const result_t& lhs,const result_t& rhs) const
       {
           return lhs.cunk_number < rhs.cunk_number;
       }
    };
}

// Type of function that can be used to process result.
// Currently to options exists:
//  - process results 'on the flygth'  (results will be directly written to output)
//  - process oredered results (accumulate, sort, write at the and).
using resulter_function_t = std::function<void(result_t&& r)>;


// Do the work using stream reading from input file.
// Producer (main thread) will reads chunks one by one and put them to the input chanel of workers pool.
// Max memmory usage is limeted with Options.QueueSize.
static void do_with_streaming(Options opts, hasher hash, const resulter_function_t& rfunc) {
    struct job_t {
        size_t              chunk_number{0};
        std::vector<char>   chank;
    };

    piped_workers_pool<job_t, result_t>
    workers (opts.Workers, opts.QueueSize, [hash](job_t&& job) mutable {
        hash.process_bytes(job.chank.data(), job.chank.size());
        return result_t{job.chunk_number, hash.result()};
    });
    
    piped_workers_pool<result_t>
    resulter (1, opts.QueueSize, workers, [&rfunc](result_t&& result) {
        rfunc(std::move(result));
    });

    // input - entry point to the pipe of worker pools.
    // All jobs should be written in it.
    auto input = workers.get_input_chan();

    // terminator - is the last chanel in the pipe.
    // If it is closed before all the job is done - something wrong happend. Producer should break and "wait" waorkers to get exception.
    auto terminator = resulter.get_output_chan();

    std::ifstream ifile(opts.InputFile, std::ifstream::binary);
    if(!ifile)
        throw error("failed to open file [" + opts.InputFile + "]");

    for (size_t i=0; ifile && !terminator->is_closed(); i++) {
        std::vector<char> buff(opts.BlockSize);
        ifile.read(buff.data(), buff.size());
        size_t readed = ifile.gcount();
        if(readed == 0)
            break;
        buff.resize(readed);
        if (!input->push(std::move(job_t{i, std::move(buff)})))
            break;
    }

    // Any exceptions from workers will be raised here
    input->close();
    workers.wait();
    resulter.wait();
}

// Do the work using "mmap" aproach.
// Producer (main thread) will map whole file to virtual memmory and pushh memmory segments to input chanel of workers pool.
// No need to limit memmory usage. Options.QueueSize has its maximum value.
static void do_with_mapping(filehasher::Options opts, hasher& hash, const resulter_function_t& rfunc) {
    namespace bi = boost::interprocess;
    struct job_t {
        size_t      chunk_number    {0};
        size_t      size            {0};
        const void  *addr           {nullptr};
    };

    piped_workers_pool<job_t, result_t>
    workers (opts.Workers, opts.QueueSize, [&hash](const job_t& job) {
        hash.process_bytes(job.addr, job.size);
        return result_t{job.chunk_number, hash.result()};
    });
    
    piped_workers_pool<result_t>
    resulter (1, opts.QueueSize, workers, [&rfunc](result_t&& result){
        rfunc(std::move(result));
    });

    try {
        // input - entry point to the pipe of worker pools.
        // All jobs should be written in it.
        auto input = workers.get_input_chan();

        // terminator - is the last chanel in the pipe.
        // If it is closed before all the job is done - something wrong happend. Producer should break and "wait" waorkers to get exception.
        auto terminator = resulter.get_output_chan();
    
        bi::file_mapping ifile(opts.InputFile.c_str(), bi::read_only);
        bi::mapped_region region(ifile, bi::read_only);

        size_t size = region.get_size();
        const void* addr = region.get_address();
        for (size_t i = 0, num = 0; i < size && !terminator->is_closed(); i += opts.BlockSize) {
            if(!input->push(job_t{num++,std::min((size_t)opts.BlockSize, size - i), (const char*)addr + i}))
                break;
        }

        // Any exceptions from workers will be raised here
        input->close();
        workers.wait();
        resulter.wait();
    } catch (const bi::interprocess_exception& e) {
        throw error("failed to map file [" + opts.InputFile + "]: " + e.what());
    }
}

// Store results to provided container (will be ordered).
// Results will be written at the and of execution.
void process_ordered_results(result_t&& result, std::multiset<result_t>& dst) {
    if(dst.size() >= results_limit)
        throw error("too many results (try unordered output)");
    dst.insert(std::move(result));
}

// Just write unordered chunks directly to provided output stream...
void process_unordered_results(result_t&& result, std::ostream& dst) {
    dst << result.cunk_number << ": " << result.hash << std::endl;
}

int main(int argc, char *argv[]) {

    try {
        Options opts;

        opts = ParseCommandLine(argc, argv);

        // If 'help' was requested - print usage
        if(opts.Cmd == Command::help) {
            WriteUsage(std::cout);
            return 0;
        }

        // Select output stream depending on 'OutputFile' options flag.
        std::ofstream ofile;
        if(!opts.OutputFile.empty()) {
            ofile.open(opts.OutputFile, std::ifstream::trunc);
            if(!ofile) throw error("failed to open output file [" + opts.OutputFile + "]");
        }
        std::ostream& output = ofile.is_open() ? ofile : std::cout;

        // Select result processing method depending on 'Sorted' options flag.
        std::multiset<result_t> results;
        resulter_function_t rfunc;
        if (opts.Sorted) {
            rfunc = [&results](result_t&& r) { process_ordered_results(std::move(r), results);};
        } else {
            rfunc = [&output](result_t&& r) { process_unordered_results(std::move(r), output);};
        }

        // Get hashing function (only CRC16 with Boost implementation is supported).
        auto hash = GetHasher(opts);

        // Adjust workers count dending on file (we dont need more workers than count of blocks to be processed).
        size_t fsize{0};
        try {
            fsize = std::filesystem::file_size(opts.InputFile);
            opts.Workers = std::min(opts.Workers, (fsize/opts.BlockSize) + (fsize%opts.BlockSize ? 1 : 0));
        } catch(const std::filesystem::filesystem_error& e) {
            throw error("faild to get file [" + opts.InputFile + "] size: " + e.what());
        }
        size_t blocks_count = fsize / opts.BlockSize + (fsize % opts.BlockSize ? 1 : 0);
        opts.Workers = std::min(opts.Workers, blocks_count);

        // Check memory limits and calculate queue size.
        // For mapping mode - use max queue size (blocks will not occupie phisical RAM).
        opts.QueueSize = opts.Mapping ?  queue_limit : std::min(soft_memmory_limit / opts.BlockSize, queue_limit);

        std::cout << "Running..." << std::endl;
        auto stime = std::chrono::high_resolution_clock::now();

        // Select input file reading mode (streamed/maped) depending on 'Mapping' options flag.
        if(opts.Mapping)
            do_with_mapping(opts, hash, rfunc);
        else
            do_with_streaming(opts, hash, rfunc);

        // If orderd output was selected - flush it.
        if (opts.Sorted) {
            for (auto&& r : results) {
                output << r.cunk_number << ": " << r.hash << std::endl;
            }
        }

        auto etime = std::chrono::high_resolution_clock::now();
        std::cout << "Done [with " << (opts.Mapping ? "mapping": "streaming") << "] in " << std::chrono::duration_cast<std::chrono::microseconds>(etime-stime).count() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    }catch(const options_error& e) {
        std::cout << "ERROR while parsing options: " << e.what() << std::endl;
        WriteUsage(std::cout);
        return -1;
    }catch(const error& e) {
        std::cout << "ERROR: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
