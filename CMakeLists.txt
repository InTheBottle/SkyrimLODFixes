cmake_minimum_required(VERSION 3.21)

project(
	SkyrimLODFixes
	VERSION 0.12.0
	DESCRIPTION "Standalone SKSE plugin that resets Skyrim LOD across worldspace changes"
	LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMONLIBSSE_NG_DIR "" CACHE PATH "Path to a CommonLibSSE-NG checkout")

if(NOT COMMONLIBSSE_NG_DIR)
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG/CMakeLists.txt")
		set(COMMONLIBSSE_NG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG")
	elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../skyrim-community-shaders/extern/CommonLibSSE-NG/CMakeLists.txt")
		set(COMMONLIBSSE_NG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../skyrim-community-shaders/extern/CommonLibSSE-NG")
	endif()
endif()

if(NOT EXISTS "${COMMONLIBSSE_NG_DIR}/CMakeLists.txt")
	message(FATAL_ERROR "CommonLibSSE-NG not found. Set -DCOMMONLIBSSE_NG_DIR=<path to checkout>.")
endif()

message(STATUS "CommonLibSSE-NG: ${COMMONLIBSSE_NG_DIR}")

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${COMMONLIBSSE_NG_DIR}" CommonLibSSE EXCLUDE_FROM_ALL)

add_library(
	${PROJECT_NAME} SHARED
	src/main.cpp
	src/InstanceGroupFix.cpp
	src/MapLODFix.cpp
	src/Diagnostics.cpp
	src/Settings.cpp
	src/PCH.h)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE CommonLibSSE::CommonLibSSE)

target_compile_definitions(
	${PROJECT_NAME}
	PRIVATE
	# Without this, windows.h's min/max macros break std::max at any call site.
	NOMINMAX
	WIN32_LEAN_AND_MEAN
	SKLF_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
	SKLF_VERSION_MINOR=${PROJECT_VERSION_MINOR}
	SKLF_VERSION_PATCH=${PROJECT_VERSION_PATCH})

target_compile_options(${PROJECT_NAME} PRIVATE /permissive- /Zc:preprocessor /utf-8 /W4 /Zi)

# Full PDB even in Release -- crash logs from users are useless without it.
target_link_options(${PROJECT_NAME} PRIVATE /DEBUG:FULL /OPT:REF /OPT:ICF)
set_target_properties(
	${PROJECT_NAME} PROPERTIES
	PDB_NAME ${PROJECT_NAME}
	PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

set(SKLF_DIST "${CMAKE_SOURCE_DIR}/dist/SKSE/Plugins")
add_custom_command(
	TARGET ${PROJECT_NAME}
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory "${SKLF_DIST}"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE:${PROJECT_NAME}>" "${SKLF_DIST}/"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${SKLF_DIST}/"
	COMMENT "Staging DLL + PDB into dist/SKSE/Plugins")

# Optional: copy the built plugin into a Skyrim Data folder (semicolon-separated).
if(DEFINED ENV{SkyrimLODFixesOutputDir})
	foreach(DEPLOY_TARGET $ENV{SkyrimLODFixesOutputDir})
		message(STATUS "Deploying to ${DEPLOY_TARGET}")
		add_custom_command(
			TARGET ${PROJECT_NAME}
			POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"$<TARGET_FILE:${PROJECT_NAME}>"
				"${DEPLOY_TARGET}/SKSE/Plugins/$<TARGET_FILE_NAME:${PROJECT_NAME}>")
	endforeach()
endif()





