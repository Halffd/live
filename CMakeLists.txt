cmake_minimum_required(VERSION 3.28.1)
project(live C)
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)

file(GLOB_RECURSE source_files "src/*.c")

add_executable(live ${source_files})

target_include_directories(live PRIVATE lib)
# Add the library directory to the linker search paths
target_link_directories(live PRIVATE S:/msys64/mingw64/lib)

# Create an imported library target for libcurl
add_library(curl SHARED IMPORTED)

# Set the IMPORTED_IMPLIB property to the path of the libcurl import library
set_target_properties(curl PROPERTIES IMPORTED_IMPLIB "S:/msys64/mingw64/lib/libcurl.a")

# Set the IMPORTED_LOCATION property to the path of the libcurl shared library
set_target_properties(curl PROPERTIES IMPORTED_LOCATION "S:/msys64/mingw64/lib/libcurl.a")

# Link the 'live' target with the 'curl' library
#target_link_libraries(live PRIVATE curl)
target_link_libraries(live PRIVATE curl -lcurl)

target_compile_options(live PRIVATE
        -Wall        # Enable all warnings
        -Wextra     # Enable extra warnings
        -Wpedantic   # Enable strict ISO C/C++ compliance
        -Werror     # Treat warnings as errors
)

# Set the output directory for the assembly files
set(CMAKE_ASM_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/asm)

# Set the target properties to generate assembly files during compilation
set_target_properties(live PROPERTIES
    COMPILE_FLAGS "-S -O0"  # Generate assembly and no optimization
)