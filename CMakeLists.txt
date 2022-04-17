cmake_minimum_required(VERSION 3.20)

project(Matching)

include(FetchContent)
FetchContent_Declare(
	json
	GIT_REPOSITORY https://github.com/json-c/json-c.git
)
FetchContent_MakeAvailable(json)

add_executable(Matching main.c)
