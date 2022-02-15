set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/test)
link_libraries (CppEffects)

add_executable (traits traits.cpp)
add_executable (command-lifetime command-lifetime.cpp)
