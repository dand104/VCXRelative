cmake_minimum_required(VERSION 3.15)
project(VCXRelative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG        v1.16
)
FetchContent_MakeAvailable(pugixml)

add_executable(vcxrelative
        main.cxx
        path_relativizer.cxx
        path_relativizer.hxx
        project_processor.hxx
        project_processor.cxx
)

if(MSVC)
    target_compile_options(vcxrelative PRIVATE /W4 /permissive-)
else()
    target_compile_options(vcxrelative PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(vcxrelative PRIVATE pugixml::pugixml)