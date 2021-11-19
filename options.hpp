#ifndef FILEHASHER_OPTIONS_HPP
#define FILEHASHER_OPTIONS_HPP

#include <string>
#include <stdexcept>

#include "commondefs.hpp"
#include "hasher.hpp"

namespace filehasher {

    struct options_error : public error {
        options_error(const std::string& what) : error(what) {}
    };

    enum class Command { help, run };

    class Options {
    public:
        Command         Cmd         {Command::run};
        std::string     InputFile;
        std::string     OutputFile; 
        size_t          BlockSize   {0};
        size_t          Workers     {0};
        bool            Sorted      {false};
        bool            Mapping     {false};
        size_t          QueueSize   {0};
    };

    Options ParseCommandLine(int argc, char *argv[]);
    void WriteUsage(std::ostream& os);
    void PromptUsage(std::ostream& os);
    hasher GetHasher(const Options& opts);

}//namespace filehasher

#endif//FILEHASHER_OPTIONS_HPP