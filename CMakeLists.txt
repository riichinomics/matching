cmake_minimum_required(VERSION 3.20)

project(Matching)

include(FetchContent)
FetchContent_Declare(
	json
	GIT_REPOSITORY https://github.com/json-c/json-c.git
)

set(BUILD_SHARED_LIBS OFF)

FetchContent_MakeAvailable(json)

add_executable(
	Matching
	${PROJECT_SOURCE_DIR}/main.c
)

# target_include_directories(Matching PUBLIC "${json_BINARY_DIR}" "${json_SOURCE_DIR}" )
target_link_libraries(Matching json-c)
