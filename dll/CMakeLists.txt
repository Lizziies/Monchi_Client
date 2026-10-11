cmake_minimum_required(VERSION 3.20)
project(Monchi LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(MONCHI_VERSION "0.1.0" CACHE STRING "Client version")

# The account that hosts updates and the online service is not part of the source. local.cmake (not in the repository,
# see local.cmake.example) sets it for the builds of the project owner.
include("${CMAKE_CURRENT_LIST_DIR}/../local.cmake" OPTIONAL)
set(MONCHI_REPO_OWNER "" CACHE STRING "GitHub account that hosts updates, signatures and server rules")
set(MONCHI_REPO_NAME "" CACHE STRING "GitHub repository for updates")
set(MONCHI_ONLINE_URL "" CACHE STRING "Default address of the Monchi Online service")
add_compile_definitions(MONCHI_REPO_OWNER="${MONCHI_REPO_OWNER}" MONCHI_REPO_NAME="${MONCHI_REPO_NAME}" MONCHI_ONLINE_URL="${MONCHI_ONLINE_URL}")

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
# adapted Flarial files are compiled inside flarial_core, see cmake/FlarialCore.cmake
list(FILTER SOURCES EXCLUDE REGEX "/src/flarial/")
list(APPEND SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../common/I18n.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../common/DataDir.cpp)
file(GLOB MINHOOK_SOURCES lib/minhook/src/*.c lib/minhook/src/hde/hde64.c)

include(FetchContent)
FetchContent_Declare(lua GIT_REPOSITORY https://github.com/lua/lua.git GIT_TAG v5.4.8 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(lua)
file(GLOB LUA_SOURCES ${lua_SOURCE_DIR}/*.c)
list(FILTER LUA_SOURCES EXCLUDE REGEX "/(lua|luac|onelua|ltests)\\.c$")

option(MONCHI_DEV "Development build with the script explorer, never ship this" OFF)
if(MONCHI_DEV)
    set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_BUILD_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
    set(CAPSTONE_ARCHITECTURE_DEFAULT OFF CACHE BOOL "" FORCE)
    set(CAPSTONE_X86_SUPPORT ON CACHE BOOL "" FORCE)
    FetchContent_Declare(capstone GIT_REPOSITORY https://github.com/capstone-engine/capstone.git GIT_TAG 5.0.3 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(capstone)
endif()

set(IMGUI_SOURCES
    lib/imgui/imgui.cpp
    lib/imgui/imgui_draw.cpp
    lib/imgui/imgui_tables.cpp
    lib/imgui/imgui_widgets.cpp
    lib/imgui/backends/imgui_impl_dx11.cpp
    lib/imgui/backends/imgui_impl_win32.cpp
)

option(MONCHI_FLARIAL "build Flarial's game side into the client" OFF)
if(MONCHI_FLARIAL)
    include(cmake/FlarialCore.cmake)
endif()

add_library(Monchi SHARED ${SOURCES} ${MINHOOK_SOURCES} ${LUA_SOURCES} ${IMGUI_SOURCES} res/resources.rc)

file(GLOB SIG_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../sigs/*.json")
set(SIG_RESOURCES "")
foreach(SIG_FILE IN LISTS SIG_FILES)
    get_filename_component(SIG_NAME "${SIG_FILE}" NAME)
    string(APPEND SIG_RESOURCES "\"${SIG_NAME}\" RCDATA \"${SIG_FILE}\"\n")
endforeach()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/signatures.rc" CONTENT "${SIG_RESOURCES}")
target_sources(Monchi PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/signatures.rc")

target_include_directories(Monchi PRIVATE
    src
    ../common
    lib/minhook/include
    ${lua_SOURCE_DIR}
    lib/imgui
    lib/imgui/backends
    lib/json
    lib/stb
)

target_compile_definitions(Monchi PRIVATE
    MONCHI_VERSION="${MONCHI_VERSION}"
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    IMGUI_DEFINE_MATH_OPERATORS
)

if(MONCHI_DEV)
    target_compile_definitions(Monchi PRIVATE MONCHI_DEV)
    target_include_directories(Monchi PRIVATE ${capstone_SOURCE_DIR}/include)
    target_link_libraries(Monchi PRIVATE capstone)
endif()

target_link_libraries(Monchi PRIVATE d3d11 d3d12 dxgi d3dcompiler ws2_32 winhttp version psapi winmm dwmapi bcrypt ole32)

if(MSVC)
    target_link_libraries(Monchi PRIVATE windowsapp)
    set_property(TARGET Monchi PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(Monchi PRIVATE /utf-8 /W3 /permissive- /EHa /bigobj)
    if(MONCHI_DEV)
        target_compile_options(Monchi PRIVATE /Zi)
        target_link_options(Monchi PRIVATE /DEBUG /OPT:REF /OPT:ICF)
    endif()
else()
    target_compile_options(Monchi PRIVATE -Wall -Wno-unknown-pragmas -Wno-missing-field-initializers)
    target_link_options(Monchi PRIVATE -static -static-libgcc -static-libstdc++)
endif()

set_target_properties(Monchi PROPERTIES PREFIX "" OUTPUT_NAME "Monchi")
