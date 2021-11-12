#include <iostream>
#include <fstream>
#include <chrono>
#include <set>
#include <boost/interprocess/managed_mapped_file.hpp>

#include "options.hpp"
#include "threading.hpp"

using namespace filehasher;

struct result_t {
    size_t      cunk_number;
    std::string hash;
};

namespace std {
    template<> struct less<result_t>
    {
       bool operator() (const result_t& lhs,const result_t& rhs) const
       {
           return lhs.cunk_number < rhs.cunk_number;
       }
    };
}

using resulter_function_t = std::function<void(result_t&& r)>;

static void do_with_streaming(filehasher::Options opts, hash_function_t hfunc, const resulter_function_t& rfunc) {
    struct job_t {
        size_t              chunk_number{0};
        std::vector<char>   chank;
    };

    piped_workers_pool<job_t, result_t>
    workers (opts.Workers, [hfunc](job_t&& job) {
        return result_t{job.chunk_number, hfunc(job.chank.data(), job.chank.size())};
    });
    
    piped_workers_pool<result_t>
    resulter (1, workers, [&rfunc](result_t&& result) {
        rfunc(std::move(result));
    });

    auto& input = workers.get_input_chan();
    auto& terminator = resulter.get_output_chan();

    std::ifstream ifile(opts.InputFile, std::ifstream::binary);
    if(!ifile)
        throw error("failed to open file [" + opts.InputFile + "]");

    for (size_t i=0; ifile && !terminator.is_closed(); i++) {
        std::vector<char> buff(opts.BloclSize);
        ifile.read(buff.data(), buff.size());
        size_t readed = ifile.gcount();
        if(readed == 0)
            break;
        buff.resize(readed);
        if (input.push(std::move(job_t{i, std::move(buff)})) != boost::fibers::channel_op_status::success)
            break;
    }

    input.close();
    workers.wait();
    resulter.wait();
}

static void do_with_mapping(filehasher::Options opts, const hash_function_t& hfunc, const resulter_function_t& rfunc) {
    namespace bi = boost::interprocess;
    struct job_t {
        size_t      chunk_number    {0};
        size_t      size            {0};
        const void  *addr           {nullptr};
    };

    piped_workers_pool<job_t, result_t>
    workers (opts.Workers, [&hfunc](const job_t& job) {
        return result_t{job.chunk_number, hfunc(job.addr, job.size)};
    });
    
    piped_workers_pool<result_t>
    resulter (1, workers, [&rfunc](result_t&& result){
        rfunc(std::move(result));
    });

    try {
        auto& input = workers.get_input_chan();
        auto& terminator = resulter.get_output_chan();
    
        bi::file_mapping ifile(opts.InputFile.c_str(), bi::read_only);
        bi::mapped_region region(ifile, bi::read_only);

        size_t size = region.get_size();
        const void* addr = region.get_address();
        for (size_t i = 0, num = 0; i < size && !terminator.is_closed(); i += opts.BloclSize) {
            if(input.push(job_t{num++,std::min((size_t)opts.BloclSize, size - i), (const char*)addr + i}) != boost::fibers::channel_op_status::success)
                break;
        }

        input.close();
        workers.wait();
        resulter.wait();
    } catch (const bi::interprocess_exception& e) {
        throw error("failed to map file [" + opts.InputFile + "]: " + e.what());
    }
}

void process_ordered_results(result_t&& result, std::multiset<result_t>& dst) {
    // Dummy limit for result set...
    // In some cases (when we process 10GB file by 10B chunks for example) - it will fails.
    // This cam be fixed with "external" sorting implementation. But it will involeves more I/O operations
    static const size_t results_limit = 100000;

    if(dst.size() >= results_limit)
        throw error("too many results (try unordered output)");
    dst.insert(std::move(result));
}

void process_unordered_results(result_t&& result, std::ostream& dst) {
    // Just write unordered chunks directly to provided output stream...
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

        // Get hashing function (only CRC16 with Boost implementation is supported).
        auto hfunc = GetHashFunction(opts);
        if(!hfunc) throw error("failed to determine hash algorithm");

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

        std::cout << "Running..." << std::endl;
        auto stime = std::chrono::high_resolution_clock::now();

        // Select input file reading mode (streamed/maped) depending on 'Mapping' options flag.
        if(opts.Mapping)
            do_with_mapping(opts, hfunc, rfunc);
        else 
            do_with_streaming(opts, hfunc, rfunc);

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
