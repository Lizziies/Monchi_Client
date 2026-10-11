function(flarial_write_shims root imgui minhook stb)
    file(CONFIGURE OUTPUT "${root}/minhook/MinHook.h" CONTENT
        "#pragma once\n#include \"${minhook}\"\n" @ONLY)
    foreach(backend imgui_impl_dx11 imgui_impl_dx12 imgui_impl_win32)
        file(CONFIGURE OUTPUT "${root}/imgui/${backend}.h" CONTENT
            "#pragma once\n#include \"${imgui}/backends/${backend}.h\"\n" @ONLY)
    endforeach()
    file(CONFIGURE OUTPUT "${root}/imgui/imgui_freetype.h" CONTENT
        "#pragma once\n#include \"${imgui}/misc/freetype/imgui_freetype.h\"\n" @ONLY)
    file(CONFIGURE OUTPUT "${root}/imgui/stb.h" CONTENT
        "#pragma once\n#include \"${stb}\"\n" @ONLY)
endfunction()
