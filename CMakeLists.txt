cmake_minimum_required(VERSION 3.28.1)
project(live C)

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