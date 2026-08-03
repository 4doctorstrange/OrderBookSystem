# Minimum cmake version required
cmake_minimum_required (VERSION 3.16)

# Project name and language
project (OrderBook LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create executable from source files
add_executable(myapp
    src/main.cpp
    src/Order.cpp
    src/OrderBook.cpp
    src/PriceLevel.cpp
    src/Trade.cpp
)

# Include directories (where headers live)
target_include_directories(myapp PRIVATE include/)

