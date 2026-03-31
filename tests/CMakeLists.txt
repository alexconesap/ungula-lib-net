cmake_minimum_required(VERSION 3.16)
project(ungula_net_lib_tests)

set(CMAKE_CXX_STANDARD 17)

set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_LIST_DIR}/vendor)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

set(LIB_SRC ${CMAKE_CURRENT_LIST_DIR}/../src)

include_directories(
    ${LIB_SRC}
)

find_package(CURL REQUIRED)

add_executable(test_http_client
    test_http_client.cpp
    ${LIB_SRC}/http/http_client.cpp
)
target_link_libraries(test_http_client gtest_main CURL::libcurl)

enable_testing()
include(GoogleTest)
gtest_discover_tests(test_http_client)
