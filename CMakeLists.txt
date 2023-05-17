cmake_minimum_required(VERSION 3.11)

project(egraph CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE "Debug")

include_directories(.)

if(BUILD_EGRAPH_TESTS)

    include(FetchContent)

    FetchContent_Declare(
        pegtl
        GIT_REPOSITORY https://github.com/taocpp/PEGTL.git
        GIT_TAG        3.2.7
    )
    FetchContent_MakeAvailable(pegtl)

    set(JUST_INSTALL_CEREAL ON CACHE INTERNAL "Skip all Cereal tests/docs/etc")
    FetchContent_Declare(
        cereal
        GIT_REPOSITORY https://github.com/USCiLab/cereal.git
        GIT_TAG        v1.3.2
    )
    FetchContent_MakeAvailable(cereal)

    add_executable(Tests Tests.cpp)

    target_link_libraries(Tests PRIVATE
        cereal::cereal
        pegtl)

endif()
