set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/benchmark)

add_executable (bench-exceptions exceptions.cpp)
add_executable (bench-function function.cpp)
