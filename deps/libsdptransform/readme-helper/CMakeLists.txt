include_directories(${sdptransform_SOURCE_DIR}/include)

set(
	SOURCE_FILES
	readme.cpp
)

add_executable(sdptransform_readme_helper ${SOURCE_FILES})

target_link_libraries(sdptransform_readme_helper sdptransform)
