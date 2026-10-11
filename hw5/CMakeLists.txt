cmake_minimum_required(VERSION 3.20)
project(hw5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
set(SQLITECPP_INTERNAL_SQLITE ON CACHE BOOL "Build bundled SQLite" FORCE)
set(SQLITECPP_BUILD_TESTS OFF CACHE BOOL "No SQLiteCpp tests" FORCE)
set(SQLITECPP_BUILD_EXAMPLES OFF CACHE BOOL "No SQLiteCpp examples" FORCE)
FetchContent_Declare(sqlitecpp
    GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
    GIT_TAG 3.3.3
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(sqlitecpp)

add_executable(hw5 main.cpp)
target_compile_features(hw5 PRIVATE cxx_std_23)
target_link_libraries(hw5 PRIVATE SQLiteCpp)