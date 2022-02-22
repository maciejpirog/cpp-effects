cmake_minimum_required (VERSION 3.10)

project (CppEffects)

set (CMAKE_BUILD_TYPE Release)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

include_directories ("include")

set (CMAKE_CXX_STANDARD 17)
add_compile_options (-Wall -Wextra -pedantic -Werror -O3)

# For local boost:
# set (BOOST_ROOT <boost-directory>)
FIND_PACKAGE (Boost 1.70 COMPONENTS context REQUIRED)

if (Boost_FOUND)
  add_subdirectory (src)
  add_subdirectory (examples)
  add_subdirectory (test)
  add_subdirectory (benchmark)
else()
  message (STATUS "Boost not found!")
endif()

