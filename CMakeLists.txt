# Minimum cmake version required
cmake_minimum_required (VERSION 3.16)

# Project name and language
project (OrderBook LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#========================= Static Library Gen ====================================:

# Create Static library
add_library(book STATIC
    src/Order.cpp
    src/OrderBook.cpp
    src/PriceLevel.cpp
    src/Trade.cpp
)

# Include Headers or .hpp files 
target_include_directories(book PUBLIC include/)


#========================= Main Executable  ====================================:

# Create executable binary from source files
add_executable(myapp src/main.cpp)

# Link Library to executable
target_link_libraries(myapp PRIVATE book)

#========================= Test Enabling ====================================:

# REQUIRED forces CMake to stop if GoogleTest is missing
find_package(GTest REQUIRED)

# BUILD AND CONFIGURE TESTS 

enable_testing()

# create test executable
add_executable(unitTest test/unitTest.cpp)

# Link your library and the local Brew GTest targets

target_link_libraries(unitTest PRIVATE
    book
    GTest::gtest_main
)

# Discover tests automatically
include(GoogleTest)
gtest_discover_tests(unitTest)


#========================= Bench Enabling ====================================:

# create bench executable
add_executable(order_bench bench/benchMark.cpp)

# Link book with bench executbale
target_link_libraries(order_bench PRIVATE book)

#Compile optimization:  -O3 -march=native on just this target guarantees the benchmark is optimized regardless of the overall build type.
target_compile_options(order_bench PRIVATE -O3 -march=native)  


