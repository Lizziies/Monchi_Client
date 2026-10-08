cmake_minimum_required(VERSION 3.20)
project(MonchiLauncher LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(LIB ${CMAKE_CURRENT_SOURCE_DIR}/../dll/lib)
set(MONCHI_VERSION "0.1.0" CACHE STRING "Version shown in the launcher")

# The account that hosts updates and the online service is not part of the source. local.cmake (not in the repository,
# see local.cmake.example) sets it for the builds of the project owner.
include("${CMAKE_CURRENT_LIST_DIR}/../local.cmake" OPTIONAL)
set(MONCHI_REPO_OWNER "" CACHE STRING "GitHub account that hosts updates, signatures and server rules")
set(MONCHI_REPO_NAME "" CACHE STRING "GitHub repository for updates")
set(MONCHI_ONLINE_URL "" CACHE STRING "Default address of the Monchi Online service")
add_compile_definitions(MONCHI_REPO_OWNER="${MONCHI_REPO_OWNER}" MONCHI_REPO_NAME="${MONCHI_REPO_NAME}" MONCHI_ONLINE_URL="${MONCHI_ONLINE_URL}")

file(GLOB SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(APPEND SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../common/I18n.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../common/DataDir.cpp)
# the player figure of the start page is drawn by the client's own cosmetics code
set(COSMETICS ${CMAKE_CURRENT_SOURCE_DIR}/../dll/src/cosmetics)
list(APPEND SOURCES ${COSMETICS}/Cosmetics.cpp ${COSMETICS}/Preview.cpp ${COSMETICS}/GpuPreview.cpp)

set(MONCHI_DLL "" CACHE FILEPATH "Monchi.dll to embed, makes the launcher a single exe")
set(MONCHI_COSMETICS "" CACHE PATH "Cosmetics folder to embed next to it")
set(MONCHI_CORE "" CACHE FILEPATH "MonchiFlarial.dll (the Flarial core) to embed next to the client")
foreach(payload MONCHI_DLL MONCHI_CORE)
    if(${payload} AND NOT EXISTS "${${payload}}")
        message(FATAL_ERROR "${payload} does not exist: ${${payload}}")
    endif()
endforeach()
if(MONCHI_CORE AND NOT MONCHI_DLL)
    message(FATAL_ERROR "MONCHI_CORE requires MONCHI_DLL")
endif()
if(MONCHI_DLL AND EXISTS "${MONCHI_DLL}")
    file(TO_CMAKE_PATH "${MONCHI_DLL}" EMBED_DLL)
    set(EMBED_RC ${CMAKE_CURRENT_BINARY_DIR}/embed.rc)
    set(EMBED_TEXT "#include \"${CMAKE_CURRENT_SOURCE_DIR}/res/resource.h\"\nIDR_CLIENT RCDATA \"${EMBED_DLL}\"\n")
    set(EMBED_DEPENDS "${MONCHI_DLL}")
    find_package(Python3 COMPONENTS Interpreter)
    if(NOT Python3_FOUND)
        message(FATAL_ERROR "Python is required to package client signatures")
    endif()
    set(EMBED_SIGS ${CMAKE_CURRENT_BINARY_DIR}/signatures.pack)
    execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/pack.py ${EMBED_SIGS} ${CMAKE_CURRENT_SOURCE_DIR}/../sigs COMMAND_ERROR_IS_FATAL ANY)
    string(APPEND EMBED_TEXT "IDR_SIG_PACK RCDATA \"${EMBED_SIGS}\"\n")
    file(GLOB SIG_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../sigs/*.json)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SIG_FILES})
    list(APPEND EMBED_DEPENDS ${EMBED_SIGS})
    if(MONCHI_COSMETICS AND EXISTS "${MONCHI_COSMETICS}" AND Python3_FOUND)
        set(EMBED_PACK ${CMAKE_CURRENT_BINARY_DIR}/cosmetics.pack)
        execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/pack.py ${EMBED_PACK} ${MONCHI_COSMETICS})
        string(APPEND EMBED_TEXT "IDR_COSMETICS RCDATA \"${EMBED_PACK}\"\n")
        list(APPEND EMBED_DEPENDS ${EMBED_PACK})
    endif()
    if(MONCHI_CORE AND EXISTS "${MONCHI_CORE}")
    file(TO_CMAKE_PATH "${MONCHI_CORE}" EMBED_CORE)
    string(APPEND EMBED_TEXT "IDR_CORE RCDATA \"${EMBED_CORE}\"\n")
    list(APPEND EMBED_DEPENDS ${MONCHI_CORE})
endif()
file(WRITE ${EMBED_RC} "${EMBED_TEXT}")
    list(APPEND SOURCES ${EMBED_RC})
    set_source_files_properties(${EMBED_RC} PROPERTIES OBJECT_DEPENDS "${EMBED_DEPENDS}")
endif()

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/versions.rc "#include \"${CMAKE_CURRENT_SOURCE_DIR}/res/resource.h\"\nIDR_VERSION_INSTALL RCDATA \"${CMAKE_CURRENT_SOURCE_DIR}/res/version_install.ps1\"\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/versions.rc "IDR_SIG_INDEX RCDATA \"${CMAKE_CURRENT_SOURCE_DIR}/../sigs/index.json\"\n")
list(APPEND SOURCES ${CMAKE_CURRENT_BINARY_DIR}/versions.rc)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/versions.rc PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/res/version_install.ps1")

add_executable(MonchiLauncher WIN32
    ${SOURCES}
    ${LIB}/imgui/imgui.cpp
    ${LIB}/imgui/imgui_draw.cpp
    ${LIB}/imgui/imgui_tables.cpp
    ${LIB}/imgui/imgui_widgets.cpp
    ${LIB}/imgui/backends/imgui_impl_dx11.cpp
    ${LIB}/imgui/backends/imgui_impl_win32.cpp
    res/launcher.rc
)

target_include_directories(MonchiLauncher PRIVATE src shim ${COSMETICS} ${CMAKE_CURRENT_SOURCE_DIR}/../common ${LIB}/imgui ${LIB}/imgui/backends ${LIB}/json ${LIB}/stb)

target_compile_definitions(MonchiLauncher PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    IMGUI_DEFINE_MATH_OPERATORS
    MONCHI_VERSION="${MONCHI_VERSION}"
)

target_link_libraries(MonchiLauncher PRIVATE d3d11 dxgi d3dcompiler winhttp version bcrypt dwmapi shell32 ole32 comdlg32 advapi32 uuid)

if(MSVC)
    set_property(TARGET MonchiLauncher PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(MonchiLauncher PRIVATE /utf-8 /W3 /permissive-)
    target_link_options(MonchiLauncher PRIVATE /MANIFEST:NO)
else()
    target_compile_options(MonchiLauncher PRIVATE -Wall -Wno-unknown-pragmas -Wno-missing-field-initializers)
    target_link_options(MonchiLauncher PRIVATE -static -static-libgcc -static-libstdc++ -municode)
endif()
