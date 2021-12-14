set(SOURCE_FILES
	pc/test/fake_audio_capture_module.cc
)

# Create target.
add_library(webrtc STATIC ${SOURCE_FILES})

# Private (implementation) header files.
target_include_directories(webrtc PRIVATE ${PROJECT_SOURCE_DIR}/deps/libwebrtc)

# Public (interface) headers from dependencies.
target_include_directories(webrtc PUBLIC
	"${LIBWEBRTC_INCLUDE_PATH}"
	"${LIBWEBRTC_INCLUDE_PATH}/third_party/abseil-cpp"
)

# Compile definitions for libwebrtc.
target_compile_definitions(webrtc PUBLIC
	$<$<NOT:$<PLATFORM_ID:Windows>>:WEBRTC_POSIX>
	$<$<PLATFORM_ID:Windows>:WEBRTC_WIN>
	$<$<PLATFORM_ID:Windows>:NOMINMAX>
	$<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
	$<$<PLATFORM_ID:Darwin>:WEBRTC_MAC>
)

