cmake_minimum_required(VERSION 4.1)
project(web_agent)

set(CMAKE_CXX_STANDARD 17)

# External libraries
include(FetchContent)

# HTTP
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
        GIT_TAG c492b4fed1fd7288eacec6fe8d89e37df78d373a)
FetchContent_MakeAvailable(cpr)

# GOOGLE TEST
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

#Internal libraries
add_library(agent STATIC)
target_sources(agent PRIVATE libs/agent/src/agent.cpp)
target_include_directories(
        agent PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/agent/include>
        $<INSTALL_INTERFACE:include>
)

add_library(api STATIC)
target_sources(api PRIVATE libs/api/src/api.cpp)
target_include_directories(
        api PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/api/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(api PRIVATE cpr::cpr)

# TESTS
enable_testing()

add_executable(tests
        tests/main_tests.cpp
)

target_link_libraries(tests GTest::gtest_main)

# EXECUTABLE
add_executable(web_agent main.cpp)
target_link_libraries(web_agent PRIVATE agent api cpr::cpr)