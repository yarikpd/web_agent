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

# Dotenv
FetchContent_Declare(
        dotenv
        GIT_REPOSITORY https://github.com/laserpants/dotenv-cpp.git
        GIT_TAG master
)
FetchContent_MakeAvailable(dotenv)

# json
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

# ------
# Internal libraries
add_library(agent STATIC)
target_sources(agent PRIVATE libs/agent/src/agent.cpp)
target_include_directories(
        agent PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/agent/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/logger/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/settings/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(agent PRIVATE nlohmann_json::nlohmann_json classes dotenv logger settings)

add_library(api STATIC)
target_sources(api PRIVATE libs/api/src/api.cpp)
target_include_directories(
        api PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/api/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/logger/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(api
        PUBLIC
        nlohmann_json::nlohmann_json
        classes
        PRIVATE
        cpr::cpr
        logger
)

add_library(classes STATIC)
target_sources(classes PRIVATE libs/classes/src/classes.cpp)
target_include_directories(
        classes PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/classes/include>
        $<INSTALL_INTERFACE:include>
)

add_library(logger STATIC)
target_sources(logger PRIVATE libs/logger/src/logger.cpp)
target_include_directories(
        logger PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/logger/include>
        $<INSTALL_INTERFACE:include>
)

add_library(settings STATIC)
target_sources(settings PRIVATE libs/settings/src/settings.cpp)
target_include_directories(
        settings PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/settings/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(settings PRIVATE dotenv)

# ------
# TESTS
enable_testing()

add_executable(tests
    tests/api_tests.cpp
    tests/agent_tests.cpp
)

target_link_libraries(tests
    GTest::gtest
    api
    agent
    classes
    logger
    settings
    cpr::cpr
    nlohmann_json::nlohmann_json
    dotenv
)
target_compile_definitions(api PRIVATE UNIT_TESTS)
include(GoogleTest)
gtest_discover_tests(tests)

if (WIN32)
    add_custom_command(TARGET tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:tests>
            $<TARGET_FILE_DIR:tests>
        COMMAND_EXPAND_LISTS
    )
endif()

# ------
# EXECUTABLE
add_executable(web_agent main.cpp)
target_include_directories(web_agent PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/agent/include
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/api/include
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/classes/include
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/logger/include
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/settings/include
)
target_link_libraries(web_agent PRIVATE
    agent
    api
    classes
    logger
    settings
    cpr::cpr
    nlohmann_json::nlohmann_json
    dotenv
)

if (WIN32)
    add_custom_command(TARGET web_agent POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:web_agent>
            $<TARGET_FILE_DIR:web_agent>
        COMMAND_EXPAND_LISTS
    )
endif()
