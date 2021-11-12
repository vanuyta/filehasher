#include <thread>
#include <filesystem>
#include <algorithm>
#include <boost/program_options.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>

#include "options.hpp"
#include "threading.hpp"

namespace po = boost::program_options;
namespace x3 = boost::spirit::x3;

static const char *about = 
"About:\n"\
"  Splits input file in blocks with specified size and calculate their hashes.\n"\
"  Writes generated chain of hashes to output file or stdout.\n"\
"  Author: 'Ivan Pankov' (ivan.a.pankov@gmail.com) nov. 2021\n"\
"Usage:\n"\
"  filehasher [options] <PATH TO FILE> \n"\
"\nOptions";

static unsigned int parse_size(std::string value)
{
    auto count {0UL};
    auto scale {'B'};
    auto getter = std::tie(count, scale);    

    if( x3::parse(value.cbegin(), value.cend(), x3::ulong_ >> -x3::char_ >> x3::eoi, getter) ) {
        switch (std::toupper(scale)) {
        case 'K':
            if (count < ULLONG_MAX / 1024)
                return count * 1024;
            break;
        case 'M':
            if (count < ULLONG_MAX / (1024 * 1024))
                return count * 1024 * 1024;
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
            ("workers,w", po::value<size_t>()->default_value(def_workers)->value_name("NUM"), "Number of workers to calculate hashes (number of H/W threads supported - if not specified).")
            ("blocksize,b", po::value<std::string>()->default_value("1M")->value_name("SIZE"), "Size of block. Scale suffixes are allowed:\n`K` - Means Kbyte(example 128K)\n`M` - Means Mbyte (example 10M)")
            ("queuesize,q", po::value<size_t>()->value_name("COUNT"), "Maximum count of blocks waiting to be processed. By default it is calculated using 'blocksize' to fit waiting blocks in 1GB of RAM.")
            ("ordered", "Ennables results ordering by chunk number.\nOrdering option has restriction in 100000 chunks. Unordered output is faster and uses less momory.")
            ("mapping", "Ennables `mmap` option instead of stream reading. Could be faster and does not usess phisical RAM memmory to store chunks.\nOn Win x86 will definitely fail with files more than 2GB.");
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
            opts.Workers = vm["workers"].as<size_t>();
            opts.BloclSize = parse_size(vm["blocksize"].as<std::string>());
            size_t fsize = std::filesystem::file_size(opts.InputFile);
            opts.Workers = std::min(opts.Workers, (fsize/opts.BloclSize) + (fsize%opts.BloclSize ? 1 : 0));

            if(opts.BloclSize == 0)
                throw po::validation_error{po::validation_error::invalid_option_value, "blocksize"};
            if(vm.count("outfile"))
                opts.OutputFile = vm["outfile"].as<std::string>();
            if(vm.count("ordered"))
                opts.Sorted = true;
            if(vm.count("mapping"))
                opts.Mapping = true;

            if(vm.count("queuesize"))
                opts.QueueSize = vm["workers"].as<size_t>();
            else 
                opts.QueueSize = std::min((1024 * 1024 * 1024) / opts.BloclSize, (filehasher::max_queue - 1));
            // std::cout << "Block " << opts.BloclSize << " Workers " << opts.Workers << " Queue " << opts.QueueSize;

        } catch (const po::error& e){
            throw options_error(e.what());
        }
        return opts;
    }

    void WriteUsage(std::ostream& os) {
        os << get_options();
    }

    hash_function_t GetHashFunction(const Options& opts) {
        return hash_crc16;
    }

}
