cmake_minimum_required(VERSION 3.22)
project(TMS VERSION 4.1.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Enable if building on the cluster
# the command line use -DBUILD_CLUSTER=1
set(BUILD_CLUSTER false CACHE BOOL "Enable for builds on the cluster")

if(WIN32)
  set(CMAKE_CXX_FLAGS_RELEASE "-DNOMINMAX ${CMAKE_CXX_FLAGS_RELEASE} /MT")
  set(CMAKE_CXX_FLAGS_DEBUG "-DNOMINMAX ${CMAKE_CXX_FLAGS_DEBUG} /MTd")

  # Sets the flag to disable Linux specific error handling
  add_definitions(-D__DISABLE_CRIT_ERR)
endif()

# Include external projects
set(EXT_PROJECTS_DIR ${PROJECT_SOURCE_DIR}/ext)

# Add the source subdirectory
add_subdirectory(src)

# Setup test project
option(BUILD_TESTS "Build all tests." OFF) # Makes boolean 'test' available.
if(BUILD_TESTS)
  add_subdirectory(test)
endif()
