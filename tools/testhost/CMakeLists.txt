cmake_minimum_required(VERSION 3.20)
project(MonchiTestHost CXX)
set(CMAKE_CXX_STANDARD 20)

# a D3D11 window that loads Monchi.dll and drives the menu (also built by tools/cross.sh for Wine)
add_executable(testhost main.cpp)
target_link_libraries(testhost PRIVATE d3d11 dxgi user32 winmm)
target_compile_definitions(testhost PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)

# the same on D3D12, where Monchi draws through D3D11On12 like in the game
add_executable(testhost12 dx12.cpp)
target_link_libraries(testhost12 PRIVATE d3d12 dxgi user32 winmm)
target_compile_definitions(testhost12 PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS NOMINMAX)

add_executable(bridge_bench bridge_bench.cpp)
target_link_libraries(bridge_bench PRIVATE d3d11 d3d12 dxgi user32)
target_compile_definitions(bridge_bench PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
