### About filehasher.
Console application to calculate hashes on file chunks. Implemented as a test exercise for an interview.

```
$ ./filehasher --help
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
  -b [ --blocksize ] SIZE (=1M) Size of block. Scale suffixes are allowed:
                                `K` - mean Kbyte(example 128K)
                                `M` - mean Mbyte (example 10M)
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
