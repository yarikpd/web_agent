cmake_minimum_required(VERSION 3.14)
project(web_agent)

set(CMAKE_CXX_STANDARD 17)

# ------
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

# json
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

# ------
#Internal libraries
add_library(agent STATIC)
target_sources(agent PRIVATE libs/agent/src/agent.cpp)
target_include_directories(
        agent PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/agent/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(agent PRIVATE nlohmann_json::nlohmann_json classes)

add_library(api STATIC)
target_sources(api PRIVATE libs/api/src/api.cpp)
target_include_directories(
        api PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/api/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(api PRIVATE cpr::cpr nlohmann_json::nlohmann_json classes)

add_library(classes STATIC)
target_sources(classes PRIVATE libs/classes/src/classes.cpp)
target_include_directories(
        classes PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/classes/include>
        $<INSTALL_INTERFACE:include>
)

# ------
# TESTS
enable_testing()

add_executable(tests
    tests/main_tests.cpp
)

target_link_libraries(tests
    GTest::gtest_main
    api
    classes
    cpr::cpr
    nlohmann_json::nlohmann_json
)
target_compile_definitions(api PRIVATE UNIT_TESTS)
include(GoogleTest)
gtest_discover_tests(tests)

# ------
# EXECUTABLE
add_executable(web_agent main.cpp)
target_link_libraries(web_agent PRIVATE
    agent
    api
    classes
    cpr::cpr
    nlohmann_json::nlohmann_json
)
