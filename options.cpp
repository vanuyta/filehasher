#include <thread>
#include <filesystem>
#include <algorithm>
#include <boost/program_options.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>

#include "options.hpp"
#include "commondefs.hpp"

namespace po = boost::program_options;
namespace x3 = boost::spirit::x3;

const char *about = 
"About:\n"\
"  Splits input file in blocks with specified size and calculate their hashes.\n"\
"  Writes generated chain of hashes to specified output file or stdout.\n"\
"  Author: 'Ivan Pankov' (ivan.a.pankov@gmail.com) nov. 2021\n"\
"Usage:\n"\
"  filehasher [options] <PATH TO FILE> \n"\
"\nOptions";

static bool try_parse_unsigned(std::string value, unsigned long& dst)
{
    if( x3::parse(value.cbegin(), value.cend(), x3::ulong_ >> x3::eoi, dst) ) {
        return true;
    }
    return false;
}

static size_t parse_size(std::string value)
{
    auto count = size_t{0};
    auto scale {'B'};
    auto getter = std::tie(count, scale);    

    if( x3::parse(value.cbegin(), value.cend(), x3::ulong_ >> -x3::char_ >> x3::eoi, getter) ) {
        switch (std::toupper(scale)) {
        case 'K':
            if (count < std::numeric_limits<decltype(count)>::max() / 1024)
                return count * 1024;
            break;
        case 'M':
            if (count < std::numeric_limits<decltype(count)>::max() / (1024 * 1024))
                return count * 1024 * 1024;
            break;
        case 'G':
            if (count < std::numeric_limits<decltype(count)>::max() / (1024 * 1024 * 1024))
                return count * 1024 * 1024 * 1024;
            break;
        case 'B':
            return count;
        default:
            break;
        }
    }
    return 0;
}

static const po::options_description get_options() {
    static auto once = false;
    static po::options_description options{about};
    if(!once) {
        once = true;

        auto def_workers = std::thread::hardware_concurrency();
        if(def_workers <= 0) def_workers = 1;

        options.add_options()
            ("help", "Produces this message.")
            ("infile,i", po::value<std::string>()->value_name("PATH"), "Path to the file to be processed.")
            ("outfile,o", po::value<std::string>()->value_name("PATH"), "Path to the file to write results (`stdout` if not specified).")
            ("workers,w", po::value<std::string>()->default_value(std::to_string(def_workers))->value_name("NUM"), "Number of workers to calculate hashes (number of H/W threads supported - if not specified).\n'0' value can be used to forse sync processing.")
            ("blocksize,b", po::value<std::string>()->default_value("1M")->value_name("SIZE"), "Size of block. Scale suffixes are allowed:\n`K` - mean Kbyte(example 128K)\n`M` - mean Mbyte (example 10M)\n`G` - mean Gbyte (example 1G)")
            ("ordered", "Ennables results ordering by chunk number.\nOrdering option has restriction in 100000 chunks. Unordered output is faster and uses less memory.")
            ("mapping", "Ennables `mmap` option instead of stream reading. Could be faster and does not usess physical RAM memory to store chunks.\nOn Win x86 will definitely fail with files more than 2GB.");
    }

    return options;
}

namespace filehasher {

    Options ParseCommandLine(int argc, char *argv[]) {
        Options opts;
        try {
            po::positional_options_description p;
            p.add("infile", -1);

            po::variables_map vm;
            po::store(po::command_line_parser(argc, argv).options(get_options()).positional(p).run(), vm);
            po::notify(vm);

            if(vm.count("help")) {
                opts.Cmd = Command::help;
                return opts;
            }

            opts.Cmd = Command::run;
            if(!vm.count("infile"))
                throw po::validation_error{po::validation_error::at_least_one_value_required, "infile"};
            opts.InputFile = vm["infile"].as<std::string>();

            auto wrks = 0UL;
            if(!try_parse_unsigned(vm["workers"].as<std::string>(), wrks))
                throw po::validation_error{po::validation_error::invalid_option_value, "workers"};
            opts.Workers = wrks;

            opts.BlockSize = parse_size(vm["blocksize"].as<std::string>());
            if(opts.BlockSize == 0)
                throw po::validation_error{po::validation_error::invalid_option_value, "blocksize"};

            if(vm.count("outfile"))
                opts.OutputFile = vm["outfile"].as<std::string>();

            if(vm.count("ordered"))
                opts.Sorted = true;

            if(vm.count("mapping"))
                opts.Mapping = true;

            size_t fsize = std::filesystem::file_size(opts.InputFile);
            size_t blocks_count = fsize / opts.BlockSize + (fsize % opts.BlockSize ? 1 : 0);
            if (blocks_count == 0)
                throw options_error("input file is empty");

            //Adjust workers count and queue size to satisfy all limitations

            // If only 1 file block will be processed set queue size and workers to 0 to fall back to sync execution
            // If 0 workers were requested - set queue size to 0 to fall back to sync execution
            if (blocks_count == 1 || opts.Workers == 0) {
                opts.QueueSize = opts.Workers = 0;
                return opts;
            }
            
            // Check memory limits and calculate queue size.
            // For mapping mode - use max queue size (blocks will not occupie phisical RAM).
            size_t memory_blocks_limit = soft_memmory_limit / opts.BlockSize;
            opts.QueueSize = opts.Mapping ? queue_limit : memory_blocks_limit > 0 ? std::min(memory_blocks_limit - 1, queue_limit) : 0;
            // The number of workers should be less or equal to queue size to prevent new blocks allocations
            opts.Workers = std::min(opts.Workers, opts.QueueSize);
            // The number of workers should not be grater than number of blocks to count
            opts.Workers = std::min(opts.Workers, blocks_count);

        } catch (const std::filesystem::filesystem_error& e){
            throw options_error(e.what());
        } catch (const po::error& e){
            throw options_error(e.what());
        }
        return opts;
    }

    void PromptUsage(std::ostream& os) {
        os << "Try: filehasher --help\n";
    }

    void WriteUsage(std::ostream& os) {
        os << get_options();
    }

    hasher GetHasher(const Options& opts) {
        //Only CRC16 is implemented.
        return hasher{hasher::hash_types::crc_16};
    }

}//namespace filehasher
