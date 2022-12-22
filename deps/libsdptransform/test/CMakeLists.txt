include_directories(${sdptransform_SOURCE_DIR}/include)
include_directories(include)

set(
	SOURCE_FILES
	tests.cpp
	parse.test.cpp
	catch_amalgamated.cpp
)

set(
	HEADER_FILES
)

add_definitions(-DCATCH_AMALGAMATED_CUSTOM_MAIN)
add_executable(test_sdptransform ${SOURCE_FILES} ${HEADER_FILES})

if(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
  	target_link_libraries(test_sdptransform sdptransform android log)
 else()
 	target_link_libraries(test_sdptransform sdptransform)
 endif()
