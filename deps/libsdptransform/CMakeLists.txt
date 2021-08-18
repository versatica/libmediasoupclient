cmake_minimum_required(VERSION 3.0)

project(sdptransform VERSION 1.2.9)

# For CMake >= 3.1.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# For CMake < 3.1.
add_compile_options(-std=c++11)

set(
	SOURCE_FILES
	src/grammar.cpp
	src/parser.cpp
	src/writer.cpp
)

set(
	HEADER_FILES
	${sdptransform_BINARY_DIR}/include/sdptransform.hpp
)

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
	add_subdirectory(test)
endif()

option(BUILD_README_HELPER "Build readme-helper" ON)
if(BUILD_README_HELPER)
	add_subdirectory(readme-helper)
endif()

option(BUILD_SHARED_LIBS "Build shared library" OFF)
add_library(sdptransform ${SOURCE_FILES})

target_include_directories(sdptransform PUBLIC ${sdptransform_BINARY_DIR}/include)

option(USE_EXTERNAL_JSON "Use external nlohmann_json instead of bundled" OFF)
if(USE_EXTERNAL_JSON)
	find_package(nlohmann_json REQUIRED)
	target_link_libraries(sdptransform nlohmann_json::nlohmann_json)
	set(JSON_HPP_LOCATION "<nlohmann/json.hpp>")
else()
	target_include_directories(sdptransform PUBLIC thirdparty)
	list(APPEND HEADER_FILES thirdparty/json.hpp)
	set(JSON_HPP_LOCATION "\"json.hpp\"")
endif()
configure_file(include/sdptransform.hpp.in include/sdptransform.hpp)

install(TARGETS sdptransform)
install(FILES ${HEADER_FILES} DESTINATION include/sdptransform)
