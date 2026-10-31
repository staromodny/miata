cmake_minimum_required(VERSION 3.25)

project(Miata
    VERSION 1.9
    DESCRIPTION "CS2 foss RadarHack"
    LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Miata only supports Windows.")
endif()

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

file(GLOB_RECURSE MIATA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE MIATA_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

include(FetchContent)

FetchContent_Declare(
        Windower
        GIT_REPOSITORY https://github.com/staromodny/Windower.git
        GIT_TAG 1.0
)

FetchContent_MakeAvailable(Windower)

add_executable(Miata WIN32
    ${MIATA_SOURCES}
    ${MIATA_HEADERS}
)

target_compile_definitions(Miata PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

target_include_directories(Miata PRIVATE
        "${PROJECT_SOURCE_DIR}/thirdparty"
)

target_link_libraries(Miata PRIVATE
        Windower::Windower
        d3d9
)