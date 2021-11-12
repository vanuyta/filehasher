#ifndef FILEHASHER_OPTIONS_HPP
#define FILEHASHER_OPTIONS_HPP

#include <string>
#include <stdexcept>

#include "misc.hpp"

namespace filehasher {

    struct options_error : public error {
        options_error(const std::string& what) : error(what) {}
    };

    enum class HashType { crc16 };
    enum class Command { help, run };

    class Options {
    public:
        Command         Cmd         {Command::run};
        std::string     InputFile;
        std::string     OutputFile; 
        size_t          BloclSize   {0};
        size_t          Workers     {1};
        HashType        Hash        {HashType::crc16};
        bool            Sorted      {false};
        bool            Mapping     {false};
        size_t          QueueSize   {1};
    };

    Options ParseCommandLine(int argc, char *argv[]);
    void WriteUsage(std::ostream& os);
    hash_function_t GetHashFunction(const Options& opts);
};
#endif//FILEHASHER_OPTIONS_HPP