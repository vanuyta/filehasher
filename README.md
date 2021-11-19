### About filehasher.
Console application to calculate hashes on file chunks. Implemented as a test exercise for an interview.  
  
Filehasher uses concurent approach with one file-chunks *producer*, several *workers* calculating hashes on chunks, and one *results writer*.
File chunks piped from *producer* to *workers* pool and then to *results writer* using chanels.
To do the stuff three helper template classes were implemented:

  - `filehasher::thread_group`  
    Helps to launch workers on different threads and wait when they all will finish. It also propagate exceptions raised in any of launched thread.
  - `filehasher::chanel`  
    Implemets chanel.
  - `filehasher::piped_workers_pool`
    Helps to manage pipe with input chanel, pool of workers and output chanels.  Two `piped_workers_pool` can be connected to each other with output chanel of first one and input chanel of second.

Implemented solution is not `task` based. Each worker runs in separate thread using `filehasher::thread_group` and doing CPU-heavy computation all the time. It has no any option for context switching. Once one file chunck was processed, it gets next one from chanel.

Filehasher can process input file in 2 modes:

  - *Streamed* file reading using standart `std::ifstream`
  - File *mapping* using crossplarform `boost::interproces::file_mapping`

File *mapping* is faster in most cases. But it can be used only on 64 bit systems. 32 bit Windows limits not only physical RAM size, but the virtual memory available to user-space to 2GB.  
Stream reading works in all cases. For stream reading *soft* memory limit is introduced. It will not queing more file chunks to the workers pool than can fit in 1GB of RAM.
If requested block is bigger than 512MB - `filehasher` will fallback to synchronous execution.  
  
Results can be outputed in `ordered` or `unordered` mode.  
In `unordered` mode - each hash provided by workers pool to result writer will be written immediately.  
In `ordered` mode - results will be ordered by chunck number and written at the end of execution.  
For ordered mode one more limit is implemented (100000 items). This limit can be eliminated with *external sorting* implementation (write to file and *merge*-sort at the and of execution).  
  
Only one hash-algorithm is suported - **CRC16**. But new one can be introduced without refactoring all the sources.

### Dependencies.
Only **Boost** was used as external dependency. `filehasher` uses:

  - Boost headers: spirit, format, crc
  - Boost libraries: programm_options

Initial iimplementation did also use boost::fibers (for its `chanels`). But was replaced with own implementations later.

### Platform.
Compilation was tested on Windows 10 x64 (Visual Studio Build Tools 2019) and Ubuntu 20.04 running in WSL2 (GCC 9.3.0).  
Visual Studio Code + CMakeLists.txt was used as codding/building/debuggin environment.  
CMake searches Boost libraries and headers in default locations.

### Usage.
```
$ ./build/bin/filehasher --help
About:
  Splits input file in blocks with specified size and calculate their hashes.
  Writes generated chain of hashes to specified output file or stdout.
  Author: 'Ivan Pankov' (ivan.a.pankov@gmail.com) nov. 2021
Usage:
  filehasher [options] <PATH TO FILE> 

Options:
  --help                        Produces this message.
  -i [ --infile ] PATH          Path to the file to be processed.
  -o [ --outfile ] PATH         Path to the file to write results (`stdout` if 
                                not specified).
  -w [ --workers ] NUM (=8)     Number of workers to calculate hashes (number 
                                of H/W threads supported - if not specified).
                                '0' value can be used to forse sync processing.
  -b [ --blocksize ] SIZE (=1M) Size of block. Scale suffixes are allowed:
                                `K` - mean Kbyte(example 128K)
                                `M` - mean Mbyte (example 10M)
                                `G` - mean Gbyte (example 1G)
  --ordered                     Ennables results ordering by chunk number.
                                Ordering option has restriction in 100000 
                                chunks. Unordered output is faster and uses 
                                less memory.
  --mapping                     Ennables `mmap` option instead of stream 
                                reading. Could be faster and does not usess 
                                physical RAM memory to store chunks.
                                On Win x86 will definitely fail with files more
                                than 2GB.
```

### Valgrind output.
```
$ valgrind --tool=memcheck ./build/bin/filehasher -b 1m -o tmp.out ../../profindustry/moniron-most/publish/Moniron.Most.API
==11101== Memcheck, a memory error detector
==11101== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==11101== Using Valgrind-3.15.0 and LibVEX; rerun with -h for copyright info
==11101== Command: ./build/bin/filehasher -b 1m -o tmp.out ../../profindustry/moniron-most/publish/Moniron.Most.API
==11101== 
Running...
Done [with streaming] in 1644109
==11101== 
==11101== HEAP SUMMARY:
==11101==     in use at exit: 0 bytes in 0 blocks
==11101==   total heap usage: 561 allocs, 561 frees, 86,131,830 bytes allocated
==11101== 
==11101== All heap blocks were freed -- no leaks are possible
==11101== 
==11101== For lists of detected and suppressed errors, rerun with: -s
==11101== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
``` 

### TODO:

  - External sorting for results in file
  - Memmory pooling

