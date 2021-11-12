project(FileHasher)
cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS program_options context fiber)
find_package(Threads REQUIRED)

add_executable(filehasher
    main.cpp options.cpp threading.cpp misc.cpp
)
target_link_libraries(filehasher Threads::Threads Boost::boost Boost::program_options Boost::context Boost::fiber)
target_compile_definitions(filehasher PRIVATE NOMINMAX)
