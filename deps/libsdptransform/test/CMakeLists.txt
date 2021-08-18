set(
	SOURCE_FILES
	tests.cpp
	parse.test.cpp
)

set(
	HEADER_FILES
	include/helpers.hpp
	include/catch.hpp
)

add_executable(test_sdptransform ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(test_sdptransform PRIVATE include)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
  	target_link_libraries(test_sdptransform sdptransform android log)
 else()
 	target_link_libraries(test_sdptransform sdptransform)
 endif()
